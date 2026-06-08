# Aurex M10 Supertrait Upcasting 设计基线

日期：2026-06-08

状态：**M10a Supertrait Upcasting Design Baseline** 已完成。本阶段只固定 borrowed dyn supertrait
upcasting 的语言、query、ABI、IR/backend 和测试设计，不实现 parser、sema、IR lowering、LLVM backend runtime
或标准库代码。M10a 建立在 [Aurex M9 Dyn ABI / Tooling Release Baseline](m9-release-baseline.md) 之上。

## 0. 结论

M10 选择的第一个 advanced dyn 方向是 **supertrait upcasting**，原因是它能直接复用 M8/M9 已完成的 borrowed
dyn view、origin/loan/lifetime facts、checked vtable facts 和 dyn ABI tooling facts，同时不要求标准库、owner
container、allocator 或 dynamic Drop runtime。

M10a 固定的核心语义是：

- 只设计 borrowed dyn-to-dyn coercion：`&dyn Child -> &dyn Parent` 和
  `&mut dyn Child -> &mut dyn Parent`。
- upcast 是显式 coercion site 上发生的转换，**不是普通子类型**。`dyn Child` 不在所有类型关系中自动当成
  `dyn Parent`。
- upcast 不创建 ownership，不复制对象，不延长 origin，不放宽 loan，不把 shared borrow 升级成 mutable borrow。
- upcast 保持 data pointer 不变，只把 vtable pointer 替换或投影为 parent trait 的 vtable metadata。
- 当前 `borrowed_view_v1` fat view 可以继续作为 borrowed reference 表示；当前 `borrowed_methods_only_v1`
  metadata 不足以表达 supertrait edge，必须新增 `supertrait_vptr_metadata_v1`。
- 新事实面必须包括 `TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionKey`、`DynUpcastAbiDescriptor` 和
  `VTableSupertraitEdgeDescriptor`。
- M10a 不实现标准库，不实现 `Box<dyn Trait>`，不实现 owning dyn，不实现 dynamic Drop dispatch，不实现
  multi trait composition。

一句话：M10 不是把 dyn trait 变成 Rust/Swift/C++ 的对象模型，而是在 Aurex 已有 **origin-bound erased view**
上增加一个可查询、可验证、可缓存、可投影的 borrowed supertrait view。

## 1. 当前代码事实审计

M10a 以当前仓库代码为准，不从文档或外部语言倒推。

### 1.1 Source 和 AST

`src/frontend/parse/grammar/parser_trait.cpp` 目前解析的 trait declaration 是：

```text
trait Name[Params] where ... { ... }
```

它接受 trait name、optional generic params、optional `where` constraints 和 trait body；当前没有
`trait Child: Parent` 或 direct supertrait list。`include/aurex/frontend/syntax/ast/item_nodes.hpp` 中的 trait
item shape 也没有保存 supertrait edge。

现有 `where T: A + B` 是 generic constraint，不是 trait inheritance。M10 不能把 supertrait 偷偷编码成
`where Self: Parent`，否则 parser、AST、diagnostics、query key、vtable metadata 和 tooling 都得不到稳定 edge
identity。

### 1.2 Sema trait signature

`include/aurex/frontend/sema/checked_module.hpp` 中的 `TraitSignature` 当前保存：

- trait name/module/item/visibility/stable id
- generic params
- associated type requirements
- method requirements
- source range 和 part index

它没有 direct supertrait edge、supertrait closure、edge ordinal、parent trait args 或 associated equality mapping。
这意味着 M10b 必须先补 signature/fact 层，不能从 method lookup 或 string name 临时推断 parent relation。

### 1.3 Trait object query key

`include/aurex/infrastructure/query/trait_object_key.hpp` 当前只有三类 dyn query key：

- `TraitObjectTypeKey`
  - principal trait
  - trait args
  - associated equalities
  - object origin
  - object callability schema
  - ABI policy `borrowed_view_v1`
- `VTableLayoutKey`
  - concrete type
  - target object type
  - slot schema
  - impl evidence
  - method slot count
  - metadata policy `borrowed_methods_only_v1`
- `TraitObjectCoercionKey`
  - concrete source type
  - source origin
  - target object type
  - vtable layout
  - borrow kind

`src/infrastructure/query/trait_object_key.cpp` 对 `TraitObjectCoercionKey` 的 validation 明确要求
`vtable_layout.concrete_type == source_type`，因此它只能表达 concrete-to-dyn borrowed coercion，不能表达
`dyn Child` 到 `dyn Parent` 的 upcast。

### 1.4 Sema coercion

`src/frontend/sema/facade/sema_types.cpp` 中的 `can_borrowed_dyn_trait_coerce` 当前要求 source 和 destination 都是
reference，destination pointee 是 trait object，并且 source pointee 不能已经是 `TypeKind::trait_object`。

这条规则是 M8/M9 正确的保守边界：当前语言能做 `&T -> &dyn Trait` 和 `&mut T -> &mut dyn Trait`，但不能做
`&dyn Child -> &dyn Parent`。M10b 如果实现 upcast，必须新增专门的 dyn-to-dyn coercion path，不能放宽现有
concrete coercion key。

### 1.5 Checked facts 和 ABI DTO

当前 `CheckedModule` 中与 dyn 有关的事实包括：

- `TraitObjectCallabilityFact`
- `TraitObjectMethodSlotFact`
- `VTableLayoutFact`
- `TraitObjectCoercionFact`

M9 新增的 `FunctionDynAbiFacts` 能把 object/vtable/slot/coercion/dispatch 投影为 library-independent facts，但它
只承认：

- ABI policy：`borrowed_view_v1`
- metadata policy：`borrowed_methods_only_v1`

`DynAdvancedDesignGate` 已把 `supertrait_upcasting` 标成 `design_gate`，并要求新 metadata policy
`supertrait_vptr_metadata_v1`。M10a 就是把这个 gate 变成完整设计基线。

### 1.6 IR 和 LLVM backend

当前 IR 已有：

- `trait_object_pack`
- `trait_object_data`
- `trait_object_vtable`
- `vtable_slot`
- `TraitObjectVTableLayout`

LLVM backend 目前把 borrowed dyn view lowering 为 `{data*, vtable*}`，并把 vtable global 生成为 function pointer
array。这个 array 只包含 method slots，没有 parent vtable pointer、parent projection table、drop/size/align
metadata 或 type metadata。

因此 M10c 不能在当前 array layout 上追加 hidden slot 后继续叫 `borrowed_methods_only_v1`。只要 vtable global
承载 supertrait edge，就必须切换到新 `supertrait_vptr_metadata_v1` 并由 verifier/backend negative matrix 固定。

## 2. 当前已经能做什么

M8/M9 结束后，Aurex 当前已经能稳定支持：

- `&T -> &dyn Trait` borrowed coercion。
- `&mut T -> &mut dyn Trait` borrowed coercion。
- `dyn Trait[Assoc = Type]` associated equality dispatch。
- object-callability diagnostics：缺 self receiver、非法 receiver、缺失 associated equality、unconstrained
  `Self` 等会被拒绝。
- checked vtable facts：trait object type、vtable layout、method slot、coercion fact。
- dyn receiver method call：绑定为 `TraitMethodDispatchKind::vtable_slot`。
- IR trait-object pack/extract/vtable-slot。
- LLVM `{data*, vtable*}` borrowed view 和 indirect call runtime dispatch。
- `FunctionDynAbiFacts` query/tooling projection、fingerprint、dump 和 IDE hover。
- `DynAdvancedDesignGate` 对 supertrait、owning dyn、dynamic Drop、allocator 和 multi trait composition 的设计准入。

当前还不能做：

- 不能写 `trait Child: Parent { ... }`。
- 不能从 `&dyn Child` coercion 到 `&dyn Parent`。
- 不能在 child dyn vtable 中取 parent vtable metadata。
- 不能把 parent method lookup 自动应用到 `dyn Child`。
- 不能表达 `TraitSupertraitEdgeFact` 或 `TraitObjectUpcastCoercionKey`。
- 不能生成 `trait_object_upcast` IR。
- 不能生成带 supertrait vptr metadata 的 LLVM vtable global。

## 3. 为什么 M10 先做 supertrait upcasting

M9c gate 里的五个 advanced dyn 方向不是同一类问题：

| 方向 | 是否适合 M10 第一包 | 原因 |
| --- | --- | --- |
| supertrait upcasting | 适合 | 需要新 metadata policy，但不要求标准库和 owner runtime；可复用 borrowed origin/loan/fat view。 |
| owning dyn | 不适合 | 需要 owner container、allocation、move/destroy、drop ownership policy，属于标准库/资源容器阶段。 |
| dynamic Drop dispatch | 不适合 | 需要 destructor metadata、drop glue runtime ABI、cleanup policy migration，属于 runtime 阶段。 |
| allocator policy | 不适合 | 需要标准库 allocator API、placement、ownership transfer。 |
| multi trait composition | 暂不适合 | 需要 principal set identity、namespace merge、associated equality merge，容易和 supertrait edge 混淆。 |

Supertrait upcasting 的范围足够小，但能验证 M9 的关键原则：advanced dyn 必须通过新 policy/schema/facts 进入，而不是
复用 `borrowed_methods_only_v1` 偷偷扩展。

## 4. 外部参考与取舍

M10a 参考成熟系统的教训，但不照抄模型。

- Rust RFC 3324 dyn upcasting coercion：
  https://rust-lang.github.io/rfcs/3324-dyn-upcasting.html
- Rust Reference trait objects：
  https://doc.rust-lang.org/reference/types/trait-object.html
- Rust Reference dyn compatibility：
  https://doc.rust-lang.org/reference/items/traits.html#dyn-compatibility
- Swift ABI Stability Manifesto：
  https://github.com/swiftlang/swift/blob/main/docs/ABIStabilityManifesto.md

采纳的教训：

- Upcast 应被建模为 coercion，不是普通子类型；这能避免泛型替换、overload、method lookup 和 cache identity
  被隐式类型关系污染。
- Trait object 的 vtable metadata 必须显式版本化；supertrait vptr 不是普通 method slot。
- Borrowed view 和 owning existential 是不同 ABI 家族；owning existential 会牵出 value buffer、type metadata、
  copy/destroy 和 allocator policy。
- Tooling 和 query cache 应消费 facts，不应扫描 backend dump。

明确拒绝的做法：

- 不采用 Rust lifetime surface 或 auto trait composition。
- 不把 Swift owning existential/value buffer 放进 M10。
- 不采用 Go structural interface conformance；Aurex 继续保持 nominal trait + explicit impl/coherence。
- 不采用 C++ object inheritance/vptr-in-object；Aurex 的 vtable witness 仍是 coercion 产生的 external metadata。

## 5. 语义模型

### 5.1 Upcast 的类型关系

M10 只承认 borrowed dyn-to-dyn coercion：

```aurex
trait Parent {
    fn parent(self: &Self) -> i32;
}

trait Child: Parent {
    fn child(self: &Self) -> i32;
}

fn use_parent(value: &dyn Parent) -> i32 {
    value.parent()
}

fn demo(value: &dyn Child) -> i32 {
    use_parent(value) // coercion: &dyn Child -> &dyn Parent
}
```

Mutability rules：

- `&dyn Child -> &dyn Parent` 允许。
- `&mut dyn Child -> &mut dyn Parent` 允许。
- `&dyn Child -> &mut dyn Parent` 永远不允许。
- `&mut dyn Child -> &dyn Parent` 可作为 shared target reborrow/downgrade 进入同一 upcast checker，但记录的目标
  borrow kind 是 shared；它不是把 parent view 变成 mutable。

Origin / loan rules：

- source reference 的 origin 原样进入 target reference。
- source loan 的有效期不因 upcast 变长。
- upcast 不创建新 owned value，不创建 cleanup obligation。
- upcast 不改变 M7 loan conflict matrix；target view 只是同一 borrowed storage 的窄接口。
- `&mut` upcast 只保留既有 mutable loan 的权限，不额外证明 alias freedom。

### 5.2 Method set 和 object callability

`dyn Child` 的 principal trait 仍是 `Child`。`dyn Parent` 的 principal trait 仍是 `Parent`。M10 不引入
`dyn Child + Parent` 或 principal set。

Parent method 在 child dyn value 上的处理策略：

- Sema method lookup 可以发现 `Parent` 是 `Child` 的 supertrait。
- 如果 receiver 是 `&dyn Child` 且调用的是 inherited parent method，Sema 记录一个 upcast path，再绑定 parent
  `vtable_slot` dispatch。
- IR lowering 显式产生 parent view 或 parent vtable projection，不把 parent method slot 复制进 child method table。

这样可以避免 child vtable 与 parent vtable slot 双写、避免 slot ordinal 漂移，并保持 `VTableSupertraitEdgeDescriptor`
是唯一 metadata 来源。

### 5.3 Associated equality

Supertrait edge 允许携带 parent trait args 和 associated equality mapping。规则是：

- `dyn Parent[Assoc = T]` 的 associated equality 仍必须显式或由 edge facts 可证明。
- Child object 的 associated equality 不能被无条件复制给 Parent；必须经过 `TraitSupertraitEdgeFact` 的 mapping。
- 如果多条 edge path 到同一个 parent 产生不一致 equality，M10b 必须诊断 ambiguity，不生成 upcast key。
- 如果 Parent 的 dyn-callable methods 需要 associated equality，而 target object 缺失 proof，upcast 拒绝。

## 6. Source Surface

M10 选择的 surface 是 direct supertrait clause：

```aurex
trait Child: Parent, Printable {
    fn child(self: &Self) -> i32;
}
```

设计理由：

- `:` 在 trait declaration 中表达 inheritance edge；`where` 继续表达 generic predicate。
- 每个 direct parent 有稳定 ordinal，可进入 query key 和 vtable metadata。
- 多个 direct parents 是 trait declaration graph，不是 multi trait object composition；每个 dyn object 仍只有一个
  principal trait。
- Upcast target 一次只能是一个 principal parent trait object，如 `dyn Parent`，不支持 `dyn Parent + Printable`。

M10b parser/AST 需要新增：

- `TraitSupertraitDecl`
  - parent path / qualified name
  - parent trait args
  - associated equality syntax
  - source range
  - direct edge ordinal
- `ItemNode::trait_supertraits` 或同等 field

Visibility rules：

- Parent trait 必须在 Child declaration 处可见。
- `pub trait Child: PrivateParent` 默认诊断为 public API 暴露 private supertrait。
- Cross-module re-export 后的 parent identity 必须用 resolved `DefKey`，不能用 alias text。

Cycle rules：

- 直接 cycle：`trait A: A` 拒绝。
- 间接 cycle：`A: B`, `B: C`, `C: A` 拒绝。
- cycle diagnostic 应指向 edge declaration，并给出形成 cycle 的上一条 edge note。

为什么不使用 `where Self: Parent`：

- `where Self: Parent` 没有 direct edge ordinal，不能稳定编码 vtable parent pointer table。
- 它在泛型 predicate solver 中是 obligation，不代表 `dyn Child` 的 metadata 包含 `dyn Parent` view。
- 它不能自然表达 parent method lookup、associated equality mapping 或 public API 继承关系。
- 它会让 trait inheritance 和 generic constraints 的诊断混在一起。

## 7. Query Key 和 Schema 设计

### 7.1 `TraitSupertraitEdgeFact`

M10b 需要新增 checked fact：

```cpp
struct TraitSupertraitEdgeFact {
    query::DefKey child_trait;
    query::DefKey parent_trait;
    std::vector<query::CanonicalTypeKey> parent_trait_args;
    std::vector<query::TraitObjectAssociatedTypeEqualityKey> parent_associated_equalities;
    query::StableFingerprint128 edge_fingerprint;
    query::StableFingerprint128 parent_callability_schema;
    query::StableFingerprint128 associated_equality_mapping;
    base::u32 direct_edge_ordinal;
    base::u32 closure_depth;
    bool visible_from_child_module;
};
```

Direct edge 和 transitive closure 可以分两层存储，但 public query/tooling view 必须能回答：

- Parent 是否是 Child 的 direct 或 transitive supertrait。
- Edge path 是否唯一且无 conflicting associated equality。
- Parent object type 的 callability schema 是什么。
- Parent vtable metadata 在 source vtable 中的 edge ordinal/path 是什么。

### 7.2 `TraitObjectTypeKey`

`TraitObjectTypeKey` 继续表示单 principal trait object，不变成 principal set。M10b 可以把 schema 升级到 v2，以说明
`object_callability_schema` 已经是 supertrait-aware schema。

不建议把完整 supertrait closure 直接嵌入 `TraitObjectTypeKey`，因为：

- 同一个 `dyn Child` type identity 不应因为某个 concrete impl 的 parent vtable layout 而变化。
- Upcast path 是 coercion/layout metadata，不是 object type principal identity。
- Tooling 需要能单独 diff supertrait edge facts。

### 7.3 `TraitObjectUpcastCoercionKey`

Concrete-to-dyn 的 `TraitObjectCoercionKey` 不应被复用。M10b 需要新增：

```cpp
struct TraitObjectUpcastCoercionKey {
    TraitObjectTypeKey source_object_type;
    StableFingerprint128 source_origin;
    TraitObjectTypeKey target_object_type;
    VTableLayoutKey source_vtable_layout;
    VTableLayoutKey target_vtable_layout;
    StableFingerprint128 supertrait_edge_path;
    TraitObjectBorrowKindKey borrow_kind;
    base::u32 schema;
    base::u64 global_id;
};
```

Validation 必须检查：

- source 和 target 都是 valid trait object type key。
- target principal trait 是 source principal trait 的 supertrait。
- source origin 和 target origin 一致。
- source vtable layout 使用 `supertrait_vptr_metadata_v1`。
- target vtable layout 与 target object type 匹配。
- borrow kind 不把 shared 升级成 mutable。
- edge path fingerprint 来自已验证的 `TraitSupertraitEdgeFact` closure。

### 7.4 `VTableLayoutKey`

M10c 需要允许新 metadata policy：

```cpp
enum class TraitObjectMetadataPolicyKey : base::u8 {
    borrowed_methods_only_v1 = 1,
    supertrait_vptr_metadata_v1 = 2,
};
```

`borrowed_methods_only_v1` 继续用于没有 supertrait metadata 的 M8/M9 layout。`supertrait_vptr_metadata_v1` 用于
vtable 中包含 parent vtable pointer/projection metadata 的 layout。两者不能被当成同一 ABI。

## 8. ABI 和 Metadata Model

M10 的 borrowed view representation 仍是：

```text
{ data*, vtable* }
```

`borrowed_view_v1` 表示 reference fat view 的外形，不表示 vtable 内部 metadata。M10 只新增 metadata policy：
`supertrait_vptr_metadata_v1`。

推荐 vtable metadata 模型：

- data pointer：不变，仍指向 erased concrete object。
- source vtable pointer：指向 Child layout metadata。
- parent vtable pointer：通过 Child vtable 的 supertrait edge table 取得。
- target dyn view：复用同一个 data pointer，替换为 parent vtable pointer。

M10c backend 不应依赖“parent method slots 一定是 child method slots 前缀”。前缀策略在 diamond inheritance、
generic associated equality 和 default method slot 下容易失效。推荐使用 explicit parent vtable pointer table：

```text
Child vtable under supertrait_vptr_metadata_v1
  supertrait_edges:
    edge 0 -> Parent vtable pointer
    edge 1 -> Printable vtable pointer
  method_slots:
    child-owned method slots only
```

具体 LLVM global 可以是 struct、opaque pointer table 或 backend-specific lowering，但 query facts 必须先描述
edge table 和 method table，backend 不能反向定义语义。

明确不加入的 metadata：

- drop-in-place pointer
- size / align
- allocator
- type metadata
- dynamic destructor
- owner container state
- multi principal trait set

这些属于后续 dynamic Drop、owning dyn、allocator 或 multi trait composition 阶段。

## 9. Checked Facts 和 DTO

### 9.1 `VTableSupertraitEdgeDescriptor`

M10b/M10c 需要在 checked/IR/ABI facts 中表达 vtable parent edge：

```cpp
struct VTableSupertraitEdgeDescriptor {
    query::VTableLayoutKey source_layout;
    query::VTableLayoutKey target_layout;
    query::DefKey child_trait;
    query::DefKey parent_trait;
    query::StableFingerprint128 edge_path;
    query::StableFingerprint128 associated_equality_mapping;
    base::u32 edge_ordinal;
    query::DynMetadataPolicy metadata_policy;
};
```

它的职责是证明：给定 source layout 的 vtable pointer，可以取得 target layout 的 parent vtable pointer。

### 9.2 `DynUpcastAbiDescriptor`

`FunctionDynAbiFacts` 需要新增 upcast descriptor：

```cpp
struct DynUpcastAbiDescriptor {
    query::TraitObjectUpcastCoercionKey upcast_key;
    query::TraitObjectTypeKey source_object;
    query::TraitObjectTypeKey target_object;
    query::VTableLayoutKey source_layout;
    query::VTableLayoutKey target_layout;
    query::StableFingerprint128 edge_path;
    DynBorrowKind borrow_kind;
    DynAbiPolicy abi_policy;
    DynMetadataPolicy metadata_policy;
    std::string source_reference_display;
    std::string target_reference_display;
};
```

DTO dump/summary 必须能展示：

```text
upcast &dyn Child -> &dyn Parent abi=borrowed_view_v1 metadata=supertrait_vptr_metadata_v1
```

### 9.3 `DynAdvancedDesignGate` 迁移

M9c 的 `supertrait_upcasting` candidate 目前是 design gate。M10a 完成后，后续 M10b 可以新增一个 M10-specific
stage fact，例如 `DynSelectedAdvancedDesign`，记录：

- selected capability：`supertrait_upcasting`
- selected metadata policy：`supertrait_vptr_metadata_v1`
- non-goals：`standard_library_runtime_not_in_m10`
- implementation stage：frontend/query 或 runtime/backend

M10a 不要求立即改 M9c gate DTO；它是 release baseline，应该保持可回溯。

## 10. Sema Pipeline

M10b 的 frontend/query 实现顺序应为：

1. Parser/AST 接受 `trait Child: Parent, Printable { ... }`。
2. Trait signature registration 保存 direct supertrait declarations。
3. Name resolution 将 parent path 转为 `DefKey`。
4. Visibility checker 拒绝不可见或 private-leaking parent。
5. Supertrait graph solver 做 cycle detection、duplicate direct parent detection 和 closure computation。
6. Associated equality mapper 为每条 edge/path 生成 stable mapping fingerprint。
7. Trait impl checking 把 supertrait predicates 加入 obligation：`impl Child for T` 需要可证明 `T: Parent`。
8. Object-callability schema 变成 supertrait-aware schema。
9. Borrowed dyn-to-dyn coercion checker 生成 `TraitObjectUpcastCoercionKey`。
10. Checked facts / dump / fingerprint / IDE semantic facts 投影 upcast edge。

Trait impl policy：

- `trait Child: Parent` 不自动合成 `impl Parent for T`。
- `impl Child for T` 必须在当前 coherence/evidence system 中能证明 `T: Parent`。
- 这样 parent vtable layout 来自真实 parent impl evidence，不会凭空生成 parent witness。

## 11. IR 和 Backend Plan

M10c 才实现 runtime lowering。建议新增 IR value：

```text
trait_object_upcast source_object, source_layout, target_layout, edge_ordinal
```

Lowering 规则：

- source object 必须是 borrowed dyn reference。
- data value 来自 `trait_object_data(source)`。
- parent vtable value 来自 source vtable 的 `VTableSupertraitEdgeDescriptor`。
- result type 是 target dyn reference。

Verifier negative matrix：

- `trait_object_upcast` source 不是 trait object reference。
- source layout 缺失。
- target layout 缺失。
- source layout metadata policy 不是 `supertrait_vptr_metadata_v1`。
- edge ordinal 越界。
- edge target trait 与 target object principal trait 不一致。
- source/target origin 不一致。
- shared source 被错误 upcast 为 mutable target。
- parent vtable pointer type 与 target vtable layout 不匹配。

LLVM backend：

- `borrowed_methods_only_v1` layout 可保持当前 function pointer array。
- `supertrait_vptr_metadata_v1` layout 使用 explicit metadata shape。
- `vtable_slot` emission 不应硬编码 raw array layout；应经 layout descriptor 取得 method slot table。
- `trait_object_upcast` emission 复用 data pointer，并读取/投影 parent vtable pointer。

## 12. Diagnostics

M10b/M10c 至少需要覆盖这些用户可见错误：

- unknown supertrait：`unknown supertrait 'Parent'`。
- private parent leak：`public trait exposes private supertrait`。
- duplicate direct parent：`duplicate supertrait 'Parent'`。
- supertrait cycle：`supertrait cycle detected`。
- non-trait parent：`supertrait must name a trait`。
- associated equality conflict：`supertrait associated equality is ambiguous`。
- missing parent evidence in impl：`impl Child requires Parent evidence for this type`。
- invalid upcast target：`dyn upcast target is not a supertrait`。
- mutability upgrade：`cannot upcast shared dyn reference to mutable dyn reference`。
- metadata policy mismatch：`supertrait upcast requires supertrait_vptr_metadata_v1`。

Diagnostics 应给 primary range 和至少一个 note：edge declaration range、candidate conflicting edge 或 missing impl
evidence range。

## 13. Tests 和 Coverage

M10b/M10c 的测试必须按模块拆，不再把所有 dyn trait case 塞进一个巨型测试文件。建议：

- `tests/gtest/frontend/parse/trait_supertrait_parse_tests.cpp`
- `tests/gtest/frontend/sema/trait_supertrait_facts_tests.cpp`
- `tests/gtest/frontend/sema/dyn_trait_upcast_tests.cpp`
- `tests/gtest/infrastructure/query/trait_object_upcast_key_tests.cpp`
- `tests/gtest/midend/ir/trait_object_upcast_ir_tests.cpp`
- `tests/gtest/backend/llvm/trait_object_upcast_llvm_tests.cpp`
- `tests/samples/positive/traits/trait_dyn_supertrait_upcast.ax`
- `tests/samples/negative/traits/trait_dyn_supertrait_upcast_invalid.ax`

Coverage 要求按当前仓库门槛保持 90% 以上。M10a 是文档/设计基线，新增 production C++ 代码为 0；documentation
tests 必须固定本设计中的关键词和非目标，防止后续漂移。

## 14. 分阶段实现计划和代码量预估

M10 不再拆成很多小阶段，但仍需要清楚的工程边界。建议按照四个实现包推进：

| 阶段 | 内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M10a design baseline | 本设计文档、README/progress/version/next-steps、documentation tests；不改 runtime | 700-1,100 行 |
| M10b frontend/query/sema | supertrait syntax/AST、trait graph facts、cycle/visibility/equality diagnostics、`TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionKey`、checked dump/fingerprint/tooling facts | 1,800-3,000 行 |
| M10c IR/backend runtime | `supertrait_vptr_metadata_v1` vtable layout、`VTableSupertraitEdgeDescriptor`、`trait_object_upcast` IR、verifier、LLVM lowering、runtime samples | 1,600-2,800 行 |
| M10d hardening/release | negative matrix、query/cache invalidation、IDE hover polish、coverage closure、language manual/sample/release docs | 700-1,300 行 |

合计预估：M10 完整 supertrait upcasting 需要约 4,800-8,200 行新增/修改。M10a 的代码量主要是文档和测试；如果实际
行数超过预估，通常原因会是文档测试锚点、状态入口同步或 design matrix 比预想更详细，而不是 production runtime
代码提前进入。

## 15. M10a 非目标

M10a 明确不做：

- 不实现标准库。
- 不实现 `Box<dyn Trait>`。
- 不实现 owning dyn。
- 不实现 allocator policy。
- 不实现 dynamic Drop dispatch。
- 不实现 dyn destructor ABI。
- 不实现 Drop/size/align/type metadata。
- 不实现 multi trait composition。
- 不实现 `dyn A + B`。
- 不实现 auto trait / marker trait composition。
- 不实现 structural conformance。
- 不实现 consuming receiver dyn dispatch。
- 不改变 M8/M9 已有 `&dyn Trait` / `&mut dyn Trait` 行为。
- 不复用 `borrowed_methods_only_v1` 表达 supertrait metadata。

这些不是永久禁止，而是阶段边界。M10 先把 borrowed supertrait view 做正确，再讨论 owning dyn、Drop dispatch、
allocator 或 multi trait composition。

## 16. 下一步

M10a 完成后，下一步应进入 **M10b Supertrait Frontend / Query / Sema Implementation**：

- 先实现 `trait Child: Parent` surface 和 `TraitSupertraitEdgeFact`。
- 再实现 `TraitObjectUpcastCoercionKey` 和 checked upcast facts。
- 暂不做 LLVM runtime lowering，直到 M10c。
- 保持“不实现标准库”的阶段边界。

M10b 的退出标准不是“能跑一个 happy path”，而是 parser/sema/query/fingerprint/diagnostics/coverage 都能证明
supertrait graph 和 borrowed dyn-to-dyn coercion 的事实链稳定。

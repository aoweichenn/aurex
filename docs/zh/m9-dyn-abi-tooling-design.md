# Aurex M9 Dyn ABI / Tooling 设计基线

日期：2026-06-07

状态：M9b implementation baseline 已完成。M8 release closure 已完成；M9 从 `m9` 分支开始，第一包只做 dyn ABI、
metadata、query/cache、tooling 和 verifier/backend negative matrix 的设计/实现基线，不实现标准库、
`Box<dyn Trait>`、owning dyn、allocator、dynamic Drop dispatch、supertrait upcasting 或多 trait object
composition。

## 0. 结论

M9 的目标不是把 M8 的 `&dyn Trait` 继续“加功能”，而是把已经能运行的 borrowed dyn runtime dispatch 固化成
**可被 backend、query/cache、IDE/tooling 和后续高级 dyn 设计共同消费的 ABI 事实层**。

M8 已经证明当前语言 surface 可用：

- `&dyn Trait` / `&mut dyn Trait` 可以从 `&T` / `&mut T` coercion 得到。
- `dyn Trait[Assoc = Type]` 可以把 associated equality 带入 object-callability 和 slot signature。
- checked facts 会记录 trait-object type、vtable layout、method slot 和 coercion。
- IR 有 `trait_object_pack`、`trait_object_data`、`trait_object_vtable` 和 `vtable_slot`。
- LLVM backend 会把 borrowed dyn lowering 为 `{data*, vtable*}` fat view，并通过 vtable slot indirect call 派发。
- sample/native tests 已覆盖 shared dispatch、mutable dispatch、default method slot 和 associated equality dispatch。

M9a 选择的方向是 **facts-first dyn ABI DTO**：

- 让 dyn ABI 成为稳定、可 dump、可 fingerprint、可 query 的 DTO，而不是 LLVM lowering 的私有细节。
- 保留 Aurex 的 **origin-bound erased view** 特色：dyn view 的生命周期由外层 reference origin 管，不把 dyn
  变成 owning container。
- 让 `TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey` 继续作为 query identity 输入，但新增
  面向 ABI/tooling 的投影层，避免 IDE、driver cache 或 backend 重新解释 sema/IR 内部结构。
- 第一版只固定 borrowed view ABI policy `borrowed_view_v1` 和 metadata policy
  `borrowed_methods_only_v1`；后续 advanced dyn 只能通过明确的 policy/schema gate 进入。

M9a 不改语言语义。M9b 已实现 DTO / tooling projection / invalidation tests / verifier negative tests 的第一包。
M9c 才研究 advanced dyn design，而且仍必须单独 gate。

## 1. 当前代码事实

M9 设计必须从当前代码出发，而不是从别的语言模型倒推。

### 1.1 Query identity

当前 query 层已经有三类结构化 key，位于 `include/aurex/infrastructure/query/trait_object_key.hpp`：

- `TraitObjectTypeKey`
  - principal trait：`DefKey principal_trait`
  - trait args：`std::vector<CanonicalTypeKey> trait_args`
  - associated equalities：`TraitObjectAssociatedTypeEqualityKey`
  - origin：`StableFingerprint128 object_origin`
  - callability：`StableFingerprint128 object_callability_schema`
  - policy：`TraitObjectAbiPolicyKey::borrowed_view_v1`
- `VTableLayoutKey`
  - concrete type：`CanonicalTypeKey concrete_type`
  - object type：`TraitObjectTypeKey object_type`
  - slot schema：`StableFingerprint128 slot_schema`
  - impl evidence：`StableFingerprint128 impl_evidence`
  - method slot count：`base::u32 method_slot_count`
  - policy：`TraitObjectMetadataPolicyKey::borrowed_methods_only_v1`
- `TraitObjectCoercionKey`
  - source type + source origin
  - target object type
  - vtable layout
  - borrow kind：shared / mutable

这些 key 已经有 stable serialize、fingerprint、debug string、hash、decoder layout validation 和 malformed rejection。
M9 不复活无语义形状的 `CanonicalTypeKind::trait_object` 占位；trait object identity 必须继续结构化。

### 1.2 Sema checked facts

当前 sema type 使用 `TypeKind::trait_object` 表达 dyn object；`TypeInfo` 中保存：

- `trait_object_key`
- trait name/module/name id
- trait args
- associated equalities

trait analyzer 会生成和消费：

- `TraitObjectCallabilityFact`
- `TraitObjectMethodSlotFact`
- `VTableLayoutFact`
- `VTableMethodSlotFact`
- `TraitObjectCoercionFact`

dyn receiver method call 绑定为 `TraitMethodDispatchKind::vtable_slot`，binding 记录 `VTableLayoutKey`、
slot ordinal 和 method name。M9 不能让 backend 从 method name 或 dump 文本反推 slot；slot 必须来自 checked fact。

### 1.3 IR facts

当前 IR 已经把 dyn dispatch 表达为 target-independent value kind：

- `trait_object_pack`
- `trait_object_data`
- `trait_object_vtable`
- `vtable_slot`

`TraitObjectVTableLayout` 记录：

- `query::VTableLayoutKey layout_key`
- concrete type
- object type
- vtable global symbol
- method slots

`TraitObjectVTableMethodSlot` 记录 slot ordinal、function id、function type、receiver type、return type 和 method name。
这些已经接近 M9 DTO 的内部输入，但还没有独立 ABI/tooling DTO 边界。

### 1.4 IR verifier

当前 verifier 已覆盖 M8 runtime dispatch 的核心不变量：

- vtable layout key 必须有效且唯一。
- object type 必须是 `TypeKind::trait_object`。
- method slot 数量必须匹配 layout key。
- slot ordinal 必须在范围内且不重复。
- slot function target 必须存在。
- slot function type 必须匹配 target function signature。
- erased receiver ABI 必须是 `u8` pointer，并且 mutability 兼容。
- `trait_object_pack` 的 data source type 必须匹配 layout concrete type。
- `trait_object_data` / `trait_object_vtable` 的 object 必须是 trait object reference。
- bare trait object 不能作为普通 storage type。

M9 的 verifier negative matrix 应围绕这些不变量扩展，而不是只增加 happy-path runtime sample。

### 1.5 LLVM backend

当前 LLVM backend 把 `&dyn Trait` / `&mut dyn Trait` lowering 为 `{data*, vtable*}` fat view。vtable 是 internal
global；slot entry 会被取出并转换为函数指针，最终走现有 indirect call path。

M9 设计要把这个 ABI 从 backend 私有实现提升为可查询事实：

- fat view field order
- data pointer erased type
- vtable pointer type
- vtable symbol visibility
- slot entry function pointer ABI
- receiver erasure rule
- policy/schema version

## 2. 外部参考的取舍

M9 参考外部系统只为避免踩坑，不照抄对象模型。

- Rust Reference 的 trait object 和 dyn compatibility 说明了：显式 dyn 边界、fat pointer 和 object-callability
  规则是必要的，但完整 owning dyn、drop metadata、supertrait upcasting 和 auto trait composition 会迅速扩大范围。
- Swift ABI 的 existential container / witness table 模型说明了：一旦 owning existential 进入 ABI，就必须同时处理
  value buffer、type metadata、copy/destroy 和 allocation policy。Aurex M9a 不把这些引入第一包。
- Go spec 的 interface type set 说明了：method set 可以作为动态边界的抽象，但 Aurex 已经选择 nominal trait 和
  explicit impl/coherence，所以不采用 structural conformance。
- Itanium C++ ABI 的 virtual table 规则说明了：vtable layout 一旦暴露为 ABI，就需要强约束、版本和可验证 dump；
  Aurex 不把 vptr 内嵌到 concrete object，也不引入 class inheritance object model。

M9 采纳的是工程原则：

- ABI 一旦进入跨模块/cache/tooling，就必须有 schema version。
- vtable/witness 是 evidence，不是随手生成的函数指针数组。
- owning dyn 与 borrowed dyn 必须是不同 policy，不能靠同一个 layout 隐式扩展。
- tooling 应消费稳定 facts，不应扫描 IR dump 或 backend 文本。

参考入口：

- Rust Reference: https://doc.rust-lang.org/reference/types/trait-object.html
- Rust dyn compatibility: https://doc.rust-lang.org/reference/items/traits.html#dyn-compatibility
- Swift ABI Stability Manifesto: https://github.com/swiftlang/swift/blob/main/docs/ABIStabilityManifesto.md
- Go specification interface types: https://go.dev/ref/spec#Interface_types
- Itanium C++ ABI virtual tables: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable

## 3. M9a 非目标

M9a 是设计和文档测试阶段，不实现下列内容：

- 不实现标准库。
- 不实现 `Box<dyn Trait>`。
- 不实现 owning dyn / owning existential container。
- 不实现 allocator / placement / heap object policy。
- 不实现 dynamic Drop dispatch。
- 不实现 dyn destructor ABI。
- 不实现 consuming receiver 的 dyn dispatch。
- 不实现 supertrait upcasting。
- 不实现多 principal trait object composition。
- 不实现 auto trait / marker trait composition。
- 不改变 M8 的 `&dyn Trait` / `&mut dyn Trait` 语义。
- 不把 dyn conformance 改成 structural conformance。

这些非目标不是“以后永远不做”，而是不能进入 M9 第一设计包。M9 的第一性问题是让现有 borrowed dyn ABI
可描述、可验证、可缓存、可投影。

## 4. M9a 目标

M9a 的完成标准：

- 新增 M9 设计文档，明确 ABI DTO、metadata/fingerprint schema、tooling projection、cross-module invalidation
  和 verifier/backend negative matrix。
- 更新 progress / next-steps / version / README，使 M9 成为当前主线。
- 文档测试固定 M9a 的状态、非目标和关键设计词，避免后续误把 owning dyn 或标准库塞进第一包。
- 不改变语言行为，不增加 runtime feature。

M9b 的实现入口由 M9a 设计产物决定。

## 5. ABI DTO 设计

M9b 应新增一个 library-independent dyn ABI DTO 层。建议归属优先级：

- public query-facing shape：`include/aurex/infrastructure/query/dyn_abi.hpp`
- IR extraction / adapter：`include/aurex/midend/ir/dyn_abi_facts.hpp`
- implementation：`src/midend/ir/dyn_abi/...` 或 `src/infrastructure/query/...`
- tooling projection：`src/application/tooling/...` 或现有 semantic fact 投影模块

DTO 不应该依赖 LLVM 类型，也不应该持有 sema/IR arena 指针。它只保存 stable key、stable fingerprint、symbol text、
ordinal、policy、counts 和可用于 UI/diagnostic 的轻量 display 字段。

建议第一版 DTO：

```cpp
enum class DynAbiPolicy {
    borrowed_view_v1,
};

enum class DynMetadataPolicy {
    borrowed_methods_only_v1,
};

enum class DynBorrowKind {
    shared,
    mut,
};

struct DynObjectAbiDescriptor {
    query::TraitObjectTypeKey object_key;
    query::StableFingerprint128 descriptor_fingerprint;
    query::StableFingerprint128 origin_fingerprint;
    query::StableFingerprint128 callability_schema;
    DynAbiPolicy abi_policy;
    base::u32 trait_arg_count;
    base::u32 associated_equality_count;
};

struct DynVTableSlotAbiDescriptor {
    base::u32 slot;
    query::StableFingerprint128 method_member_fingerprint;
    query::StableFingerprint128 function_signature_fingerprint;
    query::StableFingerprint128 receiver_abi_fingerprint;
    std::string method_name;
    bool receiver_is_mutable;
    bool bound_to_default_method;
};

struct DynVTableAbiDescriptor {
    query::VTableLayoutKey layout_key;
    query::StableFingerprint128 descriptor_fingerprint;
    query::StableFingerprint128 impl_evidence;
    query::StableFingerprint128 slot_schema;
    DynAbiPolicy abi_policy;
    DynMetadataPolicy metadata_policy;
    std::string symbol;
    base::u32 method_slot_count;
    std::vector<DynVTableSlotAbiDescriptor> slots;
};

struct DynCoercionAbiDescriptor {
    query::TraitObjectCoercionKey coercion_key;
    query::StableFingerprint128 descriptor_fingerprint;
    DynBorrowKind borrow_kind;
    bool source_is_mutable_reference;
};
```

命名可以在实现前微调，但边界必须保持：

- DTO 面向 query/tooling/backend 共同消费。
- DTO 不包含 LLVM `Type*` / `Value*`。
- DTO 不包含 raw `TypeHandle`，除非明确是 IR-local adapter 而非 query-facing DTO。
- DTO fingerprint 只由 stable key、schema、policy、slot schema、impl evidence、symbol text 和 slot descriptors 组成。

## 6. Borrowed View ABI v1

当前唯一正式 ABI policy 是 `borrowed_view_v1`。

布局：

```text
BorrowedDynViewV1 {
  data: *u8 or *mut u8
  vtable: *const DynVTableV1
}
```

语义：

- view 不拥有 `data`。
- view 不负责 drop `data`。
- view 的 origin 来自外层 reference。
- `&dyn Trait` 只能进行 shared receiver dispatch。
- `&mut dyn Trait` 可以进行 mutable receiver dispatch，但仍不拥有 concrete receiver。
- vtable pointer 指向 checked vtable witness。
- vtable witness 当前只包含 method slot entries，不包含 drop/size/align/supertrait metadata。

receiver erasure：

- slot target 的第一参数是 erased receiver。
- verifier 当前要求 erased receiver 是 `u8` pointee pointer。
- const erased receiver 可兼容 mutable/const receiver 的只读路径；mutable receiver 不能伪装成 shared source 之外的 owning access。

backend lowering：

- `trait_object_pack` 生成 fat view。
- `trait_object_data` 提取 data pointer。
- `trait_object_vtable` 提取 vtable pointer。
- `vtable_slot` 从 vtable 中取 slot function pointer。
- `call` 使用 indirect call 路径。

M9b 的 DTO 要把这些规则变成 dumpable facts。

## 7. Metadata Schema

M9b 应显式定义 metadata schema，而不是让 metadata 隐式藏在 key 名称里。

当前 schema：

```text
DynMetadataSchema borrowed_methods_only_v1
  abi_policy = borrowed_view_v1
  entries:
    method_slot[0..N)
  absent:
    drop_in_place
    size
    align
    supertrait_vptr
    type_metadata
    allocator
```

这意味着：

- M8/M9b 的 vtable 不能被 owning dyn 复用。
- backend 不能假设 slot 0 是 drop 或 metadata。
- tooling 不能提示“dyn object has dynamic drop”。
- future owning dyn 必须新增 metadata policy，例如 `owning_boxed_v1`，不能改变
  `borrowed_methods_only_v1` 的含义。

## 8. Fingerprint Schema

M9b 的 fingerprint 设计应分层。

### 8.1 Object descriptor fingerprint

输入：

- schema version
- ABI policy
- principal trait def key
- trait args canonical keys
- associated equality member/value canonical keys
- object origin fingerprint
- object-callability schema fingerprint

变化影响：

- 改 trait identity、trait args、associated equality、origin schema 或 callability schema，会 invalid object descriptor。
- 只改 impl body，不应 invalid object descriptor。

### 8.2 VTable descriptor fingerprint

输入：

- schema version
- ABI policy
- metadata policy
- concrete type canonical key
- object descriptor fingerprint
- slot schema fingerprint
- impl evidence fingerprint
- method slot count
- slot descriptor fingerprints
- vtable symbol text

变化影响：

- 改 impl selection、default method binding、slot signature、slot count 或 symbol，会 invalid vtable descriptor。
- 只改方法 body但 signature/evidence 不变时，需要 invalid lowered function body，但不一定 invalid vtable descriptor；
  这一点要由 impl evidence 是否包含 body fingerprint 来决定。M9b 必须明确当前 evidence 的粒度。

### 8.3 Coercion descriptor fingerprint

输入：

- schema version
- borrow kind
- source type canonical key
- source origin fingerprint
- target object descriptor fingerprint
- vtable descriptor fingerprint

变化影响：

- 改 source type、origin、target object 或 vtable，会 invalid coercion。

### 8.4 Tooling projection fingerprint

输入：

- object descriptor fingerprint
- vtable descriptor fingerprint
- coercion descriptor fingerprint
- source range / stable symbol relation
- diagnostic-facing display fields

变化影响：

- source edit 只影响 range/display 时，IDE semantic fact 可以局部更新。
- ABI key 变化时，workspace index 必须重新关联 dyn facts。

## 9. Tooling Projection

M9b 应让 IDE/tooling 获取 dyn facts，而不是扫描 dumps。

建议新增 tooling facts：

```text
dyn_object_type
  stable object key
  principal trait display
  associated equalities
  origin fingerprint
  abi policy
  callability schema

dyn_vtable_layout
  stable layout key
  concrete type display
  object type display
  symbol
  metadata policy
  slot count

dyn_vtable_slot
  layout key
  slot ordinal
  method name
  receiver access
  target function stable symbol
  default-method-or-impl-override origin

dyn_coercion
  coercion key
  borrow kind
  source type display
  target object display
  source range
```

IDE behavior:

- hover on `&dyn Trait` shows `dyn abi=borrowed_view_v1 metadata=borrowed_methods_only_v1`.
- hover on dyn method call shows `dispatch=vtable_slot slot=N`.
- go-to-definition on dyn method call can show trait requirement and concrete selected impl when current module facts are available.
- semantic facts can expose coercion source range, vtable layout key and object key.
- incremental reuse plan can report whether a dyn fact was reused, invalidated by trait signature, invalidated by impl evidence, or invalidated by body-local source edit.

M9b 不要求 LSP 协议新增复杂 UI；先让 `IdeSnapshot` / semantic facts / hover text 有可测试输出即可。

## 10. Cross-Module Invalidation Matrix

M9b 的核心质量点是 invalidation。建议测试矩阵如下：

| 变化 | Object descriptor | VTable descriptor | Coercion descriptor | Lowered caller | Tooling fact |
| --- | --- | --- | --- | --- | --- |
| 改 trait 名称/DefKey | invalid | invalid | invalid | invalid | invalid |
| 改 trait method signature | invalid | invalid | invalid | invalid | invalid |
| 新增可调用 trait method slot | invalid | invalid | invalid | invalid | invalid |
| 改 associated type requirement | invalid | invalid | invalid | invalid | invalid |
| 改 dyn associated equality value | invalid | invalid | invalid | invalid | invalid |
| 改 impl method body，不改 signature | same 或 invalid by evidence policy | same 或 invalid by evidence policy | same 或 invalid by evidence policy | invalid target body | updated |
| 改 impl method signature | invalid evidence | invalid | invalid | invalid | invalid |
| 删除 impl | object same | invalid | invalid | invalid with diagnostic | invalid |
| 改 source body中 `&T -> &dyn Trait` coercion point | object same | vtable same | invalid range/source relation | invalid caller | invalid local fact |
| 改 only comments/trivia | same | same | same | reusable | reusable |
| 改 import path / package identity | package-scoped invalid | package-scoped invalid | package-scoped invalid | invalid as needed | invalid as needed |

M9b 要特别关注跨模块：

- imported trait signature change
- imported impl change
- root caller body edit
- package manifest/source-root identity change
- query replay malformed dyn stable key rejection

## 11. Verifier And Backend Negative Matrix

M9b 应新增 focused negative tests。建议覆盖：

### 11.1 Query/DTO validation

- invalid object descriptor schema。
- unknown ABI policy。
- object descriptor 缺 principal trait。
- associated equality 未排序或重复。
- vtable descriptor 的 slot schema 与 object callability schema 不一致。
- vtable descriptor 的 metadata policy 与 ABI policy 不匹配。
- coercion descriptor 的 source origin 与 target object origin 不一致。
- coercion descriptor 的 vtable object type 与 target object type 不一致。

### 11.2 IR verifier

- duplicate vtable layout global id。
- method slot count mismatch。
- duplicate slot ordinal。
- slot ordinal out of range。
- slot function target out of range。
- slot function type mismatch。
- erased receiver 不是 `*u8` / `*mut u8`。
- `trait_object_pack` data pointee 与 concrete type 不匹配。
- `trait_object_data` object 不是 trait object reference。
- `trait_object_vtable` result 不是 pointer。
- `vtable_slot` object 不是 pointer。
- bare trait object 出现在 storage type。

### 11.3 Backend

- missing vtable layout should fail before LLVM emission or emit structured diagnostic。
- vtable symbol collision should be deterministic and diagnosable。
- invalid slot pointer type should be rejected by verifier before backend。
- backend dump should expose `{data*, vtable*}` shape in a stable test。

## 12. Module Boundaries

M9b 不应把 DTO 逻辑塞进一个现有大文件。建议边界：

- query key and DTO schema：query/infrastructure 层。
- checked facts to DTO：sema 或 checked-module adapter。
- IR vtable layout to ABI DTO：midend/ir dyn ABI facts adapter。
- verifier negative：midend/ir verifier tests。
- backend consumption：LLVM backend 只消费 validated IR/DTO，不重新做 sema proof。
- tooling projection：tooling semantic fact adapter。

如果新增 `internal/`，只能作为 implementation root，下面继续拆 role-specific 子目录，例如：

```text
src/midend/ir/dyn_abi/
  sources/facts.cpp
  sources/dump.cpp
  private/dyn_abi_facts.hpp
```

## 13. Diagnostics

M9b 的用户可见 diagnostics 不需要新增大量文案，但内部错误必须结构化：

- dyn ABI descriptor invalid
- dyn vtable descriptor invalid
- dyn coercion descriptor invalid
- dyn ABI policy unsupported
- dyn metadata policy unsupported
- dyn vtable slot descriptor mismatch

原则：

- sema 阶段报用户源代码错误。
- IR verifier 报 compiler-internal invariant 错误。
- backend 不吞掉 verifier 能发现的问题。
- tooling fact 缺失时显示 unsupported/unknown，不伪造可用 dyn ABI。

## 14. M9a 到 M9c 路线

| 阶段 | 内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M9a design baseline | 本文档、progress/next-steps/version/README 更新、documentation tests | 500-900 行 |
| M9b ABI/tooling implementation | query-facing DTO、checked/IR adapter、dump/fingerprint、tooling projection、invalidation tests、verifier/backend negative tests；已完成 | 1,000-2,000 行 |
| M9c advanced dyn design | owning dyn / dynamic Drop / supertrait upcasting / trait-object composition 的独立设计与小原型 gate；继续不实现标准库 | 2,500-4,500 行 |

M9b 实际实现的主要代码量来源：

- DTO schema 与 validation。
- stable fingerprint、summary、dump。
- checked adapter 与 function-local IR adapter。
- lower-IR query/cache invalidation 和 driver subject 传递。
- IDE semantic fact、hover 和 reuse/index kind。
- query、sema、IR、tooling、documentation tests。

如果实际代码量超出预估，最可能的原因是：

- 现有 tooling semantic fact 结构需要拆分以承载 dyn facts。
- current impl evidence 粒度不足，需要先补 stable evidence DTO。
- verifier negative helper 需要新 test fixture。
- cross-module incremental tests 需要更多 driver harness。

如果实际代码量低于预估，通常意味着 M9b 只实现 query/IR DTO，没有完成 tooling projection 或 invalidation matrix；
这种情况不能算 M9b 完整收口。

## 15. 验收门槛

M9a：

- 文档测试固定 M9 入口、非目标、DTO、invalidation matrix 和 verifier/backend negative matrix。
- `aurex_tests` 通过。
- `aurex_frontend_tests` 通过。
- coverage gate 保持 90% 以上。

M9b：

- DTO validation 有 positive/negative unit tests。已完成。
- checked/IR adapter 有 dump/fingerprint tests。已完成。
- tooling projection 有 IDE semantic fact/hover tests。已完成。
- lower-IR invalidation 有 query/driver subject tests。已完成。
- verifier/backend negative matrix 有 focused tests。已覆盖当前 borrowed ABI facts 相关路径。
- coverage gate 保持 90% 以上。

M9c：

- 先写 design review，不默认实现。
- 每个 advanced dyn 方向必须有独立非目标、ABI impact、borrow/drop/resource impact、tooling/cache impact 和 tests。
- 标准库仍是独立阶段，不在 M9c 自动实现。

## 16. 当前阶段能做什么

M9b 完成后，仓库具备：

- M8 release closure 的正式封口状态。
- M9 dyn ABI / tooling 的正式设计入口和第一包实现基线。
- 清晰的 M9 非目标，避免标准库或 owning dyn 混入第一包。
- `FunctionDynAbiFacts` DTO：object、vtable、slot、coercion 和 dispatch descriptor。
- 当前唯一 ABI/metadata policy：`borrowed_view_v1` / `borrowed_methods_only_v1`。
- checked facts 到 ABI facts 的 adapter：可从 `TraitObjectTypeKey`、`VTableLayoutKey`、
  `TraitObjectCoercionKey`、method slot 和 dyn dispatch binding 投影。
- IR facts 到 ABI facts 的 adapter：按函数 value closure 提取 `trait_object_pack/data/vtable/vtable_slot`
  实际使用的 layout 和 slot。
- lower-function-IR result fingerprint 会混入 cleanup facts 与 dyn ABI facts。
- IDE semantic facts / hover 可显示 `abi=borrowed_view_v1`、`metadata=borrowed_methods_only_v1` 和
  `dispatch=vtable_slot slot=N`。
- 文档测试防漂移。

M9b 完成后，仓库仍不会新增：

- 新语言语法。
- 新标准库 API。
- owning dyn runtime。
- dynamic Drop dispatch。
- allocator 或 `Box`。
- supertrait upcasting。
- 多 trait object composition。

这是刻意的：M9 先把“已经能跑的 borrowed dyn”变成工程上能长期维护、缓存、调试和扩展的 ABI/tooling 基线。

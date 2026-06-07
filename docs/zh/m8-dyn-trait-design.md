# Aurex M8 Dyn Trait、Erased View 与动态派发设计基线

日期：2026-06-07

状态：M8a Borrowed Erased Trait View query foundation 已完成；代码层已移除 query canonical type 中无语义形状的
`trait_object` 占位，并新增结构化 trait object / vtable / coercion query identity，避免把错误稳定 key
形态带入 M8。

## 0. 结论

M8 的主线不是“照抄 Rust `dyn Trait`”，也不是把 C++ virtual、Go interface 或 Swift existential 直接搬进
Aurex。Aurex 需要的是一套服务于现有语言地基的 **erased trait view**：

- 以 M7 已完成的 origin / loan / lifetime / resource facts 为边界。
- 第一版只开放 borrowed dyn：`&dyn Trait` 与 `&mut dyn Trait`。
- dyn object 默认是运行时多态的“借用视图”，不是 owning 容器。
- vtable 是由 checked trait facts 生成的稳定 witness，不是 class inheritance object model。
- query/cache/tooling 必须先有稳定事实和 fingerprint，再让 IR/backend 消费。
- 当前阶段继续不实现标准库，不实现 `Box<dyn Trait>`，不实现 owning existential container。

M8 第一刀建议命名为 **M8a Borrowed Erased Trait View**。用户表面可以保留 `dyn Trait` 这个行业通用词，
但内部模型不叫“任意 trait object”，而叫 **origin-bound erased view**：一个带 origin 的引用，引用目标的
concrete type 被擦除，方法调用通过 checked vtable slot 间接派发。

## 1. 当前代码事实

当前代码不具备 dyn trait 闭环，不能把 M8 当成“补一个 parser 分支”。

- syntax type kind 只有 `primitive / named / pointer / reference / array / slice / tuple / function`，
  没有 dyn trait type node。
- type payload arena 只存 named、pointer、reference、array、slice、tuple、function payload，没有 trait
  object payload。
- parser `parse_type_atom()` 只接受 function type、tuple/paren、primitive 和 named type；frontend 没有
  `kw_dyn`。
- Sema `TypeKind` 没有 trait object，`TypeTable` 也没有 principal trait、associated equality、object
  origin/lifetime、vtable key 等字段。
- trait checked facts 目前支持 static predicate/evidence/impl/default method dispatch；
  `TraitMethodDispatchKind` 只有 `param_env / impl_override / trait_default`。
- IR 当前有 function value indirect call，但没有 trait object construct/extract、vtable symbol、slot load、
  erased receiver ABI 或 fat pointer invariant。
- query 层曾经预留 `CanonicalTypeKind::trait_object`，但没有 principal trait、origin/lifetime、assoc equality
  或 vtable identity，只是 0-child key。这个设计已经在 M8 第一改中移除。

这个移除是故意的：如果现在保留一个无信息 `trait_object` 稳定 key，后续一旦 cache、generic instance、
drop glue 或 tooling 消费它，就会把 dyn trait 的身份错误地固定成“只有一个 kind tag”。真正可用的 trait
object canonical key 至少必须表达 principal trait、trait args、associated equality、object lifetime/origin
和 vtable layout identity。

## 2. 外部系统调研

### 2.1 Rust：显式 dyn 边界和 dyn compatibility

Rust Reference 把 trait object 定义为实现一组 traits 的 opaque value；这个集合由一个 dyn-compatible base
trait、auto traits 和 lifetime bound 组成。trait object 通常位于 pointer 后面；指针里包含 concrete value
指针和 vtable 指针，方法调用从 vtable 加载函数指针后间接调用。

Rust 的教训：

- 好处：`impl Trait` / `dyn Trait` 把静态抽象和动态抽象分开，边界清楚。
- 好处：dyn compatibility 规则能防止无法对象化的方法进入 vtable。
- 代价：规则复杂，尤其是 `Self`、associated items、generic methods、supertraits、drop metadata、upcasting。
- 对 Aurex 的采纳：采用显式 dyn 边界和 object-callability 规则。
- 对 Aurex 的拒绝：不照搬 Rust lifetime 语法；不第一版支持完整 owning dyn、auto traits、supertrait upcasting。

Rustc 源码中的 vtable entry 包含 drop-in-place、size、align、method、vacant slot 和 supertrait vptr。这个事实说明：
即使 Aurex 第一版只做 borrowed dyn，vtable key 也不能设计成“方法指针数组”这么薄；后续 owning dyn 和
supertrait 扩展会需要元数据位。

### 2.2 Swift：existential container 与 witness table

Swift ABI 文档把 protocol / protocol composition / `Any` 值布局为 existential container；非 class-bound
existential 需要容纳任意大小的值，可能包含 value buffer、type metadata 和 witness tables。

Swift 的教训：

- 好处：existential 可以承载 owning value，表达能力强。
- 代价：容器布局、side allocation、metadata、witness table、copy/destroy 规则会迅速成为 ABI 主线。
- 对 Aurex 的采纳：witness table / vtable 作为 conformance evidence 的工程模型值得参考。
- 对 Aurex 的拒绝：M8a 不做 owning existential container，不做任意大小值内联/装箱，不把标准库/allocator 拉进来。

### 2.3 Go：method set / type set 与隐式实现

Go spec 中 interface 由 type set 定义，非空 interface 的 type set 是满足方法集的非 interface 类型集合；
类型是否实现 interface 由 method set/type set 关系决定。

Go 的教训：

- 好处：结构化 interface 对插件式边界很轻，调用方不需要显式 impl 声明。
- 代价：Aurex 当前 trait 是 nominal static trait，已经有显式 impl/coherence/fingerprint。改成结构化
  interface 会破坏 M4-M7 的 trait evidence、orphan/coherence 和 query identity。
- 对 Aurex 的采纳：method-set 的“只把可调用方法纳入动态边界”思想。
- 对 Aurex 的拒绝：不做隐式 structural conformance，不让 dyn trait 绕过现有 impl/coherence。

### 2.4 C++ ABI：vtable 一旦冻结，维护成本很高

Itanium C++ ABI 详细规定 virtual table layout、address point、offset-to-top、typeinfo pointer 和 virtual
function pointer 顺序。这个系统强大，但会把 inheritance、RTTI、object layout、construction/destruction
规则紧紧绑在一起。

C++ 的教训：

- 好处：ABI 规则完整，适合 class inheritance object model。
- 代价：一旦暴露对象模型和 vtable layout，后续改动代价极高。
- 对 Aurex 的采纳：vtable layout 必须稳定、可验证、可 dump。
- 对 Aurex 的拒绝：不引入 class-first inheritance，不把 concrete object layout 改造成每对象内置 vptr。

## 3. Aurex 的特色设计

### 3.1 Origin-bound erased view

M8a 的 dyn trait 不是 owning value，而是带 origin 的 erased view。

概念形态：

```aurex
trait Draw {
    fn draw(self: &Self) -> void;
}

fn render(x: &dyn Draw) -> void {
    x.draw();
}
```

语义解释：

- `&dyn Draw` 是一个 shared borrowed erased view。
- `&mut dyn Draw` 是一个 mutable borrowed erased view。
- concrete `T` 必须有 checked evidence 证明 `T: Draw`。
- coercion 点把 `&T` 或 `&mut T` 转成 `{data_ptr, vtable_ptr}`。
- object lifetime/origin 由外层 reference 的 origin 管理，复用 M7 `&[origin] T` / `&mut[origin] T` 地基。
- dyn view 不拥有被擦除值，因此 M8a 不需要 allocator、Box 或 owning drop dispatch。

这和 Rust 的 `&dyn Trait` 表面接近，但 Aurex 的差异在于：origin 是 Aurex 自己的 `&[origin]` surface 与
borrow summary facts，不采用 apostrophe lifetime 泛型作为第一表达。

### 3.2 Static trait 与 dyn trait 分层

当前 static trait 保持不变：

- generic bound：`where T: Trait`
- static dispatch：trait method call resolved 到 impl/default/direct function
- associated projection：通过 checked associated type equality resolved

dyn trait 是显式边界：

- 只有写出 `dyn Trait` 才进入 runtime dispatch。
- dyn 不改变 impl/coherence 规则。
- dyn 不让 structural method shape 自动成为 conformance。
- dyn method call 产生独立 dispatch kind：`vtable_slot` 或等价枚举。

### 3.3 Vtable 是 checked witness，不是 class object layout

Aurex 不给每个 struct 内嵌 vptr。vtable 只在 coercion 到 dyn view 时出现。

建议 vtable key 组成：

- concrete self type canonical key
- principal trait stable def key
- trait generic args canonical keys
- associated type equality canonical keys
- object-safe method slot schema fingerprint
- ABI policy version
- future metadata policy flags：drop/size/align/supertrait-vptr 是否存在

M8a 可以先不使用 drop slot，但 key 必须为后续扩展保留有语义的 policy version，而不是无信息 kind tag。

### 3.4 Object safety 改名为 object callability

Rust 叫 object safety / dyn compatibility。Aurex 文档建议对用户使用“dyn-compatible”，内部实现命名为
`ObjectCallability`，原因是 M8a 检查重点不是“对象是否安全”，而是“哪些 trait requirements 能被 erased
receiver 调用”。

M8a 初始规则：

- trait 不能要求 `Self: Sized` 或等价 sized-only capability。
- dispatchable method 必须有 self receiver。
- receiver 只允许 shared / mutable borrowed receiver：`&Self`、`&mut Self`。
- method 不能有 method-level generic params。
- method 参数和返回类型不能裸用 `Self`，除 receiver 位置外。
- associated type requirement 必须全部在 dyn type 上显式约束，或者该 trait 暂时不能 dyn 化。
- static associated function 不进入 vtable，除非标记为 non-dispatchable。
- default method 可以进入 vtable，但 vtable slot 必须绑定到当前 impl/default instantiated function。

M8a 暂不支持：

- consuming receiver
- owning dyn value
- dyn destructor dispatch
- supertrait upcasting
- multi-principal trait object
- auto trait / marker trait object composition
- generic associated types
- associated constants

## 4. Pipeline 设计

### 4.1 Lexer / Parser / Syntax

新增：

- `kw_dyn`
- `syntax::TypeKind::dyn_trait` 或 `trait_object`
- payload：principal trait path、type args、associated equality constraints、range

建议语法：

```aurex
&dyn Draw
&mut dyn Draw
&[view] dyn Draw
&mut[view] dyn Draw
dyn Iterator[Item = i32]
```

解析约束：

- `dyn` 只在 type context 中解析。
- bare `dyn Trait` 作为 DST-like unsized type，M8a 只允许作为 reference pointee。
- associated equality 复用现有 `Name[T]`/constraint 风格时要避免和 generic type args 混淆；第一版可以只支持
  already existing associated equality predicate 形态，不急着加花哨语法。

### 4.2 Sema Type

新增 sema type kind 时，不能只加枚举；必须同时加完整 identity：

- principal trait：module + stable def id + name id
- trait args：`TypeHandleList`
- associated equalities：member key + value type
- object origin/lifetime：外层 reference origin 优先；bare dyn type 可记录 default/unknown origin policy
- object ABI policy：M8a borrowed-only

`TypeTable::same`、`display_name`、`canonical_type_builder` 必须同批实现。

### 4.3 Trait Analyzer

新增分析：

- object-callability collector：按 trait signature 生成 method slot schema。
- dyn compatibility diagnostics：指出具体不兼容 requirement。
- vtable layout facts：为 `(concrete type, trait object type)` 生成 slot binding。
- method binding：`TraitMethodDispatchKind::vtable_slot`，binding 记录 slot ordinal 和 vtable key。

### 4.4 Borrow / Lifetime

M8a 复用 M7 facts：

- `&dyn Trait` 创建 shared loan。
- `&mut dyn Trait` 创建 mutable loan。
- dyn method receiver access 使用 existing `ReceiverAccessKind`。
- returned dyn reference 的 summary 记录 origin dependency。
- dyn view 不拥有 storage，因此 cleanup 不 drop erased value。

重点风险：

- 不允许 dyn view 从 local temporary 逃逸。
- 不允许通过 dyn mutable receiver 与现有 shared/mutable loan 冲突。
- 不把 raw pointer 派生的 erased view 纳入 safe proof。

### 4.5 IR

M8a 新 IR 不应直接把 vtable slot call 写成普通 call 的特殊名字。建议显式 IR facts：

- `trait_object_pack`：data pointer + vtable pointer -> dyn view
- `trait_object_data`：extract data pointer
- `trait_object_vtable`：extract vtable pointer
- `vtable_slot`：vtable pointer + slot ordinal -> function pointer
- `call`：复用已有 indirect function call 作为最后一步

这样 IR verifier 可以检查：

- dyn view 必须是双指针布局或目标定义的等价 layout。
- slot ordinal 必须来自 checked vtable layout fact。
- receiver data pointer 类型和 slot function receiver ABI 匹配。

### 4.6 Backend

M8a backend 只需要 borrowed dyn method call：

- emit vtable global constant
- vtable entries 指向 concrete impl/default function
- coercion 生成 `{data*, vtable*}`
- dyn call 提取 slot function pointer并 indirect call

不做：

- allocator
- owning dyn move/copy/destroy
- dynamic drop glue
- side allocation
- panic/unwind cleanup ABI

## 5. Query / Cache / Tooling

M8a 已经移除了错误的 `CanonicalTypeKind::trait_object` 占位，并新增独立 query identity：

- `TraitObjectTypeKey`：表示 borrowed erased view 的类型身份，包含 principal trait、trait args、
  associated type equality、object origin、object-callability schema fingerprint 和 ABI policy。
- `VTableLayoutKey`：表示 `(concrete type, trait object type)` 的 checked vtable witness 身份，包含 concrete
  canonical type、object type、slot schema、impl evidence、method slot count、metadata policy 和 ABI policy。
- `TraitObjectCoercionKey`：表示 `&T` / `&mut T` 到 borrowed dyn view 的 coercion 身份，包含 source type、
  source origin、target object type、vtable layout 和 borrow kind。

这三类 key 是分开的：canonical type、vtable layout、conformance/coercion evidence 不混塞进
`CanonicalTypeKey::children`。后续重新引入 trait object canonical type 时必须满足：

- 有公开 constructor，不允许测试手写 kind tag 当 valid key。
- decoder 验证 trait object key 的 child count 和 header fields。
- stable key 包含 principal trait 和 object-callability schema fingerprint。
- generic instance / drop glue / vtable key 之间 identity 不混淆。
- IDE hover/index 可以展示 dyn method dispatch fact 和 vtable fingerprint。

M8a decoder 会拒绝无效 schema、无效 policy、非 trait principal、非 associated type member、非法 canonical
type child 和三类 key 布局混用。associated equality 在 constructor 中按 member identity 归一化，保证用户书写顺序
不会污染 stable fingerprint。

## 6. 分阶段路线

当前状态：M8a-M8c 已完成。M8 当前能做 frontend/query 层 borrowed dyn trait：`dyn Trait` 类型语法、
object-callability 诊断、`&T` / `&mut T` 到 `&dyn Trait` / `&mut dyn Trait` 的 coercion、checked vtable
layout facts，以及 dyn receiver method call 的 `vtable_slot` binding。当前阶段继续不实现标准库，不做
IR/backend runtime dynamic dispatch，不做 owning dyn、dynamic Drop dispatch、supertrait upcasting 或多 trait
object composition。

### M8a：设计基线与 query 地基

目标：

- 固定 M8 dyn trait 设计。
- 移除无语义 trait object canonical key。
- 增加 object-callability 规则设计与测试计划。
- 增加 `TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey` 的结构化 query identity。
- 增加 stable key decoder layout 校验和 malformed key 测试。
- 不实现语言 surface。

验收：

- `docs/zh/m8-dyn-trait-design.md`
- query tests 不再承认无信息 `trait_object` key。
- query tests 覆盖 trait object type / vtable layout / coercion key 的稳定序列化、hash、debug、identity decode
  和 malformed layout rejection。
- full query/frontend tests 通过。

### M8b：syntax + sema type + object-callability diagnostics

状态：已完成。

目标：

- `dyn Trait` 类型能 parse / AST dump / sema resolve。
- 只能出现在 reference pointee 或显式 allowed context。
- object-callability diagnostics 完整：缺少 self receiver、非法 receiver、缺失/未知/重复 associated equality、
  unconstrained `Self` 都会被诊断；显式约束的 `Self.Assoc` 会被 `dyn Trait[Assoc = Type]` 接受并替换。
- 不做 backend dispatch。

验收：

- parser positive 已覆盖 `&dyn Draw`、`&mut dyn Draw`、qualified `dyn core.iter.Iterator[...]` 和
  associated equality AST dump。
- sema whitebox 覆盖 object-safe trait、mutable receiver、associated equality return substitution、bare dyn
  storage rejection、missing/unknown/duplicate associated equality、invalid receiver、`Self` return 和 missing impl。
- canonical / query identity 使用 M8a 结构化 `TraitObjectTypeKey`，不复活 0-child canonical trait object key。

### M8c：borrowed dyn coercion + checked vtable facts

状态：已完成。

目标：

- `&T -> &dyn Trait`
- `&mut T -> &mut dyn Trait`
- checked vtable layout facts。
- method binding 产生 `vtable_slot` dispatch。

验收：

- borrowed dyn coercion 会检查可见 nominal impl、trait args 和 associated equality，失败时给出 dyn trait impl
  missing 诊断。
- checked facts 覆盖 `TraitObjectCallabilityFact`、`TraitObjectMethodSlotFact`、`VTableLayoutFact` 和
  `TraitObjectCoercionFact`；CheckedModule copy/move/swap、stable fingerprint 和 dump 已纳入新 facts。
- trait method binding whitebox 覆盖 shared/mutable dyn receiver、`vtable_slot` ordinal、receiver access 和
  associated equality return substitution。
- query authority 已混入 M8 fact counts；backend dynamic dispatch 仍留给 M8d。

### M8d：IR/backend dynamic dispatch

目标：

- IR 显式表达 trait object pack/extract/vtable slot。
- LLVM backend 生成 vtable global 和 indirect call。
- execution tests 验证不同 concrete type 经同一 dyn call 得到不同实现。

验收：

- IR verifier 覆盖非法 slot、非法 receiver、非法 vtable。
- backend execution tests。
- coverage source lines/functions/regions 均达 90% 以上；这是最低 release gate，新增测试仍优先覆盖
  正确性、错误路径和 query/前端边界。

### M8e：hardening 和后续扩展评估

候选：

- associated equality 更完整 surface。
- default method slot hardening。
- supertrait slot layout。
- owning dyn 设计研究。

### 剩余阶段代码量预估

这是基于当前代码结构和 M8a-M8c 已完成 frontend/query foundation 后的粗估。实际代码量会受 IR verifier
粒度、LLVM ABI 测试数量、execution samples、incremental/tooling 投影和 coverage 补洞影响：

| 阶段 | 主要内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M8d | IR pack/extract/vtable-slot、verifier、LLVM vtable global 与 indirect dispatch、execution tests | 2,800-4,200 行 |
| M8e | hardening、incremental invalidation、default method slot 边界、associated equality surface 补齐、性能/coverage 收口 | 1,200-2,000 行 |

如果 M8d 提前纳入 dynamic Drop、supertrait upcasting 或 owned dyn，代码量会明显上浮；这些仍按非默认项处理。

仍不默认进入 M8：

- 标准库 `Box`
- owning dyn
- dynamic Drop dispatch
- allocator
- trait upcasting
- multi-trait object composition

## 7. 拒绝方案

| 方案 | 拒绝原因 |
| --- | --- |
| 直接照抄 Rust 完整 dyn trait | 过早引入 auto trait、upcasting、owning dyn、drop metadata 和复杂 lifetime bound；不贴合 Aurex origin surface。 |
| Go-style structural interface | 会绕过现有 nominal trait impl/coherence/query evidence，破坏 M4-M7 地基。 |
| Swift-style owning existential container | 会把 allocator、side allocation、copy/destroy 和标准库 ABI 拉进 M8a。 |
| C++ class/vptr object model | 会把每个 concrete object layout 和 inheritance/vtable 绑定，和 Aurex trait + ADT 模型冲突。 |
| 保留 0-child `CanonicalTypeKind::trait_object` | 稳定 key 缺少 trait identity、origin、assoc equality、vtable schema，会污染 cache 和 ABI 设计。 |

## 8. 测试策略

每个阶段都要保持 90% coverage gate；不能为了覆盖率数字牺牲测试质量，关键语义和边界路径仍必须有正常仓库测试。

M8b 测试：

- lexer/parser：`dyn` keyword、type context、recovery。
- AST：payload roundtrip、dump。
- sema：unknown trait、non-trait path、generic method rejection、`Self` misuse、associated type missing equality。

M8c 测试：

- `&T` / `&mut T` coercion。
- borrow conflict：shared dyn receiver、mutable dyn receiver、returned dyn view local escape。
- checked facts：vtable layout fingerprint、method slot ordinal、trait method binding dispatch。

M8d 测试：

- IR verifier：pack/extract/slot/call invariants。
- backend execution：两个 concrete impl 通过同一 dyn parameter 调用不同函数。
- incremental cache：impl method body、trait method signature、vtable layout 改动会失效。

## 9. 参考来源

- Rust Reference: Trait objects: https://doc.rust-lang.org/reference/types/trait-object.html
- Rust Reference: Dyn compatibility: https://doc.rust-lang.org/reference/items/traits.html#dyn-compatibility
- Rust compiler vtable model: https://github.com/rust-lang/rust/blob/master/compiler/rustc_middle/src/ty/vtable.rs
- Swift ABI TypeLayout, existential containers: https://github.com/swiftlang/swift/blob/main/docs/ABI/TypeLayout.rst
- Swift Generics Manifesto, generalized existentials / protocol dispatch pitfalls:
  https://github.com/swiftlang/swift/blob/main/docs/GenericsManifesto.md
- Go Language Specification, interface types and type sets: https://go.dev/ref/spec#Interface_types
- Itanium C++ ABI, virtual table layout: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable

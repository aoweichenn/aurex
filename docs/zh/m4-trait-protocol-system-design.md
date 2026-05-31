# Aurex M4-WP1 Trait / Protocol 系统调研与设计基线

## 1. 状态和范围

本文是 M4-WP1 的设计基线。它不是语法小样，也不是马上实现的任务清单，而是进入 trait / protocol
实现之前必须先固定的语言设计、编译器边界、风险约束和分阶段路线。

当前结论：

- Aurex 采用 **nominal static trait** 作为 M4 的第一层能力。
- 语言关键字采用 `trait`；`protocol` 只作为设计术语，用来强调“行为契约”而不是对象模型。
- trait conformance 必须通过显式 `impl Trait for Type` 声明，不做 Go 风格 structural / duck typing。
- trait 默认只作为泛型约束和静态分派依据，M4.0 不引入 `dyn Trait`、trait object、vtable ABI 或 object safety。
- M4.0 不引入 RAII、`Drop`、`Copy`、move-only、borrow checker、resource capability 或 destructor lowering。
- M4.0 不引入默认方法、associated const、specialization、negative impl、unsafe trait、auto trait 或任意 blanket impl。
- associated type 作为设计目标进入 M4，但实现分期：M4.0 先做 trait 声明、impl registry、coherence 和静态方法分派；M4.1 再做 associated type projection 和 equality constraints。

这个选择不是因为其他设计不能做，而是因为 Aurex 当前已经完成 M3 的 module / generic / query-backed sema /
tooling / lowering authority boundary。M4 必须沿着这条现代编译器主线继续推进，不能把类继承、动态对象、资源语义和 trait solver
一次性塞进语言核心。

## 2. 当前 Aurex 基线

M3.9 已经固定了后续语言特性必须复用的架构边界：

- `CompilationSession`、`CompilationPipeline`、`FrontendPipeline`、`LoweringPipeline`、`BackendPipeline` 和
  `PipelineStage` 是 driver / profile / diagnostics / cache 的主路径。
- module / project 层已有 `ModuleKey`、`ModulePartKey`、`ProjectKey` 和 persistent query DB。
- generics 已有 `GenericInstanceKey`、`GenericTemplateSignature`、`GenericInstanceSignature` 和
  `GenericInstanceBody`，不能重新退回 display string 或 session-local handle 作为语义身份。
- sema 已经有 query-backed `ItemSignature`、`FunctionBodySyntax`、`TypeCheckBody` 和泛型 authority 边界。
- tooling 和 LSP 只能消费 protocol-neutral value types，不能绕过 parser / sema / query 读取私有状态。
- lowering / backend 已经区分 target-independent IR unit、layout / ABI fingerprint 和 LLVM emission unit。

当前代码中，trait 相关身份已经部分预留：

- `include/aurex/query/query_key.hpp` 已有 `DefNamespace::trait_`、`DefNamespace::impl_`、`DefKind::trait_`、
  `DefKind::trait_method`、`DefKind::associated_type`、`DefKind::associated_const`、`MemberKind::trait_method`、
  `MemberKind::associated_type`、`MemberKind::associated_const` 和 `BodySlotKind::trait_default_method`。
- `include/aurex/query/canonical_type_key.hpp` 已有 `CanonicalTypeKind::associated_type_projection` 和
  `CanonicalTypeKind::trait_object`。
- `include/aurex/query/generic_instance_key.hpp` 中的 `ParamEnvKey` 已经为“泛型实例在某个约束环境下成立”预留了位置。
- 语法层目前没有 `kw_trait`，`ItemKind` 没有 `trait_decl`，`ImplBlockItemPayload` 只描述 inherent `impl Type { ... }`，
  还没有 `impl Trait for Type { ... }`。
- parser 的 `where T: Eq + Hash` 当前解析为 `GenericConstraintDecl.capability_names`。
- sema 的 `CapabilityKind` 当前只承认 `Sized`、`Eq`、`Ord`、`Hash`，并显式拒绝 `Copy` / `Drop` 这类资源能力。

因此 M4 的第一任务不是“凭空新增 trait”，而是把已有 capability 和预留 query identity 正式升级为结构化 trait
predicate，同时保持 M3 已经建立的 incremental / query / tooling 稳定边界。

## 3. 外部语言和编译器调研结论

### 3.1 Rust traits

Rust 的 trait 是 Aurex 最直接的参照：trait 定义行为契约，`impl` 定义某个类型如何满足契约，泛型约束在编译期解析，默认路径走单态化和静态分派。

Rust 给 Aurex 的可采纳点：

- trait 是 nominal identity，不靠“方法长得像”自动匹配。
- trait 和 impl 分离，适合给已有类型补行为。
- associated items 包括 methods、associated types 和 associated constants，可以把“输入类型”和“输出类型”分开建模。
- coherence / orphan rules 让跨包 impl 不会无限冲突。
- trait solving 使用 obligation、selection、fulfillment 等概念，比把约束写成临时 bool predicate 更可维护。

Rust 给 Aurex 的警示：

- dyn compatibility / object safety 规则很复杂，associated const、generic associated type、`Self` 出现位置、receiver 形状都会影响对象化。
- blanket impl、specialization、negative impl 和 auto trait 会显著增加 overlap 检查、semver 兼容性和 solver 复杂度。
- Rust 的 trait solver 长期演进，Chalk-style logic goal 很强，但不适合在 Aurex M4.0 一次性实现。

Aurex 采纳：nominal trait、显式 impl、orphan / coherence、obligation worklist、associated type output model。
Aurex 暂缓：dyn trait、default methods、specialization、negative impl、auto trait、GAT、unsafe trait。

### 3.2 Swift protocols

Swift protocol 强调显式 conformance，protocol 可以作为 generic constraint，也可以进入 existential 形态。Swift 还用 associated
type 描述协议内的占位类型，并通过 witness table 支撑调用。

Swift 给 Aurex 的可采纳点：

- 类型不会因为“刚好有同名方法”自动采用协议，conformance 必须显式声明。
- associated type 适合表达 iterator item、collection index、serializer output 等“由实现决定的类型”。
- generic where clause 可以把 protocol conformance 和 associated type equality 放在同一个约束环境里。

Swift 给 Aurex 的警示：

- existential protocol 值会引入额外存储、间接调用和 witness table 管理。
- `Self` / associated type 相关 protocol 做 existential 时有长期复杂度。
- protocol extension / default implementation 提高易用性，但也可能让方法来源和冲突诊断变复杂。

Aurex 采纳：显式 conformance、associated type 分期、where equality 设计。
Aurex 暂缓：existential-first 设计、protocol extension / default implementation、动态 witness table ABI。

### 3.3 Kotlin interfaces

Kotlin interface 可以包含抽象方法、属性要求和默认方法实现，但不能真正存储状态。多个 interface 默认实现冲突时，需要显式覆盖消解。

Kotlin 给 Aurex 的可采纳点：

- trait / interface 不应该拥有对象状态；状态属于 struct / class-like data layout。
- 多个行为能力可以叠加，类型通过实现多个接口获得组合式抽象。

Kotlin 给 Aurex 的警示：

- 默认方法冲突会把“调用哪个实现”变成语言问题。
- interface property 如果过早引入，容易让“字段、getter、计算属性、ABI layout”混在一起。

Aurex 采纳：trait 无状态。
Aurex 暂缓：默认方法、property requirement、diamond-style conflict resolution。

### 3.4 Go interfaces

Go interface 是 structural：类型的方法集满足 interface 即自动实现。Go 1.18 之后 interface 还能描述 type set。

Go 给 Aurex 的可采纳点：

- interface 很适合轻量 API 抽象，调用端不必修改原类型声明。
- 小接口组合能改善可测试性。

Go 给 Aurex 的警示：

- structural conformance 容易因为同名方法偶然匹配，重构时可能改变 API 关系。
- type set 和 comparable 规则复杂，和泛型约束绑定后不再只是“几个方法”的问题。
- query / tooling / rename 需要稳定 symbol identity，structural 匹配会让“谁实现了谁”变成全局搜索问题。

Aurex 不采纳 Go-style structural interface。M4 必须使用 nominal conformance。

### 3.5 C++ concepts 和传统 class 体系

C++ 提供两条相关经验：一条是继承式 class / virtual dispatch，另一条是 C++20 concepts / constraints。

C++ class 的优势：

- 数据、构造、析构、方法、封装和多态都在同一套机制里。
- 与底层 ABI 和性能控制结合紧密。
- 虚函数适合插件边界和异构集合。

C++ class 的不足：

- inheritance 和 subtyping 不是同一个概念，继承经常同时表达代码复用、接口承诺和对象替换关系，语义过载。
- 多继承、diamond、virtual base、object slicing、虚析构、override 规则和 ABI layout 都会把语言核心拖进长期复杂度。
- 基类改动会通过 override、protected state 和虚派发影响大量派生类，也就是典型 fragile base class 问题。
- 模板、overload、ADL、concepts、SFINAE 和 partial ordering 叠加后，诊断和编译时间成本很高。

C++ concepts 的优势：

- 能把模板约束写成编译期 predicate，提升模板错误诊断。
- 约束满足、规范化、合取/析取和 partial ordering 为高阶泛型表达提供很强能力。

C++ concepts 的不足：

- arbitrary requires-expression 和 constraint normalization 对一个还在建立 trait solver 的语言过重。
- 约束不是名义 conformance，容易形成“约束表达式相等性”和“实现身份”分离的问题。

Aurex 不先做 class inheritance，也不做 arbitrary requires-expression。M4 只接受 canonical trait predicate 和必要的 associated type equality。

### 3.6 Scala 3 given / type class

Scala 3 用 trait + given instance 表达 type class。它很灵活，能把行为实例定义在类型之外，并通过上下文参数自动寻找。

Scala 给 Aurex 的可采纳点：

- 行为实例可以和数据类型分离，适合开放扩展。
- extension method 能提供接近 method call 的易用性。

Scala 给 Aurex 的警示：

- implicit / given search 会让调用解析变得不透明，用户很难预测到底选了哪个 instance。
- 模块导入、优先级、ambiguous given 和派生规则会显著增加 diagnostics 和 IDE 解释成本。

Aurex 采纳“实例和类型分离”的部分，不采纳 M4.0 的隐式搜索。trait method resolution 必须由 lexical scope、显式 bounds 和 impl
registry 解释清楚。

### 3.7 Haskell / GHC type classes

Haskell type class 是 ad-hoc polymorphism 的经典形式。论文和 GHC 经验说明，它非常强，但 overlap、multi-parameter class、functional
dependency、undecidable instance 等扩展会很快把求解推向高复杂度。

Haskell 给 Aurex 的可采纳点：

- type class 的核心思想是“约束产生 dictionary / evidence，调用通过 evidence 解析”。
- associated type / functional dependency 都在解决“输入类型决定输出类型或其他类型参数”的问题。

Haskell 给 Aurex 的警示：

- overlapping instances 默认必须避免，否则“唯一 impl”不再成立。
- multi-parameter type class 和 functional dependencies 会引入歧义、终止性和推断复杂度。
- undecidable instance 这类扩展不适合系统语言默认主路径。

Aurex M4.0 只做 single-self-type trait，不做 multi-parameter trait、functional dependency 或 undecidable instance。

### 3.8 MLIR interfaces

MLIR 的 interface 不是源语言 trait，但它的编译器架构经验很有价值：interface 把“某类对象能被某种 pass 通用处理”的事实抽象出来，model
可以外部注册，但必须被验证。

MLIR 给 Aurex 的可采纳点：

- trait declaration 和 impl model 分离，适合 query-backed compiler architecture。
- interface model 必须显式注册和验证，不能只靠约定。

MLIR 给 Aurex 的警示：

- “promised interface” 如果没有实现体，会在使用点炸成很差的错误。
- 外部 model 灵活，但需要严格的 ownership / coherence 规则。

Aurex 的 impl registry 必须是可查询、可验证的事实源。不能允许“先声明以后补”的未验证 impl。

### 3.9 论文层面的结论

Type class 的经典论文说明，ad-hoc polymorphism 的核心收益是把同名操作从临时重载升级为有类型约束和 evidence 的系统；这支持 Aurex
把 `Eq` / `Ord` / `Hash` 从内建字符串能力升级成 trait predicate。

Traits as composable units of behavior 的研究说明，trait 的价值在于可组合行为单元，而不是类继承树；这支持 Aurex 用 `struct + impl + trait +
composition` 替代 class inheritance 作为默认抽象路线。

Inheritance is not subtyping 的 OO 研究说明，继承经常被错误地同时用作实现复用和子类型关系。Aurex 因此不把 M4 做成 class-first：实现复用继续靠组合和函数，行为约束靠 trait，动态对象以后单独设计。

## 4. 类系统相关问题和 Aurex 取舍

用户关心“类”的原因通常不是想要继承树，而是想要：

- 数据和方法组织在一起。
- `obj.method(...)` 调用自然。
- 封装边界清楚。
- 能复用能力接口。
- 未来能安全管理资源。

这些需求不必由一个继承式 class 机制一次性解决。对 Aurex，更稳的拆法是：

| 需求 | 继承式 class 做法 | Aurex 推荐做法 |
|:--|:--|:--|
| 数据布局 | class fields | `struct` |
| 方法组织 | class methods | inherent `impl Type` |
| 能力抽象 | base class / interface | `trait` |
| 行为复用 | inheritance / mixin | composition + helper functions + later default methods |
| 静态多态 | templates / concepts | generic `where T: Trait` |
| 运行时多态 | virtual / interface object | later explicit `dyn Trait` |
| 资源清理 | destructor / RAII | later resource semantics / `Drop` |

M4 因此选择先做 trait，而不是先做 class inheritance。优势是：

- 不锁死对象布局和 vtable ABI。
- 不引入 fragile base class 和 inheritance/subtyping 混淆。
- `struct + impl` 已经能保留 method ergonomics。
- trait predicate 能直接接到 M3 的 generic / query authority。
- static dispatch 和 monomorphization 更容易内联、做 incremental invalidation 和 backend reuse。

不足是：

- 短期没有“一个 `class` 声明里同时写字段、构造器、trait conformance、destructor”的便利。
- 没有 dynamic object 时，异构集合、插件边界和 runtime polymorphism 暂时不能优雅表达。
- 没有 default method 时，一些 trait impl 会有样板代码。
- static dispatch 会带来泛型实例数量和代码体积风险，需要后续 codegen-unit / IR reuse 继续管控。

这些不足是有意接受的阶段性成本，目的是保留后续设计空间。

## 5. 选定的 Aurex 语义模型

### 5.1 Trait declaration

M4 语言层新增 nominal trait declaration：

```aurex
pub trait Reader {
    fn read(self: &mut Self, buf: []mut u8) -> usize;
}
```

语义规则：

- trait 是 type namespace 中的 named definition，拥有稳定 `DefKey`。
- trait 本身不定义 object layout，不占用值存储。
- trait body 只允许 requirement item。M4.0 只允许 method requirement。
- trait method 不允许 body。默认方法后置。
- trait 中的 `Self` 是隐式 type parameter，代表实现该 trait 的 concrete type。
- trait 可以有 visibility，trait method 默认继承 trait visibility；更细粒度规则等实现时固定。

### 5.2 Trait implementation

```aurex
impl Reader for File {
    fn read(self: &mut File, buf: []mut u8) -> usize {
        return file_read(self, buf);
    }
}
```

语义规则：

- `impl Trait for Type` 是 conformance fact，不是新的 nominal type。
- impl 产生稳定 `DefKey` / impl identity，进入 impl registry。
- impl method 必须覆盖 trait method requirement，参数和返回类型按 `Self := Type` 替换后匹配。
- inherent `impl Type` 和 trait `impl Trait for Type` 分开建模。
- impl 中不能新增 trait 未要求的方法。新增方法应放在 inherent impl 中。
- M4.0 不允许 partial impl body、promised impl 或 deferred implementation。

### 5.3 Generic bounds and static dispatch

```aurex
fn copy_one[R](reader: &mut R, buf: []mut u8) -> usize
where R: Reader {
    return reader.read(buf);
}
```

语义规则：

- `where R: Reader` 产生 `TraitPredicate{trait = Reader, self = R}`。
- type checking 将 trait requirement 变成 `TraitObligation`。
- solver 在当前 `ParamEnv` 中解析 obligation。
- generic body 内的 trait method call 绑定到 requirement evidence，而不是靠字符串查找。
- 单态化后，trait method call 降低为具体 impl method 的 direct call；M4.0 不产生 vtable。

方法解析优先级：

1. 先查 inherent methods。
2. 再查当前 lexical scope 可见 trait bounds 和 imported trait 中的 applicable trait methods。
3. 如果同名方法有多个 trait candidate，要求用户显式消歧。

M4.0 必须把“为什么不可调用”诊断清楚：

- 类型没有满足 trait bound。
- trait 未导入或不可见。
- impl 受 orphan / visibility 限制不可用。
- 多个 trait method 候选冲突。
- impl method signature 不匹配 requirement。

### 5.4 Coherence and orphan rules

M4 必须从第一版实现 coherence，否则跨模块和未来 package resolver 会失控。

规则：

- 一个 trait ref + self type 在某个 package 世界中最多只能有一个适用 impl。
- 当前 package 可以实现：
  - 当前 package 定义的 trait for 任意可命名类型。
  - 任意可见 trait for 当前 package 定义的 nominal type。
- 当前 package 不能实现外部 trait for 外部 nominal type。
- builtin primitive 的 trait facts 由 compiler-owned builtin impl provider 管理，不走用户 impl。
- M4.0 禁止 arbitrary blanket impl，例如 `impl[T] Display for T where T: Debug`。
- M4.0 可以允许 local generic self type impl，但必须是 current-package nominal head，例如 `impl[T] Reader for Buffer[T] where T: Sized`。
- overlap 检查基于 canonical trait ref、canonical self type 和 param env，不基于 display string。

这相当于把 Rust / Chalk 的“唯一 impl”原则裁剪到 Aurex 当前 module / package 基线。

### 5.5 Associated types

associated type 进入 M4 设计，并已在 M4-WP6 落地第一版实现。当前实现刻意限定在这里描述的 static-dispatch、
non-object、non-GAT 模型。

目标语义：

```aurex
trait Iterator {
    type Item;
    fn next(self: &mut Self) -> Option[Self.Item];
}

impl Iterator for ByteIter {
    type Item = u8;
    fn next(self: &mut ByteIter) -> Option[u8] { ... }
}

fn sum[I](iter: &mut I) -> i32
where I: Iterator[Item = u8] {
    ...
}
```

设计原则：

- associated type 是 trait impl 的 output，不参与 impl selection 的输入匹配。
- impl selection 先根据 `(Trait, SelfType, ParamEnv)` 找到唯一 impl，再读取该 impl 给出的 associated type value。
- `Iterator[Item = u8]` 是语法糖，降低为 trait predicate + associated type equality predicate。
- 投影的 canonical type 使用现有 `CanonicalTypeKind::associated_type_projection`，base type 是 `Self` 或 generic parameter，member key 指向 trait associated type。
- 如果 `T.Item` 在多个 trait bound 中不唯一，必须报 ambiguity；后续可引入显式 `(T as Iterator).Item` 消歧。
- M4 不做 generic associated types。GAT 会把 solver、lifetime/resource 和 equality constraint 全部拉复杂。
- M4-WP6 暂不做 associated constant、default associated type value、显式 qualified projection syntax 和
  trait-object projection ABI。当前 `T.Item` shorthand 只有在当前 trait bound 中 associated type 名称唯一，或可由匹配的
  equality predicate 归一化时才接受。

### 5.6 Capability migration

当前 `Sized`、`Eq`、`Ord`、`Hash` 是内建 capability。M4 不能直接删除它们，否则 M3.1 泛型测试和已有样例会被破坏。

迁移策略：

1. 保留 `CapabilityKind` 作为兼容入口。
2. 为四个 capability 建立 compiler-known builtin trait identity。
3. `where T: Eq + Hash` 解析后不再停留在 string capability，而是 lowered 为 builtin trait predicates。
4. operator availability 和 trait satisfaction 保持分层：
   - `==` 的直接操作符规则仍由 type checker 控制。
   - `T: Eq` 是泛型 predicate，需要通过 builtin impl 或用户 impl 满足。
5. `Copy` / `Drop` 继续拒绝，直到 resource semantics 单独设计。

## 6. 不采纳方案

| 方案 | 优势 | 不采纳原因 |
|:--|:--|:--|
| 继承式 class-first | 写法熟悉，封装/构造/虚派发一体化 | 过早锁死对象模型、vtable、析构和 inheritance/subtyping 关系，和 M3 query/generic 主线不匹配 |
| Go structural interface | 写法轻，适合小接口 | 偶然 conformance、rename/refactor 不稳定、query identity 难固定 |
| Swift existential-first | 动态多态体验好 | witness table、existential storage、`Self` / associated type 限制会过早锁 ABI |
| Rust dyn trait first | 运行时多态边界明确 | object safety、vtable layout、drop glue 和资源语义尚未设计 |
| Kotlin default method first | 减少样板 | 多 trait 默认实现冲突和 method origin 诊断复杂 |
| Scala implicit/given search | 扩展性强 | 调用解析不透明，IDE/diagnostics 成本高 |
| C++ arbitrary requires | 泛型表达力强 | constraint normalization / partial ordering 复杂，不适合 M4.0 |
| Haskell multi-param/fundep | 可表达复杂类型关系 | 歧义、终止性和 solver 复杂度过高 |
| Auto / unsafe marker trait | 能表达深层语义性质 | 容易与资源/并发/unsafe 边界耦合，必须等 resource/unsafe 体系成熟 |

## 7. 风险矩阵

| 风险 | 成因 | 其他语言经验 | Aurex 缓解 |
|:--|:--|:--|:--|
| impl 冲突 | 多个 impl 同时适用 | Rust / Chalk 必须做 coherence 和 overlap | M4.0 先做 orphan + no blanket impl + unique impl check |
| 跨包破坏 semver | 上游新增 impl 让下游冲突 | Rust orphan rules 解决 compatible worlds | 当前 package 只能实现本地 trait 或本地 nominal type |
| solver 非终止 | 递归 trait bound、关联类型投影循环 | GHC undecidable instance 和 Rust solver 都有复杂约束 | M4.0 使用有限 obligation worklist、cycle detection 和 depth budget |
| associated type 歧义 | 多个 trait 都定义同名 associated type | Rust 需要 qualified projection，Swift PAT/existential 复杂 | M4.1 先要求唯一 shorthand，必要时用显式 projection |
| dynamic ABI 锁死 | trait object 要定义 data/vtable/drop/layout | Rust dyn compatibility 规则复杂 | M4 不做 dyn trait，保留 `trait_object` key 但不开放语义 |
| 默认方法冲突 | 多个 trait 提供同名 default | Kotlin 必须显式覆盖冲突 | M4.0 不做 default method |
| structural accidental conformance | 同名方法偶然满足接口 | Go interface 重构风险 | 只做 nominal explicit impl |
| 隐式搜索不可解释 | 自动 instance/given 选择隐藏来源 | Scala given / Haskell instance search 复杂 | M4.0 不做 implicit search，candidate 必须来自显式 bounds/scope/registry |
| capability 兼容性 | `Eq` 等从 capability 迁到 trait | 直接替换会破坏泛型测试 | builtin trait identity 兼容迁移，旧诊断保持 |
| query cache 不稳定 | trait predicate 若用 display string 会变动 | M3 已关闭 display string identity 风险 | 使用 `DefKey`、`MemberKey`、`GenericInstanceKey`、canonical type key |
| diagnostics 退化 | solver 错误容易变成“约束不满足” | C++ concepts / Rust trait errors 都容易冗长 | obligation 记录 source range、bound source、candidate rejection reason |
| tooling 泄漏内部 | LSP 直接读取 sema 私有对象 | M3 已禁止 LSP DTO 进入 compiler internals | IDE 只消费 protocol-neutral trait facts |
| 资源语义压力 | `Drop` / `Copy` 看起来像普通 trait | Rust Send/Sync/Drop 都涉及深层安全 | M4-WP1 明确 RAII/resource out of scope |
| 代码体积增长 | static dispatch / monomorphization | Rust / C++ 都需要 codegen-unit 和 LTO 管控 | 复用 M3.8 IR unit fingerprint，后续加 trait instantiation profile |

## 8. 编译器管线影响

### Lex / Parse / AST

- 新增 `kw_trait`。
- `ItemKind` 新增 `trait_decl`。
- 新增 `TraitItemPayload`，至少包含 name、generic params、where constraints、requirements。
- `ImplBlockItemPayload` 增加可选 trait type / trait path，区分 inherent impl 和 trait impl。
- `where` 约束从 capability names 演进为 trait predicate syntax tree。
- Parser recovery 必须处理 `trait Name { ... }`、`impl Trait for Type { ... }` 和 trait body requirement。

### Sema / Query

- 新增 `TraitSignature` query 或扩展 item signature authority，使 trait requirement 是 durable fact。
- 新增 impl registry query，按 package / module / trait / self type 建索引。
- 新增 `TraitPredicate`、`TraitObligation`、`TraitEvidence` 和 `ParamEnv` 中的 predicate list。
- coherence check 作为 sema 阶段 gate，失败时阻止 lowering。
- capability migration 走 builtin trait provider，不继续扩张字符串 capability。

### Lowering / Backend

- M4.0 trait call 全部静态分派。
- generic monomorphization 后调用具体 impl method symbol。
- 不生成 vtable，不生成 trait object layout，不生成 drop glue。
- IR dump 应能解释 trait call 的 selected impl 或至少保留 debug metadata 供测试。

### Tooling

- semantic tokens 需要识别 trait name、trait method、impl block 和 associated type。
- completion 应能在 `where T:` 后补可见 trait。
- hover / definition 应能从 trait method requirement 跳到 impl method 或反向列出实现。
- rename 必须用 `DefKey` / `MemberKey`，不能按文本重命名 trait method。

## 9. M4 work packages

截至 2026-05-31，M4-WP1 到 M4-WP7 已完成。M4-WP8 release closure 是当前唯一剩余的 M4 活跃 work package。

| Work package | 目标 | 完成标准 |
|:--|:--|:--|
| M4-WP1 | 调研和设计基线 | 本文档、路线图、风险矩阵、文档测试收口 |
| M4-WP2 | Syntax / AST / query identity scaffolding | `trait` token、trait AST、trait impl AST、stable dump 和 parser tests |
| M4-WP3 | Trait declaration and impl registry | trait signature fact、impl fact、requirement matching、registry query |
| M4-WP4 | Coherence and generic predicates | orphan / overlap check、`where T: Trait` obligation、builtin capability migration |
| M4-WP5 | Static method resolution and lowering | trait method call binding、monomorphized direct calls、IR/backend tests |
| M4-WP6 | Associated type model | associated type declaration/impl、projection、equality predicate、ambiguity diagnostics |
| M4-WP7 | Tooling and diagnostics | completion、hover、definition、rename、semantic tokens、candidate rejection notes |
| M4-WP8 | Release closure | docs、coverage、stress、query/cache/profile gates、unsupported matrix 更新 |

## 10. M4-WP1 验收门

M4-WP1 完成后必须满足：

- 中英文设计文档存在，并写清 selected design、rejected alternatives、risk matrix、class comparison 和 implementation route。
- `next-steps` 明确 M4 当前最高优先级是 trait/protocol design baseline。
- `progress` 记录 M4-WP1 状态。
- README 文档索引包含 M4 设计稿和路线图。
- documentation integration test 把新增文档纳入 required list。
- 至少运行：
  - `clang-format -i tests/gtest/integration/documentation_tests.cpp`
  - `cmake --build cmake-build-release -j4`
  - `./cmake-build-release/bin/aurex_tests --gtest_color=auto --gtest_filter='AurexIntegrationTest.DocumentationLayoutIsStable'`
  - `tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')`
  - `git diff --check`

## 11. 参考资料

- Rust Reference: traits and dyn compatibility, https://doc.rust-lang.org/stable/reference/items/traits.html
- Rust Reference: implementations and coherence, https://doc.rust-lang.org/stable/reference/items/implementations.html
- Rust Reference: associated items, https://doc.rust-lang.org/stable/reference/items/associated-items.html
- rustc-dev-guide: trait resolution, https://rustc-dev-guide.rust-lang.org/traits/resolution.html
- rustc-dev-guide: Chalk-based trait solving, https://rustc-dev-guide.rust-lang.org/traits/chalk.html
- Chalk book: coherence, https://rust-lang.github.io/chalk/book/clauses/coherence.html
- Swift Book: Protocols, https://docs.swift.org/swift-book/documentation/the-swift-programming-language/protocols/
- Swift Book source: Protocols, https://raw.githubusercontent.com/swiftlang/swift-book/main/TSPL.docc/LanguageGuide/Protocols.md
- Swift Book source: Generics, https://raw.githubusercontent.com/swiftlang/swift-book/main/TSPL.docc/LanguageGuide/Generics.md
- Kotlin Documentation: Interfaces, https://kotlinlang.org/docs/interfaces.html
- Kotlin Documentation: Generics, https://kotlinlang.org/docs/generics.html
- Go Language Specification: Interface types, https://go.dev/ref/spec
- C++ working draft: constraints and concepts, https://eel.is/c++draft/temp.constr
- Scala 3 Reference: Type Classes, https://docs.scala-lang.org/scala3/reference/contextual/type-classes.html
- GHC User Guide: Type classes and instances, https://downloads.haskell.org/ghc/latest/docs/users_guide/exts/instances.html
- GHC User Guide: Functional dependencies, https://downloads.haskell.org/ghc/latest/docs/users_guide/exts/functional_dependencies.html
- MLIR Interfaces, https://mlir.llvm.org/docs/Interfaces/
- Wadler and Blott, How to make ad-hoc polymorphism less ad hoc, https://homepages.inf.ed.ac.uk/wadler/papers/class/class.pdf
- Scharli, Ducasse, Nierstrasz, Black, Traits: Composable Units of Behaviour, https://scg.unibe.ch/archive/papers/Scha03aTraits.pdf
- Cook, Hill, Canning, Inheritance is not Subtyping, https://www.cs.utexas.edu/~wcook/papers/InheritanceSubtyping90/CookPOPL90.pdf

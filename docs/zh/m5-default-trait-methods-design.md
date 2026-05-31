# Aurex M5 Default Trait Methods 调研与设计基线

## 1. 状态和范围

本文是 M5-WP1 设计基线。它不是语法小样，也不是马上开写的实现清单，而是在实现 default trait methods
之前必须固定的语言语义、编译器边界、风险约束和分阶段路线。

当前结论：

- M5 在现有 nominal static trait 上增加 **trait 默认方法体**。
- M5 继续沿用 M4 的 conformance 模型：`trait`、显式 `impl Trait for Type`、nominal identity、orphan /
  overlap 检查、associated type 和 static dispatch。
- 带 body 的 trait method 仍然是 requirement。这个 body 是默认实现，只在某个 impl 没有显式 override 时使用。
- impl 中签名匹配的方法优先于默认实现。impl 省略没有默认体的 requirement 仍然是错误。
- dispatch 继续保持静态。单态化之后，override 调用降低到 impl method；省略后继承 default 的调用降低到 trait-owned
  default method body，并按选中的 `Self` / trait args / associated-type outputs 实例化。
- method origin 必须成为显式语义事实。有 default 后，call binding 不能只记录“这是一个 trait method”。

M5 明确不做：

- `dyn Trait`、trait object、vtable ABI、object safety 或 witness-table ABI。
- specialization、overlapping defaults、negative impl、auto trait、unsafe trait、blanket impl 或“most specific”
  default selection。
- associated const、generic associated type、default associated type value、higher-ranked bound 或新 solver。
- RAII、`Drop`、`Copy`、move-only value、borrow checker、destructor lowering 或 resource capability。
- Swift-style protocol extension、Scala-style mixin linearization、Kotlin / Java / C# runtime interface dispatch
  或 Go-style structural conformance。
- 隐式全局 instance search 或 extension method。

这个范围刻意很窄。M4 已经有 nominal static trait 和 associated type。M5 最有价值的下一步，是在不冻结动态对象 ABI
和 specialization 规则的前提下提供行为复用。

## 2. 当前 Aurex 基线

M4 release baseline 已提供所需地基：

- Parser / AST 已能识别 `trait Name { ... }` 和 `impl Trait for Type { ... }`。
- `TraitSignature` 记录 generic params、visibility、associated type requirements 和 method requirements。
- `TraitImplInfo` 记录显式 impl facts、associated type assignments 和 impl methods。
- `TraitPredicate`、`TraitObligation`、`TraitEvidence` 和 `ParamEnvInfo` 描述 generic trait bound 和 evidence。
- `TraitMethodCallBinding` 记录 trait method call，当前分为 `param_env`、`impl_override` 或
  `trait_default`。
- static trait call 在单态化后降低为 direct call。
- tooling 已通过 stable key 索引 trait definition、trait method、impl method、associated type 和 rename identity。

已实现的 M5-WP3/WP4 baseline：

- `TraitMethodRequirement` 记录 requirement 是否有 default body，以及 body syntax id。
- default body 会在 trait context 中只 type-check 一次，并使用 `Self`、trait generic params、trait where
  predicates、associated projections 和 `trait_self` evidence。
- `TraitMethodDispatchKind` 已区分 `param_env`、`impl_override` 和 `trait_default`。
- `validate_trait_impl_block()` 会把省略的 defaulted requirement 视为 inherited default，同时保持省略
  non-default requirement 报错。
- `resolve_impl_trait_method_call()` 现在会把 selected impl 省略的 defaulted requirement 绑定到
  `trait_default` origin，而不是继续当作 missing method。

剩余 M5-WP5/WP6 工作是通过 `BodySlotKind::trait_default_method` materialize default-body query identity，
按 selected impl environment 实例化 trait-owned default，并把 selected `trait_default` call 降低为 direct symbol，
同时保持 M4 的 static dispatch 模型。

因此 M5 不是“parser 放开 `{ ... }`”这么简单，而是要补 method origin、稳定 default-body identity、trait context
下的类型检查，以及保持 M4 direct-call 模型的 lowering 策略。

## 3. 外部语言和编译器调研结论

### 3.1 Rust traits

Rust trait method 可以提供默认体，implementor 可以 override。默认体可以通过 trait contract 调用其他 trait method，
这非常适合“实现一个原语操作，继承一组便利操作”的模式。

Aurex 应采纳：

- 用 body presence 表示 default method，不新增关键字。
- default 仍属于 trait requirement set。override compatibility 仍按同一份签名检查。
- default body 由 trait method 持有，不把源文本复制进每个 impl。
- default body 可以通过当前 trait evidence 调用其他 trait requirement。

Aurex M5 应避免：

- Rust 完整 dyn-compatibility 规则。default method 能不能通过 trait object 调用不是 M5 问题。
- specialization 或 `default impl` block。这需要 overlap ordering 和 semver 规则，当前 solver 不具备。
- blanket impl 交互。M4 已经禁止任意 blanket impl，M5 不能重新打开这个边界。

Aurex 采纳 Rust-like default method bodies，但只做 static-only 和 trait-owned。

### 3.2 Swift protocols 和 protocol extensions

Swift 区分 protocol requirement 和 protocol extension 中“只由扩展提供”的 member。这很强，但也是 Aurex 必须警惕的坑：
extension-only member 和真正 requirement 的调用分派行为可能不同。

Aurex 应采纳：

- default implementation 必须绑定在已声明的 requirement 上。
- tooling 必须能说明某个调用选中了 override 还是 default。
- default implementation 可以帮助 API 演进，但只能在清楚的 trait contract 内生效。

Aurex M5 应避免：

- extension-only method。它看起来像 trait method，但不是 requirement，容易制造 dispatch 陷阱。
- dynamic existential dispatch 和 witness-table ABI。
- 不经过显式 impl fact 的 retroactive behavior injection。

因此 M5 不做 protocol extension。只有 trait 内部的方法体可以作为 default-method surface。

### 3.3 Kotlin interfaces

Kotlin interface 可以有抽象方法和默认方法，但不能持有对象状态。多个 interface 默认实现冲突时，Kotlin 要求显式 override
并用 qualified delegation 选择父实现。

Aurex 应采纳：

- trait 保持无状态。default method 可以使用参数、receiver 上的 trait method、associated type、当前语言规则允许的常量和 helper
  function，但不引入 stored state。
- 冲突必须显式处理。如果多个可见 trait method 都是候选，Aurex 继续使用 M4 ambiguity diagnostic，不因为某个候选有 default
  就偷偷选择它。

Aurex M5 应避免：

- interface property、backing field 或 stateful mixin。
- 在显式 qualified trait-call syntax 设计前增加 `super<Trait>.method()` 之类 delegation。

M5 只能接受唯一选中的 default。多个同名 trait candidate 仍然按 M4 规则报歧义。

### 3.4 Java default methods

Java 引入 default interface methods 的核心动机之一，是让已有 interface 可以演进而不迫使所有实现者马上补方法。经验说明：
default 很有价值，但 class-vs-interface precedence 和 conflicting defaults 需要很细规则。

Aurex 应采纳：

- impl override 优先于 trait default。
- 在 M5 生效后，给已有 trait method 加 default 对“省略该方法的 impl”可以是源码兼容变化。
- diagnostics 必须解释 selected origin。

Aurex M5 应避免：

- class / interface runtime dispatch precedence。Aurex M5 没有 class inheritance 层。
- 通过运行时继承层级解决多默认实现冲突。Aurex 保持静态歧义。

Java 的教训是：default 不只是语法，它也是语言演进策略。

### 3.5 C# default interface members

C# default interface members 提供了更激进的 API 演进路径，interface member 可以携带实现，并有运行时 “most specific
implementation” 规则。这很强，但依赖 Aurex 当前没有的对象模型和运行时分派系统。

Aurex 应采纳：

- default method 可以减少下游样板代码。
- conflict 和 origin 规则必须先于实现固定。

Aurex M5 应避免：

- runtime “most specific” implementation selection。
- 绑定到 VM 或 dynamic dispatch ABI 的版本演进假设。

M5 保持 direct call 和 compile-time origin selection。

### 3.6 Haskell / GHC type classes

Haskell type class 支持默认方法定义，instance 可以 override。GHC 的 `MINIMAL` pragma 存在，正是因为多个互相依赖的 default
会让“一个 instance 最少该实现什么”变得不清楚。

Aurex 应采纳：

- default 可以通过 evidence 调用其他 required method。
- impl checker 必须继续保证所有非 default method 都被实现。
- 设计上要给未来 minimal-implementation annotation 留空间。

Aurex M5 应避免：

- 从 default dependency graph 推断复杂 minimal method set。
- 让 default cycle 变成编译期 expansion cycle。

M5 第一版规则保持简单：有 body 的 requirement 可以省略；没有 body 的 requirement 必须实现。递归 default 是普通函数递归，不是宏展开。
未来可以加 lint 检测明显无限递归。

### 3.7 Scala traits 和 mixins

Scala trait 可以包含 concrete methods，也可以 mix into classes，并由 linearization 决定 method resolution order。
这提供强组合能力，但行为会依赖 mixin 顺序和 class hierarchy。

Aurex 应采纳：

- interface-like abstraction 中携带可复用 concrete behavior 是有价值的。

Aurex M5 应避免：

- trait parameters、initialization order、fields 或 mixin linearization。
- 叠多个 trait default 后按顺序让编译器自动选择。

Aurex trait 仍是行为契约加可选默认体，不是 mixin class。

### 3.8 Go interfaces

Go interface 是 structural，且不携带 default implementation。它对 Aurex 的价值主要是反例：一旦 conformance 是隐式的，
给 interface 增加方法可能静默改变哪些类型满足它。

Aurex 保持 nominal explicit conformance。default method 绑定到声明过的 trait member，并通过显式 impl fact 或 generic
trait predicate 选择。

### 3.9 C++ concepts、CRTP 和 inheritance

C++ 有很多行为复用手段：virtual inheritance、带默认实现的 abstract base class、CRTP mixin 和 C++20 concepts。
它们都解决真实问题，也都带来成本：object layout、virtual destructor、fragile base class、模板诊断、overload /
partial-ordering complexity。

Aurex 不应该借 M5 引入 class inheritance 或 arbitrary requires-expression。default trait method 是更小、更局部的复用机制：
在 nominal trait contract 内复用 checked behavior，不引入 subtyping 或 vtable。

### 3.10 编译器架构经验

LLVM/MLIR 这类现代编译器基础设施强调：interface 和 model 应该是显式、可查询的事实。对应到 Aurex：

- trait signature 是 default method body 的 owner。
- impl registry 记录某个 impl 是否提供 override。
- method-call binding 记录 selected origin。
- lowering 消费 checked origin facts，而不是重新做 name lookup。
- query identity 必须使用 `DefKey`、`MemberKey`、`BodyKey`、canonical type keys 和 `ParamEnvKey`，不能使用
  display string。

## 4. 用户可见语言模型

### 4.1 基础语法

M5 允许 trait method requirement 带 body：

```aurex
trait Reader {
    fn read(self: &Self) -> i32;

    fn is_empty(self: &Self) -> bool {
        return self.read() == 0;
    }
}
```

impl 可以省略 `is_empty`，因为它有默认实现：

```aurex
impl Reader for FileReader {
    fn read(self: &FileReader) -> i32 {
        return self.value;
    }
}
```

impl 也可以 override：

```aurex
impl Reader for CachedReader {
    fn read(self: &CachedReader) -> i32 {
        return self.value;
    }

    fn is_empty(self: &CachedReader) -> bool {
        return self.cached_empty;
    }
}
```

不新增关键字。函数是否有 body，就决定这个 requirement 是否带 default。

### 4.2 Requirement 和 default body

每个 trait method 都有：

- requirement signature：name、parameters、return type、unsafe / variadic shape、self-parameter shape、visibility、
  stable member key 和 ordinal。
- 可选 default body：source range、body syntax key、body fingerprint、body slot 和 checked body result。

default body 不改变 requirement signature。override matching 仍按 M4 规则，在替换 `Self`、trait arguments 和 impl
associated type outputs 后检查。

### 4.3 Impl 完整性和 override 规则

对每个 trait method requirement：

- 如果 impl 提供签名匹配的方法，该方法就是 override。
- 如果 impl 提供同名但签名不兼容的方法，报告已有 signature mismatch diagnostic。
- 如果 impl 省略该方法且 requirement 有 default body，impl 对该方法是完整的，继承 trait default。
- 如果 impl 省略该方法且 requirement 没有 default body，报告 missing method。
- duplicate impl method 仍然是错误。
- unknown impl method 仍然是错误。

impl registry 应该记录 explicit overrides 和 inherited defaults，但不需要为 inherited default 合成源 AST 节点。

### 4.4 Dispatch 和 method origin

M5 引入显式 method-origin model：

| Origin | 含义 | Lowering target |
| --- | --- | --- |
| `impl_override` | 选中的 impl 提供方法体。 | 现有 impl method function symbol。 |
| `trait_default` | 选中的 impl 省略方法，trait requirement 有 default body。 | 按 selected impl environment 实例化 trait-owned default body。 |
| `param_env` | generic body check 阶段只有 trait evidence。 | 单态化时重新选择 `impl_override` 或 `trait_default`。 |

inherent method 仍然优先于 trait method。如果多个 trait candidate 具有同一调用形状，继续使用 M4 ambiguity 规则。M5
不会因为某个候选有 default body 就用它打破歧义。

### 4.5 Default body 类型检查

default body 在 trait context 中检查：

- `Self` 是隐式 implementing type parameter。
- trait generic parameters 可见。
- 当前 trait predicate 作为 evidence 可用，因此 body 可以在 `Self` 上调用其他 requirement。
- `Self.Item` 之类 associated type projection 使用 M4 associated-type 规则。
- trait 上的 where constraints 进入 default body 的 parameter environment。
- default body 不能假设未来某个 concrete impl type 的字段。
- default body 不能调用 impl-only helper，除非该 helper 能通过普通 name lookup 找到且不依赖 concrete `Self` field。

default body 的错误指向 trait declaration，而不是每个继承它的 impl。如果 default body 调用非 default requirement，这是合法的，因为
impl completeness check 会保证每个 concrete impl 要么实现该 requirement，要么继承一个有效 default。

### 4.6 Associated types

default body 可以提到 associated type：

```aurex
trait Source {
    type Item;
    fn next(self: &Self) -> Self.Item;
    fn first_or(self: &Self, fallback: Self.Item) -> Self.Item {
        return self.next();
    }
}
```

规则：

- associated type 仍是 impl output，不是 impl-selection input。
- default body 先用 projected type 检查，再在 impl 提供 associated type outputs 或 equality predicates 时归一化。
- projection cycle 使用 M4 现有 cycle checks 诊断。
- M5 不做 default associated type value。

### 4.7 Generic calls

generic body 中：

```aurex
fn empty[T](value: &T) -> bool
where T: Reader {
    return value.is_empty();
}
```

该调用先绑定到 `param_env` evidence。实例化 `empty[FileReader]` 时，编译器再选择 `FileReader` 的 override 或
`Reader.is_empty` default。这样 generic type checking 不绑定到具体 impl，同时单态化后仍保持 direct call。

### 4.8 递归和 default 依赖

default body 是函数体，不是文本展开。因此：

- default method 可以递归调用自己。这是普通运行时递归，可能是 bug，但不是编译期 expansion cycle。
- 两个 default method 可以互相调用。这也是运行时递归。
- associated type projection cycle 仍是类型层面的 cycle，继续在编译期报错。

M5 不尝试通过依赖分析推断 minimal method set。未来 M5.x 或 M6 可以在用户确有需要时引入类似 GHC `MINIMAL` pragma
思想的 annotation，例如“必须实现 `read` 或 `read_into` 之一”。

## 5. 拒绝方案

| 方案 | 优点 | 拒绝原因 |
| --- | --- | --- |
| sema 阶段把 default body 复制进每个 impl | 心智模型简单 | 复制 AST/sema facts，诊断不稳定，扩大 invalidation，并丢失 trait-owned source origin。 |
| 要求 `default fn` 语法 | defaultness 更显眼 | 新增关键字但不解决歧义；body presence 足够，且符合主流语言经验。 |
| 把 default 只当 synthetic impl method | lowering target 简单 | 隐藏 source owner；tooling、diagnostics 和 query invalidation 都需要 trait method identity。 |
| 只在每个 impl 上 type-check default body | 提前看到 concrete associated type outputs | 每个 impl 重复报错，无法给 trait-local diagnostics。 |
| Swift-style protocol extensions | 易用，支持 retroactive helper surface | 产生 requirement-vs-extension dispatch 陷阱和新 lookup 规则。 |
| Kotlin / Scala mixin traits | 行为复用强 | 需要 state、linearization、`super<Trait>` 或 initialization rules，超出 M5。 |
| Java / C# runtime default dispatch | 对 OO interface API 演进友好 | 依赖 Aurex 没暴露的 runtime object model 和 dynamic dispatch ABI。 |
| defaults 与 specialization 同时做 | 表达力强 | 需要 overlap ordering、“more specific” selection、semver policy 和更强 solver。 |
| 同阶段做 default associated types | 减少 associated-type 样板 | 与 projection normalization 和 equality predicates 耦合，必须单独设计。 |

## 6. 选定的编译器设计

### 6.1 Syntax 和 AST

Parser 改动：

- trait body 内 `fn` item 可以以分号结束，也可以带 body。
- requirement 既没有分号也没有合法 body 时，保持当前 recovery，不吞掉下一个 trait item。
- associated type declaration 不变。
- impl parsing 不变；impl method 本来就有 body。

AST / syntax model：

- trait method 继续作为 trait body 拥有的 `ItemKind::fn_decl`。
- 增加显式状态，表示 trait method item 是否有 body，并能访问其 body slot。
- default body identity 使用 `BodySlotKind::trait_default_method`。
- AST dump 应显示 trait method 是 prototype requirement 还是 defaulted requirement。

### 6.2 Sema facts

扩展 `TraitMethodRequirement`：

- `bool has_default_body`。
- `query::BodyKey default_body_key`。
- `query::StableFingerprint128 default_body_fingerprint`。
- default body source range。
- checked-body status，或指向普通函数体同级 body-check authority 的索引。

扩展 impl facts，增加 method origin record：

- requirement ordinal。
- method name/member key。
- `TraitImplMethodOrigin::explicit_override` 或 `TraitImplMethodOrigin::inherited_default`。
- override 对应 function key。
- inherited default 对应 default body key。

扩展 call binding：

- 替换或细化 `TraitMethodDispatchKind`，避免把 `param_env` 和最终 origin 混在一起。
- 记录 selected trait member key。
- concrete receiver 时记录 selected impl key。
- override 记录 selected function key。
- default 记录 selected default body key。

### 6.3 Default body authority

trait method default body 应该是 canonical trait-owned template：

- Syntax owner：trait method item。
- Stable body key：owner trait `DefKey` + `MemberKey` + ordinal + `BodySlotKind::trait_default_method`。
- Type-check environment：trait `Self`、trait generic params、trait where predicates、current trait evidence 和
  associated type projection facts。
- Lowering environment：selected concrete impl、concrete `Self`、trait args、associated type outputs 和 generic instance
  identity。

这样可以保留 query identity，不需要把源 body 复制进 impl。

### 6.4 Name resolution 和 visibility

- default body 使用 trait declaration 所在模块的 lexical imports 和 visibility。
- 它不会获得任意未来 impl type 的 private members 访问权。
- 在 `Self` 上调用其他 trait method 通过当前 trait evidence 解析。
- 除非当前类型系统能通过普通规则证明，否则不假设 abstract `Self` 有 inherent methods。

### 6.5 Lowering 和 backend

lowering 消费 sema origin facts：

- `impl_override`：直接调用现有 impl method symbol。
- `trait_default`：把 trait default body 作为 function template，按 concrete `Self`、trait args 和 associated type outputs
  专门化。
- `param_env`：只允许存在于 generic instantiation 之前。LLVM lowering 前，monomorphization 必须重新选出 concrete
  origin。

backend symbol naming：

- override symbol 保持现有 impl method naming。
- default symbol 应来自 trait stable id、trait member key、self type canonical key、trait args 和 associated type outputs。
- display name 只能用于 diagnostics，不能用于 ABI key。

### 6.6 Query 和 incremental invalidation

M5 必须保持 M3 query 边界：

- 修改 default body 会 invalidate default body syntax、type-check、lowering，以及实际使用该 default 的 call sites。
- override 该方法的 impl 不应该因为 default body 修改而需要 body re-lowering。
- 修改 requirement signature 会 invalidate impl matching、call signatures 和 inherited default checks。
- 修改 impl override 只 invalidate 选中该 override 的调用，不影响其他 impl 中选中 default 的调用。
- query records 必须暴露 default body keys、method origin 和 dependency edges，让 tooling reuse explanation 真实可信。

## 7. Diagnostics

M5 应新增或细化这些诊断：

| 场景 | 诊断目标 |
| --- | --- |
| Trait default body 有类型错误 | 指向 trait method body，说明失败表达式。 |
| Impl 省略非 default method | 保持现有 missing-method diagnostic。 |
| Impl 省略 defaulted method | 不报错；checked dump 显示 inherited default。 |
| Impl override default 但签名错误 | 现有 signature mismatch，并 note 说明 trait method 虽有 default，override 仍必须匹配。 |
| Duplicate impl override | 保持 duplicate method diagnostic。 |
| Unknown impl method | 保持 unknown method diagnostic。 |
| 多个 trait method candidate 歧义 | 保持 M4 ambiguity；列出 candidates，不选择 default。 |
| Generic call 实例化后使用 default | checked / IR dump 显示最终 origin。 |
| Default body 调用缺 bound 的方法 | 指向 default body 和需要的 trait evidence。 |
| Default body 引用 `Self` 的具体字段 | 在 abstract `Self` 上报告普通 field lookup failure。 |

Tooling 期望：

- override 调用的 definition 跳到 impl method。
- inherited default 调用的 definition 跳到 trait default method body。
- rename identity 仍是 trait method `MemberKey`。
- concrete receiver 的 hover 应说明 selected implementation 是 override 还是 default。

## 8. 风险矩阵

| 风险 | 原因 | 其他语言证据 | Aurex 缓解 |
| --- | --- | --- | --- |
| 静默分派惊讶 | default 看起来像普通方法 | Swift protocol-extension dispatch 容易让用户误判 | 只有 trait-declared requirement 可有 default；call binding 记录 origin。 |
| 多 default 冲突 | 多个 trait 定义同名 default | Kotlin 和 Java 都需要显式冲突规则 | 继续使用 M4 ambiguity，不用 default 打破平局。 |
| Query invalidation 过宽 | 把 default body 复制进 impl | 大型编译器需要稳定 per-body identity | trait-owned body key，只 invalidate default users。 |
| Query invalidation 过窄 | body 修改没连到 call sites | incremental compiler 需要 dependency edges | call/lowering facts 记录 selected default body key。 |
| ABI 不稳定 | symbol name 使用 display text | M3/M4 已拒绝 display-string identity | 使用 stable id、member key、canonical type key。 |
| 代码体积增长 | static monomorphized default bodies | Rust/C++ monomorphization 会增大代码 | 复用 M3.8 lowering/backend fingerprint，并加 stress tests。 |
| 重复诊断 | default 按 impl 重复 type-check | Haskell/Rust default 都是 trait-owned source | 在 trait context type-check 一次，之后实例化。 |
| default 递归 | default 互相调用或自调用 | type class 允许递归 default | 当作运行时递归；未来 lint，不做编译期 expansion。 |
| associated type 歧义 | default body 在多 bound 下用 `Self.Item` | Rust/Swift associated item 都需要谨慎 projection | 使用 M4 member key 和已有 ambiguity/cycle checks。 |
| 未来 dyn trait 被锁死 | default 似乎暗示 object-callability | Rust dyn compatibility 很复杂 | M5 static-only；object safety 未来单独设计。 |
| 未来 specialization 冲突 | default 和 specialization 表面相似 | Rust specialization 难且涉及 semver | M5 不做 overlapping defaults 或 partial ordering。 |
| API 演进破坏 | 新增非 default requirement 会破坏 impl | Java default 为 interface 演进而设计 | 文档明确：新增 default 可兼容；新增非 default 是 breaking。 |

## 9. Work Packages

### M5-WP1：调研和设计基线

状态：本文。

交付：

- 中英文设计文档。
- 中英文 M5 roadmap。
- 调研 Rust、Swift、Kotlin、Java、C#、Haskell/GHC、Scala、Go、C++ 和 compiler-interface architecture。
- 选定语义模型、拒绝方案、风险矩阵、diagnostics、compiler pipeline route 和 validation gates。
- README、next-steps、progress 和 documentation integration tests 更新。

### M5-WP2：Syntax / AST / Body Identity

目标：允许 trait methods 携带 body，但先不改变 trait 语义。

交付：

- Parser 接受 trait method body，并保留 prototype requirement。
- 更新既有 negative parser tests，只拒绝 malformed bodies。
- AST / lossless syntax / dump 区分 prototype 和 defaulted requirement。
- `BodyKey` materialization 使用 `BodySlotKind::trait_default_method`。
- Query tests 覆盖 stable default-body keys。

### M5-WP3：Default Body Type Checking

目标：在 trait context 中 type-check trait-owned default bodies。

交付：

- `TraitMethodRequirement` 记录 default body metadata。
- default body checking 使用 trait `Self`、trait generic params、trait where constraints、current trait evidence 和
  associated-type projection rules。
- default-body diagnostics 指向 trait source。
- 常规仓库测试覆盖正负 default body checking。

### M5-WP4：Impl Completeness And Method Origin

目标：允许 impl 省略 defaulted methods，并显式记录 origin。

交付：

- Impl validation 接受省略 defaulted requirements。
- Override signature checks 保持严格。
- `TraitImplInfo` 按 requirement 记录 explicit override vs inherited default。
- `TraitMethodCallBinding` 为 concrete receiver 记录 selected member 和 final origin。
- checked dumps 显示 inherited defaults 和 method origin。

### M5-WP5：Lowering / Backend / Monomorphization

目标：default method call 降低为 direct call。

交付：

- concrete default calls 降低到 trait-owned default method instances。
- generic `param_env` calls 在 monomorphization 时重新选择 override vs default。
- LLVM/native tests 覆盖 default dispatch、override dispatch、generic dispatch、associated type normalization 和
  inherent-first priority。
- IR / checked dumps 暴露 origin 供回归测试使用。

### M5-WP6：Tooling、Diagnostics、Incremental Reuse

目标：让 IDE 和 query-backed reuse 正确理解 default methods。

交付：

- hover/definition 区分 override 和 default origin。
- workspace index 在 trait method member identity 下记录 default method body。
- rename 仍然基于 member key。
- incremental tests 证明 default-body edits invalidate default users，但不影响 override-only call paths。
- diagnostics 包含 candidate/default origin notes。

### M5-WP7：Release Closure

目标：把 M5 收口成 static default-method baseline。

交付：

- docs、usage notes、version notes、unsupported matrix、samples 和 release baseline 对齐 M5 surface。
- 常规仓库位置中的 positive / negative samples。
- full build、unit/integration tests、sample suite、coverage、query/cache 和 stress gates 通过。
- 后续入口写清楚：dyn traits、specialization、minimal implementation annotations、default associated types 和 resource
  semantics。

## 10. M5-WP1 验收门禁

M5-WP1 完成条件：

- 中英文设计文档存在，并覆盖 research、semantic model、selected design、rejected alternatives、risk matrix、
  compiler pipeline、diagnostics、work packages 和 references。
- 中英文 M5 roadmap docs 存在。
- `README`、`next-steps` 和 `progress` 都说明 M5 default trait methods 是当前 active design stream。
- documentation integration tests 要求新增 M5 docs。
- 至少通过：
  - `tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')`
  - `git diff --check`
  - `cmake --build cmake-build-release -j4`
  - `./cmake-build-release/bin/aurex_tests --gtest_color=auto --gtest_filter='AurexIntegrationTest.DocumentationLayoutIsStable:AurexIntegrationTest.M5DefaultTraitMethodsDesignIsPlanned'`
  - `ctest --test-dir cmake-build-release --output-on-failure -j4`

## 11. References

- Rust Book: Default implementations,
  https://doc.rust-lang.org/book/ch10-02-traits.html#default-implementations
- Rust Reference: traits,
  https://doc.rust-lang.org/reference/items/traits.html
- Rust Reference: implementations,
  https://doc.rust-lang.org/reference/items/implementations.html
- rustc-dev-guide: trait resolution,
  https://rustc-dev-guide.rust-lang.org/traits/resolution.html
- Swift Book: Protocols,
  https://docs.swift.org/swift-book/documentation/the-swift-programming-language/protocols/
- Kotlin Documentation: Interfaces,
  https://kotlinlang.org/docs/interfaces.html
- Java Language Specification, Chapter 9 Interfaces,
  https://docs.oracle.com/javase/specs/jls/se22/html/jls-9.html
- C# language reference: interface,
  https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/interface
- GHC User Guide: `MINIMAL` pragma,
  https://ghc.gitlab.haskell.org/ghc/doc/users_guide/exts/pragmas.html#minimal-pragma
- Haskell 2010 Report: Classes and instances,
  https://www.haskell.org/onlinereport/haskell2010/haskellch4.html#x10-770004.3
- Scala 3 Book: Traits,
  https://docs.scala-lang.org/scala3/book/domain-modeling-tools.html#traits
- Go Language Specification: Interface types,
  https://go.dev/ref/spec#Interface_types
- C++ working draft: constraints and concepts,
  https://eel.is/c++draft/temp.constr
- MLIR Interfaces,
  https://mlir.llvm.org/docs/Interfaces/
- Scharli, Ducasse, Nierstrasz, Black: Traits: Composable Units of Behaviour,
  https://scg.unibe.ch/archive/papers/Scha03aTraits.pdf
- Cook, Hill, Canning: Inheritance Is Not Subtyping,
  https://www.cs.utexas.edu/~wcook/papers/InheritanceSubtyping90/CookHillCanning90.pdf

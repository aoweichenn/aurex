# M5 Default Trait Methods 路线图

## 阶段定位

M5 建立在已经收口的 M4 trait / protocol release baseline 之上。M4 已经给 Aurex 带来 nominal static
trait、显式 `impl Trait for Type`、generic trait predicates、static trait method dispatch、associated types、
tooling projection、diagnostics、coherence 和 direct lowering。M5 不重新打开这些决定，只新增一个聚焦能力：
trait 内部的默认方法体。

完整设计基线见
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)。

## 目标

1. 降低 impl 样板代码，但不引入 inheritance、mixin、dynamic trait object 或 specialization。
2. 保持 M4 static-dispatch-first 模型。
3. 让 method origin 成为 sema、checked dump、tooling、lowering 和 diagnostics 的显式事实。
4. 通过 `BodySlotKind::trait_default_method` 给 default body 稳定 query identity。
5. default body 在 trait context 中只 type-check 一次，lowering / monomorphization 时按 concrete impl 实例化。
6. 保留 dyn traits、specialization、minimal implementation annotations 和 resource semantics 的未来设计空间。

## 非目标

- 不做 `dyn Trait`、trait object layout、vtable ABI、witness-table ABI 或 object safety。
- 不做 specialization、overlapping defaults、blanket impl、negative impl、unsafe trait 或 auto trait。
- 不做 associated const、default associated types、GAT 或新 solver。
- 不做 RAII、`Drop`、`Copy`、borrow checking、move-only values 或 destructor lowering。
- 不做 protocol extensions、mixin state、trait linearization 或隐式全局 search。

## Work Packages

### M5-WP1：调研和设计基线

状态：已完成。

交付：

- 中英文设计文档。
- 中英文 M5 roadmap。
- 调研和比较 Rust、Swift、Kotlin、Java、C#、Haskell/GHC、Scala、Go、C++ 和 compiler-interface architecture。
- rejected alternatives、risk matrix、compiler pipeline plan、diagnostics 和 validation gates。
- documentation index 和 integration test 更新。

验收：

- Documentation tests 要求 M5 design 和 roadmap。
- format、diff、build、documentation test 和 full ctest gates 通过。

### M5-WP2：Syntax / AST / Body Identity

状态：已完成。

目标：在 trait declaration 中接受 default body，同时保留 prototype-only requirement。

交付：

- Parser 接受 trait 内 `fn name(...) -> T { ... }`。
- Parser 继续接受 `fn name(...) -> T;` 作为 non-default requirement。
- malformed trait methods 能恢复，不吞掉下一个 trait item。
- AST / lossless syntax / AST dump 暴露 prototype vs default method state。
- `BodyKey` generation 使用 `BodySlotKind::trait_default_method`。
- query-key tests 覆盖 stable default method body identity。

风险控制：

- parser 不合成 impl methods。
- default bodies 不进入普通 top-level function validation。
- 不改变 inherent impl parsing。

### M5-WP3：Default Body Type Checking

状态：已完成。

目标：在 trait context 下检查 default method bodies。

交付：

- `TraitMethodRequirement` 记录 default-body metadata。
- type checking 使用 trait `Self`、trait generic params、trait where predicates、associated-type projections 和 current
  trait evidence。
- default body errors 指向 trait source。
- default bodies 可以通过 evidence 调用其他 trait requirements。
- negative tests 覆盖 abstract `Self` 上的 concrete field lookup、return type mismatch，以及带 inherited default
  时仍缺少 non-default requirement 的路径。

风险控制：

- trait-owned body 只 type-check 一次，不按 impl 复制 diagnostics。
- 本 WP 不推断 minimal method sets 或 dependency graphs。

### M5-WP4：Impl Completeness And Method Origin

状态：已完成。

目标：让 impl 可以省略 defaulted requirements，并记录 selected origin。

交付：

- missing non-default requirements 仍然报错。
- missing defaulted requirements 成为 inherited defaults。
- override 仍必须在 `Self`、trait arg 和 associated type substitution 后匹配 requirement signature。
- `TraitImplInfo` 按 requirement ordinal 记录 explicit override vs inherited default。
- `TraitMethodCallBinding` 记录 `impl_override`、`trait_default` 或 `param_env`。
- checked dumps 显示 inherited defaults 和 selected call origin。

风险控制：

- default body 不解决两个 visible trait candidates 的歧义。
- inherent methods 仍优先于 trait methods。

### M5-WP5：Lowering / Backend / Monomorphization

目标：保持 default dispatch direct and static。

交付：

- concrete default calls 降低到由 trait-owned default body 生成的 direct functions。
- generic calls 在 generic body checking 阶段保持 `param_env`，实例化时重新选择 override vs default。
- backend symbol names 使用 stable ids、member keys、canonical type keys、trait args 和 associated type outputs。
- IR、LLVM 和 native tests 覆盖 direct default calls、override calls、generic default selection、associated type
  normalization 和 inherent-first priority。

风险控制：

- 不生成 vtable 或 trait object ABI。
- ABI key 不使用 display string。
- 不把 source body 复制进 impl。

### M5-WP6：Tooling / Diagnostics / Incremental Reuse

目标：把 default methods 暴露为一等 semantic facts。

交付：

- hover 和 definition 对 concrete calls 区分 override vs default origin。
- rename 继续基于 trait method `MemberKey`。
- workspace semantic index 记录 default method bodies。
- diagnostics 为 inherited default 和 override mismatch 提供 origin notes。
- incremental tests 证明编辑 default body 会 invalidate default users，但不影响 override-only call paths。

风险控制：

- Tooling 保持 protocol-neutral。
- LSP DTO 不进入 compiler internals。

### M5-WP7：Release Closure

目标：把 M5 收口成 release-quality static default-method baseline。

交付：

- release baseline doc、usage notes、version notes、unsupported matrix 和 roadmap closure。
- normal repository locations 中的 positive / negative samples。
- full build、unit、integration、sample-suite、native、coverage、query/cache 和 stress gates green。
- 后续入口记录 dyn traits、specialization、default associated types、minimal implementation annotations 和 resource
  semantics。

## 完成契约

M5 完成时，Aurex 应能：

- 在 trait 内声明 default method body。
- 在 impl 中省略该方法并继承 default。
- 在另一个 impl 中 override 该方法。
- 在 trait context 中 type-check default body。
- 通过 concrete 和 generic receiver 调用 inherited defaults。
- 把所有 selected calls 降低为 direct LLVM/native calls。
- 在 checked dumps、diagnostics 和 tooling 中解释 selected origin。

涉及 dynamic dispatch、specialization、trait-object callability 或 resource cleanup 的内容继续留给后续阶段。

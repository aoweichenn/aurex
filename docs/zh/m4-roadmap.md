# M4 Trait / Protocol 系统路线图

## 阶段定位

M4 建立在 M3 release baseline 之上。M3 已经完成 module identity、generic instance identity、
query-backed sema、tooling session、incremental syntax、project graph、IDE semantic features 和
query-backed lowering / backend reuse。M4 不重新打开这些边界，而是在它们之上新增 trait / protocol 语言能力。

M4 的第一目标是 **nominal static trait**：

- `trait` 作为语言关键字。
- `protocol` 只作为设计术语。
- conformance 由显式 `impl Trait for Type` 给出。
- 泛型约束用 canonical trait predicate 表达。
- 方法调用默认静态分派，单态化后落到具体 impl method direct call。
- associated type 作为 M4 的第二层能力进入设计，但实现晚于基础 trait / impl / coherence。

M4 明确不把 RAII、`Drop`、`Copy`、move-only、borrow checker、dynamic trait object、vtable ABI、class
inheritance、closure、async/generator、derive、macro 或 package manager 混进当前阶段。资源系统会单独设计。

完整设计基线见 [Aurex M4-WP1 Trait / Protocol 系统调研与设计基线](m4-trait-protocol-system-design.md)。

## 总目标

1. 把当前 `Sized`、`Eq`、`Ord`、`Hash` capability 逐步迁移到 compiler-known builtin trait predicate。
2. 新增用户可定义 trait、显式 trait impl 和泛型 trait bound。
3. 从第一版实现 coherence / orphan / overlap 规则，避免跨模块和未来 package 生态无法维护。
4. 建立可查询的 trait declaration fact、impl registry fact、trait obligation 和 trait evidence。
5. 保持 query-backed sema、incremental cache、tooling、diagnostics、IR lowering 和 backend reuse 的既有 authority boundary。
6. 后续再推进 associated type、tooling 投影和 release closure。

## Work Packages

### M4-WP1：Research And Design Baseline

状态：已完成。

交付：

- 中英文设计文档。
- Rust、Swift、Kotlin、Go、C++ concepts、Scala、Haskell/GHC、MLIR 和相关论文调研。
- 类系统对比和“不先做 class inheritance”的设计原因。
- selected design、rejected alternatives、risk matrix 和 work package 路线。
- README、next-steps、progress 和 documentation integration test 更新。

验收：

- 文档测试包含 M4 设计和路线图。
- 格式、文档测试和 diff check 通过。

### M4-WP2：Syntax / AST / Query Identity Scaffolding

状态：已完成。

目标：只新增语法和结构化身份，不实现完整 trait sema。

交付：

- lexer 新增 `trait` keyword。
- parser 支持 `trait Name { ... }` 和 `impl Trait for Type { ... }` 的基础形状。
- AST 新增 trait item payload 和 trait impl payload。
- AST dump / lossless / stable syntax identity 覆盖 trait 声明和 impl block。
- query key / item signature authority 能识别 trait definition 和 impl definition。

风险控制：

- parser recovery 不能因为 trait body 未完成而破坏普通 item 解析。
- trait impl 和 inherent impl 必须在 AST 层就可区分，不能让 sema 猜。

### M4-WP3：Trait Declaration And Impl Registry

状态：已完成。

目标：让 trait declaration 和 impl fact 进入 query-backed sema。

交付：

- `CheckedModule::traits` 持有 `TraitSignature`，记录稳定 trait identity、visibility、generic 参数和结构化 method
  requirements。
- trait requirement prototype 不进入普通 top-level function/prototype validation，避免把 trait contract 误判成缺实现函数。
- `CheckedModule::trait_impls` 按 trait、self type 和 trait args 建立第一版 registry fact。
- `impl Trait for Type` 做 requirement matching，覆盖 `Self`、trait generic 参数替换、参数/返回类型、unsafe 和 variadic 形状。
- 支持 qualified trait reference、可见性检查、trait generic arity 诊断和 impl self target named-type 检查。
- 正负测试已落到常规仓库测试：`tests/gtest/sema/trait_tests.cpp`、
  `tests/samples/positive/traits/trait_impl_registry.ax` 和 `tests/samples/negative/traits/*.ax`。

风险控制：

- 不允许 promised impl 或 partial impl。
- WP3 只做 exact impl key duplicate 检查，不声称完成 coherence / orphan / overlap。
- trait impl generic block、`where T: Trait`、`ParamEnv` obligation、trait method call resolution、lowering/backend direct call、
  associated type 和 builtin capability migration 都保留到 WP4/WP5/WP6。
- 当前 trait impl key 的 trait args fingerprint 是 WP3 registry 级别的稳定展示键；WP4 coherence 需要升级到 canonical type
  identity / predicate identity。
- builtin primitive trait facts 必须由 compiler-owned provider 管理，不能由用户模块伪造。

### M4-WP4：Coherence And Generic Predicates

状态：已完成。

目标：把 trait bound 变成正式 obligation，并实现第一版 coherence。

交付：

- `CheckedModule` 新增 `TraitPredicate`、`TraitObligation`、`TraitEvidence` 和 `ParamEnvInfo`，`--emit=checked`
  会输出 predicate / obligation / evidence / param env facts，copy/move/rebind 路径同步覆盖。
- `where T: TraitA + TraitB` 进入正式 predicate lowering：`Sized`、`Eq`、`Ord`、`Hash` 继续保持原 capability
  行为，同时记录 compiler-owned builtin trait predicate；非内置名字解析为当前可见 user trait predicate。
- generic instantiation 会用 ParamEnv predicate 做 candidate rejection；具体类型必须有 matching `impl Trait for Type`，
  generic-to-generic 传递则要求调用点当前 ParamEnv 已有同一 trait predicate。
- trait impl registry 增加 canonical coherence fingerprint，保留 WP3 exact duplicate 诊断，并补 orphan rule 与
  first-pass overlap check。
- 正负测试已落到常规仓库测试：`tests/gtest/sema/trait_tests.cpp`、
  `tests/samples/positive/traits/trait_predicate_where_generic.ax`、
  `tests/samples/negative/traits/trait_predicate_unsatisfied_generic_arg.ax` 和
  `tests/samples/negative/traits/trait_impl_orphan_external.ax`。

风险控制：

- M4.0 继续禁止 arbitrary blanket impl；generic trait impl block 仍被拒绝，避免在 solver 未成形前引入 Rust-style
  blanket impl/overlap 复杂度。
- 当前 `where` grammar 仍只支持单个 identifier predicate 名称；qualified where predicate、generic trait predicate
  arguments、associated type constraints 和 arbitrary requires-expression 不在 WP4 范围。
- WP4 只做 first-pass candidate check，不引入全局隐式搜索；真正 trait method binding / evidence lowering 进入 WP5。
- 未来 solver 引入递归 obligation 时必须补 cycle detection 和 depth budget。

### M4-WP5：Static Method Resolution And Lowering

状态：已完成。

目标：让 trait method call 从 sema 绑定到 lowering / backend。

交付：

- inherent method 优先；trait impl method 不再进入普通 inherent method lookup，避免同名 inherent/trait method 污染。
- generic body 中 trait call 通过当前 `ParamEnv` predicate 绑定为 `TraitMethodCallBinding`。
- concrete receiver trait call 通过 visible trait + impl registry 解析到唯一 impl method，并记录 direct-call c symbol。
- associated/static 形式的 `Type.method()` trait call 也走同一条 visible trait + impl registry 解析路径。
- 单态化后的 generic trait call 会重新分析到 concrete impl method，LLVM lowering 生成具体 impl method direct call。
- `--emit=checked` 输出 `trait_method_calls`，区分 `param_env` 和 `impl` dispatch。
- IR dump / LLVM / native smoke 覆盖 receiver trait call、associated/static trait call、function-valued field fallback
  和 inherent-first 优先级。
- 诊断覆盖 ambiguous trait method、bound missing、receiver/associated impl missing 和 method signature mismatch。

风险控制：

- 不生成 vtable。
- 不引入 trait object layout。
- 不把 trait method resolution 做成隐式全局搜索。
- trait requirement method-local generic 仍按现有规则拒绝；generic trait impl block、associated type、dynamic trait
  object 和 RAII/resource semantics 不进入 WP5。

### M4-WP6：Associated Type Model

目标：在基础 trait 系统稳定后加入 associated type。

交付：

- trait associated type declaration。
- impl associated type assignment。
- `Self.Item` / generic projection 的 canonical type。
- `Trait[Item = Type]` equality predicate。
- ambiguity diagnostics 和 projection cycle diagnostics。

风险控制：

- associated type 是 impl output，不参与 impl input matching。
- 不做 generic associated type。
- 不做 associated const。

### M4-WP7：Tooling And Diagnostics

目标：让 IDE/tooling 消费 trait facts，而不是读取 sema internals。

交付：

- completion：`where T:` 后补可见 trait。
- hover / definition：trait、trait method、impl method、associated type。
- semantic tokens：trait name、trait method、impl block、associated type。
- rename：基于 `DefKey` / `MemberKey`。
- diagnostics notes：显示候选 impl、被拒绝原因、orphan / overlap 位置。

风险控制：

- LSP DTO 不进入 compiler internals。
- tooling 输出保持 protocol-neutral value types。

### M4-WP8：Release Closure

目标：把 M4 trait 系统收口为可继续扩展的 release baseline。

交付：

- docs、language manual、unsupported matrix 和 progress 收口。
- tests、coverage、query pruning、driver cache、tooling、IR/backend、stress 和 diff gates。
- trait / associated type release audit。
- 明确后续资源系统、dynamic trait、class-like sugar、default methods 和 specialization 的入口。

## 非目标

- 不做 RAII、`Drop`、`Copy`、resource semantics、borrow checker、lifetime 或 move-only struct。
- 不做 `dyn Trait`、trait object、vtable ABI、object safety 或 dynamic dispatch。
- 不做 class inheritance、virtual methods、base class state、constructor/destructor 一体化对象模型。
- 不做 default methods、associated const、specialization、negative impl、unsafe trait、auto trait。
- 不做 Go-style structural interface、Scala-style implicit/given search 或 C++ arbitrary requires-expression。
- 不做 package manager、dependency resolver、lockfile、registry protocol 或 version solver。

## 当前下一步

M4-WP1、WP2、WP3、WP4、WP5 和 WP6 已完成。当前下一步是 M4-WP7：Tooling And Diagnostics，随后进入
M4-WP8 release closure。

WP4 已在 WP3 registry 之上补齐正式 `TraitPredicate` / `TraitObligation` / `TraitEvidence` / `ParamEnv`
边界，把 `where T: TraitA + TraitB` 降低为 predicate，并实现第一版 orphan / overlap / candidate rejection
诊断。`Sized`、`Eq`、`Ord`、`Hash` 现在保持旧 capability 检查，同时进入 compiler-owned builtin trait predicate
fact，用于后续 solver/evidence 统一。

WP5 已把 trait method resolution 和 lowering 收口：generic body 中的 trait method call 会从当前 ParamEnv
绑定为 `param_env` call fact，concrete receiver 会通过 visible trait + impl registry 绑定为 `impl` direct call；
inherent method 继续优先，trait impl method 不污染普通 method lookup；单态化后 LLVM IR 直接调用具体 impl method。

WP6 已在不重新打开 WP5 静态分派边界的前提下收口第一版 associated type model：trait declaration 支持
associated type requirement，trait impl 支持 associated type assignment，`Self.Item` / generic projection
降低为 canonical associated-projection type，`Trait[Item = Type]` 会给 trait predicate 增加 equality fact，
impl method matching 会替换 impl 给出的 associated type output；sema 已诊断 ambiguity、projection cycle、缺
bound、缺失/未知/重复 associated type、builtin equality 误用、签名不匹配和 equality unsatisfied。

WP7 应把这些 trait 和 associated-type fact 暴露给 IDE/tooling 和 diagnostics：completion、hover、definition、
semantic token、rename identity，以及 candidate/rejection notes 都应该消费稳定 compiler fact，而不是直接读取 sema
内部结构。dynamic trait object 和 RAII/resource semantics 仍由后续独立设计承接。

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

状态：当前阶段。

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

目标：让 trait declaration 和 impl fact 进入 query-backed sema。

交付：

- `TraitSignature` 或等价 item signature authority。
- trait method requirement 结构化记录。
- impl registry query：按 package、module、trait 和 self type 建索引。
- impl method requirement matching。
- 正负测试覆盖缺方法、重复方法、签名不匹配、trait 不可见、impl target 不可命名。

风险控制：

- 不允许 promised impl 或 partial impl。
- builtin primitive trait facts 必须由 compiler-owned provider 管理。

### M4-WP4：Coherence And Generic Predicates

目标：把 trait bound 变成正式 obligation，并实现第一版 coherence。

交付：

- `TraitPredicate`、`TraitObligation`、`TraitEvidence` 和 `ParamEnv` predicate list。
- `where T: TraitA + TraitB` 降低为 canonical predicate。
- orphan rule、overlap check 和 candidate rejection diagnostics。
- `Sized`、`Eq`、`Ord`、`Hash` 迁移到 builtin trait predicate，保持旧样例兼容。

风险控制：

- M4.0 禁止 arbitrary blanket impl。
- solver 必须有 cycle detection 和 depth budget。

### M4-WP5：Static Method Resolution And Lowering

目标：让 trait method call 从 sema 绑定到 lowering / backend。

交付：

- inherent method 优先，trait method 按 lexical bounds / imported trait / impl registry 解析。
- generic body 中 trait call 绑定到 evidence。
- 单态化后生成具体 impl method direct call。
- IR dump / LLVM / native smoke 覆盖 trait call。
- 诊断覆盖 ambiguous trait method、bound missing、impl missing 和 method signature mismatch。

风险控制：

- 不生成 vtable。
- 不引入 trait object layout。
- 不把 trait method resolution 做成隐式全局搜索。

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

M4-WP1 收口后，下一步是 M4-WP2：Syntax / AST / Query Identity Scaffolding。WP2 只落语法和身份地基，不提前实现 solver
和 lowering。这样可以先让 token、parser、AST dump、lossless syntax、query identity 和 docs/test 形成可回归基线，再进入 WP3 / WP4
的 sema 和 coherence。

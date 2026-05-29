# M3 路线图

## 阶段定位

M3 建立在 M2.5 frontend-foundation 之上。M2.5 已经把 query key、结构化诊断、lossless syntax 和 IDE-native snapshot 收到可继续演进的地基；M3 先完成语言层最关键的两个系统，然后把它们继续下沉到 query-backed sema 架构：

1. 模块系统完善。
2. 泛型闭环完善。
3. Query-backed sema 和 IDE/tooling 可消费语义事实。
4. Tooling session、LSP adapter 边界和更细粒度 incremental sema。
5. 基于 query graph 的真实 incremental sema execution。
6. Incremental syntax 和稳定 AST identity。
7. Project graph 和 persistent query database。
8. IDE semantic features 与 query-backed lowering 闭环。

2026-05-25：R5 Compilation Pipeline / Driver Action core 已完成。2026-05-28：M3.0 模块系统
Phase 9A-D 与 M3.1 泛型 release baseline 都已完成。2026-05-29：M3.2 Query-backed Sema 和
M3.3 Tooling Session And Incremental Sema 都已 fast-forward 合并回 `m3`。M3.4、M3.5、M3.6 和
M3.7 已依次完成 real incremental sema execution、stable syntax identity、project graph/persistent query DB
和 IDE semantic features。M3.8 已完成 query-backed lowering、IR unit fingerprint 和 backend emission unit
边界。当前新主线进入 M3.9 release baseline hardening。
所有 M3 实现必须复用
R5 稳定下来的 `CompilationSession`、`CompilationPipeline`、
`FrontendPipeline`、`LoweringPipeline`、`BackendPipeline`、`PipelineStage`、query、diagnostics
和 profile/tooling contract，不允许在 module loader、parser/sema 或 query 之外另开编译旁路。

M3 明确不先实现 RAII，也不把 trait、closure、iterator、derive、package manager 或标准库重建拉进当前阶段。资源语义会影响所有权、drop timing、IR cleanup、泛型 capability 和 ABI，不应该在模块和泛型身份还没稳定前进入实现。

## 阶段目标

### M3.0：模块系统

M3.0 的目标是把当前“一个文件一个模块，loader 递归 import 后拼成 combined AST”的模型，升级为逻辑模块和源文件片段分离的模型：

```text
PackageKey
  |
  +-- ModuleKey        逻辑模块身份
        |
        +-- ModulePartKey    模块的一个源文件片段
        |
        +-- ModuleGraph      import / part / dependency edges
        |
        +-- ModuleExports    public API 和 re-export 表
```

状态：M3.0 Phase 9A-D 已完成语言级模块系统收口。第一阶段只解决同一 package 内的模块拆分，不做
package manager、版本求解或外部依赖系统。

核心交付：

- 设计并实现 module part 语义，让一个逻辑模块可以由多个文件组成。
- 同一逻辑模块的所有 parts 共享 `priv` 可见性边界。
- 保留当前 `import path as alias;` 和 `pub import` 行为，先不引入 glob import。
- 把 module graph、module exports、item list 和 item signature 的 query 边界和 `ModuleKey` / `ModulePartKey` 对齐。
- 让 diagnostics 能指出 module part、import path、重复 part、缺失 part、循环 part/import 等错误的精确 source range。
- 从使用者视角固定 part 文件输入行为：`--check` / frontend inspection 可以从 part 反查 owning primary，
  artifact-producing emit 必须拒绝 part root 并提示 primary。
- `.parts` 布局、part-local imports、module-private `priv` 和大小写冲突都必须有行动导向的 diagnostics。

### M3.1：泛型闭环

M3.1 的目标是把当前已经可用的泛型模板/实例化能力推进为稳定的 query-backed 泛型系统：

- `GenericTemplateSignature`、`GenericInstanceSignature` 和 `GenericInstanceBody` 成为泛型检查和复用的权威边界。
- 泛型 ABI suffix 不再依赖 session-only `TypeHandle` 数字或 display string，而由 `GenericInstanceKey` / canonical type identity 派生。
- `sizeof[T]` / `alignof[T]` 在 generic 函数体内完整通过 sema、IR 和 LLVM lowering。
- method-local generics 从 M2 unsupported 进入 M3 设计和实现。
- generic struct / enum / type alias / function / method 的 visibility、module identity 和 query invalidation 行为保持一致。

M3.1 仍只使用当前内建非资源 capability：`Sized`、`Eq`、`Ord`、`Hash`。用户 trait、associated type、const generic 和资源 capability 不进入 M3.1。

M3.1 的完整执行记录见 [Aurex M3.1 泛型闭环执行计划](m3.1-generics-plan.md)。该文档保留历史
work package、验收门和跨 sema / query / lowering / backend 的全局不变量，供后续阶段引用基线。

状态：M3.1 已在 `59a2ddf Complete M3.1 generic closure audit` 形成 release baseline，并合并回 `m3`。

### M3.2：Query-backed Sema

M3.2 的目标是把 sema 从“单次 eager analyzer 产生 checked module”推进为“query-backed semantic authority”：

- `ItemSignature`、`BodySyntax`、`TypeCheckBody`、`GenericTemplateSignature`、`GenericInstanceSignature` 和
  `GenericInstanceBody` 形成统一 query authority 边界。
- eager sema 可以 materialize query result，但不能继续作为 checked semantic facts 的唯一事实源。
- `CheckedModule` 分清 durable facts、session-local caches 和 lowering-only side tables，并能说明每类事实来自
  哪个 query authority。
- incremental cache、query pruning 和 provider-skip replay 能解释 sema 级结果复用，而不只是文件级复用。
- `aurex_tooling::IdeSnapshot` 和后续 LSP/IDE 消费读取 query-backed semantic facts，不绕过 parser/sema/query。
- M3.2 继承 M3.1 泛型 release baseline，不重新打开已经收口的 identity、ABI、IR/native 路径。

M3.2 仍不实现用户 trait、associated type、const generic、resource capability、RAII、closure、async/iterator
或标准库重建。执行入口见 [Aurex M3.2 Query-backed Sema 设计与执行计划](m3.2-query-backed-sema-plan.md)。

状态：2026-05-29，M3.2 WP-1 到 WP-6 已完成。非泛型 item/body query 已使用 authority-backed provider
input 和共享 result helper，incremental cache subject / provider-skip replay 能解释 item signature、
function body syntax 和 type-check body 的 sema 级复用；lookup/type/generic/body-check service boundary
已进入 sema pipeline；`IdeSnapshot` 已暴露 query-backed semantic facts、records 和 dependency edges。

### M3.3：Tooling Session And Incremental Sema

M3.3 把 M3.2 的 `IdeSnapshot` 语义面推进为长期运行的 tooling 层和最小 LSP adapter 边界：

- `ToolingSession` 持有带版本的 open-document state、package/source-role config 和 snapshot cache。
- LSP JSON-RPC 只是 tooling value types 外面的 adapter，不成为编译器内部 API。
- diagnostics、hover、definition 和 references 消费 `IdeSnapshot`、`PipelineStageMetadata` 和
  query-backed semantic facts。
- incremental reuse planner 使用 `IdeEditImpact`、query records 和 dependency edges 解释一次编辑失效了什么。
- 在完整后台索引之前，先用小型 workspace semantic index 合并 open-file facts。

M3.3 仍不实现完整 completion、rename、formatting、semantic tokens、多线程 scheduler、remote index、
package manager、用户 trait、resource semantics、RAII、closure 或 const generic。执行入口见
[Aurex M3.3 Tooling Session 与 Incremental Sema 计划](m3.3-tooling-incremental-plan.md)。

状态：2026-05-29，M3.3 WP-1 到 WP-6 已完成并合并回 `m3`。协议无关 `ToolingSession`、
versioned open-document state、snapshot cache、最小 `LspServer`、diagnostics、hover、definition、
references、document symbols、reuse planning、workspace semantic indexing 和 quality gates 组成
M3.3 基线。后续 tooling feature 必须建立在这条基线上，不能直接读取 parser、sema、query 或 driver
内部状态。

### M3.4：Real Incremental Sema Execution

状态：2026-05-29，M3.4 已在 `m3.4` 完成当前 deterministic tooling/query 边界。Previous query records
已经成为 `IdeSnapshot` construction 的可执行输入，reuse execution 通过
`ToolingIncrementalSnapshotResult` 暴露，workspace index update 会报告 retained/replaced/removed/inserted
facts，并且不对外暴露旧 document version 的 stale entries。

M3.4 把 M3.3 的 reuse explanation 变成真实局部重算。当前编译器已经能记录 query facts、
dependency edges 和 reuse summary；M3.4 要让 `ToolingSession`、`IdeSnapshot`、sema service 和
query provider 消费 previous snapshot / previous query context，使局部编辑可以复用未受影响的语义事实。

执行入口见 [Aurex M3.4 Real Incremental Sema Execution 计划](m3.4-real-incremental-sema-plan.md)。

核心交付：

- 给 IDE/tooling snapshot build path 增加 previous-snapshot 和 previous-query-cache 输入，同时不改变 CLI 语义。
- 让 query reuse decision 可执行：unchanged query record 必须进入下一次 snapshot 的 reusable semantic facts，
  而不是只出现在 debug explanation。
- 保持 body-local edit 行为：单个函数体内部编辑只重算对应 body syntax / type-check fact，不失效无关
  item signature、body、generic instance、module export 和 workspace fact。
- signature edit 与 module-surface edit 必须扩大到正确 invalidation roots，但不能退回 whole-workspace rebuild。
- workspace semantic index 尽量按 affected fact identity 更新。
- 新增 correctness、malformed-reuse、stress、profile、coverage 和 regression gates。

M3.4 仍不实现 completion、rename、semantic tokens、后台多线程 scheduler、package manager、用户 trait、
RAII、closure 或 resource semantics。

### M3.5：Incremental Syntax And Stable AST Identity

M3.5 把同一条现代编译器主线下沉到 syntax 层。它不新增语言语法，而是让现有 lossless syntax 和 AST lowering
适合反复 IDE 编辑。

核心交付：

- versioned document store 支持 range-based text edits，而不只做 full-text replacement。
- 复用未变化的 lossless syntax subtree，并让 syntax node key 在局部编辑后尽量稳定。
- reparse 未影响区域时保持 AST item/body identity 稳定。
- incomplete editor buffer 下 parser recovery diagnostics 保持局部、确定。
- tooling 的 offset-to-token、syntax-node、AST-node 和 semantic-fact projection 保持一致。

该阶段把 rust-analyzer/rowan 风格 immutable syntax reuse，以及 SwiftSyntax/Roslyn 风格 lossless tree
纪律裁剪到 Aurex 现有 parser 和 AST arena 上；不会引入第二套 frontend。

当前收口状态：

- `ToolingSession` 已新增 `ToolingDocumentTextEdit`、`change_document_range(...)` 和
  `change_document_range_with_reuse_plan(...)`，range edit 可以复用 M3.4 previous snapshot/query context。
- `LosslessNodeStableKey` 已作为位置无关 syntax identity 落地；它不使用绝对 source range 或 token index，
  prefix/local edit 后未变化 subtree 可以重新匹配。
- `compare_lossless_stable_nodes(...)` 和 `ToolingIncrementalSnapshotResult::syntax_reuse` 已能报告 syntax
  reused/recomputed/invalidated node counters。
- `IdeAstNodeInfo` / `ToolingAstNode` 已提供 offset 到 AST item/function body 的 projection，并暴露稳定
  `DefKey` / `BodyKey` 字符串。
- 当前实现仍是 eager aggregate snapshot；物理 green-tree graft、完整 subtree reparse、后台 scheduler 和持久 DB
  继续留给后续阶段。

### M3.6：Project Graph And Persistent Query DB

M3.6 把 module/package identity 提升为工程级执行模型。M3.0 已经解决语言级 module identity，
M3.3 增加了 open-file index；M3.6 要让 CLI check 和 tooling session 共享 project graph 与持久 query DB。

状态：2026-05-30，M3.6 已完成当前实现并进入收口基线。执行记录见
[Aurex M3.6 Project Graph And Persistent Query DB 计划](m3.6-project-graph-persistent-query-db-plan.md)。

核心交付：

- 定义 `ProjectModel` / `WorkspaceModel` 输入：package root、source root、import roots、target config、
  command-line options 和 open buffers。
- source-root、package identity、target config、module graph 和相关 driver options 进入稳定 cache key。
- 从显式 graph nodes 调度 module graph check，而不是依赖 module loader 偶然递归顺序。
- 用 `--check` 和 tooling session 共享的数据库形状持久化 semantic query results。
- profile / invalidation 输出解释哪些 project inputs 改变，以及哪些 query facts 被复用。

该阶段仍不引入 package manager、dependency resolver、lockfile、registry protocol 或 version solver。

### M3.7：IDE Semantic Features

状态：2026-05-30，M3.7 已完成当前 IDE semantic feature 第一层。执行记录见
[Aurex M3.7 IDE Semantic Features 计划与收口记录](m3.7-ide-semantic-features-plan.md)。

M3.7 只在 M3.4 到 M3.6 稳定后新增高层 IDE 能力。LSP 层继续只是 protocol-neutral tooling value types
外面的 adapter。

核心交付：

- completion 已基于 syntax context、sema scope、open workspace facts 和 checked generic/member facts。
- rename 已基于 symbol identity、identifier/keyword/conflict checks 和 workspace edit planning。
- semantic tokens 已由 syntax kind 加 checked semantic facts 合成。
- code actions / quick fixes 已从结构化 help diagnostics 生成 lookup suggestion 修复。
- inlay hints、workspace symbols 和 cross-file surface 已来自 workspace semantic index。
- generation handling 已在 LSP document request 边界防止 stale snapshot 结果被发布。

M3.7 不允许 LSP DTO 泄漏进编译器内部。

### M3.8：Query-backed Lowering, IR, And Backend Reuse

M3.8 已把 query architecture 推进到 sema 以下。M3.2-M3.4 已让 checked semantic facts query-backed；
M3.8 补齐 lowering、IR 和 backend 的显式事实边界，后续 native build 才能复用未受影响的 unit。

核心交付：

- 单个函数体 lowering 和单个 generic instance body lowering 已进入 query authority。
- type layout、enum layout、ABI symbol 和 lower-generic-IR facts 已通过真实 IR unit/layout fingerprint 写入。
- IR pass-manager analysis preservation / invalidation 已进入 pass summary 和 profile detail。
- target-independent IR unit 与 LLVM module/function emission unit 已通过独立 fingerprint 边界分离。
- verifier gates、profile metadata、diagnostics ownership 和 native execution 行为继续走现有 R5 pipeline。

该阶段借鉴 LLVM new pass manager 的 analysis preservation 和 rustc 风格 codegen-unit reuse，但不引入它们的完整复杂度。
收口记录见 [Aurex M3.8 Query-backed Lowering / Backend Reuse 计划与收口记录](m3.8-query-backed-lowering-backend-reuse-plan.md)。

### M3.9：M3 Closure And Release Baseline

M3.9 是 hardening 和收口阶段，不是新功能阶段。

核心交付：

- 对齐 M3.0 到 M3.8 的中英文文档。
- 清理 stale roadmap、obsolete unsupported notes、dead code，以及 M3.4-M3.8 暴露出的 unreachable fallback paths。
- 保持 `ctest`、coverage、query-pruning、query-graph fuzz、generic stress、module graph stress、
  incremental-edit stress、native smoke 和 performance threshold gates green。
- 审计 public API，确保 parser、sema、query、tooling、LSP、lowering 和 backend 不绕过彼此的 authority boundary。
- 在进入 trait/resource/language expressiveness 工作前记录最终 M3 release baseline。

## 非目标

- 不实现 RAII、`Drop`、`Copy`、move-only struct、borrow checker、lifetime 或 automatic resource cleanup。
- 不实现用户 trait / protocol、trait object、associated type、associated const 或 dynamic dispatch。
- 不实现 closure capture、`Fn` / `FnMut` / `FnOnce`、generator、async 或通用 iterator protocol。
- 不重建标准库，不引入 package manager，不做版本求解。
- 不用隐式目录扫描定义模块语义；文件系统布局可以辅助查找，但不能替代语言级 module/part 声明。

## 模块系统设计基线

M3 模块部分的当前设计基线见 [Aurex M3 模块系统设计稿](aurex-module-system-m3-design.md)，
使用体验评估样例见 [Aurex M3 模块系统使用案例](aurex-module-system-m3-example.md)。
路线不再把 primary part list 作为待决策项；M3.0 采用：

```text
显式 primary module 文件
显式 part list
part 文件自声明所属 logical module 和 part name
```

当前固定方向：

- `module path;` 继续表示 primary module 文件。
- `part name;` 在 primary 文件里显式声明 module membership。
- `module path part name;` 是 part 文件声明形式。
- 不用隐式目录扫描定义模块语义；loader 只能按 primary part list 加载 parts。
- imports 是 part-local declaration；同一 logical module 的顶层 item 跨 parts 可见。
- `priv` 表示同一逻辑模块内可见，包括 primary 和所有 parts。
- `--check` / frontend inspection 从 part 文件启动时检查 owning module；IR / LLVM IR / native artifact
  输出从 part 文件启动时拒绝，并提示 owning primary。
- module / part path 只靠大小写区分时必须给 portability diagnostic，避免不同文件系统看到不同 graph。
- `pub` 继续表示跨模块 public API。
- `pub(package)` 已进入 M3.0，用于同一 package 内的可见 surface 和 package re-export。
- `pub use module.Item [as Alias];` / `pub(package) use module.Item [as Alias];` 已作为 M3.0 selective
  re-export 收口能力；只允许 primary module 声明，拒绝 `priv use`、bare `use`、glob use、part-local use
  和 item 之后的 late use。
- M3.0 不引入 glob import/use、nested module tree、`pub(in path)`、`pub(super)`、file-private、
  workspace resolver、dependency resolver、lockfile、version solving 或 package manager。

## 泛型系统候选设计

M3 泛型部分同样会在独立设计里展开。当前路线先固定几个方向：

- 方括号泛型语法继续使用 `Name[T]` 和 `fn id[T](value: T)`，不恢复旧 `<T>` 语法。
- `where T: Sized + Eq` 继续只表示内建非资源 capability，不代表用户 trait。
- 泛型模板检查和具体实例检查都必须输出结构化 diagnostics，不能只在 lowering 或 backend 暴露错误。
- 泛型实例 identity 必须来自 stable key，不来自 display string、C ABI 名称或本次编译的 `TypeHandle` 数字。
- method-local generics 需要先设计 lookup、inference、ABI、query key 和 diagnostics，再进入实现。

M3.1 开发路线固定为：

1. **1A 泛型 ABI 稳定化**：generic struct / enum / function / method 的实例符号名统一由
   `GenericInstanceKey` 和 stable fingerprint 派生，禁止继续拼接 session-only `TypeHandle.value`。
2. **1B 泛型实例身份贯通**：`GenericInstanceIdentity`、fingerprint text、stable def id 和 semantic key
   进入 generic struct、enum、type alias、function、method 的 checked metadata，dump、lookup、query
   subject 和 lowering 不再各自重建身份。
3. **2 泛型 query 权威化**：`GenericTemplateSignature`、`GenericInstanceSignature`、
   `GenericInstanceBody` 从“可用记录”升级成 provider authority；eager sema 负责 materialize/consume，
   不再成为唯一事实源。
4. **3 泛型 body/lowering 闭环**：retained typed body、generic side table、IR lowering、LLVM lowering、
   native execution 和 checked dump 消费同一份 instance body 视图。
5. **4 `sizeof[T]` / `alignof[T]` 闭环**：generic function 内 type operand 先在 sema 形成可验证语义，
   再贯通 IR/LLVM，负例在 sema 阶段给结构化诊断。
6. **5 method-local generics**：实现局部泛型参数的 scope、lookup、inference、ABI、query key、
   capability 约束、diagnostics、IR/native 和 incremental cache 行为。
7. **6 质量门**：普通 gtest、positive/negative samples、native execution、generic stress、query pruning、
   coverage 和全量测试全部保持 green。

## 推荐落地顺序

已完成的 M3.0 模块、M3.1 泛型、M3.2 query-backed sema 和 M3.3 tooling 顺序作为历史验收保留。
M3.4 顺序现已收口：

1. Incremental snapshot build input。已完成。
2. Query record reuse execution。已完成。
3. Body-local / signature-local edit 的 semantic fact stability。已完成。
4. Workspace index incremental update。已完成。
5. Performance、malformed-reuse、coverage 和 stress gates。已完成。
6. 当前阶段文档和 release closure。已完成。

## 当前实现进度

2026-05-26：M3.0 Phase 5 Query records 已完成实现闭环。当前主线已经把 module loader 的
`ModuleRecord` 扩展为 logical module record，记录 primary part、named parts、part-local imports、
import alias 和 public/private visibility；incremental cache 侧已经把模块相关 query 收束为：

- `ModuleGraph(ModuleKey)`：只对当前 logical module 的 primary path、显式 parts 和 import edges
  做结构化 fingerprint；part list 重排不打红，新增 / 删除 part 打红。
- `ItemList(ModuleKey)`：只记录同一 module 内 def identity，签名变化不打红 item list。
- `ModuleExports(ModuleKey)`：记录 public surface，包括 public function/method、struct、public field、
  enum case、type alias、generic template、public const 和 primary-level `pub import` re-export。
- `ModuleExports -> ModuleExports`：只由 primary 文件中的 `pub import` 产生，用于 re-exported module
  public API 变更传播；part-local `pub import` 不提升为 module-level re-export。

已补齐的验收测试覆盖 query provider、query graph kind rules、edge verifier、private body / private
signature / public signature 的红绿行为、part set 结构化 fingerprint，以及 primary re-export 与
part-local `pub import` 的 dependency edge 差异。Phase 5 不引入新语法，目标是把现代编译器式的
稳定 query 边界先固定下来；这条边界随后承接了 ModulePartKey 独立 query、Phase 9 selective
`pub use` 收口，并继续为泛型 query 权威化留出清晰演进点。

下一优先级切为 Phase 6A：独立可见性层级。该阶段先把当前二值 `priv` / `pub` 模型扩展成
`priv < pub(package) < pub` 的语义格，并把 access context、effective visibility、public/package
exports query 边界、import re-export 层级和 private/package type 泄漏检查收束成独立设计。Phase
6A 不引入 `pub(super)` / `pub(in path)`，也不把 file-private、protected/friend 或 package
manager 混入当前模块系统主线。

2026-05-26：Phase 6A-1 Visibility Core 已完成第一步内部重构。当前代码层已经具备
`priv < pub(package) < pub` 的内部层级、`visibility_rank`、`visibility_at_least`、
`effective_visibility` 和稳定 dump 名称；parser 仍只开放既有 `priv` / `pub` 语法，用户可见行为
不改变。Sema 的 access 判断、public surface 泄漏检查、checked dump、AST dump、module loader
记录和 ModuleGraph / ModuleExports query fingerprint 已改为消费层级化 visibility，而不是散落的
二值 public/private 判断。Phase 6A-1 的边界是“内部模型和增量边界先稳定”，不开放
`pub(package)` 源码语法，也不新增 `ModulePackageExports` query kind。

2026-05-26：Phase 6A-2 Access Control Architecture 已完成第二步内部重构。当前代码层已经新增
`sema::DeclContext{PackageKey, ModuleKey, ModulePartKey}`、
`sema::AccessContext{PackageKey, ModuleKey, ModulePartKey}` 和 `sema::VisibilityPolicy`；
`SemanticAnalyzerCore::can_access` 只接受声明上下文，旧的 module-id 入口改名为
`can_access_module` 桥接函数。lookup、generic lookup、field access 和 projected aggregate access
调用点已经迁移到桥接入口，export surface 泄漏检查也改为消费 `VisibilityPolicy::can_expose_type`。
这一步仍不开放 `pub(package)` 源码语法，也不引入 manifest package identity；真实 `PackageKey`
来源、`ModulePackageExports` query 和 package-level re-export 进入后续阶段。

2026-05-26：Phase 6A-3 Package Identity Groundwork 已完成第三步内部重构。当前代码层已经把
root package identity 从 `CompilerInvocation::package_identity` 接入 driver，并新增 `--package`
CLI 选项；未显式指定时继续使用历史默认空 package key，保持现有用户行为和旧缓存兼容。`ModuleLoader`
现在为 root/local-relative imports 继承同一 `PackageKey`，为显式 `-I` / `--import-path` 命中的外部
import root 派生独立 `PackageKey`，并把 package 写入 `ModuleRecord` / `ModuleImportRecord`。Sema
通过 `SemanticOptions::module_packages` 按 `ModuleId` 获得 package 表，`query_module_key`、泛型
template query key 和 `pub(package)` 内部 policy 不再只能看到默认空 package。增量缓存的
source rows、`ModuleGraph`、`ModuleExports`、`ItemList`、item/generic signature subjects 和 re-export
dependency key 已消费同一套 package key；语义 subject 优先用 syntax `ModuleId` 绑定 package，避免
不同包内同名 logical module 混用 stable module fallback；cache header 记录 root package identity，
避免同一源码用不同 package identity 编译时被 coarse source reuse 误复用。该阶段仍不开放
`pub(package)` 源码语法，也不引入 manifest、版本求解或 package manager。

2026-05-26：Phase 6A-4 Package Visibility Syntax 已完成第四步用户语法接入。Parser 现在接受
`pub(package)` 作为 contextual visibility scope，不新增 `package` 关键字；顶层 item、struct field、
impl method 和 import 均能在 AST / dump 中保留 `Visibility::package_`。`pub(crate)`、
`pub(in path)` 和任意其他 scoped visibility 仍明确拒绝，避免提前继承 Rust 语义。Sema 的 exported
surface 检查从只检查 `pub` 扩展为检查 `pub(package)`，因此 package-visible function、field、
enum case、type alias 和 const 不能泄漏 `priv` type；`pub` surface 也会用分层文案诊断
package-visible type 泄漏。该阶段仍不引入 manifest / 版本求解 / package manager，也暂不新增
`ModulePackageExports` query；`pub(package) import` 已能解析并进入 AST，package-level re-export
图语义留给下一阶段独立收口。

2026-05-26：Phase 6A-5 Package Re-export Surface 已完成第五步语义收口。Sema 现在把
public-only `ModuleExports` 缓存和 access-aware lookup 视图分开：`module_export_modules(ModuleKey)`
仍只返回 public re-export 闭包，qualified lookup、module path lookup、补全建议和泛型 selector 使用
当前访问上下文计算的导出闭包。`pub(package) import` 只会在同一 `PackageKey` 内重导出，跨 package
消费者不能通过 facade 看到该边；`priv import` 仍只服务当前模块本地解析，不形成重导出。
增量缓存新增 `ModulePackageExports(ModuleKey)` query kind，只在模块存在 package-visible surface 或需要
传播同包 package surface 时生成；该 query 记录 `pub + pub(package)` surface，并通过
`ModulePackageExports -> ModulePackageExports` / `ModulePackageExports -> ModuleExports` 依赖区分同包目标和
外部 package 目标，避免把外部依赖的 package-private surface 误并入当前包。public `ModuleExports`
fingerprint 不包含 `pub(package) import`，正常测试覆盖 same-package facade、cross-package 隔离、
public re-export 保持跨包可见、private import 不重导出，以及 package import visibility 只影响
`ModuleGraph` / `ModulePackageExports` 而不污染 `ModuleExports`。

2026-05-26：Phase 6A-6 Manifest-backed PackageKey 已完成第六步 package identity 收口。Driver 现在在
没有显式 `--package` 时，会从 root input 所在目录向上查找 `aurex.toml`，读取 `[package] name/version`
与 manifest root 形成 root `PackageKey`；显式 `-I` / `--import-path` 命中的 import root 也会优先使用
该 root 或其祖先的 manifest identity，缺失 manifest 时继续回退到 canonical import root identity。CLI
`--package` 仍是最高优先级 override。增量缓存 header 改为记录解析后的 root package identity，避免
manifest name/version/root 改动后复用旧 cache。该阶段不引入 dependency resolver、lockfile、workspace
或 package manager，只把 manifest 作为模块系统和 `pub(package)` 边界的稳定身份输入。

2026-05-26：Phase 6B-1 Manifest source-root / module-root lookup 已完成模块布局收口。Manifest 现在可在
`[package]` 下声明 `source-root = "src"`；相对路径按 manifest root 解析，并且只在 manifest root 内生效。
Root package 的 local import 和带 manifest 的 `-I` import root 会优先从 source-root 下按 logical module
path 查找文件，例如 `import app.util;` 映射到 `src/app/util.ax`。无 source-root 的 manifest、无 manifest
import root 和单文件调用保持旧的 importer-dir / import-root 查找行为。显式 source-root 会进入
manifest-backed package identity，因此同名同版本但 source layout 不同的编译会话不会共享包级 query key；
incremental cache 的 import path layout 也记录 canonical source-root，避免 import root source layout 改动后
误复用旧 check cache。该阶段仍不引入 workspace、dependency resolver、lockfile、版本求解或 package manager。

2026-05-26：Phase 6B-2 Source-root module topology diagnostics 已完成。启用 manifest
`source-root` 后，primary module 文件路径必须和 `module path;` 对齐；`module app.util;` 的权威文件
位置是 `<source-root>/app/util.ax`。Root input 同样受该规则约束，source-root 外的 primary module 会被
拒绝并给出 source-root 诊断；part 文件继续由 owning primary 和显式 part list 管理，不套用一对一模块
路径规则。无 source-root 的 manifest 和旧单文件/import-root 布局保持兼容。

2026-05-27：Phase 6B-3 Source-root / module topology query 化已完成。`ModuleRecord` 现在把启用
manifest `source-root` 的 primary module 记录为结构化 topology：canonical source-root 和
source-root-relative primary path。Module loader 在首次加载和 cached loader hit 两条路径都会维护该记录；
incremental cache 的 module row 对有 source-root 的模块写出 topology 字段，旧的无 source-root module
row 保持三字段兼容；`ModuleGraph(ModuleKey)` result fingerprint 在存在 topology 时混入
source-root 与相对路径，使 source layout 变化会进入 query red/green 边界，而不是只停留在 loader
局部校验。cache 还写出 `module_source_root_topologies` 计数头，避免带有未使用 source-root import root
的新 cache 自我失效；旧 cache 如果缺少当前 source-root 会话所需的 topology row 或 topology 计数头，
不再走 fast check 复用。该阶段仍不引入 workspace、dependency resolver、nested module tree 或
`pub(in path)`。

2026-05-27：Phase 6C Module Query / Topology Hardening 已完成。`ModuleGraph(ModuleKey)` 的 import
fact 现在混入目标 module 的 `PackageKey`，相同 logical module path 解析到不同 package 时，owner graph
不再错误保持 green。incremental cache header 新增 `import_packages` / `import_package` rows，记录每个
`-I` / `--import-path` 的 resolved package identity；import root manifest name/version/source-root 或
fallback import-root identity 与当前 invocation 不一致时，不走 fast check 复用。有 import path 但缺少
import package identity rows 的旧 cache 会被保守拒绝；无 import path 的旧 cache 保持兼容。该阶段是
M3.0 query/cache 边界加固，不引入 workspace、dependency resolver、lockfile、nested module tree 或
`pub(in path)`。

2026-05-27：Phase 7A-D ModulePartKey / Part Root / Query Cache Hardening 已完成。`ModulePartKey`
现在具备完整 `is_valid` / hash / stable serialize / fingerprint / debug public-key API，`ModuleLoader`
会为 primary part 和 named part 写入 `ModuleRecord.parts[].key`。`--dump-modules` 输出现在展示每个
part 的 display name、kind、stable index、path 和 part key global id；从 part 文件启动
`--check` / `--dump-modules` 会反查 owning primary 并加载完整 owning module graph，而
`--emit=ir`、`--emit=llvm-ir` 和默认 native artifact root 会拒绝 part 文件。`ModuleGraph(ModuleKey)`
的 part fact 现在混入 module/file/name/kind 组成的 part identity，同时排除 declaration-order
`stable_index` 的 semantic 影响，保持 part list 重排 green、新增/删除/改名/换文件 red。普通 gtest
覆盖了 query key、loader part key、part root dump/artifact 行为和既有 part diagnostics；该阶段仍不引入
workspace、dependency resolver、lockfile、nested module tree、selective `pub use` 或 `pub(in path)`。

2026-05-27：Phase 8A-F Sema Part Identity / ModulePart Query Boundary 已完成。8A 把 loader 产生的
part stable index 贯通到 combined AST item：`AstModule::item_part_indices` 与 import scope part index
记录每个 item/import scope 来自 primary 还是 named part；parser-only 路径继续默认 primary part，
syntax 层仍不直接依赖 query key。`SemanticOptions::module_part_keys` 由 driver 按 `ModuleRecord.parts`
构造，sema 可通过 item 获得对应 `ModulePartKey`，并在 `DeclContext` / `AccessContext` 的当前 item
路径中携带 part identity；模块级 fallback 保持 part invalid，避免把 module-only 判断误当作 part-aware
判断。8B 新增 `QueryKind::module_part`、`ModulePartQuery` provider、stable-key decoder / edge verifier
支持和 incremental cache subject/profile/stat 字段；cache 现在可写出 `module_part` query record 以及
`module_part -> parse_file` dependency edge。`module_graph` 暂不依赖 `module_part`，因为 graph 的 part
identity 已有自身 fingerprint，且必须继续保持 part 声明重排 green；把 `stable_index` 传播为 graph
dependency 会错误放大 invalidation。8C 进一步把 part-local import scope 变成 sema 入口处的可验证
不变量：scope 必须覆盖有效 item 区间、覆盖的 item 必须全部来自同一 `ModuleId` 和同一
`part_index`，并且在 `SemanticOptions::module_part_keys` 存在时该 index 必须指向有效
`ModulePartKey`。8D 把该 part origin 消费到 checked metadata 与 debug dump：函数、结构体、枚举
case、类型别名和泛型模板签名都记录来源 `part_index`；当 checked module 含有非 primary part 声明时，
`--emit=checked` / `dump_checked_module` 会以 `@part=N` 暴露来源，函数原型/定义合并时以定义所在 part
为主，若只有 prototype 则使用 prototype 所在 part。8E 把 checked part origin 继续推进到
IDE/tooling definition surface：`IdeDefinition` 现在暴露结构化 `part_index`，hover 的 definition
也能携带该来源；checked global symbol 从函数、结构体、结构体字段、枚举 case、类型别名和泛型模板签名
复制 part origin，AST/local fallback 保持 primary part。8F 把 source-file part context 暴露到
IDE/tooling snapshot 和 structured diagnostics：primary buffer 会返回 resolved `ModulePartKey`、`part_index=0`
与 `<primary>` part name；独立打开的 part buffer 只返回从 `module path part name;` 解析出的语法
module/part 上下文，`resolved=false` 且不伪造 `ModulePartKey`，避免把 parser-only buffer 误声明为
拥有稳定 loader graph 的 module part。普通 gtest 覆盖 sema part context、part-local import scope
contract、checked dump part origin、IDE definition part origin、IDE diagnostic/source part context、
module_part query provider、stable key layout、edge verifier、query executor 和 incremental cache
写入/profile 行为。该阶段不引入 per-part sema isolation、per-part codegen artifact、workspace resolver、
dependency graph、lockfile、nested module tree、selective `pub use` 或 `pub(in path)`。

2026-05-28：Phase 9A-D M3.0 Module System Closure 已完成模块系统收口。9A 把 M3.0 的语言契约
整理成验收矩阵：module part、package visibility、package identity、source-root topology、part root、
part query、IDE source-part context 和 re-export surface 都必须有 parser / loader / sema / query / tooling
边界，不允许再靠 combined AST 偶然行为。9B 把 IDE/tooling 的 standalone part buffer 从 degraded
syntax-only context 推进到可恢复 owning-primary context：当真实路径满足 `<primary>.parts/<part>.ax`、
owning primary 存在、primary module path 与 buffer 声明一致且 primary 显式列出该 part 时，snapshot
会恢复稳定 `ModulePartKey` 和 primary declaration 派生的 `part_index`；module mismatch、unlisted part 或
非 sidecar 路径仍保持 `resolved=false`。9C 实现 primary-level selective re-export：parser 接受
`pub use module.Item;`、`pub use module.Item as Alias;` 和 `pub(package) use ...`；loader 按普通 import
resolution 加载目标模块并记录 `ModuleUseRecord`；sema 迭代解析 re-export chain，校验 alias 冲突、目标
存在性和目标 visibility 是否足以被 re-export；qualified lookup、generic lookup、module exports query、
module package exports query 和 module graph fingerprint 都消费该结构化事实。9D 补齐 conformance：
parser/AST dump、module loader cache、sema integration、IDE tooling part recovery 和既有全量测试必须通过。
该阶段仍明确拒绝 glob use/import、part-local `pub use`、bare/private use、nested module tree、custom
`part from`、`pub(in path)`、file-private、workspace/dependency resolver、lockfile、version solving 和
package manager；M3.0 的范围是语言级模块系统闭环，不是包管理系统。

2026-05-28：M3.1 Generics Completion 已在 `m3.1` 分支完成当前闭环基线。`generic_instance_abi_suffix`
的输入从 `std::vector<TypeHandle>` 改为 `GenericInstanceKey`，generic struct、generic enum、generic
type alias、generic function、owner-generic method 和 method-local generic method 都先计算
`GenericInstanceIdentity`，再用其中的 key 生成 stable id、ABI suffix、instance signature incremental key
和 query subject。`GenericTemplateSignature`、`GenericInstanceSignature`、`GenericInstanceBody` 已成为当前
query authority 边界；retained body、side table、IR lowering、LLVM lowering 和 native execution 消费同一份
instance body view；generic builtin type operand 和 value-only builtin 已通过 sema/IR/LLVM 闭环。WP-7
审计确认 `TypeHandle.value` 只保留为本 session lookup/cache fast key，display string、checked dump、
diagnostics、IR dump 和 c_name 都只作为输出，不再反向作为泛型 identity。

2026-05-29：`m3.1` 已 fast-forward 合并进 `m3`，并已切出 `m3.2` 作为 Query-backed Sema 的设计和实现分支。
M3.2 的设计入口固定为 `m3.2-query-backed-sema-plan.md`；后续推进以 M3.2 work package 为单位，不再每次重新读取
全部 M3 历史。

2026-05-29：M3.2 WP-1 到 WP-6 已 fast-forward 合并回 `m3`，并已从收口基线切出 `m3.3`。
M3.3 的设计入口固定为 `m3.3-tooling-incremental-plan.md`；WP-1 到 WP-6 已完成并合并回 `m3`，
形成 `ToolingSession`、LSP adapter、reuse planner 和 workspace semantic index 基线。

2026-05-29：M3.3 已 fast-forward 合并回 `m3`，并已从收口基线切出 `m3.4`。M3.4 的设计入口固定为
`m3.4-real-incremental-sema-plan.md`；当前阶段优先做真实 incremental sema execution，然后再进入
incremental syntax、project graph persistence、高级 IDE features 和 query-backed lowering。

2026-05-29：M3.4 Real Incremental Sema Execution 已完成当前 deterministic tooling/query 边界。
`ToolingSession` 在 document change 后保留 previous materialized snapshot、精确 edit impact 和 pending
workspace facts；`IdeIncrementalSnapshotInput` 把 previous query records 接入
`build_ide_snapshot_into(...)`；`QueryContext` 对 unchanged file/module/signature/body records 执行 green
reuse；`ToolingIncrementalSnapshotResult` 暴露已执行 reuse plan、reuse counters 和 workspace-index update
stats。该阶段已收口，后续阶段已推进到 M3.5/M3.6。

2026-05-29：M3.5 Incremental Syntax And Stable AST Identity 已完成当前 deterministic tooling/syntax 边界。
`ToolingSession` 支持 range-based text edit；`LosslessNodeStableKey` 和
`compare_lossless_stable_nodes(...)` 提供位置无关 syntax subtree reuse 统计；
`ToolingIncrementalSnapshotResult::syntax_reuse` 暴露 syntax reused/recomputed/invalidated counters；
`IdeAstNodeInfo` / `ToolingAstNode` 将 offset 投影到 AST item 或 function body，并输出稳定 `DefKey` /
`BodyKey`。该阶段已收口，后续 M3.6 Project Graph And Persistent Query DB 也已完成。

2026-05-30：M3.6 Project Graph And Persistent Query DB 已完成当前工程图和持久 query DB 边界。
`ProjectModel` / `WorkspaceModel`、`ProjectKey`、`project_graph` query、incremental cache schema v2、
module graph 显式 project/module-part dependencies、project input profile explanation 和 tooling workspace
model 已进入主路径。下一阶段是 M3.7 IDE Semantic Features。

## 验收

M3.0 模块验收：

- 同一逻辑模块可由多个 part 文件组成。
- part 内 private item、field 和 method 可被同一模块其他 part 访问，但不会暴露给外部模块。
- import、pub import、module path lookup、qualified type/value lookup 和 re-export 行为保持确定。
- primary-level `pub use` / `pub(package) use` 可选择性重导出目标模块里的 type/value/function/generic
  item；alias 冲突、未知 target、visibility 不足、glob use、part-local use 和 late use 都有稳定诊断。
- IDE/tooling 对真实 part buffer 可以在 owning primary 明确列出该 part 时恢复 resolved source-part
  context；无法证明 ownership 时保持 unresolved，不伪造 `ModulePartKey`。
- 重复 module、重复 part、缺失 part、part/module 名不匹配和循环依赖都有稳定诊断。
- part 文件作为检查入口、frontend inspection 入口和 artifact-producing 入口时都有明确且一致的 CLI 行为。
- `.parts` 可发现性、part-local import 误用、module-private `priv` 误解和大小写冲突都有可操作诊断。
- module graph/export query 的 key、dependency 和 invalidation 边界可检查。

M3.1 泛型验收：

- generic template signature、generic instance signature 和 generic instance body 具有明确 query 边界。
- generic ABI suffix 稳定，不依赖本次 session 的 `TypeHandle` 数值。
- `sizeof[T]` / `alignof[T]` 在 generic function 中可完整通过 IR/LLVM。
- method-local generics 有正负样例、diagnostics 和 lowering 覆盖。
- owner-generic method 与 method-local generic method 的 stable identity、lookup、ABI、IR/native 行为已进入
  release baseline。
- 现有泛型 stress、query pruning、sample suite 和 native execution tests 不回退。

M3.2 query-backed sema 验收：

- item signature、body syntax、body type checking、generic template/instance signature/body 都有明确
  sema authority 边界。
- eager checked-module materialization 消费或记录 query facts，不再是唯一 semantic fact 来源。
- durable checked facts、session-local caches 和 lowering-only side tables 被清晰分离并文档化。
- provider skip、query pruning 和 incremental cache trace 能以 query 粒度解释 sema 结果复用。
- tooling-facing semantic snapshot 消费和编译流水线一致的 query-backed facts。

M3.3 tooling / incremental sema 验收：

- 协议无关 `ToolingSession` 持有带版本的 open-document state 和 snapshot cache。
- LSP JSON-RPC handler 消费 tooling value types，不绕过 parser/sema/query internals。
- diagnostics、hover、definition 和 references 复用 `IdeSnapshot`、`PipelineStageMetadata` 和
  query-backed semantic facts。
- edit-impact 和 query dependency records 可以解释 unchanged、recomputed 和 invalidated facts。
- JSON-RPC fixture tests、普通 gtest、coverage、query pruning、fuzz 和 stress gates 保持 green。

M3.4 real incremental sema execution 验收：

- snapshot construction 可以消费 previous snapshot/query context，同时不改变 one-shot caller。
- query reuse decision 可执行，并能把 facts 分类为 reused、recomputed、invalidated 或 malformed。
- body-local edit 复用无关 item signature、body、generic instance、module fact 和 workspace index entry。
- signature 和 module-surface edit 只把 invalidation 扩大到依赖 facts。
- malformed previous reuse input 回退重算，不触发 assertion。
- tests、coverage、query pruning、edit stress 和相关 generic/module stress gates 保持 green。

M3.5 incremental syntax / stable AST identity 验收：

- versioned document store 支持 range-based edit 和 full-text replacement 两种入口。
- `LosslessNodeStableKey` 可在 prefix/local edit 后匹配未变化 subtree，不依赖绝对 range 或 token index。
- `ToolingIncrementalSnapshotResult` 同时报告 sema query reuse、syntax reuse 和 workspace-index update。
- offset-to-token、syntax-node、AST item/body 和 semantic-fact projection 的稳定 key 保持一致。
- incomplete editor buffer 和 out-of-range edit 不破坏 session 状态。
- tests、coverage、query pruning、query graph fuzz 和 generic stress gates 保持 green。

M3.6 project graph / persistent query DB 验收：

- CLI `--check` 和 tooling session 消费同一套 project/workspace input shape。
- source root、package identity、target config、driver options、module graph 和 open buffers 进入稳定 cache key。
- persistent cache 能写入/读取 `project_graph` query rows 和 dependency edges。
- module graph 显式依赖 project graph 和 module parts，query edge verifier 能拒绝 malformed kind/identity。
- profile 输出能解释 project input reuse/reject 和 changed inputs。
- tests、coverage、query pruning、query graph fuzz 和 stress gates 保持 green。

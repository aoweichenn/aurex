# M3 路线图

## 阶段定位

M3 建立在 M2.5 frontend-foundation 之上。M2.5 已经把 query key、结构化诊断、lossless syntax 和 IDE-native snapshot 收到可继续演进的地基；M3 不再继续扩大这条基础设施主线，而是开始设计和实现语言层下一段最关键的两个系统：

1. 模块系统完善。
2. 泛型闭环完善。

2026-05-25：R5 Compilation Pipeline / Driver Action core 已完成。M3.0 现在是当前最高优先级，
但所有 M3 实现必须复用 R5 稳定下来的 `CompilationSession`、`CompilationPipeline`、
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

第一阶段只解决同一 package 内的模块拆分，不做 package manager、版本求解或外部依赖系统。

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
- `pub(package)` / `pub(crate)` 可以作为 M3.0 后半段或 M3.2 设计，不阻塞 module part 的第一阶段。
- `pub use` / selective re-export 是 M3 模块系统的候选能力，但优先级低于 module part 和 exports query。

## 泛型系统候选设计

M3 泛型部分同样会在独立设计里展开。当前路线先固定几个方向：

- 方括号泛型语法继续使用 `Name[T]` 和 `fn id[T](value: T)`，不恢复旧 `<T>` 语法。
- `where T: Sized + Eq` 继续只表示内建非资源 capability，不代表用户 trait。
- 泛型模板检查和具体实例检查都必须输出结构化 diagnostics，不能只在 lowering 或 backend 暴露错误。
- 泛型实例 identity 必须来自 stable key，不来自 display string、C ABI 名称或本次编译的 `TypeHandle` 数字。
- method-local generics 需要先设计 lookup、inference、ABI、query key 和 diagnostics，再进入实现。

## 推荐落地顺序

1. 文档收束：M2.5 只保留 frontend-foundation，M3 模块系统以 [Aurex M3 模块系统设计稿](aurex-module-system-m3-design.md) 为当前基线。
2. 模块 AST 和 parser 设计：按已固定的 primary `part name;` 和 part 文件 `module path part name;` 语法落地。
3. ModuleLoader 设计：允许同一 `ModuleKey` 下多个 `ModulePartKey`，但拒绝重复 part、路径不匹配、
   artifact root、case-fold path collision 和循环；检查型 part root 需要反查 owning primary。
4. Sema 可见性设计：`priv` 跨同一逻辑模块所有 parts 可见；跨模块仍按 import/re-export 和 public API 检查。
5. Module graph/export query：把当前 combined AST 兼容路径保留为实现细节，query record 以 `ModuleKey` / `ModulePartKey` 为准。
6. 泛型 ABI 稳定化：把泛型实例符号名和 incremental key 从 session handle 切到 `GenericInstanceKey` 派生。
7. 泛型 query 权威化：把 instance signature/body 的计算从 eager sema 路径逐步移到 query provider 边界。
8. method-local generics：在稳定模块和泛型 query 边界后实现。

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
稳定 query 边界先固定下来，为后续 ModulePartKey 独立 query、selective `pub use` 和泛型 query
权威化留出清晰演进点。

下一优先级切为 Phase 6A：独立可见性层级。该阶段先把当前二值 `priv` / `pub` 模型扩展成
`priv < pub(package) < pub` 的语义格，并把 access context、effective visibility、public/package
exports query 边界、import re-export 层级和 private/package type 泄漏检查收束成独立设计。Phase
6A 不引入 `pub(super)` / `pub(in path)`，也不把 file-private、protected/friend 或 package
manager 混入当前模块系统主线。

## 验收

M3.0 模块验收：

- 同一逻辑模块可由多个 part 文件组成。
- part 内 private item、field 和 method 可被同一模块其他 part 访问，但不会暴露给外部模块。
- import、pub import、module path lookup、qualified type/value lookup 和 re-export 行为保持确定。
- 重复 module、重复 part、缺失 part、part/module 名不匹配和循环依赖都有稳定诊断。
- part 文件作为检查入口、frontend inspection 入口和 artifact-producing 入口时都有明确且一致的 CLI 行为。
- `.parts` 可发现性、part-local import 误用、module-private `priv` 误解和大小写冲突都有可操作诊断。
- module graph/export query 的 key、dependency 和 invalidation 边界可检查。

M3.1 泛型验收：

- generic template signature、generic instance signature 和 generic instance body 具有明确 query 边界。
- generic ABI suffix 稳定，不依赖本次 session 的 `TypeHandle` 数值。
- `sizeof[T]` / `alignof[T]` 在 generic function 中可完整通过 IR/LLVM。
- method-local generics 有正负样例、diagnostics 和 lowering 覆盖。
- 现有泛型 stress、query pruning、sample suite 和 native execution tests 不回退。

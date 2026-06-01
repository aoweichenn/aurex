# Aurex M6 full audit report v2.0 source recheck

生成日期：2026-06-01

## 0. 复核结论

这份 `aurex_m6_full_audit_report_v2.md` 不是一份废报告。它抓到了不少真正该修的工程风险，尤其是：

- 字段/索引 place 覆盖赋值与资源 cleanup 还没有闭环。
- `[]` 同时承担 generic/index/slice，当前 parser 依赖大小写和 syntactic guardrail。
- Sema ABI/layout 仍使用宿主机 `sizeof/alignof`，没有统一 TargetInfo。
- ID 窄化、SourceManager、TokenCursor、LexerCursor、BumpAllocator、Result 等基础设施需要 hardening。
- LSP framed parser 和 JSON scanner 还不是产品级协议实现。
- Sema/query 的 lazy mutation、`const_cast`、`std::function` provider 设计不适合直接升级到并行 query engine。
- 顶层 README/CMake 默认体验与 M6 实现基线存在错位。

但它也有明显不结合当前源码和 M6 边界的地方。最严重的是把 `defer 没进入 move/use-after-move 分析` 列为最高 P0。当前源码已经有 `DeferredExpression`、`CleanupStack`、defer 注册、cleanup exit edge 复查，并有覆盖 return/break/continue/`?` failure 的负样例。这个条目按当前源码是错误结论。

所以整体评价是：

- **有用，但不是可直接照单执行的“工业大师结论”。**
- **真实问题不少，但 P0/P1 分级偏激。**
- **部分条目混淆了“当前实现 bug”“已知设计取舍”和“M7/M8 才应解决的能力”。**
- **最该马上修的是 place-level cleanup/拒绝策略、TargetInfo、基础 unchecked 边界、LSP hardening，而不是追着报告里每个 P0 文案跑。**

## 1. 复核边界

复核基线：

- 分支：`m6-selfcheck`
- 日期：2026-06-01
- 原报告：`docs/review/aurex_m6_full_audit_report_v2.md`
- 主要对照源码：`src/sema`、`src/ir`、`src/parse`、`src/lex`、`src/tooling`、`src/query`、`src/base`、`cmake`、`docs/zh`

M6 语义边界必须先说清楚，否则容易误判。M6 当前目标是值分类、whole-local move、use-after-move、确定性 cleanup 和 generic drop glue；完整 borrow checker、partial move、indexed move-out、用户显式 destructor/drop 表面都不属于第一版 M6。`docs/zh/m6-resource-access-semantics-design.md:17-23` 明确写了这个边界。

`defer` 也不是 Go 语义。M6 文档冻结的是：deferred call 在 scope exit 执行，参数在执行 action 时求值，不在登记时 snapshot；并且 defer 引用的局部必须在所有可达 exit path 上仍可使用。见 `docs/zh/m6-resource-access-semantics-design.md:453-467`。

## 2. 原报告说得很有道理的部分

### 2.1 字段/索引覆盖赋值没有 place-level cleanup

这个是当前最高优先级真实风险。

证据：

- `src/ir/lower_ast_stmt.cpp:223-247` 处理赋值时只在 `local_binding_for_name_expr(stmt.lhs)` 成功时 drop 旧值并重置 cleanup flag。
- `src/ir/lower_ast_expr.cpp:910-978` 的 `lower_place_address` 明确支持 field、index 和 dereference place 地址。
- 因此 `obj.field = new_resource` / `arr[i] = new_resource` 在 lowering 层可以拿到地址并 store，但旧 place 不是简单 local name 时不会走旧值 cleanup。

这与 M6 设计文档里“覆盖赋值必须先保留旧值直到 RHS 求值结束，再清理旧值并写入新值”的目标一致，文档见 `docs/zh/m6-resource-access-semantics-design.md:69-74`。当前实现只完成了 whole-local 形态，还没有 place-level cleanup。

建议：

- 短期：Sema 对资源字段、资源索引、资源解引用覆盖赋值先明确拒绝，诊断写清楚 M6 暂不支持。
- 中期：引入 `Place { base, projections }`，按 place 执行 drop/reinit/drop flag 更新。

### 2.2 `[]` 解析依赖 syntactic/type-like 启发式

这个问题真实存在，但级别应从“P0 parser 崩盘”降为“P1 语言表面和 parser/sema 分工债”。

证据：

- `src/parse/bracket_suffix_classifier.cpp:11-20` 使用首字母大写判断 type-like。
- `src/parse/bracket_suffix_classifier.cpp:73-104` 根据 continuation、dot、generic continuation、type-like 状态在 parser 阶段决定 generic/index。
- 这不是隐藏 bug，而是文档化的 M2.1 合同：`docs/compiler/parser-architecture.md:57-64` 和 `docs/compiler/parser-architecture.md:212-228` 明确说 generic selector 要求 uppercase/type-like base/args，lowercase selector 按 value indexing。

合理批评是：这个合同会牺牲 lowercase type alias、uppercase value、依赖 name resolution 的更自然语义，也会污染错误恢复和 AST 形态。长期最好让 parser 产出 ambiguous bracket suffix，由 sema 根据 resolved base kind 决议。

### 2.3 host/target layout 混淆

这个是 P1 真实工程风险。

证据：

- `src/sema/internal/sema_type_services.cpp:1062-1118` 的 ABI layout 对 `bool`、`usize`、`isize`、pointer、slice、str 直接使用宿主机 `sizeof/alignof`。
- `src/sema/internal/sema_operator_expression_analyzer.cpp:198-201` 的 `isize/usize` 位宽也来自宿主机类型。
- `src/backend/llvm/llvm_backend_module.cpp:79-96` 后端使用 `llvm::sys::getDefaultTargetTriple()` 并设置 LLVM DataLayout；Sema layout 与 LLVM DataLayout 没有通过统一 TargetInfo/DataLayout 贯通。

只要支持交叉编译、wasm32/i686/aarch64 matrix、repr/FFI，这个就会变成错编风险。应优先引入 `TargetInfo` 并从 driver 传入 Sema/IR/Backend。

### 2.4 unchecked `usize -> u32` 和 ID 容量边界

方向正确，报告说得有价值。

证据：

- `src/base/source.cpp:90-94` `SourceId` 来自 `static_cast<u32>(files_.size())`。
- `src/syntax/ast_expr_nodes.cpp:616-620` `ExprId` 来自 `static_cast<u32>(headers_.size())`。
- `src/syntax/identifier.cpp:92-97` `IdentId` 来自 `static_cast<u32>(texts_.size())`。
- `src/sema/type.cpp:930-934` `TypeHandle` 来自 `static_cast<u32>(types_.size())`。
- `src/driver/incremental_cache/subjects/semantic.cpp:195-223` 多处 authority count 从 vector size 直接 cast 到 `u32`。

这不是普通项目里马上触发的问题，但编译器面对 fuzz、生成代码和 LSP 长生命周期输入时，应该统一 checked ID arena。超过上限时 fatal diagnostic 或禁用缓存，不能 silent wrap。

### 2.5 SourceManager、SourceRange、Token/AST lifetime 边界

这些也是真实基础设施债。

证据：

- `src/base/source.cpp:29-32` `SourceRange::length()` 对 `end < begin` 返回 0，会把坏 range 伪装为空 range。
- `src/base/source.cpp:97-100` `SourceManager::get()` 只有 assert，release 下仍直接 `files_[id.value]`。
- `src/base/source.cpp:69-77` `line_extent()` 只剥 `\n`，不剥 CRLF 的 `\r`。
- `include/aurex/syntax/token.hpp:145-159` `Token` 持有 `const char*` 指向源文本，生命周期没有类型表达。
- AST 和 Sema payload 中还有大量 `std::string_view`，这适合 batch compiler，但对 query cache/LSP snapshot 需要明确 owner。

建议：

- SourceManager 提供 `try_get()` 和 `get_unchecked()` 分层。
- SourceRange 增加 `valid()`、`length_checked()`。
- Token/AST 长期改成 `SourceRange + SourceManager` 或 `IdentId/LiteralTextId`。

### 2.6 TokenCursor、LexerCursor、BumpAllocator、Result 基础 hardening

这些条目多数属实。

证据：

- `include/aurex/parse/token_cursor.hpp:21-35` 的 `peek()/previous()` 在空 token span 下直接 `back()/front()`。
- `include/aurex/parse/token_cursor.hpp:52-68` 用 `current_ + 1`、`current_ + offset`，无溢出检查。
- `include/aurex/lex/lexer_cursor.hpp:60-67`、`:108-122` 依赖 `offset_ + lookahead` 和 assert-only slice。
- `src/base/bump_allocator.cpp:70-92` 在 `size + alignment`、`block_base + used`、`aligned + size` 上没有完整 checked arithmetic。
- `src/base/bump_allocator.cpp:166-176` `normalize_alignment()` 左移直到超过 normalized，极端 alignment 下可溢出。
- `include/aurex/base/bump_allocator.hpp:96-105` arena 为空时 adapter 隐式退到全局 new/delete，容易掩盖 arena owner bug。
- `include/aurex/base/result.hpp:49-70` `value()` / `error()` 标 `noexcept`，但内部 `std::get` 在误用时会抛 `bad_variant_access`，最后变成 terminate。

这些不是语言语义设计问题，而是编译器基础库质量问题。应该作为 M6 hardening 的第一批。

### 2.7 LSP framed parser / JSON scanner 鲁棒性

报告方向正确，但要区分 in-memory framed parser 和 stdio reader。

证据：

- `src/tooling/lsp.cpp:1092-1095` header name 匹配只看前缀，`Content-LengthX` 会匹配。
- `src/tooling/lsp.cpp:1113-1116` `from_chars` 只看 `error == {}`，没有要求 `ptr == end`，`4abc` 会被当成 4。
- `src/tooling/lsp.cpp:1144-1146` 用 `body_begin + content_length`，存在 `usize` 回绕风险。
- `src/tooling/lsp.cpp:190-227` container scan 只用 depth，不区分 `{}` 和 `[]` 类型，`{]` 这类畸形结构会被深度法吞掉。
- `src/tooling/lsp.cpp:267-327` `\uXXXX` 非 ASCII 被替换为 `?`，没有 UTF-8 编码和 surrogate pair。
- `src/tooling/lsp.cpp:166-188` string end scanner 不拒绝裸控制字符。

但 `lsp_stdio` 已经比原报告描述更严：

- `src/tooling/lsp_stdio.cpp:85-95` 会检查 `from_chars` 是否消费完整字符串。
- `src/tooling/lsp_stdio.cpp:138-151` 会拒绝超过 `streamsize::max()` 的长度。
- `tests/gtest/tooling/session_lsp_tooling_tests.cpp:1888-1903` 已覆盖 `12x` 和极大长度。

正确说法是：stdio reader 已有基本严检，但没有固定产品级 `LSP_MAX_MESSAGE_BYTES`；in-memory framed parser 还缺同等严检和溢出防御。

### 2.8 ResourceSemanticsClassifier lazy cache 和 Sema const mutation

这个是架构风险，适合列 P1/P2。

证据：

- `include/aurex/sema/resource_semantics.hpp:87-89` 通过 `mutable` map 和 `indexes_built_` 缓存。
- `src/sema/resource_semantics.cpp:190-211` `ensure_indexes() const` 实际写缓存，没有同步。
- `src/sema/sema_lookup.cpp:929-1010`、`src/sema/internal/sema_*` 多处 `const_cast<SemanticAnalyzerCore&>` 调 analyzer/indexer。

当前单线程 batch compiler 可以工作，但如果未来 query/LSP 并行，这些“const 读路径偷偷写 cache”的设计会产生 data race 和不可重放行为。应在 query database 里显式区分 read view、transaction 和 cache mutation。

### 2.9 QueryProviderSet 使用 `std::function`

报告指出的是方向问题，不是当前 bug。

证据：

- `include/aurex/query/query_provider_set.hpp:23-50` 所有 provider 都是 `std::function<std::optional<...>(...)>`。

这对当前 shadow query 和测试注入很方便。若目标是零成本、并发、可 replay 的工业 query engine，核心路由最终应改为 ops table / typed provider registry / static route table，`std::function` 留在外层 adapter。

### 2.10 CMake/README 工程入口混乱

这部分总体说对，但细节要校准。

证据：

- 顶层 `README.md:1-13` 仍写 `Aurex M2` 和 M2 baseline，而 `docs/README.md:3-6` 已写 M6 baseline。
- `CMakeLists.txt:3-8` 默认设置 clang/clang++。
- `CMakeLists.txt:17` 只有 `AUREX_FRONTEND_ONLY`，没有更直观的 `AUREX_ENABLE_LLVM`。
- `CMakeLists.txt:18-22` Release 默认开 LTO。
- `cmake/AurexLLVM.cmake:1-14` 全量构建要求 LLVM/llvm-config。
- `cmake/AurexTests.cmake:1-5` `BUILD_TESTING=ON` 时 `find_package(GTest REQUIRED)`。

但项目已经有 `CMakePresets.json:10-18` 的 `frontend-minimal`，它会关闭 LLVM 和 tests。报告说“普通配置容易失败”成立，若说“没有低依赖入口”则不成立。

### 2.11 仓库卫生

这个报告说对了。

证据：

- `.gitignore:11` 已忽略 `.idea`，但 `git ls-files .idea` 显示 `.idea/.name`、`.idea/aurex.iml`、`inspectionProfiles/Project_Default.xml` 等已经被跟踪。

应清理已跟踪的 IDE 私有状态，只保留必要的示例配置或明确团队共享配置。

## 3. 说对了但需要降级或改措辞的部分

### 3.1 `strfromutf8` / string slice 失败返回空 `str`

这个是有效设计批评，但不是隐藏实现 bug。

证据：

- `docs/zh/string-primitive-design.md:193-198` 明确写 `s[l:r]` 失败返回空 `str`。
- `docs/zh/string-primitive-design.md:343-347` 明确写 `strfromutf8` 失败返回空 `str`，需要区分时先调用 `strvalid`。
- `tests/samples/positive/types/str_checked.ax` 明确测试 invalid UTF-8 -> `strvalid == false` 且 `strblen(strfromutf8(...)) == 0`。

所以正确评价是：这个 API 会混淆合法空串和失败，需要长期改成 `Option[str]` / `Result[str, Utf8Error]` 或 checked out 参数；但它不是当前实现和文档不一致的 P0。

### 3.2 IdentifierInterner move assignment

这是一个值得优先验证的生命周期风险，但“已确认 UB”还需要 ASan/UBSan 或 allocator 语义复现。

证据：

- `src/syntax/identifier.cpp:46-58` move assignment 先 `texts_.clear()` / `ids_.clear()`，再移动 `arena_`、`texts_`、`ids_`。
- `IdentifierInterner` 自己持有 arena，又持有使用该 arena 的 vector/map，这是典型 arena-backed owner。

需要注意：`BumpAllocatorAdapter` 声明了 `propagate_on_container_move_assignment = true`，这会影响标准容器 move assignment 的 allocator 行为。报告把它直接判成确定 UB 还不够严谨。更稳的结论是：arena-backed owner 应禁止 move assignment 或统一用 swap，补 ASan 回归。

### 3.3 Tooling stale range 直接下标

报告有点夸大。

证据：

- `src/tooling/workspace_index.cpp:21-29`、`src/tooling/session.cpp:442-450`、`src/tooling/reuse.cpp:46-54` 都有 `range.source.value >= files.size()` 检查。
- `src/driver/incremental_cache/subjects/semantic.cpp:322-334` 的 `source_range_text()` 也检查了 source index 和 begin/end。

仍然可以继续抽象成统一 `try_translation_unit_for_range()`，但不能说当前 tooling 全部裸下标。

### 3.4 IR pass cond_branch 合并

报告指出的 guard 建议合理，但当前 pipeline 默认有 verifier gate。

证据：

- `src/ir/pass_pipeline.cpp:502-510` 和 `:529-535` 确实在 `then_target.value == else_target.value` 时直接改成 branch，没有先检查有效性。
- `src/ir/pass_pipeline.cpp:618-625` 默认通过 `VerifierGate` 接入 input/output/after-pass verifier。
- 测试里也有显式 input verifier 失败案例，见 `tests/gtest/ir/pass_pipeline_tests.cpp:326-339`。
- 但测试也允许关闭 verifier，见 `tests/gtest/ir/pass_pipeline_tests.cpp:685-688`、`:761-764`。

所以它是健壮性改进，不应排在资源语义和 TargetInfo 之前。

### 3.5 numeric suffix 和比较链

这两个都是语言设计/诊断质量问题，不是 M6 blocker。

证据：

- `src/lex/lexer_numbers.cpp:133-143` decimal/float suffix 会吞掉任意 identifier continuation。
- `src/parse/parser_expr.cpp:211-219` 使用 `>=` reduce，所有 binary operator 都按左结合处理，因此 `a < b < c` 会形成左结合 AST。

建议作为 P2：明确 suffix 白名单和比较运算 non-associative 诊断。但这不影响当前 M6 资源语义闭环优先级。

## 4. 明显不结合当前源码或边界的部分

### 4.1 `AX-M6-P0-001 defer 没进入 move/use-after-move 分析`

这个结论按当前源码是错的。

证据：

- `src/sema/internal/sema_body_move_analysis.cpp:75-80` 定义了 `DeferredExpression` 和 `CleanupStack`。
- `src/sema/internal/sema_body_move_analysis.cpp:476-483` 在 cleanup scope 中注册 deferred expression。
- `src/sema/internal/sema_body_move_analysis.cpp:485-511` 离开 cleanup scopes 时把 deferred expression 按逆序重新 push 到 move analysis 图中。
- `src/sema/internal/sema_body_move_analysis.cpp:902-904` 遇到 `StmtKind::defer` 时注册 deferred expression。
- `src/sema/internal/sema_body_move_analysis.cpp:942-950` return 路径会 push cleanup scopes。
- `src/sema/internal/sema_body_move_analysis.cpp:963-968` break/continue 路径会 push cleanup scopes。
- `src/sema/internal/sema_body_move_analysis.cpp:1293-1307` `?` failure 路径会 push cleanup scopes。
- `tests/gtest/integration/sample_suite_tests.cpp:100-105` 已有 `move_defer_after_move`、`move_defer_exit_after_later_move`、`move_defer_exit_after_return`、`move_defer_try_failure_after_move` 等负例。

更合理的批评是：Aurex 选择 exit-time evaluation 的 defer 语义，与 Go 的 registration-time argument snapshot 不同，可能需要更强文档、诊断和用户教育。但不能说 defer 完全没进 move analysis。

### 4.2 把 M7/M8 能力当成 M6 必须交付

原报告里对用户可定义 destructor/drop、borrow/lifetime、partial move、indexed move-out、owned String 等设计批评有价值，但不能用来否定 M6 是否完成。

当前 M6 文档明确：

- M6 第一版主线是 resource summary、whole-local move、cleanup、drop glue、tooling/query baseline。
- 完整 borrow checker 在 M7。
- partial move、indexed move-out、完整 lifetime syntax、用户 drop 表面不在第一批。

因此这些条目应放入 M7/M8 roadmap 或 hardening backlog，而不是标成 M6 P0。

### 4.3 `defer` lowering action 形态约束

原报告说 `defer` lowering 对 action 直接 `lower_expr`，表达式形态约束不足。这个建议可以保留，但要结合当前设计：M6 文档说 `defer` 只接受 call expression，当前风险更多是 parser/sema 是否把这个 surface 持续卡死，而不是 IR lowering 单独有 P2 bug。

证据：

- `src/ir/lower_ast_stmt.cpp:281-291` defer lowering 只是把 `stmt.init` 记录为 cleanup action。
- `docs/zh/m6-resource-access-semantics-design.md:453-459` 冻结了 `defer` 的 narrow call surface 和 exit-time evaluation。

建议：加 negative parser/sema tests，确保 `defer` 中不能出现 `?`、return/break/continue 或非 call 表达式。

## 5. 第二层发散 selfcheck：三阶段审计路线

本节是对 v2 复核后的第二层发散审计补充。它不替代前面的逐条复核，而是把新增发现和原报告中的真实问题拆成三个可讨论、可验收的阶段。后续可以按阶段逐个看，避免把“立刻会崩/会错位”的问题和“M7/M8 架构建设”混在一起。

### 5.0 全量问题总览矩阵

下面把本复核中出现过的所有问题重新归类。这里的“阶段”不是原报告严重级别，而是建议讨论和实施顺序。

| 项 | 结论 | 建议阶段 |
|---|---|---|
| `defer` 参数内允许 `?`，`--check` 通过但 `--dump-ir` 可崩溃 | 新增真实 P0；不是原报告的“defer 没进 move analysis”，而是 deferred call 子表达式缺 early-exit 禁令 | 阶段一 |
| LSP line/character 与 semantic token start/length 使用 byte column | 新增真实 P1；LSP 协议要求 UTF-16 code unit | 阶段一 |
| LSP framed parser `Content-LengthX` 前缀匹配、`4abc` 宽松解析、`body_begin + length` overflow | 原报告方向正确；stdio reader已有部分严检，但 in-memory framed parser 仍需统一 | 阶段一 |
| LSP JSON scanner `\uXXXX` 非 ASCII 变 `?`、不处理 surrogate pair、容器只按 depth、不拒绝裸控制字符 | 真实协议鲁棒性问题；与 UTF-16 坐标一起构成非 ASCII tooling 风险 | 阶段一 |
| LSP message size 无统一产品级上限 | 真实 DoS 风险；`lsp_stdio` 已有部分极大值拒绝，但仍缺固定 `LSP_MAX_MESSAGE_BYTES` 合同 | 阶段一 |
| safe `&T` / `&mut T` 可返回局部 alloca 地址 | 新增真实 P0/P1；M7 borrow checker 后移不等于 M6 safe surface 可悬空 | 阶段二 |
| `[]const T` / `[]mut T` / `str` borrowed view 可返回局部数组视图 | 新增真实 P0/P1；与 reference escape 同源 | 阶段二 |
| reference local 可绕过 whole-local move/use-after-move 检查 | 新增真实 P1；`let r = &value; consume(value); use(r);` 当前缺 borrow-origin 追踪 | 阶段二 |
| 字段/索引/解引用 place 覆盖赋值不 cleanup 旧资源 | 原报告最重要真实 P0；当前只处理 whole-local overwrite | 阶段二 |
| `[]` 同时承载 generic/index/slice，靠大小写和上下文启发式 | 真实语言表面和 parser/sema 分工债；不是 parser 立即崩盘 P0 | 阶段二 |
| Sema layout 使用宿主机 `sizeof/alignof`，未贯通 TargetInfo/DataLayout | 真实 P1；交叉编译、FFI、repr/layout 下是错编风险 | 阶段二 |
| `strfromutf8` / string slice 失败返回空 `str` | 文档与实现一致，不是隐藏实现 bug；但 API 错误通道设计弱，长期应改 `Option` / `Result` | 阶段二 |
| numeric suffix 过宽、比较运算默认左结合 | 真实语言/诊断质量问题；不是 M6 blocker | 阶段二 |
| safe reference 目前无完整 alias/lifetime/resource 规则 | 已知 M6 边界，但必须先有阶段二保守禁令，完整模型进 M7 | 阶段二 / M7 |
| 用户 destructor/drop syntax、自定义 destructor lowering | 原报告设计批评有价值，但明确不属于 M6 第一批 | M7/M8 |
| partial move、indexed move-out、replace/take/swap 逃生口 | 真实后续语言能力需求；不能倒逼 M6 P0 | M7/M8 |
| `()` unit、null 边界、owned String、完整 FFI/ABI surface | 设计 backlog；与 M6 hardening 有交集，但不是当前阶段一二的阻塞项 | M7/M8 |
| SourceId/ExprId/TypeHandle/SymbolId/IR Id 等 `usize -> u32` 未检查 | 真实基础设施风险；fuzz/生成代码/LSP 长生命周期会放大 | 阶段三 |
| SourceManager::get assert-only，SourceRange::length() 把 invalid range 伪装为空 range，CRLF line extent 保留 `\r` | 真实 source 基础设施债 | 阶段三 |
| Token 持有 `const char*`，AST/Sema 大量 `string_view` 生命周期未制度化 | batch 编译可用，但 query/LSP snapshot/cache 下需要 owner 规则 | 阶段三 |
| TokenCursor/Lossless 假设 token span 非空，LexerCursor 依赖 assert 和 unchecked add/slice | 真实基础边界问题，适合 hardening/fuzz 阶段 | 阶段三 |
| BumpAllocator checked arithmetic 缺失，adapter 空 arena 退回全局堆 | 真实 allocator/arena owner 风险 | 阶段三 |
| `Result<T>::value/error` 标 `noexcept` 但内部 `std::get` 可抛 | 真实 API 合同错误；误用会 terminate | 阶段三 |
| IdentifierInterner move assignment arena/container 生命周期风险 | 值得修；“已确认 UB”证据不足，需 ASan/UBSan 回归或直接禁 move assignment | 阶段三 |
| AST reserve / TypeTable::copy_from 等 size 估算和 u32 遍历问题 | 真实大输入/fuzz 风险 | 阶段三 |
| ResourceSemanticsClassifier `mutable` lazy cache，Sema read path 大量 `const_cast` | 当前单线程能跑；并行 query/LSP 前必须清理 | 阶段三 |
| QueryProviderSet 核心 provider 用 `std::function` | 当前不是 bug；工业 query engine 方向上应迁移 typed ops table / route table | 阶段三 |
| Query cycle 现在可被 provider 忽略后继续 computed | 有 cycle 识别测试，但 poisoned/failed policy 仍需工业化定义 | 阶段三 |
| 泛型实例化、trait resolution、const eval、query recursion 缺统一 compile budget | 潜在编译期 DoS；属于压力测试和 query/solver hardening | 阶段三 |
| IR pass cond_branch 同目标合并未先校验有效目标 | 有 verifier gate，属于健壮性改进，不优先于资源/ABI/LSP | 阶段三 |
| module path identity 依赖 weakly_canonical fallback 字符串 | module/source-root/package identity 长期风险 | 阶段三 |
| Tooling stale range 裸下标 | 原报告夸大；当前多处已有 range.source 检查，但可抽统一 helper | 阶段三 |
| 顶层 README 仍写 M2，docs 写 M6；CMake 默认 clang/LLVM/GTest/LTO 入口不够平滑 | 真实工程入口和文档单一真源问题 | 阶段三 |
| `.idea` 已跟踪 | 真实仓库卫生问题 | 阶段三 |
| `defer` 没进入 move/use-after-move analysis | 原报告最高 P0 按当前源码是误判；当前已有 DeferredExpression / CleanupStack / return-break-continue-`?` exit 检查和负例 | 不按原项执行 |

### 阶段一：先处理可复现硬失败和 IDE 协议错位

目标：优先处理能够直接复现的崩溃、协议错位和用户立即可感知的错误。这一阶段不扩语言能力，只收紧已有边界。

#### 5.1.1 `defer` 参数中允许 `?`，IR lowering 可崩溃

这是新增 P0。它比“defer 没进 move analysis”的原报告结论更准确：当前 defer 已进入 move analysis，但 `defer` 的 call 子表达式没有禁止 early-exit。

证据：

- `src/sema/internal/sema_statement_analyzer.cpp:932-937` 只检查 `defer` 顶层表达式是不是 call。
- `src/ir/lower_ast_stmt.cpp:687-696` cleanup exit 时直接 `lower_expr(action.defer_expr)`。
- `src/ir/lower_ast_match.cpp:639-651` `?` failure path 会构造 return 并 `emit_cleanup_scopes(0)`。

复现样例：

```aurex
module defer_try_arg;

enum ResultI32I32: u8 {
    ok(i32) = 1,
    err(i32) = 2,
}

fn mark(value: i32) -> void {
}

fn fail() -> ResultI32I32 {
    return ResultI32I32.err(5);
}

fn use() -> ResultI32I32 {
    defer mark(fail()?);
    return ResultI32I32.ok(0);
}
```

当前行为：

- `aurexc --check` 通过。
- `aurexc --dump-ir` 本地复现退出 `139`。

判断：这是 frontend/IR lowering 阶段的真实崩溃，不是设计品味问题。

阶段一验收：

- Sema 明确拒绝 deferred call 的任意子表达式包含 `?` 或 future early-exit 表达式。
- 新增 negative sample，诊断应说明 deferred call 不允许 early exit。
- `--check`、`--dump-ir` 都不能崩溃。

#### 5.1.2 LSP 坐标按字节处理，不符合 UTF-16 `character`

这是新增 P1。Aurex 内部使用 byte offset 没问题，但 LSP 协议边界必须转换为 UTF-16 code unit。

证据：

- `include/aurex/tooling/session.hpp:134-138` 明确注释 `ToolingSourcePosition` 是 line + byte character。
- `src/tooling/session.cpp:1062-1082` `tooling_offset_for_position()` 把 `position.character` 直接加到 line byte begin。
- `src/tooling/session.cpp:1085-1098` `tooling_position_for_offset()` 返回 byte distance。
- `src/tooling/lsp.cpp:513-519` diagnostics/location range 直接输出 byte column。
- `src/tooling/lsp.cpp:531-537` source position 直接输出 byte character。
- `src/tooling/lsp.cpp:977-1005` semantic tokens 的 start 和 length 直接使用 byte-derived column/range length。
- `src/base/source.cpp:59-66` `SourceFile::line_column()` 的 column 也是 byte distance。

影响：

- 中文、日文、emoji 或任意非 ASCII 出现在同一行前面时，hover、definition、references、rename、diagnostics、inlay hints 和 semantic tokens 都可能错位。
- semantic token length 用 byte length 会让客户端高亮范围跨字符或截断。

阶段一验收：

- compiler/tooling 内部继续使用 byte offset。
- LSP adapter 增加 byte offset 与 UTF-16 code unit 的双向转换。
- 增加非 ASCII source 的 hover/definition/rename/diagnostics/semantic tokens 测试。

#### 5.1.3 阶段一处理结果（2026-06-01）

状态：已完成。

落地内容：

- `defer` 语义边界已收紧：`src/sema/internal/sema_statement_analyzer.cpp` 在确认顶层是 call 后，会迭代扫描 deferred call 子树；只要包含 `try_expr` 就报 `defer statement cannot contain try expression`。
- 新增负样例 `tests/samples/negative/functions/defer_try_argument.ax`，并加入 `tests/gtest/integration/sample_suite_tests.cpp` 的普通 negative diagnostics 与 cross-stage emit 验证，覆盖 `--check`、`--dump-ir`、LLVM IR、native emit 入口。
- LSP 保持 compiler/tooling 内部 byte offset 不变，只在 `src/tooling/lsp.cpp` 协议边界转换 UTF-16 code unit：输入 position 转 byte position，输出 diagnostics/range/location/document symbol/completion/rename/code action/inlay hint/semantic token 转回 UTF-16。
- LSP framed parser 已收紧：`Content-Length` header 精确匹配、`from_chars` 必须消费完整数字、`body_begin + length` 改为差值检查，并增加统一 `LSP_MAX_MESSAGE_BYTES`。
- JSON scanner 已补齐阶段一风险点：拒绝裸控制字符、对象/数组按栈匹配、`\uXXXX` 解码为 UTF-8，并正确处理 surrogate pair。

阶段一验证：

- `cmake --build cmake-build-release -j4`
- `ctest --test-dir cmake-build-release -R 'aurex_tests_frontend_only|aurex_tests_core_unit|aurex_tests_functions|aurex_tests_sample_suite_negative' --output-on-failure -j4`
- `ctest --test-dir cmake-build-release --output-on-failure -j4`
- `cmake-build-release/bin/aurexc --check tests/samples/negative/functions/defer_try_argument.ax`
- `cmake-build-release/bin/aurexc --dump-ir tests/samples/negative/functions/defer_try_argument.ax`
- `tools/format_check.py --base HEAD include/aurex/sema/sema_messages.hpp include/aurex/tooling/lsp.hpp src/sema/internal/sema_statement_analyzer.cpp src/tooling/lsp.cpp src/tooling/lsp_stdio.cpp tests/gtest/integration/sample_suite_tests.cpp tests/gtest/tooling/session_lsp_tooling_tests.cpp`
- `git diff --check`
- `tools/check_coverage.sh -j4`

### 阶段二：收紧 M6 safe surface，先挡住引用和 borrowed view 逃逸

目标：不提前实现完整 M7 borrow checker，但要避免 M6 暴露明显 unsound 的 safe surface。这里的策略是保守拒绝，而不是尝试一次性完成 lifetime 系统。

#### 5.2.1 safe `&T` 可以返回局部 alloca 地址

这是新增 P0/P1。M6 文档明确完整 borrow/lifetime 在 M7，但当前 safe reference 已经是语言表面，不能让最小 safe 表面返回局部地址。

证据：

- `src/sema/internal/sema_operator_expression_analyzer.cpp:222-244` address-of 只检查 operand 是 place、`&mut` 可写性和 storage type。
- `src/sema/internal/sema_statement_analyzer.cpp:902-912` return 只做 expected/actual type assign 检查。
- `src/sema/internal/sema_statement_analyzer.cpp:1048-1052` return type validation 只调用 M2 value ABI 检查。

复现样例：

```aurex
module ref_escape;

fn bad() -> &i32 {
    let x: i32 = 1;
    return &x;
}
```

当前 `--dump-ir` 会生成：

```text
%0 : *mut i32 = alloca x
%3 : &i32 = ptrcast %0 to &i32
ret %3
```

判断：这是 safe reference 逃逸局部栈地址，必须在 M7 前加临时禁令。

阶段二验收：

- 禁止从函数返回源自当前函数局部 storage 的 `&T` / `&mut T`。
- 禁止把源自局部 storage 的 reference 存入可逃逸 aggregate、global 或 return value。
- 诊断应同时指向 borrow origin 和 escape site。

#### 5.2.2 slice / `str` borrowed view 可以返回局部数组视图

这是 5.2.1 的同类问题，不应单独推迟。`[]const T`、`[]mut T` 和 `str` 都是 borrowed view，M6 没有 lifetime checker 时至少要禁止局部 view 逃逸。

复现样例：

```aurex
module slice_escape;

fn bad() -> []const u8 {
    let bytes: [1]u8 = b"A";
    return bytes[:];
}
```

当前 IR 会返回指向局部 alloca 的 slice：

```text
%0 : *mut [1]u8 = alloca bytes
%5 : *const u8 = index_addr %0[%4]
%8 : []const u8 = slice %5, %7
ret %8
```

同理：

```aurex
fn bad() -> str {
    let bytes: [1]u8 = b"A";
    return strfromutf8(bytes[:]);
}
```

阶段二验收：

- 禁止返回源自局部 array/slice 的 borrowed slice。
- 禁止返回源自局部 bytes 的 `strfromutf8(...)` / unchecked string view。
- 诊断先保守，不需要完整 region inference。

#### 5.2.3 borrow alias 可绕过 move/use-after-move 检查

当前 direct borrow 会被 defer move analysis 捕获，但如果先把 borrow 存成局部 reference，再在 defer 或普通调用里使用 reference，move analysis 看不到 reference 背后的 pointee。

直接形式会报错：

```aurex
defer inspect(&value);
consume(value);
```

但 alias 形式当前通过：

```aurex
let borrowed: &T = &value;
defer inspect(borrowed);
consume(value);
```

普通非 defer 也类似：

```aurex
let borrowed: &T = &value;
consume(value);
inspect(borrowed);
```

判断：这不是要求 M6 完成完整 alias model，而是要求 M6 不要给 safe reference 一个明显绕过 whole-local move 的洞。

阶段二验收：

- 记录本作用域 reference local 的 borrow origin。
- 在 origin whole-local 被 consumed 后，拒绝后续使用仍指向该 origin 的 reference local。
- 至少覆盖 shared borrow、mutable borrow、defer action、return path。

阶段二处理结果：

- 已新增 `BorrowEscapeAnalyzer`，在函数体 Sema 完成后、move analysis 前扫描 typed AST，拒绝当前函数局部/参数 storage 派生的 `&T`、`&mut T`、`[]const T`、`[]mut T`、`strfromutf8(...)` 与 `strraw(...)` 逃逸；返回值可含 concrete borrow carrier 的 call 会保守追踪实参和方法 receiver，避免 `identity(&local)` / `identity(local[:])` 这类 wrapper 绕过。
- 已扩展 whole-local move analysis：reference local 会记录到 move-only origin，origin 被 consume 后，普通调用、defer cleanup 和 return 表达式里的 alias 使用都会复用现有 `use of moved value` 诊断；`let r = identity(&value)` 这类 call alias 同样会绑定回 origin。
- 已补 M6 临时禁令：move-only resource 的 field/index/deref 覆盖赋值先在 Sema 拒绝，避免 M6 在没有 place-level cleanup/drop flag 的情况下泄漏旧资源。
- 已把 numeric suffix 收紧到 lexer 白名单：integer 只接受 `i8/i16/i32/i64/isize/u8/u16/u32/u64/usize`，float 只接受 `f32/f64`，`1abc` 和 `1.0u8` 这类未知 suffix 不再作为宽松 literal 进入 parser/sema。
- 已把比较/相等运算链设为 non-associative 诊断：`a < b < c`、`a == b == c` 这类链式比较需要改成显式 boolean logic 或加括号。
- 已补 `SemanticTargetLayout`，Sema ABI layout 和 `isize/usize` bit width 不再直接读宿主机 `sizeof/alignof`；M6 默认仍是当前 native 64-bit layout，完整 CLI target triple 到 LLVM `DataLayout` 的产品化贯通继续留给阶段三。
- 新增负例覆盖 reference return 全路径（direct、`&mut`、block、block alias、call、if、index、match、method、tuple、struct、tuple/slice/struct pattern alias、param slot、field assignment）、slice/str/strraw borrowed-view return（含 `strraw` pointer alias、pointer pattern alias、`strfromutf8` block / unsafe-block pointer carrier），全套 borrow alias use-after-move（direct、assignment、block、inner block alias、aggregate carrier、call、deref、field、if、index、method、显式泛型 method receiver、name、tuple carrier、tuple/slice/enum pattern alias、reference-field、defer、return、mutable borrow），resource field/index/deref assignment，以及 `chained_comparison` / `chained_equality`；lexer unit 另覆盖 `1abc`、`1eabc` 与 `1.0u8` unknown suffix，sema whitebox 覆盖 32-bit target layout 下 pointer-sized ABI 与 `isize/usize` literal range。
- 已清理 borrow-origin 遍历中的不可达合法路径：当前 `cast` 只产生 numeric/bool，`ptrcast`/`bitcast` 只产生 raw pointer 或标量，不能合法生成 `&T` / slice / `str` carrier；阶段二只保留 `strfromutf8` 的真实 borrowed carrier 传播。
- 阶段二最终验证通过：相关定向 `ctest` 通过，`tools/check_coverage.sh -j4` 通过，source totals 为 lines 95.48%、functions 99.01%、regions 95.01%。
- `[]` generic/index/slice 的现状是已有 `BracketSuffixClassifier` 承担上下文判定和白盒测试，本阶段未发现新的直接崩溃；长期语法去歧义仍属于后续语言表面整理。
- `strfromutf8` 失败返回空 `str` 是当前文档化合同，本阶段修的是 borrowed-view lifetime 逃逸；`Option`/`Result` 错误通道属于后续核心库/API 设计。
- 这里仍然不是完整 M7 lifetime/borrow checker：M6 只做保守 escape/use-after-move 阻断；field-level reinitialize、partial move、用户析构、分支敏感 alias merge 以及抽象 generic/associated projection 的完整 lifetime 约束仍归后续阶段。

### 阶段三：做 M6 hardening 和工业化边界收口

目标：阶段一、二解决直接硬失败和 safe surface 洞之后，再系统处理原 v2 报告中真实但更偏地基工程化的问题。

阶段三包含：

1. checked ID / source range / source manager：统一 checked `u32` ID 分配，SourceManager 增加 `try_get()`，SourceRange 区分 invalid 和 empty，CRLF line extent 清理。
2. token / AST / `string_view` lifetime：Token 逐步从 raw pointer 借用转向 range/owner，AST/Sema payload 建立 interner/source owner 规则。
3. cursor / lexer / parser hardening：TokenCursor 非空 EOF invariant、LexerCursor checked add/slice；numeric suffix 白名单和比较链诊断已在阶段二完成，不再作为阶段三重复项。
4. allocator / arena owner：BumpAllocator checked add/mul/align，空 arena adapter 的默认 heap fallback 保持兼容，同时补显式 `heap_backed()` / `strict_empty()` 策略，IdentifierInterner 禁 move assignment 或 swap 化。
5. API contract：`Result<T>::value/error` 去掉错误 `noexcept` 或改明确 fatal trap；AST reserve / TypeTable copy 等 size 估算加 checked arithmetic。
6. Query/Sema 并发前置：ResourceSemanticsClassifier lazy mutable cache、Sema `const_cast` read path、`std::function` provider core route、query cycle poisoned policy 和 CompileBudget。
7. IR/pass/backend hardening：cond_branch rewrite 校验有效 block，verifier gate 继续前置，TargetInfo 后续从 CLI target triple 贯通到 LLVM backend matrix。
8. module/tooling identity：ModuleIdentity 区分 package、logical path、physical file、source root；tooling stale range helper 统一。
9. docs/CMake/repo hygiene：顶层 README baseline、LLVM/test/LTO 入口、已跟踪 `.idea` 清理。

阶段三处理结果：

- 已在 `base::integer` 增加 checked `u32` / `usize` add / mul helper，并把 SourceId、syntax AST node/payload id、IdentifierInterner id、Sema TypeHandle/SymbolId、IR Value/Function/Block/Record id、driver ModuleId、QueryNodeId 等核心分配点改成 checked 转换；无法构造 40 亿节点的路径用白盒 helper 覆盖溢出拒绝。
- `SourceManager` 新增 `try_get()`，`text()` 对 stale source 返回空 view；`SourceRange` 增加 `well_formed()`，`empty()` 不再把反向 range 当空 range，CRLF line extent 会去掉行尾 `\r`；driver JSON/text diagnostics 和 IDE diagnostics 不再对 stale source/range 直接 `get()` 崩溃。
- Token 文本从裸 `const char* + range.length()` 改为显式 borrowed `std::string_view`，range 继续作为 source identity；TokenCursor 对空 token span 合成 EOF，避免 parser/lossless/test harness 空流 UB；LexerCursor 的 lookahead、advance_bytes、slice/nonempty_slice 都改成边界检查路径。
- BumpAllocator 补齐 size/alignment/统计 checked arithmetic；BumpAllocatorAdapter 保留默认 heap fallback 以兼容 detached AST materialization，同时新增显式 `heap_backed()` / `strict_empty()` 策略并覆盖 strict-empty 抛错路径；TokenBuffer reserve 和 ExprNodeList touched reserve 的 size 估算也走 checked arithmetic。
- `Result<T>::value/error/take_value` 去掉错误 `noexcept` 合同；IdentifierInterner move assignment 改成 move-construct + swap 化，避免 arena/container owner 重绑时半更新。
- ResourceSemanticsClassifier 不再用 `mutable` lazy cache，构造时一次性建立 struct/enum indexes；分类栈里不再复制 component vector。
- QueryContext 增加 active evaluation cycle poison：provider 即使忽略 reentrant `cycle` 结果并返回 valid output，当前及外层 evaluation 也会失败，不再把循环依赖误标 computed；active poison 栈使用 RAII guard 收口。
- IR CFG cleanup 只在同目标 cond_branch 的目标 block 有效时降级为 branch；invalid same-target cond_branch 会保留给 verifier output gate 报错。
- CMake 增加直观 `AUREX_ENABLE_LLVM`，`frontend-minimal`/`frontend-tests` preset 显式关闭 LLVM，普通 Release 不再默认打开 LTO；README 顶层 baseline 更新到 M6，已跟踪 `.idea` 从索引清理并依赖既有 `.gitignore` 保持本地 IDE 状态不入库。
- 本阶段刻意没有把 typed query route table、完整 CompileBudget、完整 ModuleIdentity/TargetInfo matrix、owned String、partial move、用户 destructor 或完整 borrow checker 塞进 M6；这些仍是 M7/M8 架构项。阶段三完成的是 M6 当前实现能安全收口的 hardening 边界，并对长期项保留明确路线。

阶段三验收：

- 每个 hardening 子项都先补红测试或小型白盒测试。
- 所有变更跑对应 unit/integration tests；进入 release closure 时跑全量 build、ctest 和 coverage。
- 不把用户 destructor、partial move、完整 borrow checker、owned String 等 M7/M8 能力塞回 M6 hardening。

## 6. 建议修复优先级

### P0：资源语义闭环

1. 先补红测试：resource field overwrite、resource indexed overwrite、resource deref overwrite。
2. 短期 Sema 拒绝不支持的 resource place overwrite。
3. 中期做 place-level cleanup/drop flag/reinitialize。

这比改 `[]` 或 QueryProviderSet 更急，因为它直接关系资源泄漏/错编。

### P1：TargetInfo 与基础安全边界

1. 引入 `TargetInfo`，统一 Sema layout、IR lowering、LLVM DataLayout。
2. 建 `checked_u32` / `IdArena`，替换分散 `static_cast<u32>(size())`。
3. SourceManager 加 `try_get()`；SourceRange 加 checked length。
4. TokenCursor 封住 non-empty EOF token invariant。
5. BumpAllocator 使用 checked add/mul/align；Result 去掉错误的 `noexcept` 或改 fatal trap。

### P1：LSP hardening

1. `parse_lsp_content_messages()` 与 `lsp_stdio` 使用同一套 header parser。
2. `Content-Length` 精确 header name，`from_chars` 必须消费完。
3. `body_begin + length` 改成 `length > bytes.size() - body_begin`。
4. 加 `LSP_MAX_MESSAGE_BYTES`。
5. JSON scanner 要么补完整 UTF-8/surrogate/container stack/control char，要么引入小型 JSON parser。

### P1/P2：Query/Sema 并发化前置清理

1. ResourceSemanticsClassifier 构造期建索引，或交给 query DB 管理。
2. Sema const read path 不再 `const_cast` 写缓存。
3. QueryProviderSet 核心路由从 `std::function` 迁移到 typed ops table。

### P2：规范和入口清理

1. 顶层 README 改成 M6 baseline，历史 M2 文档标 archive/current。
2. 增加直观 `AUREX_ENABLE_LLVM`，保留 `AUREX_FRONTEND_ONLY` 作为 preset 组合。
3. Release LTO 默认从普通 Release 移到 explicit preset。
4. 清理已跟踪 `.idea` 文件。
5. `[]` ambiguous AST 与字符串 API 错误通道进入后续语言设计流。

## 7. 最终判断

这份 v2 报告最有价值的地方，是逼出了 Aurex M6 当前最该 harden 的地基：place-level cleanup、TargetInfo、checked ID/arena/source range、LSP 协议边界和 query/sema 并发前置设计。

它最大的问题，是没有充分对齐当前源码和 M6 文档边界：把已经实现的 `defer` move analysis 说成缺失，把已文档化的字符串 empty fallback 当成实现漏洞，把 M7/M8 的 borrow/drop/partial move 能力压回 M6 P0。

执行建议：不要按原报告的 P0/P1 原样排期。按本复核的三阶段 selfcheck 做 `m6-hardening` 工作包：第一阶段先修 `defer` + `?` 崩溃和 LSP UTF-16 坐标，第二阶段收紧 reference/slice/str borrowed-view 逃逸、borrow alias use-after-move、resource place overwrite 临时禁令、numeric/comparison 诊断和 Sema target layout hook，第三阶段再系统处理 place-level cleanup、CLI TargetInfo/LLVM DataLayout 贯通、checked ID/source/arena、LSP parser 和 Query/Sema hardening。

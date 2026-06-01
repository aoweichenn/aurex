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

## 5. 建议修复优先级

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

## 6. 最终判断

这份 v2 报告最有价值的地方，是逼出了 Aurex M6 当前最该 harden 的地基：place-level cleanup、TargetInfo、checked ID/arena/source range、LSP 协议边界和 query/sema 并发前置设计。

它最大的问题，是没有充分对齐当前源码和 M6 文档边界：把已经实现的 `defer` move analysis 说成缺失，把已文档化的字符串 empty fallback 当成实现漏洞，把 M7/M8 的 borrow/drop/partial move 能力压回 M6 P0。

执行建议：不要按原报告的 P0/P1 原样排期。按本复核的顺序做一个 `m6-hardening` 工作包：先资源覆盖赋值和 TargetInfo，再基础 unchecked 边界，再 LSP/Query hardening，最后处理语言表面和文档入口。

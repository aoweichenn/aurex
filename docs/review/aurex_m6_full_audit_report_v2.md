# Aurex M6 源码、语言设计与工业级编译器工程审计完整报告 v2.0

生成日期：2026-06-01

## 0. 审计边界

本报告基于用户上传的 `aurex-m6.zip` 离线源码审计、定向构建验证、源码模式扫描和前几轮问题汇总整理。静态审计不能数学保证“所有 bug 一个不漏”；更高置信度需要完整 CI、ASan/UBSan、fuzz、全量 `.ax` 回归、LLVM 后端测试、LSP 协议压力测试、增量缓存一致性测试和跨 target layout matrix。

已验证：`AUREX_FRONTEND_ONLY=ON`、`BUILD_TESTING=OFF`、`AUREX_ENABLE_LTO=OFF` 下 frontend-only 可构建；默认全量构建会硬依赖 LLVM；默认测试会依赖 GTest；Release 默认 LTO 在缺 LLVMgold/IPO 插件环境下容易失败。

## 1. 执行摘要

Aurex M6 已经具备现代编译器雏形：手写 Lexer/Parser、紧凑 AST、Typed ID、Sema、Trait/Generic、Move/Resource、IR、LSP、Query 影子和 LLVM 后端方向。但它还没有形成工业级编译器的硬边界。当前最应优先修复的不是新增语言特性，而是资源语义闭环、checked ID/arena 基础设施、host/target 分离、LSP 鲁棒性、Query 并发模型和规范单一真源。

最高风险集中在：
- defer 没进入 move/use-after-move 分析。
- 字段/索引覆盖赋值没有 place-level cleanup。
- [] 泛型/索引/切片靠大小写和上下文启发式分类。
- IdentifierInterner move assignment 存在 arena/container 生命周期 UB 风险。
- Sema 类型布局使用宿主机 sizeof/alignof，而不是目标 TargetInfo。
- strfromutf8 和字符串切片失败返回空 str。
- u32 ID / stable key / AST payload index 大量未检查截断。
- BumpAllocator、reserve、LexerCursor、TokenBuffer 等基础层缺 checked arithmetic。
- LSP parser 过宽且 Content-Length 无上限。
- Sema 中 const_cast/lazy mutation 太多，不适合并行 Query。

## 2. 完整问题清单

| ID | 级别 | 领域 | 问题 | 位置 | 影响 | 修复建议 |
|---|---|---|---|---|---|---|
| AX-M6-P0-001 | P0 | 资源语义 / Move Analysis | defer 表达式没有进入 move/use-after-move 分析 | src/sema/internal/sema_body_move_analysis.cpp；src/ir/lower_ast_stmt.cpp | defer 中引用的资源可能在后续被 move；退出作用域执行 defer 时访问已 move 对象，形成漏诊 use-after-move / double-close / use-after-release。 | 先冻结 defer 求值语义；推荐 defer call 参数注册时求值，defer block 退出时执行但禁 early-exit；把 defer action 纳入 CFG exit edge 与 move dataflow；补 return/break/continue/?/normal exit 回归。 |
| AX-M6-P0-002 | P0 | 资源语义 / IR Cleanup | 覆盖赋值只 cleanup 整个局部变量，不 cleanup 字段/索引 place | src/ir/lower_ast_stmt.cpp | obj.field = new_resource 或 arr[i] = new_resource 会直接覆盖旧资源，旧值 drop 丢失；这是确定资源泄漏/错编风险。 | 引入 Place 抽象和 place-level drop/reinitialize；暂不支持字段/索引资源覆盖时由 sema 明确拒绝。 |
| AX-M6-P0-003 | P0 | 语法 / Parser | [] 同时承载泛型实参、索引、切片，并靠大小写/上下文启发式分类 | src/parse/bracket_suffix_classifier.cpp；src/parse/parser_postfix.cpp | parser 用首字母大写猜类型，误判小写 type alias / 大写值变量；错误恢复和 AST 形态被启发式污染。 | Parser 产出 AmbiguousBracketSuffix，由 sema 按 resolved base kind 决议；删除 type-like 大写契约。 |
| AX-M6-P0-004 | P0/P1 | 字符串语义 | strfromutf8 / 字符串切片失败返回空 str | docs/zh/string-primitive-design.md；tests/samples/positive/types/str_checked.ax | 合法空串与非法 UTF-8 / 非法边界无法区分，错误通道错误，可能产生静默数据错误。 | 改为 Option[str] / Result[str, Utf8Error] / bool+out；text[l:r] 必须定义为 trap、Option 或仅对已证明合法 range 开放。 |
| AX-M6-P0-005 | P0/P1 | 内存安全 / Arena Owner | IdentifierInterner move assignment 存在 arena/container 生命周期 UB 风险 | src/syntax/identifier.cpp | move assignment 先 clear 容器、移动覆盖 arena，再移动容器；旧容器 capacity/allocator 可能仍绑定旧 arena，arena 被销毁后容器释放旧 storage 可能用悬空 allocator。 | 删除 move assignment 或实现整体 swap；所有 arena-backed owner 建立统一不可 move-assign 规则；加 ASan 回归。 |
| AX-M6-P1-001 | P1 | Target / ABI | Sema 类型布局使用宿主机 sizeof/alignof，而不是目标平台 TargetInfo | src/sema/internal/sema_type_services.cpp；backend target 初始化相关 | host/target 混淆会导致 usize、pointer、struct layout、FFI ABI、LLVM IR 类型在交叉编译时错编。 | 引入 TargetInfo；Driver 解析 target triple 后传给 Sema/IR/Backend；禁止 sema 对被编译语言语义使用 sizeof(void*) / sizeof(size_t)。 |
| AX-M6-P1-002 | P1 | ID / 容量安全 | SourceId/ExprId/TypeHandle/SymbolId/IR Id 等大量 usize -> u32 未检查窄化 | src/base/source.cpp；src/syntax/ast_*；src/sema/type.cpp；src/sema/symbol.cpp；src/ir/ir.cpp；driver/query/tooling 多处 | 超大输入或 fuzz 可触发 ID 回绕、错误复用、越界访问、错编。 | 统一 IdArena；push 时 checked_u32；超上限 fatal diagnostic 或禁用缓存。 |
| AX-M6-P1-003 | P1 | Source 管理 | SourceManager::get assert-only，release 可越界 | src/base/source.cpp | 非法或 stale SourceId 在 release 下直接 vector 越界；LSP/增量场景危险。 | 提供 try_get() 与 get_unchecked()；tooling 全部用 try_get。 |
| AX-M6-P1-004 | P1 | Source Range | SourceRange::length() 对 end < begin 返回 0，吞掉坏 range | src/base/source.cpp | 坏 range 被伪装为空 range，诊断/LSP hover/文本切片错位且难定位。 | 提供 valid()/length_checked()/length_unchecked；debug assert，release 返回错误。 |
| AX-M6-P1-005 | P1 | Token 生命周期 | Token 持有 const char* 借用源文本，生命周期没有类型表达 | include/aurex/frontend/syntax/core/token.hpp | SourceManager 或临时文本释放后 token.text() 悬空；缓存、LSP、增量复用风险高。 | Token 只存 SourceRange；取文本通过 SourceManager/SourceBufferHandle。 |
| AX-M6-P1-006 | P1 | AST 生命周期 | AST payload 大量 std::string_view，持久化 AST/Query 下生命周期不闭合 | syntax AST payload、sema source-name 字段多处 | AST 被复制/缓存后仍借旧源文本或旧 interner，可能悬空。 | identifier/field/module 用 IdentId；literal 原文用 SourceRange 或 LiteralTextId；generated name 走 owned interner。 |
| AX-M6-P1-007 | P1 | Parser 边界 | TokenCursor/TokenBuffer/Lossless 假设 token span 非空但 API 未封住 | include/aurex/frontend/parse/token_cursor.hpp；src/syntax/lossless.cpp；src/lex/token_buffer.cpp | 空 token span 下 front/back UB；IDE/formatter/fuzz 输入可触发。 | 引入 NonEmptyTokenStream，构造时检查 tokens 非空且末尾 EOF。 |
| AX-M6-P1-008 | P1 | Lexer 边界 | LexerCursor peek/advance/slice 依赖 assert，存在整数 wrap 与越界 string_view 风险 | include/aurex/frontend/lex/lexer_cursor.hpp | offset+lookahead 溢出可读取错误位置；release 下坏 begin/end 生成非法 string_view。 | 使用 checked_add/边界检查；unchecked API 重命名为 *_unchecked。 |
| AX-M6-P1-009 | P1 | Allocator | BumpAllocator allocate/normalize_alignment 存在 size/alignment 加法、左移、地址计算溢出风险 | src/base/bump_allocator.cpp | 极大 size/alignment 下可能欠分配、越界写或死循环。 | 统一 checked_add/checked_align_forward；失败抛 bad_alloc 或返回 Result。 |
| AX-M6-P1-010 | P1 | Allocator | BumpAllocatorAdapter arena 为空时退化到全局堆 | include/aurex/infrastructure/base/bump_allocator.hpp | 核心路径以为使用 arena，实际隐式 new，性能/内存模型不稳定，掩盖 bug。 | 拆 NonNullBumpAllocatorAdapter 与显式 fallback adapter。 |
| AX-M6-P1-011 | P1 | AST reserve | AST reserve/allocation_bytes 估算存在乘加溢出 | src/syntax/ast_expr_nodes.cpp 等 | reserve 过小、内存行为异常，配合自定义 arena 风险更大。 | reserve estimate 使用 checked_mul/checked_add；失败时禁用预留或诊断 too large。 |
| AX-M6-P1-012 | P1 | Parser reserve | ParseSession reserve 对 try expr 的统计错误：? 被计入 unary，exprs.tries 没进 header count | include/aurex/frontend/parse/parse_session.hpp | TryExprPayload reserve 偏小，UnaryPayload 偏大；说明 AST 估算和真实节点不同步。 | question 计入 exprs.tries；parser_expr_reserved_node_count 加 exprs.tries；补测试。 |
| AX-M6-P1-013 | P1 | Result API | Result<T>::value/error 标 noexcept 但内部 std::get 可能抛异常并 terminate | include/aurex/infrastructure/base/result.hpp | release 下误用 Result 会 bad_variant_access -> noexcept -> terminate，不是可诊断错误。 | 用 std::get_if + fatal_trap；或去掉 noexcept。 |
| AX-M6-P1-014 | P1 | Type Table | TypeTable::copy_from 用 u32 index 遍历 usize size | src/sema/type.cpp | types_.size() 超 u32 后 index 回绕，循环错误甚至无限循环。 | 循环用 usize；checked_u32 只在构造 TypeHandle 时做。 |
| AX-M6-P1-015 | P1 | Tooling/LSP | Tooling/LSP 用 range.source.value 直接下标访问 files | src/tooling/workspace_index.cpp；src/tooling/session.cpp；src/tooling/reuse.cpp | 旧 snapshot/stale SourceRange 可直接崩溃。 | 所有 tooling range 查找改为 try_translation_unit_for_range。 |
| AX-M6-P1-016 | P1 | LSP 协议 | LSP Content-Length header 匹配只看前缀 | src/tooling/lsp.cpp | Content-LengthX / Content-Length-evil 可能被当合法 header。 | 按冒号前 header name 精确匹配，大小写不敏感。 |
| AX-M6-P1-017 | P1 | LSP 协议 | LSP Content-Length 解析不检查 from_chars ptr 是否消费完 | src/tooling/lsp.cpp | Content-Length: 4abc 会被接受成 4；与 lsp_stdio 行为不一致。 | 要求 error=={} 且 ptr==end。 |
| AX-M6-P1-018 | P1 | LSP 协议 | LSP body_begin + content_length 有整数溢出 | src/tooling/lsp.cpp | 超大 Content-Length 可回绕绕过 incomplete body 检查。 | 用 content_length > bytes.size() - body_begin 检查。 |
| AX-M6-P1-019 | P1 | LSP 协议 | LSP stdio 按 Content-Length 无上限分配 body string | src/tooling/lsp_stdio.cpp | 恶意客户端可请求分配几十 GB，造成 DoS。 | 设置 LSP_MAX_MESSAGE_BYTES，例如 64 MiB。 |
| AX-M6-P1-020 | P1 | LSP JSON | LSP JSON \uXXXX 非 ASCII 全部变成 ? | src/tooling/lsp.cpp | 中文/日文/emoji 文件路径、URI、symbol 被破坏。 | 实现完整 UTF-8 解码和 surrogate pair。 |
| AX-M6-P1-021 | P1 | LSP JSON | LSP JSON 容器扫描不匹配 {} 与 [] | src/tooling/lsp.cpp | 畸形 {] 也可能被当完整容器。 | 使用栈匹配 expected closer；或引入小型 JSON parser。 |
| AX-M6-P1-022 | P1 | LSP JSON | LSP JSON string scanner 接受裸控制字符 | src/tooling/lsp.cpp | 非法 JSON 被继续处理，协议行为不稳定。 | 拒绝 <0x20 裸控制字符。 |
| AX-M6-P1-023 | P1 | 增量缓存 | Incremental stable key/semantic authority 中 size -> u32 未检查 | src/driver/incremental_cache/subjects/semantic.cpp；src/tooling/ide.cpp | 计数截断会让不同语义实体产生相同 authority key，增量缓存错误复用。 | checked_add + checked_u32；失败时禁用缓存而不是截断。 |
| AX-M6-P1-024 | P1 | 资源分类器 | ResourceSemanticsClassifier const lazy cache 未来并发 Query 下数据竞争 | src/sema/resource_semantics.cpp | const classify/ensure_indexes 实际写 mutable 状态，并行 LSP/module checking 会 data race。 | 构造期 build indexes；或放进受锁 SemaDatabase；或 task-local classifier。 |
| AX-M6-P1-025 | P1/P2 | 资源分类器 | ResourceSemanticsClassifier cycle 分支破坏 active set 递归栈不变量 | src/sema/resource_semantics.cpp | active 节点提前写 completed 后 pop 时不 erase active，算法不变量脆弱。 | 用 SCC/cycle 标记统一处理；保证 frame exit 必 erase active。 |
| AX-M6-P1-026 | P1/P2 | Sema 并发 | 大量 const_cast 表明 const query 偷偷写 cache | src/sema/internal/*；src/sema/sema_lookup.cpp | 后续并行 query/LSP 会产生非确定性和数据竞争。 | 拆 SemaDatabase/SemaReadView/SemaTransaction，不用 const_cast 假装只读。 |
| AX-M6-P1-027 | P1/P2 | Query | QueryProviderSet 核心 query 用 std::function | include/aurex/infrastructure/query/query_provider_set.hpp | 可能隐式堆分配，性能和 ABI 不透明，不符合零成本路由目标。 | 核心用 void* self + ops table 或 consteval query route table；std::function 只做外层 adapter。 |
| AX-M6-P1-028 | P1 | 编译期 DoS | 泛型实例化、trait resolution、const eval、query recursion 缺少统一硬预算 | generics/traits/query/const eval 相关模块 | 递归泛型/trait/const eval 可导致编译器卡死或内存爆炸。 | 引入 CompileBudget，限制 max_generic_depth、max_trait_steps、max_const_eval_steps、max_query_stack_depth 等。 |
| AX-M6-P2-001 | P2 | 文档工程 | README 仍写 M2，docs 写 M6，版本基线混乱 | README.md；docs/README.md | 贡献者不清楚当前 normative spec，历史文档和当前实现边界混乱。 | 顶层 README 改成 M6 baseline，历史文档标 archive/current。 |
| AX-M6-P2-002 | P2 | CMake | 默认 CMake BUILD_TESTING 依赖 GTest，普通配置容易失败 | cmake/AurexTests.cmake | 用户只想编译前端也可能因为缺 GTest 配置失败。 | 新增 AUREX_BUILD_TESTS OFF 默认，或 FetchContent GTest。 |
| AX-M6-P2-003 | P2 | CMake | AUREX_ENABLE_LLVM=OFF 直觉选项不存在，只有 FRONTEND_ONLY 能绕开 LLVM | CMakeLists.txt；cmake/AurexLLVM.cmake | 默认全量构建硬依赖 LLVM；构建接口不直观。 | 新增 AUREX_ENABLE_LLVM option，FRONTEND_ONLY 作为组合 preset。 |
| AX-M6-P2-004 | P2 | CMake | Release 默认打开 LTO，缺 LLVMgold/IPO 插件会配置失败 | CMakeLists.txt | 普通 Release 构建不稳定。 | LTO 默认 OFF，放入 release-lto preset。 |
| AX-M6-P2-005 | P2 | CMake | 顶层 CMake 默认强设 clang/clang++ | CMakeLists.txt | 干扰用户 toolchain、交叉编译和 CI matrix。 | 移入 CMakePresets，不在项目顶层强设。 |
| AX-M6-P2-006 | P2 | Source 诊断 | SourceManager line_extent 只剥 LF 不剥 CR | src/base/source.cpp | CRLF 文件诊断可能保留 CR，快照/LSP 列位置异常。 | 同时剥离 CR。 |
| AX-M6-P2-007 | P2 | Lexer | numeric suffix 在 Lexer 过宽，1abc 整体吞为 integer literal | src/lex 数字扫描相关 | IDE 高亮/错误恢复不清晰；未知 suffix 与 identifier 边界不明确。 | Lexer 直接识别固定 suffix，未知 suffix 产生 invalid numeric literal。 |
| AX-M6-P2-008 | P2 | Parser | 比较运算默认左结合，a < b < c 解析成 (a < b) < c | src/parse/parser_expr.cpp | 错误信息差，C 风格陷阱不适合现代系统语言。 | 比较运算设 non-associative，要求 a < b && b < c。 |
| AX-M6-P2-009 | P2 | IR Lowering | defer lowering 对 action 直接 lower_expr，表达式形态约束不足 | src/ir/lower_ast_stmt.cpp；docs m6 defer | defer 中出现 ?、无意义表达式、复杂副作用时语义/cleanup 难以闭合。 | M6 只允许 call expr 或受限 block，禁止 early-exit。 |
| AX-M6-P2-010 | P2 | IR Pass | IR pass_pipeline cond_branch 合并未先校验目标有效性 | src/ir/pass_pipeline.cpp | 坏 IR 的 invalid/invalid 分支可能被改写为 branch invalid，掩盖原始错误。 | pass 前后运行 verifier；rewrite 先 is_valid。 |
| AX-M6-P2-011 | P2 | Driver | native toolchain shell_hint 不转义路径 | src/driver/native_toolchain.cpp | 路径带空格时错误信息误导。 | 展示用 shell escaping。 |
| AX-M6-P2-012 | P2 | 模块系统 | module path identity 依赖 weakly_canonical fallback 字符串 | src/driver/project_model.cpp；src/driver/module_loader_support.cpp | symlink/大小写/不存在路径/挂载点可能导致 module identity 分裂。 | 引入 ModuleIdentity：package + logical path + physical file + source root。 |
| AX-M6-P2-013 | P2 | 软件工程 | Sema/Tooling 巨型文件过大，OCP 已失控 | src/sema/internal/sema_generic_analyzer.cpp；src/tooling/ide.cpp 等 | 新增功能持续污染巨型 analyzer，难测试难并行难维护。 | 拆 NameResolution/TypeCheck/Generics/Traits/Resource/Place/ABI passes。 |
| AX-M6-P2-014 | P2 | CI / 质量门 | 严格 warning 下已有 signedness/conversion/shadow 债 | base/query/syntax/parse/tooling 多处 | 工业级 C++20 质量门不够硬，后续警告债会指数增长。 | CI 加 strict warning lane，base/syntax/query 先清零。 |
| AX-M6-P2-015 | P2 | 仓库卫生 | IDE 私有目录/配置进入仓库风险 | 项目根目录 | 个人 IDE 状态污染仓库，影响协作一致性。 | gitignore .idea/、build/、cmake-build-*；推荐配置用 example 文件。 |

## 3. 语言设计缺陷清单

| ID | 设计缺陷 | 影响 | 修复建议 |
|---|---|---|---|
| AX-M6-D-001 | 缺少 () unit 类型/字面量，void 被迫承担过多语义 | Result[(), E]、空成功、块尾表达式、泛型参数都不自然；void 易被滥用为存储类型。 | 引入 unit `()`，区分 no-value/no-return 与 zero-sized value。 |
| AX-M6-D-002 | defer 当前退出时求值，用户心智容易和 Go 风格混淆 | defer close(fd); fd = open(b); 退出时看到的是新 fd，不是注册时快照。 | 区分 defer call 注册时求值参数，defer { } 才退出时环境求值；或强文档+诊断。 |
| AX-M6-D-003 | 资源语义缺少用户可定义 destructor/drop 表面 | compiler drop glue 有，但用户不能自然定义 File/Socket/MutexGuard 的 RAII 析构。 | 设计最小 drop(self: &mut Self)，drop 不可 fail/async/move self。 |
| AX-M6-D-004 | 不支持 partial move/field move 时缺少标准 replace/take/swap 逃生口 | 资源型 ADT 使用困难，用户被迫整体 move 或手写样板。 | 先提供 replace(&mut place, new)、take(&mut Option[T])、swap。 |
| AX-M6-D-005 | null 类型边界未冻结 | 裸 null 容易污染 reference/Option/raw pointer 设计。 | null 只允许 expected raw pointer；&T 不可 null；nullable 用 Option[*T] 或 ?*T。 |
| AX-M6-D-006 | FFI/ABI 规范还不完整 | repr(c)、packed/align、calling convention、variadic、symbol visibility、target ABI 等未完全冻结。 | 建立 ABI spec 与 ABI verifier；FFI 类型/布局必须显式。 |
| AX-M6-D-007 | str 是 borrowed UTF-8 view，但 lifetime/origin analysis 未闭合 | strfromutf8(temp_bytes) 返回后可能悬空，除非 M7 borrow checker 证明。 | M7 前禁止 str 借用逃逸；或返回 owned String/Result。 |
| AX-M6-D-008 | safe &T / &mut T 表面已存在，但完整 borrow/lifetime 推迟到 M7 | 返回局部引用、borrow 后 move、mut/shared alias 等基础安全性在 M6 边界不够硬。 | M6 先限制为 non-escaping statement-local borrow；拒绝返回/存储局部引用、borrow 后 move、重叠 &mut。 |

## 4. 重点问题详解与修复模型

### 4.1 defer 与 move analysis 没有闭环

当前风险不是“defer 功能还没完善”，而是 lowering 已经把 defer 当 cleanup action 执行，但 move analysis 直接跳过 defer statement。这会让 defer 捕获的资源在后续被 move 后仍于退出作用域时使用。

推荐修复：先冻结语义。建议采用 `defer close(x);` 参数注册时求值，`defer { ... }` 退出时执行但 block 内禁止 `return/break/continue/?`。随后把 defer action 放入 CFG exit edge，所有 early exit 和 normal exit 都跑同一 cleanup/move 检查。

### 4.2 覆盖赋值必须从 local-level cleanup 升级为 place-level cleanup

当前 lowering 只识别 lhs 是简单局部变量的覆盖赋值。资源字段、数组元素、解引用 place 覆盖都可能绕过旧值 drop。现代资源语义必须以 Place 为中心，而不是以 local name 为中心。

建议引入模型：

```cpp
struct Place {
    LocalId base;
    SmallVector<Projection> projections;
};

Result<void> drop_place_if_initialized(Place place, TypeId type);
Result<void> store_place(Place place, ValueId value);
Result<void> mark_place_initialized(Place place);
```

### 4.3 [] 泛型/索引/切片不能靠大小写猜测

大小写启发式会迫使类型名遵循风格约定，并让 parser 在没有 name resolution 的阶段做语义判断。这不符合 Aurex “不要猜测，不要假设”的方向。

推荐 AST 先保留歧义：

```cpp
ExprKind::bracket_suffix {
    ExprId base;
    BracketArgList args;
    SourceRange range;
}
```

Sema 根据 resolved base kind 决议：type constructor -> generic apply；value place -> index/slice；两者都可能 -> ambiguity diagnostic。

### 4.4 TargetInfo 必须前置

任何被编译语言的布局都不能依赖宿主机 `sizeof(void*)`、`sizeof(size_t)`、`alignof(void*)`。系统语言要从第一天区分 host 与 target。

建议：

```cpp
struct TargetInfo final {
    u8 pointer_size_bytes;
    u8 pointer_align_bytes;
    u8 usize_size_bytes;
    u8 usize_align_bytes;
    u8 bool_size_bytes;
    u8 bool_align_bytes;
    Endianness endian;
};
```

Driver 解析 target triple，Sema layout、IR lowering、LLVM backend 使用同一个 TargetInfo/DataLayout。

### 4.5 Arena-backed owner 必须有统一移动规则

IdentifierInterner 暴露的问题本质是：对象自己持有 arena，又持有使用该 arena 的容器。此类对象必须禁止 move assignment，或者保证 arena 与所有容器 storage/allocator 成组 swap。

建议建立 `NonAssignableArenaOwner` 基类或工程规则：arena-backed database/interner/store 默认 delete copy 和 move assignment，只允许 move construct 或稳定 heap owner。

### 4.6 Query Engine 需要制度化

当前 query 影子已经出现，但还不是真正 query engine。工业级 query 需要强类型 key、cycle detection、poisoned 状态、并发安全、cancel、deterministic replay、per-query diagnostics，并且 query key 必须带 ModuleId、TargetId、Epoch。

## 5. 软件工程与现代编译器工业实现评价

### 5.1 Pass 边界不够硬

现代编译器每个 pass 都应该有输入/输出 contract，并在 debug/CI 下运行 verifier。当前 IR verifier 存在，但还没有成为 pass pipeline 的硬边界，坏 IR 可能被后续 pass 改写并掩盖根因。

### 5.2 Sema 巨型中心化，不符合 OCP

SemanticAnalyzerCore 承担 name resolution、type checking、generic、trait、resource、pattern、layout、diagnostic 等过多职责。新增功能会继续污染巨型 analyzer，难以并行、难以增量、难以单测。

### 5.3 Query 还不是严格 Query Engine

当前 query 影子存在，但 key 强类型化、cycle detection、poisoned 状态、并发模型、per-query diagnostics、cancellation 和 deterministic replay 还不完整。

### 5.4 Target/ABI 没有前置到全管线

系统语言必须把 target triple、pointer width、endianness、ABI、DataLayout 从 driver 传入 Sema/IR/Backend。host sizeof/alignof 不能进入被编译语言语义。

### 5.5 Arena / ID / string_view 生命周期缺少制度

项目大量使用 compact ID、arena、string_view，这很适合高性能编译器，但必须有统一 owner/move/lifetime 规则，否则 LSP、增量、query 缓存会放大悬空引用。

### 5.6 LSP/Tooling 应有独立鲁棒性质量门

LSP 是长生命周期服务，输入来自外部客户端。它需要 malformed input fuzz、消息大小限制、snapshot generation、stale range 防御，而不是沿用 batch compiler 的 assert-only 假设。

### 5.7 文档规范没有单一真源

README、docs、历史设计文档同时存在且版本基线冲突。语言项目必须明确 current spec、historical design note、implementation note 的边界。

## 6. 建议新增红测试

1. IdentifierInterner move assignment 在 ASan/UBSan 下不崩溃，arena 与容器 storage 始终成组迁移。
2. defer 中使用被 move 的值必须报错，诊断同时指向 defer statement 和 move origin。
3. defer + return / break / continue / ? / normal exit 都必须执行相同 cleanup stack。
4. obj.field = open(...) 覆盖资源字段时旧资源必须 drop；暂不支持则必须拒绝。
5. arr[i] = resource 覆盖元素时旧元素 drop；错误路径 index 表达式 cleanup 正确。
6. Box[size] 中 size 是小写 type alias 不应靠大小写误判。
7. values[T] 中 T 是值变量不应误判为泛型。
8. strfromutf8(empty) 与 strfromutf8(invalid) 必须可区分。
9. TokenCursor/Lossless 输入空 token stream 不得 UB。
10. SourceManager try_get stale SourceId 返回 null，不崩溃。
11. Content-Length: 4abc 必须拒绝；Content-LengthX 必须拒绝。
12. LSP Content-Length 超过上限必须拒绝，不能分配大内存。
13. file:///tmp/中文.ax 的 JSON URI 必须往返不损坏。
14. x86_64 / wasm32 / i686 下 pointer、usize、struct layout 与 TargetInfo/LLVM DataLayout 一致。
15. ID arena 超过 u32 上限必须 fatal diagnostic，不得截断。
16. incremental semantic authority 计数超过 u32 时禁用缓存或报错，不得截断复用。
17. Result.value() on error 在 release 下走明确 fatal_trap，不走 std::terminate。
18. CRLF 文件诊断 line extent 不包含 CR。
19. 泛型/trait/const-eval 递归超过 CompileBudget 时产生可读诊断，而不是卡死。

## 7. 修复路线图

### 阶段 0：先加红测试，不急着改代码

- 把本报告列出的 P0/P1 问题全部变成 failing tests。
- 使用 ASan/UBSan 跑 IdentifierInterner、BumpAllocator、TokenCursor、LSP malformed input。
- 建立 target layout matrix：x86_64、i686、wasm32、aarch64。

### 阶段 1：修基础 UB 和容器生命周期

- 修 IdentifierInterner move assignment；建立 ArenaOwner 移动规则。
- Result<T> 使用 get_if + fatal_trap 或去掉 noexcept。
- NonEmptyTokenStream 封住 token 输入不变量。
- SourceManager/Tooling 引入 try_get 系列。

### 阶段 2：统一 checked arithmetic 和 IdArena

- 新增 checked_add/checked_mul/checked_usize_to_u32。
- 统一 IdArena，消除分散 static_cast<u32>(size)。
- AST reserve、TokenBuffer reserve、BumpAllocator 全部改 checked。

### 阶段 3：资源语义 P0 收口

- 冻结 defer 求值语义。
- defer 纳入 move/exit CFG。
- assignment 改成 place-level cleanup。
- 暂不支持的 resource field/index assignment 直接拒绝。

### 阶段 4：TargetInfo / ABI 前置

- Driver 解析 target triple 并生成 TargetInfo。
- Sema layout、const eval、ABI check 全部依赖 TargetInfo。
- Backend 使用同一个 TargetInfo/LLVM DataLayout。

### 阶段 5：Parser / LSP / Tooling hardening

- [] 改成 AmbiguousBracketSuffix，由 sema 决议。
- LSP Content-Length 精确解析 + 消息上限。
- JSON UTF-8/surrogate/container stack 补齐或换库。
- stale range/snapshot generation 防御。

### 阶段 6：Sema / Query 工业化重构

- 移除 const_cast lazy mutation。
- 拆分 NameResolution、TypeCheck、Generics、Traits、Resource、ABI passes。
- Query key 带 ModuleId、TargetId、Epoch；query cache 只通过 QueryDatabase 写入。

## 8. 推荐目录重构方向

```text
sema/
  name_resolution/
  type_formation/
  expr_check/
  generics/
  traits/
  resources/
    place.cpp
    move_cfg.cpp
    move_dataflow.cpp
    drop_obligation.cpp
    cleanup_elaboration.cpp
  pattern/
  abi/
    target_info.cpp
    layout.cpp
    repr_c.cpp
query/
  database.cpp
  key.hpp
  provider_ops.hpp
  cycle_detector.cpp
```

## 9. 最终判断

Aurex M6 方向正确，但当前应先完成 hardening，而不是继续堆新特性。建议开 `m6-hardening` 分支，目标是消除 UB、错编、资源语义漏洞、host/target 混淆、unchecked ID/arena 隐患、LSP 输入边界和 query 并发隐患。这个分支完成后，再推进 M7 borrow checker、owned String、标准库、高级 trait 和并行增量 Query，地基才稳。

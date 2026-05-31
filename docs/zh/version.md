# 版本文档

## M5 default trait methods design baseline

当前文档基线是 M5 default trait methods design。M5 建立在已经收口的 M4 trait/protocol release baseline
之上，只选择一个聚焦的 M4 后设计流：nominal static trait 上的 default method body。

M5 设计基线记录在
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)，阶段路线见
[M5 Default Trait Methods 路线图](m5-roadmap.md)。M5 范围是 trait-owned default bodies、显式 method origin、
impl override vs inherited-default completeness、单态化后的 static direct-call lowering，以及 tooling /
diagnostics / query projection。

M5 不包含 dynamic trait object、object safety、vtable ABI、specialization、associated constants、default
associated types、generic associated types、blanket impl、package-level coherence expansion、class-like sugar 或
resource semantics。

## M4 trait/protocol release baseline

当前已实现的 trait 基线是 M4。M4 建立在已经收口的 M2 language-core-no-std、M2.5 frontend/query foundation
和 M3 module/generic/query-backed compiler architecture 之上，完成 nominal static trait、显式 trait impl、
generic trait predicate、static trait method dispatch、associated type，以及 IDE/tooling/diagnostics 投影。

发布契约记录在 [Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md)。M5 从该基线出发，不重新打开
M4 WP1-WP8。

## M2 language-core-no-std

M2 从 M1 的失败经验中收缩出来：停止继续修补 M1 的标准库、自举和系统样例路线，转而冻结标准库、删除干扰项，并重新聚焦语言核心设计。

M1 已经舍弃。它的问题是扩张面过大：标准库、host support、构建工具样例、自举实验和语言语义同时推进，导致核心语法与类型规则没有稳定下来。M2 不再把 M1 的 std/selfhost/build-tool 产物当作当前基线。

已完成：

- 删除 `std/` 源树和 host-c support。
- 删除 driver 标准库查找、import path 注入、support source 链接和相关头/源文件。
- 删除 CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 删除安装规则中的 `share/aurex/std`。
- 删除 std/M1/system 样例和 std 专项测试。
- 将 `Result` / `Option` 相关语言样例改为自包含定义。
- 将 defer/variadic 样例改为局部 `extern c`，不依赖 std 包装。
- 移除语义层 `std.core.vec/map/result` 专用 ownership 约束。
- 移除 M1 的语言级 `move(...)`、`noncopy struct` 和 use-after-move 追踪。
- 更新测试清单，使 sample suite 回到语言核心。

当前有效基线：

- C++20 Stage0 编译器。
- 手写 lexer/parser 和 ID-backed AST。
- sema、Aurex IR、IR verifier、pass pipeline。
- LLVM backend 和 clang native 输出。
- 自包含 positive/negative `.ax` 语言样例。

风险控制：

- 保留 match payload、`?`、`for`、`defer` 和基础类型系统的语言级正/负例。
- 保留 native hello、IR/LLVM lowering、安装后 compiler 执行验证。
- 后续资源约束不再通过标准库 hardcode 添加，应作为独立资源语义专题设计。

明确暂缓：

- 重新设计库层；不是复活旧 `std`。
- 重新评估 selfhost/Stage1。
- 重新设计 M1 build tool / system examples。
- 重新评估 host support 自动链接。

这些内容必须等 M2 基础语法、值语义、`unsafe`、slice/string 和泛型约束稳定后再重新评估；拥有型资源库还需要资源语义专题完成。

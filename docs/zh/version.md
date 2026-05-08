# 版本文档

## language-core-no-std

本分支从 `m1` 拉出，用于冻结标准库并继续语言核心设计。

已完成：

- 删除 `std/` 源树和 host-c support。
- 删除 driver 标准库查找、import path 注入、support source 链接和相关头/源文件。
- 删除 CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 删除安装规则中的 `share/aurex/std`。
- 删除 std/M1/system 样例和 std 专项测试。
- 将 `Result` / `Option` 相关语言样例改为自包含定义。
- 将 defer/variadic 样例改为局部 `extern c`，不依赖 std 包装。
- 移除语义层 `std.core.vec/map/result` 专用 ownership 约束。
- 更新测试清单，使 sample suite 回到语言核心。

风险控制：

- 保留 noncopy enum、match payload、`?`、`for`、`defer` 的语言级正/负例。
- 保留 native hello、IR/LLVM lowering、安装后 compiler 执行验证。
- 后续 copy/drop 约束不再通过标准库 hardcode 添加，应进入 capability / trait / where 设计。

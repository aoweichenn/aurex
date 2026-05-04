# 版本文档

版本：0.1.2

## 版本定位

0.1.2 是当前文档基线。本文档不再按每个小版本列出零散修改点，而是把 0.1.x 阶段能力收束为一个整体状态说明。

版本文档只描述当前公开状态、兼容策略和后续方向。历史小版本的逐条改动不再保留为独立文档；需要追溯时使用 git history。

## 0.1.2 范围

包含：

- Stage0 C++20 编译器主链路。
- lexer/parser/sema/IR/LLVM/driver/cli 分层。
- Aurex IR verifier 和 pass pipeline。
- std 可重定位查找。
- std host-c backend support 和 `aurex_std_v0_*` 稳定符号。
- selfhost lexer/parser/IR emitter 切片。
- bootstrap、golden、positive、negative 测试链路。
- 中英文主题文档集。

不包含：

- 完整 Stage1 sema。
- Stage1 TAC verifier。
- Stage1 LLVM backend 接入。
- fixed-point self-host。
- 完整跨块 SSA/mem2reg 和生产级优化器。

## 兼容性策略

- 标准库 host support 新符号使用 `aurex_std_v0_*`。
- C FFI 声明和 host-c support 现在统一放在 `std/ffi/c/`。
- 新文档入口统一为 `docs/zh/` 和 `docs/en/`。
- 文档不再恢复 `docs/M0V0.1.x.md` 形式的逐小版本文件。

## 公开稳定面

- CLI 选项名称和 `--emit=` 模式。
- Aurex IR dump 的基础结构。
- `std` 模块路径和安装目录 `share/aurex/std`。
- std host support ABI v0 符号。
- C++ driver、IR pass 和 standard-library helper 的头文件接口。

## 允许演进面

- M0 语法和语义细节。
- IR pass 的数量和优化强度。
- Stage1 selfhost 覆盖范围。
- std backend support 的 backend 类型。
- LLVM lowering 的内部实现。

## 后续版本方向

- 完整 IR constant folding。
- 跨块 mem2reg 和 phi 插入。
- 更完整 ABI 属性和目标配置。
- Stage1 sema 和 IR verifier。
- Stage1 产物接入现有 LLVM 后端。
- fixed-point self-host 验证。

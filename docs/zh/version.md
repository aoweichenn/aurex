# 版本文档

版本：0.1.3

## 版本定位

0.1.3 是当前文档基线。本文档不再按每个小版本列出零散修改点，而是把 0.1.x 阶段能力收束为一个整体状态说明。

版本文档只描述当前公开状态、兼容策略和后续方向。历史小版本的逐条改动不再保留为独立文档；需要追溯时使用 git history。

## 0.1.3 范围

包含：

- Stage0 C++20 编译器主链路。
- lexer/parser/sema/IR/LLVM/driver/cli 分层。
- Aurex IR verifier 和 pass pipeline。
- std 可重定位查找。
- std host-c backend support 和 `aurex_std_v0_*` 稳定符号。
- 当前语言切片，包括可见性、泛型基础、generic function MVP、sum type、pattern matching、
  表达式、受控推导、`extern c` 变长参数和作用域级 `defer`。
- M1 验收样例 baseline，包括 `examples/m1/frontend` 和 `examples/m1/axbuild`。
- `std.sys.process::Command` subprocess / stdout/stderr capture baseline，通过 host-c support 调用 `fork` / `execvp` / `waitpid`，并用独立 pipe 同时 drain stdout/stderr。
- `std.fs.file::FileMetadata` metadata / mtime baseline，通过 host-c support 调用 `stat`。
- `std.fs.dir` directory source discovery baseline，通过 host-c support 调用 `opendir` / `readdir` / `stat` 按后缀统计普通文件。
- M1 axbuild target graph validation / topological build baseline，包括 dependency bounds、cycle / invalid dependency 状态和按拓扑顺序 build。
- M1 axbuild target name lookup baseline，包括 name -> id 查找、missing lookup 检测和 duplicate target name 状态。
- M1 axbuild target graph diagnostic/message/name/cycle-path baseline，包括 `GraphDiagnostic`、status、target index、related index、message、target name、related name、cycle index path、cycle name path，以及 duplicate / invalid dependency / cycle back-edge 定位。
- golden、positive、negative 和语言特性测试链路。
- M1 样例的 checked/IR/native integration test 覆盖。
- 中英文主题文档集。

不包含：

- fixed-point self-host。
- 旧自举实验代码。
- 完整跨块 SSA/mem2reg 和生产级优化器。
- 完整 M1 自举前端。
- 完整 M1 typed build tool。
- 完整 OS/process/cwd/env/pipe/timeout 标准库。

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
- M3 之后的新自举覆盖范围。
- std backend support 的 backend 类型。
- LLVM lowering 的内部实现。
- M1 样例的内部结构和验收深度。

## 后续版本方向

- 完整 IR constant folding。
- 跨块 mem2reg 和 phi 插入。
- 更完整 ABI 属性和目标配置。
- 模块隔离、可见性、泛型约束和 pattern matching 完整性。
- M1 frontend 从 summary parser 推进到真实 AST、diagnostic、name resolution 和 type checking。
- M1 axbuild 从 source/stamp mtime、source discovery、target name lookup、target graph smoke、stdout/stderr capture 和结构化 graph diagnostic/message/name/cycle-path 推进到完整目录项列表、递归遍历、cwd/env、dependency value diagnostic 和结构化输出报告。
- M3 之后以新语言特性重新设计自举链路。
- fixed-point self-host 验证。

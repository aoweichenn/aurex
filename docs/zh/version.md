# 版本文档

版本：0.1.7

## 版本定位

0.1.7 是当前文档基线。本文档不再按每个小版本列出零散修改点，而是把 0.1.x 阶段能力收束为一个整体状态说明。

版本文档只描述当前公开状态、兼容策略和后续方向。历史小版本的逐条改动不再保留为独立文档；需要追溯时使用 git history。

## 0.1.7 范围

包含：

- Stage0 C++20 编译器主链路。
- lexer/parser/sema/IR/LLVM/driver/cli 分层。
- Aurex IR verifier 和 pass pipeline。
- std 可重定位查找。
- std host-c backend support 和 `aurex_std_v0_*` 稳定符号。
- 当前语言切片，包括可见性、泛型基础、generic function MVP、sum type、pattern matching、
  表达式、受控推导、`extern c` 变长参数和作用域级 `defer`。
- M1 验收样例 baseline，包括 `examples/m1/frontend` 和 `examples/m1/axbuild`。
- `std.sys.process::Command` subprocess / stdout/stderr capture / cwd / env baseline，通过 host-c support 调用 `fork` / `execvp` / `waitpid`，并用独立 pipe 同时 drain stdout/stderr。
- `std.fs.file::FileMetadata` metadata / mtime baseline，通过 host-c support 调用 `stat`；`std.fs.file` 同时提供 `Path` 包装入口和 `write_str` / `write_str_path`，让新文件 API 可以避开普通路径/文本场景中的裸 `c"..."`。
- `std.fs.dir` directory create / directory entries / recursive directory entries / source discovery baseline，通过 host-c support 调用 `mkdir`、`opendir` / `readdir` / `stat` / `lstat` 读取拥有型单层/递归目录项，并按后缀统计普通文件，提供单层和递归计数入口；目录 path 已有 `Path` 包装入口，suffix 已有 `str` 包装入口，`DirectoryEntry` 由 bytes-backed `Path` 保存 name/path，并提供 raw bytes 视图和 checked UTF-8 视图。
- `std.core.map` Vec-backed 泛型 `Map<K, V>` 和 borrowed C string -> usize 的 `CStringUsizeMap` baseline。
- 字符串基础类型方向已冻结为 `str` = borrowed UTF-8 text slice、`String` = owned UTF-8 buffer、`Bytes` / `Span<u8>` = raw bytes、`CStr` / `CString` = C FFI、`Path` = platform path bytes；当前已落地字符串字面量 UTF-8/escape 诊断、`std.core.str` borrowed API 和 scalar API、`std.core.string.String` 的 `from_str/from_utf8/as_str/append(str)/push_scalar/insert_scalar/pop_scalar/remove_scalar_at/slice_bytes_checked/truncate_bytes_checked` UTF-8 surface、`String.as_mut_span` 移除、`std.core.bytes.Bytes` raw bytes 容器、bytes-backed `std.fs.path.Path`、`std.fs.file` 和 `std.fs.dir` 的 `Path` / `str` 新入口、bytes-backed `DirectoryEntry` raw bytes / checked UTF-8 视图，以及 `std.ffi.c.string.CStr` / `CString` FFI 边界类型。
- M1 axbuild target graph validation / topological build baseline，包括 dependency bounds、cycle / invalid dependency 状态和按拓扑顺序 build。
- M1 axbuild target name lookup cache baseline，包括 name -> id 查找、lookup cache 与线性扫描对照、missing lookup 检测和 duplicate target name 状态。
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
- 完整 OS/process/pipe/timeout 标准库。

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
- M1 axbuild 从 source/stamp mtime、directory create、owned single-level/recursive directory entry read、`Path` + `str` suffix + bytes entry 名称匹配的 source discovery、single-level and recursive source discovery、target name lookup cache、target graph smoke、stdout/stderr capture、cwd/env 和结构化 graph diagnostic/message/name/cycle-path 推进到 streaming directory iterator / walk callback、glob/pattern、hash/bucketed map、dependency value diagnostic 和结构化输出报告。
- M3 之后以新语言特性重新设计自举链路。
- fixed-point self-host 验证。

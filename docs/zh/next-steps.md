# 下一步计划文档

版本：0.1.7

## 当前阶段

旧 selfhost 自举路线已经从 active tree 删除。当前项目重心是继续推进 C++ Stage0、Aurex IR 和 LLVM 后端，让语言核心具备足够强的表达力、模块隔离能力和后端契约。M1 的目标不再只是补语言特性，而是让 Aurex 能优雅编写两个真实系统级程序：一个自举前端样例，以及一个类似 CMake 的 typed 构建工具样例。完整替换 C++ Stage0 可以放到后续阶段，但 M1 必须证明这些程序已经能用 Aurex 自然表达。

本阶段最新进度见：[M1 进度报告 2026-05-07](m1-progress-2026-05-07.md)。该报告记录了当前已落地的 subprocess / stdout/stderr capture / cwd / env baseline、file metadata / mtime baseline、directory create / directory entries / recursive directory entries / single-level source discovery / recursive source discovery baseline、Map / CStringUsizeMap baseline、target graph validation / topological build baseline、target name lookup cache baseline、target graph diagnostic/message/name/cycle-path/cycle-path-name baseline、测试 direct process runner、M1 frontend 样例、M1 typed build-tool 样例、集成测试覆盖和测试耗时基线。

基础字符串类型设计先行见：[字符串基础类型设计草案](string-primitive-design.md)。当前已落地 Phase 1 字面量诊断基线、Phase 2 `std.core.str` 基础 API、Phase 3 `String` UTF-8 surface、Phase 4 `CStr` / `CString` 第一批、Phase 5 scalar API 第一批，以及 raw `Bytes` / bytes-backed `Path` / std fs `Path` 入口风险收口：普通字符串字面量验证 UTF-8、非法 escape 变成 lexer 诊断、支持 `\u{...}` Unicode scalar escape、拒绝 `c"..."` 内部 NUL，提供 `byte_len`、`as_bytes`、`is_boundary`、`slice_bytes_checked`、`is_scalar_value`、`scalar_utf8_width`、`scalar_at`、`scalar_count`、`from_utf8` 等 borrowed `str` API，让 `std.core.string.String` 支持 `from_str`、`from_utf8`、`as_str`、`append(str)`、`push_scalar`、`insert_scalar`、`pop_scalar`、`remove_scalar_at`、checked UTF-8 construction、checked slice/truncate 和旧 byte API 的边界保护，并移除 `String.as_mut_span()`；`std.core.bytes.Bytes` 负责拥有型 raw bytes 和 `as_mut_span`，`std.fs.path.Path` 已改为 bytes-backed，`from_span` 接受非 UTF-8 path bytes 但拒绝内部 NUL，`std.fs.file` 已提供 `metadata_path/read_bytes_path/read_text_path/write_bytes_path/write_text_path/file_exists_path/remove_file_path/rename_file_path` 和按 `str` 长度写入的 `write_str/write_str_path`，`std.fs.dir` 已提供目录 path 的 `*_path` 包装、suffix 的 `*_str` / `*_path_str` 包装、bytes-backed `DirectoryEntry.name_bytes()/path_bytes()` 和 checked `name_utf8()/path_utf8()`；`examples/m1/axbuild` 的目录扫描、target source list、project stamp path、source/stamp metadata、stamp 写入、clean 和临时 source cleanup 已改用 `Path` / `str` 入口，target 加入 project 时也通过 `Vec.take()` / field reset 显式转移 owned 字段，避免 owned `Path` / owned container 浅拷贝释放风险；`std.ffi.c.string.CStr` / `CString` 负责 C FFI NUL-terminated 字符串边界，其中 `CStr` 是 borrowed copyable 视图，`CString` 已迁为 `noncopy` 拥有型资源。后续进程和剩余 FFI 参数继续推进时，应以该文档里的 `str` = 借用 UTF-8 文本切片、`String` = 拥有 UTF-8 buffer、`Bytes` / `Span<u8>` = 原始字节、`CStr` / `CString` = C FFI 边界、`Path` = 平台路径 bytes 这五层分工为准。

## 当前已具备能力

Stage0 主链路：

- 手写 lexer 和递归下降 parser。
- 模块声明、import 搜索路径和标准库查找。
- 语义分析、类型表、符号表、ABI 名称和基础诊断。
- Aurex typed CFG/SSA-like IR、IR verifier 和保守 pass pipeline。
- LLVM IR lowering，并通过 clang 输出 assembly、object 和 executable。
- build-tree 与 install-tree 下的 std 查找和 host-c backend support。

当前语言切片：

- 结构体、枚举、类型别名和 opaque 类型。
- 泛型 struct / enum 的基础实例化，包含嵌套泛型类型实参 substitution 和
  struct literal 的期望类型/字段值推断。
- `match` 表达式、literal pattern、wildcard、or-pattern 和 guard。
- block / if 表达式。
- local 和 return 类型推导的受控切片。
- 函数原型与递归函数检查。
- generic function MVP：支持显式 `<T>`、调用时类型实参，以及基于实参/期望返回类型的基础推断。
- `extern c` 变长参数声明与调用，包含 C ABI 默认实参提升。
- 作用域级 `defer` 语句，按反序在正常离开、`return`、`break` / `continue` 路径执行清理调用。
- `for init; condition; update { ... }` 语句已落地，三段均可为空；`continue` 先进入 update 再回到 condition，`break` 进入 exit，并沿用 `defer` 的循环退出清理规则。模块合并时 `for_init` / `for_update` 已纳入 AST ID remap，带 import 的 `for + defer + continue/break` 已有回归覆盖。
- 最小所有权语义已启动：`noncopy struct` 可把结构体标记为 move-only，`move(value)` 显式转移 local/parameter 所有权；`move(copyable)` 也允许作为显式值传递，不会把源 binding 标记为 moved，用来支撑一份泛型 std 代码同时服务 copyable 和 noncopy 类型；语义分析已禁止 move-only 值在 local 初始化、赋值、返回、函数实参、method receiver、struct literal 字段和 enum payload 中隐式复制，并检查 move 后继续使用。enum copyability 已按 payload 传播，`Result<NonCopy, E>` / `Option<NonCopy>` 会随 payload 变成 noncopy；`match` 和 `?` 会消费 noncopy enum 源值，`if`/`else if` 的 moved-state 合流已按 fallthrough 分支处理。当前还给部分 std 泛型 API 增加了临时 copyability 约束：`Vec` 的 raw-copy / by-value read API、`Map` 的按值 key/value API，以及 `Result` / `Option` 的消费式状态/兜底 API 会拒绝 noncopy payload。
- `impl` / method / associated function MVP，支持显式 `self`、实例 `value.method()`、公开字段 `value.field`、`Type.function()` 风格 associated call、`impl<T> Type<T>` 泛型实例方法，以及 `fn method<U>` 方法级泛型参数；跨模块 private field / private method 访问已有稳定诊断。
- 标准 `Result` / `Option` / `?` 切片已落地，可用于显式返回的错误传播与早返回控制流；`Option<T>` / `Result<T, E>` 已有基础 method API，包含使用方法级泛型的 `Option<T>.ok_or<E>`，以及不消费 payload 的 `is_some_ref` / `is_none_ref` / `is_ok_ref` / `is_err_ref`。对 noncopy payload，状态查询优先使用 `*_ref` 或显式 `match move(...)`，消费式 `is_some/is_none/is_ok/is_err/unwrap_or/ok_or` 只保留给当前可安全复制/丢弃的 payload 组合。
- 标准库容器/文本/路径基线已启动，包含 borrowed UTF-8 `str` 基础 API 和 scalar API、拥有型 UTF-8 `String` 的 `from_str/from_utf8/as_str/append(str)/push_scalar/insert_scalar/pop_scalar/remove_scalar_at/slice_bytes_checked/truncate_bytes_checked` surface、`std.core.bytes.Bytes` raw bytes 拥有型容器、`std.ffi.c.string.CStr` borrowed 视图和 `CString` noncopy 拥有型 FFI 边界类型、泛型 `Span<T>` / `MutSpan<T>`、`noncopy` 泛型 `Vec<T>` 的容量、追加、插入、take/reset 转移和泛型 method API、Vec-backed `noncopy` 泛型 `Map<K, V>`、borrowed C string -> usize 的 `noncopy` `CStringUsizeMap`、`String` 兼容 byte API 的 UTF-8 边界保护，以及 bytes-backed `Path` 的查询与 join API。`Vec` 的 copy-only API 已在语义层临时限制为 copyable element type；`Map<K,V>` 的当前按值 key/value API 也限制到 copyable key/value，避免 noncopy 元素被 raw copy、by-value read、覆盖或 clear 绕过所有权。
- 标准库文件/host 文件读写已迁移到 `Result` 风格的拥有型 buffer API，旧的 `BufferU8` 与手写文件 result 结构已从 in-tree 用法移除；`std.fs.file::FileMetadata` 已提供 exists/is_file/is_dir/size/modified_time_ns baseline，`std.fs.file` 还提供 `Path` 包装入口和 `write_str` / `write_str_path`，新文本写入按 `str` byte length 处理内部 `\0`；`std.fs.dir` 已提供目录创建、拥有型单层/递归目录项读取、目录 path 的 `Path` 包装、suffix 的 `str` 包装、目录项 raw bytes 视图和 checked UTF-8 视图，以及按后缀统计普通文件的单层/递归 source discovery baseline。
- 标准库进程能力已启动，`std.sys.process::Command` 提供 typed argv、`arg()`、`cwd()`、`env()`、`run()`、`run_capture()` 和 `destroy()`，底层通过 host-c support 的 `fork` / `execvp` / `waitpid` 运行子进程，并已有 stdout/stderr capture、cwd 和 env baseline；当前还没有 stdin/stdout/stderr pipe 和 timeout API。
- `pub` / `priv` 可见性关键字、跨模块 private item 过滤和 private field 访问检查。
- examples 已经包含 CLI、文件 IO、内存/arena、std 模块、泛型结果类型、可见性和 re-export facade 的系统级小案例。
- M1 验收样例骨架已进入 active tree：`examples/m1/frontend` 覆盖 source manager、diagnostic、lexer、token stream、parser subset 与 AST/IR summary；`examples/m1/axbuild` 覆盖 project/target、typed dependency/source/include/custom command、subprocess stdout/stderr capture、cwd/env、source/stamp mtime incremental check、directory create、owned single-level/recursive directory entry read、source discovery by entries、single-level and recursive source discovery count、target name lookup cache、duplicate target detection、target graph validation、topological build order、结构化 graph diagnostic/message/name/cycle index path/cycle name path、build/clean/run/test 流程。两者已纳入 integration tests 的 checked/IR/native smoke 覆盖。

## 关键语言缺口

- 可见性还应继续扩展到更细粒度的 API 边界，例如构造器、枚举 payload、type alias 传播和 re-export 规则。
- 模块隔离还缺显式 package/crate 边界、导入别名、选择性导入、循环依赖诊断优化和稳定 public surface dump。
- 调用模型已有 `impl` / method MVP、泛型 impl 实例方法、方法级泛型参数和跨模块成员可见性诊断，但还缺 trait/class 复用、method public surface tooling 和 overload/trait 场景下更完整的诊断。
- 泛型仍缺约束、where-like predicate、trait/interface 设计、单态化缓存策略和诊断可解释性。
- 错误处理已有标准 `Result<T, E>` / `Option<T>` 与 `?` 传播切片，但还缺更完整的 std API 迁移和可组合诊断模型。
- 资源管理已经有 `noncopy struct` / `move(value)` MVP，`FileBytes`、`HostFileBytes`、`Command`、`ProcessOutput`、`CString`、`Bytes`、`String`、`Path`、`DirectoryEntry`、`Vec<T>`、`Map<K,V>` 和 `CStringUsizeMap` 等第一批拥有型资源已迁为 noncopy；`Vec`、`Map`、`Result` / `Option` 的一批 copy-only API 也已有语义护栏。但还缺析构/Drop 约定、借用检查、部分 move、语言级 move-only 泛型约束，以及 arena / 临时目录 / 复杂 OS 资源的统一生命周期策略。
- 标准库还缺更完整的 `Vec<T>`、hash/bucketed `Map<K, V>`、owned string-key map、streaming directory iterator / walk callback、文件 metadata、subprocess 和 incremental build 需要的 OS 能力。
- 字符串基础类型需要继续冻结到 public API：`str` 已按和 `int` 同层级的借用 UTF-8 文本切片推进，普通文本、原始 bytes、拥有型 `String`、C FFI 字符串和平台 `Path` 已在核心 std 类型上拆开；`std.fs.file` / `std.fs.dir` 已有 `Path` / `str` 新入口，M1 axbuild 的 source path、stamp path、目录扫描和临时文件清理已收口到 `str` / `Path`，后续风险主要是进程 argv/env/cwd 和剩余 FFI 场景中的底层 `c"..."` / `*const u8` 兼容入口还要继续收口。
- 需要一个兼容传统 OOP 思维的 class/object model：封装、继承和动态多态，但它应作为迁移友好层，不替代 struct/enum/trait/generic 的核心设计。
- pattern matching 还需要更完整的 exhaustiveness、绑定一致性、enum layout 交互和 lowering 验证。
- AIR 仍需要作为 Stage0 内部后端契约继续强化：slot/lvalue descriptor、record/enum layout、phi/SSA 合流、dominance、call signature 和跨模块 item binding 都应可验证。
- LLVM 后端需要继续跟进新特性的 lowering，避免语言前端特性只停留在 check/dump 层。

## M1 验收目标

M1 结束时应能在 active tree 中保留两个 Aurex 编写的系统级样例，并由 integration tests 覆盖：

1. 自举前端样例  
   用 Aurex 写一个小型 compiler frontend，包括 source manager、lexer、token stream、parser 子集、AST/IR dump 和 diagnostic。它不要求替换 C++ Stage0，但必须证明 Aurex 可以自然编写编译器核心代码。当前已有最小可运行样例，后续还应补 source span 更完整的 diagnostic、AST 节点层级、导入解析和更接近真实前端的错误恢复。

2. typed 构建工具样例  
   用 Aurex 写一个类似 CMake 的小构建工具，包括 project、target、library、executable、source list、include path、dependency、custom command、subprocess、incremental check、build、clean、run 和 test。构建描述应是 typed Aurex API，而不是 shell 字符串拼接。当前已有最小可运行样例、stdout/stderr capture baseline、cwd/env baseline、source/stamp mtime incremental check、directory create、owned single-level/recursive directory entry read、source discovery by entries、single-level and recursive source discovery count、target name lookup cache、duplicate target detection、target graph validation、topological build order 和结构化 graph diagnostic/message/name/cycle index path/cycle name path，后续还应补 streaming directory iterator / walk callback、glob/pattern 和带 dependency value 的错误报告。

## M1 优先级

1. 完成 method / associated function / `impl` 调用模型  
   MVP 已落地：支持显式 `self` 参数、method call lowering、associated function、公开字段访问、`impl<T> Type<T>` 泛型实例方法、方法级泛型参数和跨模块 method visibility 诊断。后续应补 method public surface dump、overload/trait 诊断，并继续把 examples 从 C 风格 helper 迁移到 method API。

2. 建立标准 `Result` / `Option` / `?` 错误处理模型  
   自举和构建工具都需要大量可组合错误路径。M1 应提供标准泛型结果类型、错误传播、稳定诊断和 examples 改写，而不是继续依赖手写状态码 helper。

3. 补齐 `Span` / `String` / `Vec` / `Map` / `Path` 标准库基础  
   compiler frontend 需要 token buffer、AST list、symbol table 和 source span；构建工具需要 path list、target graph、dependency map 和 command argv。标准容器应成为 M1 的核心交付物。

4. 把泛型从基础实例化推进到可约束模型  
   generic function MVP 已落地。下一步增加最小 `where`、trait/interface、trait impl 和静态分派。M1 不需要 trait object，但需要支撑容器、算法和 typed build graph。

5. 加入兼容性 class/object model  
   提供面向传统 OOP 使用者的封装、继承和动态多态层。M1 建议先做单继承、显式 `virtual`、`override`、`abstract`、`final`、`pub` / `priv` / `protected` 可见性，以及通过 base pointer/reference 的 vtable dispatch。多继承不进入 M1；需要多态组合时优先使用 trait/interface，class 主要服务迁移和老代码建模。

6. 建立资源管理和 OS 工程能力  
   `defer` MVP 已落地，`for` 的 `continue` / update / exit 路径也已复用 scope cleanup；第一批文件/进程/FFI 拥有型资源已迁为 noncopy。后续继续支持 Drop/destructor、借用检查、目录遍历、文件 metadata、subprocess、cwd/env、临时文件和路径规范化。没有这一步，构建工具只能是玩具。

7. 强化 sum type / pattern matching 到工业可用边界  
   重点是 exhaustiveness、unreachable arm、payload binding、guard 约束和 enum layout 与 LLVM lowering 的一致性。自举前端会大量依赖 token/AST match。

8. 稳定 AIR/IR 后端契约  
   AIR 先作为 Stage0 内部设计目标推进到可验证形态，LLVM 继续作为当前生产后端。自举前端样例先能输出结构化 dump，完整后端 handoff 后续再做。

9. 改善诊断与 public surface tooling  
   给模块边界、泛型约束、method/class dispatch、match 覆盖和可见性错误提供稳定、可测试的错误信息。大型语言特性没有诊断就无法进入工程可用状态。

## 实施顺序

当前 `impl` / method MVP 已完成。下次继续实现时，建议从标准 `Result` / `Option` / `?` 切片开始，让文件、CLI、parser、build graph 这类代码能自然传播错误，而不是继续写手动状态码 helper。

1. `impl` / method MVP  
   已完成。Parser 接受 `impl Type { ... }`、`impl<T> Type<T> { ... }` 和 `method<U>` 方法级泛型参数，sema 把 method 注册到类型关联作用域，call resolver 支持 `value.field`、`value.method(args)`、`value.method<U>(args)` 和 `Type.function(args)`，跨模块访问遵守 `pub` / `priv`，测试覆盖 parse、sema、IR lowering、negative diagnostics，并把 examples 中一部分 helper 改成 method。

2. `Result` / `Option` / `?`  
   已完成。方法基础上已经有标准错误传播的 `?` 切片，可用于 `Result` 和 `Option` 的早返回；`Option<T>` / `Result<T, E>` 也有 `is_some`、`is_ok`、`unwrap_or`、`ok_or<E>` 等基础方法，并补充了只读 enum tag、不消费 payload 的 `*_ref` 状态检查。消费式状态/兜底方法当前只适合 copyable payload；noncopy payload 使用 `*_ref` 或显式 `match move(...)`。下一步继续扩展标准库 API，让 `File.read_all(path)?`、`Parser.next()?` 这类代码更自然。

3. `Span` / `Vec` / `Map` / `String` / `Path`
   已启动。当前已有 `Span<T>` / `MutSpan<T>`、`noncopy Vec<T>` 结构、容量、追加、插入、take/reset 转移、copyable 元素的删除/随机访问、`Vec<T>` 泛型 method API、`std.core.bytes.Bytes` raw bytes 拥有型容器和 method API、Vec-backed `noncopy Map<K, V>`、borrowed C string -> usize 的 `noncopy CStringUsizeMap`、borrowed `str` 基础 API 和 scalar API、拥有型 UTF-8 `String` 的 `from_str/from_utf8/as_str/append(str)/push_scalar/insert_scalar/pop_scalar/remove_scalar_at/slice_bytes_checked/truncate_bytes_checked` surface、`String` 兼容 byte API 的 UTF-8 边界保护、`String.as_mut_span` 移除、`std.ffi.c.string.CStr` borrowed 视图和 `CString` noncopy 拥有型边界类型，以及 bytes-backed `Path` 的绝对路径判断、parent、file name、file stem、extension、from_str、span/c-string join 和 with-extension，并用 std 集成样例覆盖 method API、`Result` / `Option` 和 `?` 组合。旧 `BufferU8` 已迁到 `VecU8`，`std.fs.file::read_text` 现在通过 `String.from_utf8` 验证文件内容，`std.fs.file` / `std.sys.host` 的文件读写入口也已改为 `Result<FileBytes, i32>`、`Result<usize, i32>` 等 M1 风格 API；`std.fs.file` 新增 `Path` 包装和 `write_str_path`，`std_file` 样例覆盖 `"path\0text"` 不被 C 字符串截断；`std.fs.dir` 新增目录 path 的 `Path` 包装、suffix 的 `str` 包装、bytes-backed `DirectoryEntry` 原始 bytes/checked UTF-8 视图，`std_dir` 样例覆盖目录创建、读取、递归读取、后缀统计、null entry 防御和 `defer` 清理；`examples/m1/axbuild` 已用 `CStringUsizeMap` 维护 target name -> id lookup cache，并将目录扫描、source list、stamp 和临时 cleanup 迁到 `Path` / `str` 入口，同时用 `Vec.take()` / field reset 转移 target owned 字段。下一步继续扩展到 token buffer、owned string-key map、hash/bucketed map，以及更通用的 path/build graph 场景。

4. generic constraints / trait / `where`
   generic function、generic impl method 和 method-specific generic 参数已落地。下一步补最小 trait/interface 或 capability predicate，再推进约束、method-like resolution 和单态化缓存。完成后补 typed graph 和 map-like examples。

5. class/object model MVP  
   在 method 和 trait 基础稳定后实现 class，这样 class 的成员解析、visibility、vtable lowering 可以复用已有调用模型。完成后增加一个 OOP 风格插件/任务 runner example。

6. `defer` / noncopyable / OS 能力  
   已启动。当前 `defer call();` 会在当前词法作用域退出时反序执行，并覆盖正常退出、`return`、`break` / `continue` lowering；`for` 的 update 和 exit lowering 已有带 import 的回归覆盖；subprocess / stdout/stderr capture / cwd / env baseline 已通过 noncopy `std.sys.process::Command` / `ProcessOutput` 接入 host-c support；文件 metadata / mtime baseline 已通过 `std.fs.file::FileMetadata` 接入 host-c support；`FileBytes`、`HostFileBytes`、`CString`、`Bytes`、`String`、`Path`、`DirectoryEntry`、`Vec<T>`、`Map<K,V>` 和 `CStringUsizeMap` 已迁为 noncopy；directory create、owned single-level/recursive directory entry read 和 source discovery count baseline 已通过 `std.fs.dir` 接入 host-c support，计数能力包含单层和递归后缀计数。下一步补 Drop/destructor、borrow checking、部分 move、语言级 move-only 泛型约束、streaming directory iterator / walk callback、stdin/stdout/stderr pipe 和临时目录能力，让文件、进程、arena、临时目录能安全组合。

7. 自举前端和 typed 构建工具验收  
   已启动。`examples/m1/frontend` 和 `examples/m1/axbuild` 已进入 active tree，并由 integration tests 覆盖 checked surface、IR surface 和 native smoke；axbuild 还覆盖了 `GraphDiagnostic` 的 checked/IR surface、message surface、target/related name surface、cycle index/name path surface 和 duplicate/invalid/cycle 三类图错误定位。后续继续扩大样例深度，目标仍是让这两个程序从“最小验收样例”推进到足够真实的 M1 工程基准，并继续要求覆盖率保持 90% 以上。

## 长期优先级

1. 完成模块可见性与隔离基线  
   `pub` / `priv` 已落地，下一步应补 re-export、导入别名、选择性导入和 public API dump。模块系统是后续自举、包管理和大型项目可维护性的前提。

2. 强化 sum type / pattern matching 到工业可用边界  
   重点是 exhaustiveness、unreachable arm、payload binding、guard 约束和 enum layout 与 LLVM lowering 的一致性。

3. 把泛型从基础实例化推进到可约束模型  
   generic function MVP 已完成，下一步设计最小 trait/interface 或 capability predicate，再推进约束、method-like resolution 和单态化缓存。

4. 稳定 AIR/IR 后端契约  
   AIR 先作为 Stage0 内部设计目标推进到可验证形态，LLVM 继续作为当前生产后端。自举只要求未来能输出 AIR 级别的结构，不急于自举后端。

5. 改善诊断与 public surface tooling  
   给模块边界、泛型约束、match 覆盖和可见性错误提供稳定、可测试的错误信息。大型语言特性没有诊断就无法进入工程可用状态。

## 后续自举策略

不恢复旧 selfhost。新的自举应基于当前路线重新写：模块隔离、显式可见性、method、标准错误处理、泛型/约束、trait、必要的 class 兼容层、sum type、pattern matching、资源管理和 AIR 输出。M1 的自举目标是交付 Aurex 编写的 frontend 样例；完整替换 C++ Stage0 和后端 handoff 可以在后续阶段推进。LLVM 仍作为生产后端继续承载新特性。

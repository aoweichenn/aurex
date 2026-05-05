# 下一步计划文档

版本：0.1.2

## 当前阶段

旧 selfhost 自举路线已经从 active tree 删除。当前项目重心是继续推进 C++ Stage0、Aurex IR 和 LLVM 后端，让语言核心具备足够强的表达力、模块隔离能力和后端契约。M1 的目标不再只是补语言特性，而是让 Aurex 能优雅编写两个真实系统级程序：一个自举前端样例，以及一个类似 CMake 的 typed 构建工具样例。完整替换 C++ Stage0 可以放到后续阶段，但 M1 必须证明这些程序已经能用 Aurex 自然表达。

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
- `impl` / method / associated function MVP，支持显式 `self`、实例 method call 和 `Type.function()` 风格 associated call。
- 标准 `Result` / `Option` / `?` 切片已落地，可用于显式返回的错误传播与早返回控制流。
- 标准库容器/文本/路径基线已启动，包含泛型 `Span<T>` / `MutSpan<T>`、泛型 `Vec<T>` 的 `VecU8` 专用 API、拥有型 `String` 和拥有型 `Path`。
- 标准库文件/host 文件读写已迁移到 `Result` 风格的拥有型 buffer API，旧的 `BufferU8` 与手写文件 result 结构已从 in-tree 用法移除。
- `pub` / `priv` 可见性关键字、跨模块 private item 过滤和 private field 访问检查。
- examples 已经包含 CLI、文件 IO、内存/arena、std 模块、泛型结果类型、可见性和 re-export facade 的系统级小案例。

## 关键语言缺口

- 可见性还应继续扩展到更细粒度的 API 边界，例如构造器、枚举 payload、type alias 传播和 re-export 规则。
- 模块隔离还缺显式 package/crate 边界、导入别名、选择性导入、循环依赖诊断优化和稳定 public surface dump。
- 调用模型已有 `impl` / method MVP，但还缺 generic impl、trait/class 复用、method public surface tooling 和更完整的诊断。
- 泛型仍缺约束、where-like predicate、trait/interface 设计、单态化缓存策略和诊断可解释性。
- 错误处理还缺标准 `Result<T, E>` / `Option<T>`、`?` 传播和可组合诊断模型。
- 资源管理仍缺最小 move/noncopyable 语义和文件、进程、arena 等资源的统一用法。
- 标准库还缺更完整的 `Vec<T>`、`Map<K, V>`、目录遍历、文件 metadata、subprocess 和 incremental build 需要的 OS 能力。
- 需要一个兼容传统 OOP 思维的 class/object model：封装、继承和动态多态，但它应作为迁移友好层，不替代 struct/enum/trait/generic 的核心设计。
- pattern matching 还需要更完整的 exhaustiveness、绑定一致性、enum layout 交互和 lowering 验证。
- AIR 仍需要作为 Stage0 内部后端契约继续强化：slot/lvalue descriptor、record/enum layout、phi/SSA 合流、dominance、call signature 和跨模块 item binding 都应可验证。
- LLVM 后端需要继续跟进新特性的 lowering，避免语言前端特性只停留在 check/dump 层。

## M1 验收目标

M1 结束时应能在 active tree 中保留两个 Aurex 编写的系统级样例，并由 integration tests 覆盖：

1. 自举前端样例  
   用 Aurex 写一个小型 compiler frontend，包括 source manager、lexer、token stream、parser 子集、AST/IR dump 和 diagnostic。它不要求替换 C++ Stage0，但必须证明 Aurex 可以自然编写编译器核心代码。

2. typed 构建工具样例  
   用 Aurex 写一个类似 CMake 的小构建工具，包括 project、target、library、executable、source list、include path、dependency、custom command、subprocess、incremental check、build、clean、run 和 test。构建描述应是 typed Aurex API，而不是 shell 字符串拼接。

## M1 优先级

1. 完成 method / associated function / `impl` 调用模型  
   MVP 已落地：支持显式 `self` 参数、method call lowering、associated function 和基本 method visibility。后续应补 generic impl、method public surface dump、跨模块 method 诊断，并继续把 examples 从 C 风格 helper 迁移到 method API。

2. 建立标准 `Result` / `Option` / `?` 错误处理模型  
   自举和构建工具都需要大量可组合错误路径。M1 应提供标准泛型结果类型、错误传播、稳定诊断和 examples 改写，而不是继续依赖手写状态码 helper。

3. 补齐 `Span` / `String` / `Vec` / `Map` / `Path` 标准库基础  
   compiler frontend 需要 token buffer、AST list、symbol table 和 source span；构建工具需要 path list、target graph、dependency map 和 command argv。标准容器应成为 M1 的核心交付物。

4. 把泛型从基础实例化推进到可约束模型  
   generic function MVP 已落地。下一步增加最小 `where`、trait/interface、trait impl 和静态分派。M1 不需要 trait object，但需要支撑容器、算法和 typed build graph。

5. 加入兼容性 class/object model  
   提供面向传统 OOP 使用者的封装、继承和动态多态层。M1 建议先做单继承、显式 `virtual`、`override`、`abstract`、`final`、`pub` / `priv` / `protected` 可见性，以及通过 base pointer/reference 的 vtable dispatch。多继承不进入 M1；需要多态组合时优先使用 trait/interface，class 主要服务迁移和老代码建模。

6. 建立资源管理和 OS 工程能力  
   `defer` MVP 已落地，后续继续支持最小 noncopyable 资源规则、目录遍历、文件 metadata、subprocess、cwd/env、临时文件和路径规范化。没有这一步，构建工具只能是玩具。

7. 强化 sum type / pattern matching 到工业可用边界  
   重点是 exhaustiveness、unreachable arm、payload binding、guard 约束和 enum layout 与 LLVM lowering 的一致性。自举前端会大量依赖 token/AST match。

8. 稳定 AIR/IR 后端契约  
   AIR 先作为 Stage0 内部设计目标推进到可验证形态，LLVM 继续作为当前生产后端。自举前端样例先能输出结构化 dump，完整后端 handoff 后续再做。

9. 改善诊断与 public surface tooling  
   给模块边界、泛型约束、method/class dispatch、match 覆盖和可见性错误提供稳定、可测试的错误信息。大型语言特性没有诊断就无法进入工程可用状态。

## 实施顺序

当前 `impl` / method MVP 已完成。下次继续实现时，建议从标准 `Result` / `Option` / `?` 切片开始，让文件、CLI、parser、build graph 这类代码能自然传播错误，而不是继续写手动状态码 helper。

1. `impl` / method MVP  
   已完成。Parser 接受 `impl Type { ... }`，sema 把 method 注册到类型关联作用域，call resolver 支持 `value.method(args)` 和 `Type.function(args)`，测试覆盖 parse、sema、IR lowering、negative diagnostics，并把 examples 中一部分 helper 改成 method。

2. `Result` / `Option` / `?`  
   已完成。方法基础上已经有标准错误传播的 `?` 切片，可用于 `Result` 和 `Option` 的早返回。下一步继续扩展标准库 API，让 `File.read_all(path)?`、`Parser.next()?` 这类代码更自然。

3. `Span` / `Vec` / `String` / `Path`
   已启动。当前已有 `Span<T>` / `MutSpan<T>`、`Vec<T>` 结构、`VecU8` 专用操作、拥有型 `String` 和拥有型 `Path`，并用 std 集成样例覆盖 method API、`Result` / `Option` 和 `?` 组合。旧 `BufferU8` 已迁到 `VecU8`，`std.fs.file` / `std.sys.host` 的文件读写入口也已改为 `Result<FileBytes, i32>`、`Result<usize, i32>` 等 M1 风格 API。下一步继续扩展到 token buffer、source list 和更通用的 path/build graph 场景。

4. generic constraints / trait / `where`
   generic function MVP 已落地。下一步补最小 trait/interface 或 capability predicate，再推进约束、method-like resolution 和单态化缓存。完成后补 typed graph 和 map-like examples。

5. class/object model MVP  
   在 method 和 trait 基础稳定后实现 class，这样 class 的成员解析、visibility、vtable lowering 可以复用已有调用模型。完成后增加一个 OOP 风格插件/任务 runner example。

6. `defer` / noncopyable / OS 能力  
   已启动。当前 `defer call();` 会在当前词法作用域退出时反序执行，并覆盖正常退出、`return`、`break` / `continue` lowering。下一步补 noncopyable 资源规则、目录遍历、文件 metadata、subprocess、cwd/env 和临时目录能力，让文件、进程、arena、临时目录能安全组合。完成后开始写 `axbuild` 样例。

7. 自举前端和 typed 构建工具验收  
   两个系统级样例进入 integration tests，并继续要求覆盖率保持 90% 以上。

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

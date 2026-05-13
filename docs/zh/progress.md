# 当前进度文档

版本：0.1.2
阶段：M2 language-core-no-std

## 总体状态

当前仓库已经进入 M2 阶段。M2 的目标不是继续修补 M1，而是重新收口语言核心：冻结并删除标准库和 M1 系统样例，把注意力放回基础语法、类型系统、模式匹配、`unsafe` 边界、IR 和 LLVM 后端。

M1 阶段已经舍弃。主要原因不是单个功能失败，而是整体设计方向不稳：

- 标准库、host support、构建工具样例和语言核心同时扩张，导致测试结果很难判断是语言问题、库问题还是工具链问题。
- 部分能力依赖标准库名字或样例约定，而不是语言级规则，例如资源释放、容器 API 约束、`Result` / `Option` 形状。
- 语法地基还没有冻结时就推进 selfhost/build-tool 路线，造成前端、库 API、资源模型互相牵制。
- M1 的很多样例验证的是“能跑通当前 demo”，不是验证语言核心是否稳定、可解释、可长期扩展。

因此 M2 只承认当前 C++ Stage0 编译器、Aurex IR、LLVM backend 和自包含语言样例作为有效基线。当前仓库没有 `std/` 目录，也没有 `selfhost/` 目录；相关旧路线只作为历史输入，不再代表当前进度。

## 已完成能力

当前生产编译器位于 `src/` 和 `include/`，使用 C++20 实现。

- CLI 支持 `--check`、`--dump-*`、`--emit=*`、`--opt-level`、`-I`、`-o`、`--clang` 和 `--clang-arg`。
- driver 能完成文件 IO、模块加载、编译流水线调度、LLVM IR 临时文件生成和 clang 调用。
- module loader 支持根模块和 import 模块合并，能检测模块名不匹配、缺失 import、循环 import、重复模块名和重复加载。
- lexer/parser 是手写实现，输出 ID-backed AST；token、AST、module、checked、IR、LLVM IR 都有 dump 路径。
- 语义分析已具备类型表、符号表、函数签名、ABI 名称、struct/enum 元数据、泛型实例化、表达式类型表和 source range 诊断。
- 当前语言样例覆盖函数、函数指针类型、`extern c`、`export c fn`、普通 `fn main`、import、可见性、const、type alias、struct、enum、opaque struct、泛型、method、指针、数组、slice、字段访问、索引、cast、内建 size/align、`if` 表达式、block 表达式、`match`、`while`、C-style `for`、基础 `for i in range(...)`、`defer` 和 `?`。
- 程序入口支持根模块普通 `fn main`，签名包括无参数或 `argc/argv`，返回 `i32` 或 `void`。
- Aurex IR 是 typed CFG/SSA-like 中间层，后端只消费 IR，不回读 AST。
- IR verifier 会检查函数、block、value、terminator、类型和引用一致性。
- pass pipeline 支持 `O0` 到 `O3` 选项；当前实际优化以保守的局部 mem2reg 和 CFG cleanup 为主。
- LLVM backend 使用 LLVM C++ API 从 Aurex IR 生成 LLVM IR，并通过 clang 生成 asm、object 和 executable。

## 已删除能力

M2 明确删除并暂缓这些 M1 内容：

- `std/` 源树。
- host C support 源文件和自动链接。
- driver 的标准库查找、import path 自动注入和安装后 std 查找。
- CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 安装规则中的 `share/aurex/std`。
- std/M1/system/build-tool 样例。
- 依赖标准库名字的资源语义特判。
- selfhost / Stage1 / AIR snapshot 路线在当前仓库中的实现。

这些内容不是永远不能回来，而是必须等基础语法、module/package、`unsafe`、slice/string 和泛型约束稳定后再重新设计或重新评估；拥有型资源库还要等后续资源语义专题完成。

## 质量门

当前主要质量门：

```sh
tools/run_tests.sh
tools/bench.py
```

测试覆盖：

- lexer/parser/AST dump。
- driver/CLI/native toolchain。
- positive/negative `.ax` sample suite。
- 模块、可见性、泛型、函数、方法、pattern matching、error handling 和类型系统诊断。
- Aurex IR lowering、IR verifier、pass pipeline、LLVM backend。
- native execution 和安装后 compiler 执行。

当前 `build` 目录可能不是完整测试配置；可信验证应以 `tools/run_tests.sh` 重新 configure/build/ctest 为准。

## M2 当前短板

M2 的核心短板集中在语言地基，不在标准库规模：

- block statement 和 block expression 主体规则已统一；expression block 可完整承载普通 statement，并额外要求 final expression。
- const initializer 已补齐纯标量运算；当前仍没有函数调用、控制流表达式或完整 comptime。
- compound assignment 已补齐；`++` / `--` 自增自减语法已从 M2 基础语法移除，统一使用 `+= 1` / `-= 1`。
- 基础 range-for 已补齐为 `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)`；当前仍没有容器迭代、slice 迭代或 iterator protocol。
- trailing separator 策略已冻结：圆括号/方括号列表允许 trailing comma，comma 分隔花括号列表允许但不强制最后一个 comma。
- 现代基础语法继续收口：ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer 和基础字面量体系已完成。
- default private 已完成：顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public。
- `pub fn` 返回类型已收紧为必须显式；private helper 仍可推导。
- lexer 已支持嵌套 `/* ... */` 块注释。
- enum 已支持 ADT-first 形态：普通 enum 可省略 base type 和 discriminant，tag 自动分配；多字段 payload 可用 `.case(a, b)` pattern 按字段解构；显式 `enum Status: u8 { ok = 0, err = 1 }` 仍作为 C-like/repr enum 形态保留。M2 仍不支持 generic enum。
- 数组、slice、tuple、函数指针和字面量基础语法已闭合：固定数组支持 `[1, 2, 3]` 和 `[0; 128]`，borrowed slice 支持 `[]const T` / `[]mut T` 以及 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`；tuple 支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、局部 `let (a, _) = value;` 解构和 tuple pattern；匿名 tuple 不支持直接字段访问，需要字段访问时使用 named struct；函数类型支持 `fn(i32) -> i32`、`extern c fn(*const u8, ...) -> i32`、函数名作为值、局部/参数/字段函数指针间接调用；字面量支持普通字符串、C 字符串、raw/multiline raw string、byte string、byte literal、Unicode scalar `char` 和整数/浮点类型后缀。
- 最小 `unsafe` 已落地：`unsafe { ... }` 可作为 statement 或 expression，`unsafe fn` 和 unsafe 函数指针类型会把调用限制在 unsafe context 内；raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 都必须在 unsafe context 中使用。
- 泛型没有 `where`、trait 或 capability predicate，不能表达 `K: Eq + Hash` 这类基础约束；资源类约束暂缓。
- M1 的语言级 `noncopy` / `move` MVP 已从 M2 基线删除。当前先保留普通值语义和必要的数组/含数组类型限制；copy/drop/borrow/ownership 暂缓为后续资源语义专题。
- raw pointer 仍同时承担 FFI、method receiver、临时借用和地址操作；unsafe 现在已经圈住底层危险操作，但长期仍需要 safe reference 把这些角色分层。
- `str` 已有语言级雏形，普通数组/slice 地基已落地，`strraw` 已纳入 unsafe；M2 no-std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool`、`strfromutf8(bytes) -> str` 和 `text[l:r]` checked slicing。`strfromutf8` 失败或 `str` 切片越界/落在 UTF-8 continuation byte 上时返回空 `str`，不会把无效输入包装成 `str`。更完整的 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 仍后置到库层重建。

当前完整语法库存、已支持高级能力、未完成特性和基础语法优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

## 当前结论

M2 的正确目标是先把基础语法和核心语义做窄做稳，再谈标准库、自举和构建工具。当前编译器已经能支撑语言核心实验和 native 输出，但不应把 M1 的 std/selfhost 经验继续当作有效路线推进。

下一步最重要的是继续冻结 M2 语法基线。ADT-first enum、enum multi-payload destructuring、数组值语法、slice type/expression、tuple/destructuring、function pointer / function type、字面量体系、最小 unsafe 边界、M2 pattern ergonomics 和 `str` checked UTF-8 边界已经落地；pattern 当前支持 tuple match pattern、slice pattern、struct pattern、nested enum payload destructuring、局部 struct/slice/enum destructuring、binding or-pattern alternatives、`let ... else`、`if value is pattern` / `while value is pattern`，以及 if 表达式 pattern condition。随后更适合处理非资源类 capability/trait/where、safe reference 边界，以及更精细的结构化 exhaustiveness。

# 下一步计划

## 当前分支原则

旧标准库已冻结并从 M2 当前树删除。下一阶段不要继续扩张 std，也不要用 std 样例证明语言能力。所有新能力先用自包含 `.ax` 样例验证；库层必须等基础语法、类型系统和模块边界稳定后重新设计，不复用旧 std 路线。

## 优先路线

1. 现代基础语法第一优先级

   当前阶段先完善基础语法，而不是扩张高级特性或重建库层。对照范围不只限 Rust、Go、Zig、Kotlin、C++，还要参考 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V 等现代语言，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness 和 unsafe boundary 相关研究。结论是：Aurex 的大语法骨架已经具备，第一优先级应收口现代语言共同证明过的基础表达能力，而不是先做 trait、borrow checker、macro、async、std 或通用 iterator protocol。

   第一批 P0 基础语法按这个顺序推进：

   - 内建操作拼写已经先规范为 `sizeof[T]`、`alignof[T]`、`cast[T](x)`、`ptrcast[T](p)`、`bitcast[T](x)`、`ptraddr(p)`、`ptrat[T](addr)`、`strptr(s)`、`strblen(s)`、`strvalid(bytes)`、`strfromutf8(bytes)`、`strraw(data, len)`；旧的函数式拼写不再作为源码语法。
   - 最小 `unsafe` block / `unsafe fn` 已完成：raw pointer dereference、`ptrcast`、`bitcast`、`ptrat`、`strraw` 这类破坏不变量的操作已经不能留在普通安全表达式表面。
   - ADT-first enum 已完成非泛型 M2 形态：普通 `enum OptionI32 { some(i32), none }` / `enum Token { span(usize, usize), eof }` 成为主力写法；保留 `enum Status: u8 { ok = 0, err = 1 }` 作为显式 C-like/repr enum。
   - array literal / repeat literal 已完成：`[1, 2, 3]` 和 `[0; 128]` 现在能构造固定长度数组值。

   已完成的第一优先级基础项：default private、ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer、最小 unsafe 和 M2 pattern ergonomics。顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public，`impl` / `extern` block 不能显式 `priv`。slice 当前是 `ptr + len` 的 borrowed fat value，支持 `[]const T` / `[]mut T` 和 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`，不包含容器迭代或运行时 bounds check。tuple 当前支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、局部 `let (a, _) = value;` 解构和 `match pair { (a, b) => ... }`；匿名 tuple 不支持 `.0` / `.1` 或 `.first` / `.second` 字段访问，需要字段访问时使用 named struct；空 tuple 不属于 M2。函数类型当前是非捕获函数指针，支持 `fn(...) -> T`、`unsafe fn(...) -> T`、`extern c fn(...) -> T`、`unsafe extern c fn(...) -> T`、函数名作为值以及局部/参数/字段函数指针间接调用；完整 closure 捕获仍暂缓。

   第二批 P1 基础语法中 raw/multiline raw string、byte string、Unicode scalar `char`、数值后缀、tuple/destructuring、struct pattern、slice pattern、nested enum payload pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已补齐。容器迭代、完整 closure 捕获、trait/interface/protocol、package manager、macro、async 继续暂缓。完整库存和优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

2. unsafe 与 `str` 安全边界

   最小 unsafe 已落地：`unsafe { ... }` 建立 unsafe context，带 tail expression 时作为表达式求值，没有 tail expression 时类型为 `void`；`unsafe fn` 和 unsafe 函数指针调用必须发生在 unsafe context。raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 已经是 unsafe-only。M2 这里不包含 borrow checker、lifetime、unsafe trait、unsafe impl、unsafe extern block 或资源/所有权模型。`str` 的无 std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool` 和 `strfromutf8(bytes) -> str`；失败时返回空 `str`，不会把无效输入包装成 UTF-8 文本。

3. enum ADT 与 pattern 地基

   enum ADT-first 已落地到非泛型 M2 基线：base type / discriminant 可省略，tag 自动分配，多字段 payload 可构造，并且 pattern 侧支持 `.case(a, b)`、`.case((a, b))` 等按字段/嵌套解构。局部 tuple/struct/slice/enum destructuring、match tuple/slice/struct pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已落地。

4. 数组、slice、字符串与函数类型基础语法

   Aurex 已有数组类型和值语法、borrowed slice、tuple、`str`、C string、raw/multiline raw string、byte string、byte literal、Unicode scalar `char`、函数声明、函数指针类型、C FFI 和最小 safe reference。字面量体系、tuple 基础、pattern ergonomics、`str` checked UTF-8 构造/切片和 `&T` / `&mut T` 已经补齐到 M2 基线；下一步更适合处理 capability/where 和更精确的结构化 exhaustiveness，而不是重建库层。

5. 值语义边界

   M2 当前已经删除 M1 的 `move(...)` / `noncopy struct`，避免把失败的 move-only MVP 继续当作语言地基。当前阶段不继续推进资源模型；只写清普通值传递、struct/enum payload、match payload、`?` 和数组/含数组类型限制的统一规则。

6. 资源语义暂缓

   `Copy` / `Drop` / destructor / borrow / move-out 不作为当前 M2 近期任务。等 `unsafe`、ADT、array/slice/string、function type 和 pattern 地基稳定后，再重新开资源语义专题。

7. safe reference

   最小 `&T` / `&mut T` 已进入 M2 核心：reference 和 raw pointer 是不同类型，`&mut` 要求 writable place，reference 解引用是 safe，raw pointer 解引用仍是 unsafe-only。borrow checker、lifetime、borrowed return、alias model 和资源语义继续暂缓。

8. Capability / trait / where

   用语言机制替代临时 hardcode。当前只评估非资源类约束，例如 `Eq`、`Ord`、`Hash`、`Sized`；资源相关能力等资源语义专题再定。

9. 字符串基础类型

   保留 `str` 作为语言级 borrowed UTF-8 slice 的设计方向，但不要复活旧 std 的 `String`/`Bytes` 实现。`str` 的类型、ABI、字面量、unchecked `strraw`、checked `strvalid` / `strfromutf8` 构造边界，以及 `text[l:r]` checked byte-offset slicing 已落地；未来库层 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 继续后置。

10. 测试性能继续收口

   保持测试 harness 直接调用 C++ driver，避免每个用例启动脚本。新增用例时区分 check/IR/native 三类，只有必须验证运行时行为的样例才生成并执行二进制。

## 明确暂缓

- std 容器、文件、目录、进程、console。
- M1 frontend / axbuild 样例；M1 阶段已经舍弃，不能作为当前路线继续推进。
- host support C shim。
- 安装后 std 查找。

重新设计或重新评估这些内容的前置条件是：基础语法、模块边界、`unsafe`、ADT、slice/string 和泛型约束已有稳定语言级设计和测试矩阵；拥有型资源库还需要后续资源语义专题完成。

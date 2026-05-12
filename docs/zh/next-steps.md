# 下一步计划

## 当前分支原则

旧标准库已冻结并从 M2 当前树删除。下一阶段不要继续扩张 std，也不要用 std 样例证明语言能力。所有新能力先用自包含 `.ax` 样例验证；库层必须等基础语法、类型系统和模块边界稳定后重新设计，不复用旧 std 路线。

## 优先路线

1. 现代基础语法第一优先级

   当前阶段先完善基础语法，而不是扩张高级特性或重建库层。对照范围不只限 Rust、Go、Zig、Kotlin、C++，还要参考 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V 等现代语言，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness 和 unsafe boundary 相关研究。结论是：Aurex 的大语法骨架已经具备，第一优先级应收口现代语言共同证明过的基础表达能力，而不是先做 trait、borrow checker、macro、async、std 或通用 iterator protocol。

   第一批 P0 基础语法按这个顺序推进：

   - 内建操作拼写已经先规范为 `sizeof[T]`、`alignof[T]`、`cast[T](x)`、`ptrcast[T](p)`、`bitcast[T](x)`、`ptraddr(p)`、`ptrat[T](addr)`、`strptr(s)`、`strblen(s)`、`strraw(data, len)`；旧的函数式拼写不再作为源码语法。
   - 最小 `unsafe` block / `unsafe fn`：raw pointer dereference、`ptrcast`、`bitcast`、`ptrat`、`strraw` 这类破坏不变量的操作不能继续留在普通安全表达式表面。
   - ADT-first enum：让普通 `enum Option[T] { some(T), none }` / `enum Result[T, E] { ok(T), err(E) }` 成为主力形态；保留 `enum Status: u8 { ok = 0, err = 1 }` 作为显式 C-like/repr enum。
   - array literal / repeat literal：数组类型 `[N]T` 已存在，但还缺 `[1, 2, 3]` / `[0; 128]` 这类基础值语法。

   已完成的第一优先级基础项：default private 已切换完成。顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public，`impl` / `extern` block 不能显式 `priv`。

   第二批 P1 基础语法继续补 slice type/expression、raw/multiline/byte string、Unicode scalar `char`、function type / `extern c fn` type、tuple/destructuring，以及 `if let` / `let ... else` / struct pattern。容器迭代、完整 closure 捕获、trait/interface/protocol、package manager、macro、async 继续暂缓。完整库存和优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

2. unsafe 与 `str` 安全边界

   raw pointer、`ptrcast`、`bitcast`、`ptrat`、`strraw` 当前都在普通表达式层。M2 应先设计最小 `unsafe` block / `unsafe fn` 和 unchecked builtin 诊断框架，同时冻结 `str` 的 UTF-8 / slice / FFI 边界。

3. enum ADT 与 pattern 地基

   当前 enum 偏 C-like，base type 和 discriminant 必填。M2 应先设计主力 ADT 语法，让 `Result` / `Option` / compiler AST 状态空间表达更轻，再扩展 multi-payload、struct pattern、`if let` / `let ... else`。

4. 数组、slice、字符串与函数类型基础语法

   Aurex 已有数组类型、`str`、C string、byte literal、函数声明和 C FFI，但还缺数组值语法、slice type/expression、raw/multiline/byte string、Unicode scalar `char`、function pointer / function type。现代系统语言和 ML-family 语言都说明这些属于基础表达能力，不应等到未来库层重建后再补。

5. 值语义边界

   M2 当前已经删除 M1 的 `move(...)` / `noncopy struct`，避免把失败的 move-only MVP 继续当作语言地基。当前阶段不继续推进资源模型；只写清普通值传递、struct/enum payload、match payload、`?` 和数组/含数组类型限制的统一规则。

6. 资源语义暂缓

   `Copy` / `Drop` / destructor / borrow / move-out 不作为当前 M2 近期任务。等 `unsafe`、ADT、array/slice/string、function type 和 pattern 地基稳定后，再重新开资源语义专题。

7. safe reference 方向

   文档层面保留 `&T` / `&mut T` 作为 safe reference 方向，用于和 raw pointer 分层；borrow checker、lifetime 和 borrowed return 暂缓。

8. Capability / trait / where

   用语言机制替代临时 hardcode。当前只评估非资源类约束，例如 `Eq`、`Ord`、`Hash`、`Sized`；资源相关能力等资源语义专题再定。

9. 字符串基础类型

   保留 `str` 作为语言级 borrowed UTF-8 slice 的设计方向，但不要复活旧 std 的 `String`/`Bytes` 实现。先完成 `str` 的类型、ABI、字面量、slice 边界和内建操作边界。

10. 测试性能继续收口

   保持测试 harness 直接调用 C++ driver，避免每个用例启动脚本。新增用例时区分 check/IR/native 三类，只有必须验证运行时行为的样例才生成并执行二进制。

## 明确暂缓

- std 容器、文件、目录、进程、console。
- M1 frontend / axbuild 样例；M1 阶段已经舍弃，不能作为当前路线继续推进。
- host support C shim。
- 安装后 std 查找。

重新设计或重新评估这些内容的前置条件是：基础语法、模块边界、`unsafe`、ADT、slice/string 和泛型约束已有稳定语言级设计和测试矩阵；拥有型资源库还需要后续资源语义专题完成。

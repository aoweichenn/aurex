# Builtin Surface：收窄类型查询和显式转换表面

日期：2026-06-18
状态：当前实现记录

本文固定这一轮 builtin 表面优化。当前阶段只处理高频 safe scalar cast 和 layout query；低层指针、bitcast、字符串 raw/UTF-8 builtin 保持现有短写法，之后单独设计。

## 决策

旧的泛型函数式 cast 已删除，改为 C++ 风格表达式 cast：

```aurex
let value: i32 = input as i32;
let byte: u8 = (codepoint & 0xffusize) as u8;
let ok: bool = status as bool;
```

`as` 是表达式层操作符，结果 AST 继续使用现有 `ExprKind::cast`，后续 sema 仍执行已有 scalar cast 规则。`as` 的左操作数优先级高于二元运算，因此给复合表达式加括号：

```aurex
let span_len: i32 = (end - start) as i32;
let packed: u8 = (0xc0usize | (value >> 6usize)) as u8;
```

旧的无调用括号 layout query 已删除，改为普通泛型调用外形：

```aurex
let size: usize = sizeof<Pair>();
let align: usize = alignof<*mut Pair>();
```

parser 在 postfix call 层识别 `sizeof<T>()` / `alignof<T>()`，并折叠为现有 `ExprKind::size_of` / `ExprKind::align_of`。调用必须满足：

- callee 是无作用域名字 `sizeof` 或 `alignof` 的泛型实例化。
- 恰好一个 type argument。
- value argument 列表为空。

## 暂不修改

这些 builtin 暂时保持当前短写法：

```aurex
ptraddr(value)
ptrat<*mut T>(addr)
ptrcast<*const T>(ptr)
bitcast<T>(value)
strvalid(text)
strfromutf8(bytes)
strraw(data, len)
```

这一轮不引入新的保留 namespace，也不把低层 builtin 改成长路径。后续涉及 unsafe intrinsic、runtime boundary 或标准库表面时再单独设计。

## Parser 边界

- `cast`、`sizeof`、`alignof` 不再是 keyword。
- `as` 仍是 keyword。
- 旧 cast 写法、旧无括号 layout query 不保留兼容解析。
- `sizeof<T>()` / `alignof<T>()` 只接受 type argument，不接受 value argument。

## 测试要求

- lexer：`cast`、`sizeof`、`alignof` 作为普通 identifier；`as` 仍作为 keyword。
- parser：覆盖 `x as T`、链式/括号 cast、`sizeof<T>()`、`alignof<T>()`。
- sema/integration：正负样例全部迁移到新表面，旧表面不再作为可通过语法。
- 文档和样例不能再把旧 cast 写法或旧无括号 layout query 作为当前写法。

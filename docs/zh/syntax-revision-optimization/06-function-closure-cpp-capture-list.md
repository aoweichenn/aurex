# Function / Closure Surface：C++ 风格 capture-list

日期：2026-06-18
状态：当前闭包捕获语法与语义
关联实现：parser / AST / sema / IR lowering / samples / gtest

## 结论

Aurex 闭包字面量使用 C++ 风格 capture-list，参数列表保持普通 `(...)`：

```aurex
[](value: i32) -> i32 => value + 1
[base](value: i32) -> i32 => value + base
[&base](value: i32) -> i32 => value + base
[&mut base](value: i32) -> i32 {
    base += value;
    return base;
}
[=](value: i32) -> i32 => value + base
[&](value: i32) -> i32 => value + base
[=, &adjust](value: i32) -> i32 => value + adjust
[&, copied](value: i32) -> i32 => value + copied
```

当前不保留旧 pipe lambda 成功路径。`[&mut x]` 要求被捕获对象是可写外层变量；`let` 绑定只能用
`[x]`、`[&x]` 或被 `[=]` / `[&]` 默认捕获。

## 语法规则

```text
ClosureExpr =
    CaptureList ParamList "->" Type ClosureBody

CaptureList =
    "[" CaptureDeclList? "]"

CaptureDeclList =
    CaptureDecl ("," CaptureDecl)* ","?

CaptureDecl =
    Identifier
  | "&" Identifier
  | "&" "mut" Identifier
  | "="
  | "&"

ParamList =
    "(" ParamDeclList? ")"

ClosureBody =
    "=>" Expr
  | Block
```

这不是空格敏感语法。parser 只看 token 序列：

- `[` capture-list `]` 后面必须跟 `(`，才是 closure head。
- 其他 `[` 仍然按数组字面量、slice/index、类型或 pattern 的既有规则处理。
- `[T]` 不因为没有空格或有空格改变含义。

## 捕获语义

当前实现支持：

```aurex
[]
[name]
[&name]
[&mut name]
[=]
[&]
[=, &name]
[&, name]
```

语义：

- 捕获项必须被 body 实际使用。
- 没有默认捕获时，body 实际使用的外层局部/参数必须列在 capture-list。
- `[name]` 是值捕获，环境字段保存 captured value 的副本。
- `[&name]` 是共享引用捕获，环境字段保存外层 slot 地址，闭包体只读 alias。
- `[&mut name]` 是可变引用捕获，环境字段保存外层 slot 地址，闭包体可写 alias。
- `[=]` 是默认值捕获，未显式列出的实际使用外层名字按值捕获。
- `[&]` 是默认共享引用捕获，未显式列出的实际使用外层名字按共享引用捕获。
- 默认捕获必须位于 capture-list 第一项，同一个 capture-list 只能有一个默认捕获。
- 显式捕获可以覆盖默认捕获，例如 `[=, &adjust]` 和 `[&, copied]`。

诊断：

- 重复捕获诊断为 `duplicate closure capture name`。
- 重复默认捕获诊断为 `duplicate closure capture default`。
- 默认捕获不在第一项诊断为 `closure capture default must appear first`。
- 与默认捕获模式相同的显式项诊断为 `closure capture is redundant with the capture default`。
- 列了但未使用诊断为 `closure capture list contains a name that is not captured`。
- 用了但没列且没有默认捕获时诊断为 `closure capture must be listed in the capture list`。
- 捕获闭包不是 `fn(...) -> T` 薄函数指针，不能赋给 `fn` 类型。

当前所有捕获模式的源类型仍必须是非 generic-dependent、非 borrowed-view、满足 `Copy` capability 的外层局部或参数。
这条限制服务于当前匿名环境 ABI 和 escape/lifetime 边界，不代表语法上不区分值捕获和引用捕获。

## Lowering

值捕获 lowering：

```text
outer local slot -> load value -> environment field -> closure body local slot
```

引用捕获 lowering：

```text
outer local slot address -> environment field -> closure body alias binding
```

实现落点：

- checked lambda capture 保存 capture mode。
- environment field type 对引用捕获使用内部 pointer-like carrier。
- closure body local symbol 绑定到 alias storage，而不是新 alloca 后 store。
- mutable capture 和外层 variable mutability 联动。
- IR lowering 对值捕获保存值，对引用捕获保存外层 slot 地址。

## 代码落点

- token 层不新增关键字；`&`、`mut`、`=`、identifier 复用现有 token。
- AST 使用 `LambdaCaptureKind` 和 `LambdaCaptureDecl`。
- lambda payload 持有 `captures + params + return_type + body`。
- parser 闭包只从 `[` capture-list `]` `(` param-list `)` 开始。
- sema 校验默认捕获顺序、重复、冗余显式项、捕获模式和 capture-list 使用情况。
- dump 输出 `lambda [](...)`、`lambda [base](...)`、`lambda [&base](...)`、`lambda [=](...)`、`lambda [&](...)`。
- samples 和 gtest 全部使用当前写法，不保留旧源码语法。

## 后续深化

- init-capture、move capture、consuming capture 和 closure trait 仍是后续专题。
- borrow/lifetime checker 需要继续细化 shared/mutable capture 的冲突和闭包逃逸约束。
- generic closure environment ABI、borrowed-view capture 和 heap/allocator closure box 不在本轮。

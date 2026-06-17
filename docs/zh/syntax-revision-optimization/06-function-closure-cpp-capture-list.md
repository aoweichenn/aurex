# Function / Closure Surface：C++ 风格 capture-list

日期：2026-06-17
状态：当前闭包捕获语法与语义
关联实现：parser / AST / sema / IR lowering / samples / gtest

## 结论

Aurex 闭包字面量切到 C++ 风格 capture-list：

```aurex
[](value: i32) -> i32 => value + 1
[base](value: i32) -> i32 => value + base
[base](value: i32) -> i32 {
    return value + base;
}
```

显式捕获列表支持三种模式：

```aurex
[base](value: i32) -> i32 => value + base      // 值捕获：闭包环境保存独立副本
[&base](value: i32) -> i32 => value + base     // 共享引用捕获：闭包体读取外层变量当前值
[&mut base](value: i32) -> i32 {               // 可变引用捕获：闭包体写入外层 var
    base += value;
    return base;
}
```

`[&mut x]` 要求被捕获对象是可写外层变量；`let` 绑定只能用 `[x]` 或 `[&x]`。

## 为什么不继续用 pipe lambda

旧方向：

```aurex
|value: i32| -> i32 => value + 1
|next: i32| -> i32 => next + base
```

问题：

- 捕获列表没有显式位置，读者必须扫描 body 才知道闭包是否捕获外层变量。
- 空捕获、值捕获、引用捕获、可变引用捕获没有统一视觉模型。
- `|...|` 和逻辑或 token 共享视觉噪音，参数列表也不是 Aurex 已有的普通 `(...)` 形状。
- 未来做 `Fn` / `FnMut` / `FnOnce`、escape/lifetime 检查、capture mode 诊断时，缺少 source-level anchor。

C++ 风格 capture-list 的优势：

- `[]` 一眼表示无捕获。
- `[x]` 一眼表示显式捕获。
- `[&x]` / `[&mut x]` 预留引用捕获，不需要以后再破坏语法。
- 参数列表回到普通 `(...)`，和函数声明、函数类型、调用形状一致。
- 不依赖空格，`[x](v:T)`、`[ x ] ( v : T )` 都是同一个 token 结构。

## 语法规则

当前源码成功路径：

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
[name]
[&name]
[&mut name]
```

语义：

- 捕获项必须被 body 实际使用。
- body 实际使用的外层局部/参数必须列在 capture-list。
- `[name]` 是值捕获，环境字段保存 captured value 的副本。
- `[&name]` 是共享引用捕获，环境字段保存外层 slot 地址，闭包体只读 alias。
- `[&mut name]` 是可变引用捕获，环境字段保存外层 slot 地址，闭包体可写 alias。
- `[&mut name]` 要求外层 binding 本身可写。
- 重复捕获诊断为 duplicate closure capture name。
- 列了但未使用诊断为 closure capture list contains a name that is not captured。
- 用了但没列诊断为 closure capture must be listed in the capture list。
- 值捕获仍要求非 generic-dependent、非 borrowed-view、满足 `Copy` capability。
- 捕获闭包不是 `fn(...) -> T` 薄函数指针，不能赋给 `fn` 类型。

示例：

```aurex
fn direct_capture(value: i32) -> i32 {
    let base: i32 = 40;
    let add_base = [base](next: i32) -> i32 => next + base;
    return add_base(value);
}
```

## 引用捕获 lowering

`[&x]` 的语义不是“复制 x 的当前值”，而是：

- closure environment 保存指向外层 binding storage 的引用/地址。
- closure body 内的 `x` 解析成这个外层 storage 的别名。
- shared reference capture 只能读，不能写。
- mutable reference capture 要求外层变量可写。
- borrow/lifetime checker 后续继续细化逃逸闭包和 alias 冲突约束。

值捕获 lowering：

```text
outer local slot -> load value -> environment field -> closure body local slot
```

引用捕获 lowering：

```text
outer local slot address -> environment field -> closure body alias binding
```

实现落点：

- checked lambda capture 中保存 capture mode。
- environment field type 对引用捕获使用内部 pointer-like carrier。
- closure body local symbol 绑定到 alias storage，而不是新 alloca 后 store。
- mutable capture 和外层 variable mutability、loan state 联动。

## 代码落点

- token 层不新增关键字；`&`、`mut`、identifier 复用现有 token。
- AST 新增 `LambdaCaptureKind` 和 `LambdaCaptureDecl`。
- lambda payload 持有 `captures + params + return_type + body`。
- parser 删除旧 pipe lambda 成功路径，闭包只从 `[` capture-list `]` `(` param-list `)` 开始。
- sema 校验显式 capture-list，持久化 capture mode，并拒绝 `[&mut]` 捕获不可写绑定。
- dump 输出 `lambda [](value: i32) -> i32`、`lambda [base](...)`、`lambda [&base](...)`。
- IR lowering 对值捕获保存值，对引用捕获保存外层 slot 地址，闭包体绑定 alias storage。
- samples 和 gtest 全部迁移到新写法，不保留旧源码语法。

## 后续深化
- 后续可加入 C++ 默认捕获 `[=]`、`[&]` 和 init-capture，但当前版本只实现显式 capture-list。
- 需要继续扩展嵌套引用捕获、外层 move/drop 后使用、返回闭包等生命周期负例。

- borrow/lifetime checker 需要继续细化 shared/mutable capture 的冲突和闭包逃逸约束。

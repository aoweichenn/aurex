# Function / Closure Surface：C++ 风格 capture-list 第一批落地

日期：2026-06-13
状态：语法修正优化第六手改动，第一批代码落地
关联实现：parser / AST / sema / samples / gtest

## 结论

Aurex 闭包字面量切到 C++ 风格 capture-list：

```aurex
[](value: i32) -> i32 => value + 1
[base](value: i32) -> i32 => value + base
[base](value: i32) -> i32 {
    return value + base;
}
```

第一批语义只放开按值捕获：

```aurex
[base](value: i32) -> i32 => value + base
```

引用捕获必须在语法和 AST 中一开始占住位置，但当前不能假支持：

```aurex
[&base](value: i32) -> i32 => value + base      // 当前 sema 拒绝
[&mut base](value: i32) -> i32 => value + base  // 当前 sema 拒绝
```

诊断固定为：

```text
reference capture in closures is not supported yet
```

原因很直接：当前 closure lowering 是把捕获值 load 出来，复制进匿名环境 record。这个模型只能表达 value capture，
不能表达“环境里保存外层变量地址，并且闭包体内的名字 alias 到原变量”的 reference capture。直接放开 `[&x]`
会把引用捕获偷偷降级成值捕获，这是错误语义。

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

第一批只真正支持：

```aurex
[name]
```

语义：

- 捕获项必须被 body 实际使用。
- body 实际使用的外层局部/参数必须列在 capture-list。
- 重复捕获诊断为 duplicate closure capture name。
- 列了但未使用诊断为 closure capture list contains a name that is not captured。
- 用了但没列诊断为 closure capture must be listed in the capture list。
- 捕获值仍要求非 generic-dependent、非 borrowed-view、满足 `Copy` capability。
- 捕获闭包不是 `fn(...) -> T` 薄函数指针，不能赋给 `fn` 类型。

示例：

```aurex
fn direct_capture(value: i32) -> i32 {
    let base: i32 = 40;
    let add_base = [base](next: i32) -> i32 => next + base;
    return add_base(value);
}
```

## 引用捕获为什么先拒绝

`[&x]` 的正确语义不是“复制 x 的当前值”，而是：

- closure environment 保存指向外层 binding storage 的引用/地址。
- closure body 内的 `x` 解析成这个外层 storage 的别名。
- shared reference capture 只能读，不能写。
- mutable reference capture 需要独占借用，并影响外层变量的可写/可借用状态。
- 逃逸闭包必须证明引用不超过被捕获 binding 的 lifetime。

当前 IR lowering 做的是：

```text
outer local slot -> load value -> environment field -> closure body local slot
```

引用捕获需要的是：

```text
outer local slot address -> environment field -> closure body alias binding
```

这会牵涉：

- checked lambda capture 中保存 capture mode。
- environment field type 从 captured value type 变成 reference/pointer-like carrier。
- closure body local symbol 绑定到 alias storage，而不是新 alloca 后 store。
- borrow/lifetime checker 对 closure escape 做额外约束。
- mutable capture 和外层 variable mutability、loan state 联动。

因此当前选择是：parser/AST 接受 `[&x]` / `[&mut x]`，sema 明确拒绝，后续完整实现后再删除这个 unsupported 诊断。

## 第一批代码落点

- token 层不新增关键字；`&`、`mut`、identifier 复用现有 token。
- AST 新增 `LambdaCaptureKind` 和 `LambdaCaptureDecl`。
- lambda payload 持有 `captures + params + return_type + body`。
- parser 删除旧 pipe lambda 成功路径，闭包只从 `[` capture-list `]` `(` param-list `)` 开始。
- sema 校验显式 capture-list，并拒绝 reference capture。
- dump 输出 `lambda [](value: i32) -> i32`、`lambda [base](...)`、`lambda [&base](...)`。
- samples 和 gtest 全部迁移到新写法，不保留旧源码语法。

## 后续放开引用捕获的验收条件

真正支持 `[&x]` / `[&mut x]` 前，至少要同时完成：

- checked lambda capture 持久化 capture mode。
- closure environment layout 支持引用字段。
- closure body lowering 能把捕获名绑定成 alias，不再复制到新本地 slot。
- borrow checker 能验证 shared/mutable capture 的冲突和逃逸。
- 负例覆盖 escaping reference capture、mutable alias 冲突、外层 move/drop 后捕获使用。
- 正例覆盖 shared read、mutable write、嵌套引用捕获、返回非逃逸闭包或明确拒绝逃逸。

在这些条件完成前，`[&x]` 和 `[&mut x]` 只能作为已占位语法，不能作为可运行语义。

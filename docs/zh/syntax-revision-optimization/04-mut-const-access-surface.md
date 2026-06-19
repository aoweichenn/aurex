# Mut / Const Access Surface：当前 view 权限表面

日期：2026-06-18
状态：当前源码表面和实现已落地
关联实现：parser / type display / sema / samples / gtest

## 结论

Aurex 当前把“名字能否重绑”和“通过某个 view 能否写”分开：

```aurex
let fixed: i32 = 1;   // binding 不可重绑
var slot: i32 = 1;    // binding 可重绑

let shared_ref: &i32 = &slot;
let writable_ref: &mut i32 = &mut slot;

let shared_slice: []i32 = values[:];
let writable_slice: []mut i32 = values[:];

let raw_read: *const i32 = &slot;
let raw_write: *mut i32 = &mut slot;
```

`let` / `var` 管 binding；`&T` / `&mut T`、`[]T` / `[]mut T` 和 raw pointer mutability 管访问路径的写权限。当前语言不建模 C++ 式完整 cv-qualified type，也不把 `const` 解释成对象深不可变。

## 当前类型表面

```aurex
*const T
*mut T

&T
&mut T

[]T
[]mut T
```

规则：

- `&T` 是 shared safe reference，`&mut T` 是 writable safe reference。
- `[]T` 是 shared borrowed slice view。
- `[]mut T` 是 writable borrowed slice view。
- `*const T` / `*mut T` 保留给 unsafe / FFI 边界。
- `let s: []mut T` 表示名字不能重绑，但可以通过这个 slice view 写元素。
- `var s: []T` 表示名字可以重绑，但不能通过这个 shared slice view 写元素。

## 不支持的写法

`[]const T` 不是当前成功语法。parser 只为了给出定向错误而识别这个 token 序列：

```aurex
let old: []const u8 = data[:];
```

诊断：

```text
legacy []const T is no longer supported; write []T for a shared slice view
```

这个写法不能作为 alias、warning path 或第二套 shared slice 语法继续通过编译。

## 空格规则

Aurex 不做空格敏感语法。只要 token 序列相同，含义就相同：

```aurex
[]T
[] T
[]mut T
[] mut T
&mut T
& mut T
```

同理，旧 token 序列无论是否有空格，都是同一个 legacy error：

```aurex
[]const T
[] const T
```

## 实现和测试

实现落点：

- `src/frontend/parse/grammar/parser_type.cpp`
- `src/frontend/sema/types/type.cpp`
- `tests/samples/negative/types/slice_legacy_const_type.ax`
- `tests/gtest/integration/sample_suite/`

当前 display name：

```text
[]T
[]mut T
&T
&mut T
*const T
*mut T
```

## 后续非目标

- 不用 `const` 补 deep immutable。
- 不引入 `readonly` / `immutable` / reference capability 组合。
- 不把 raw pointer aliasing 纳入 safe borrow proof。
- 如需对象深不可变或并发能力，后续作为独立类型系统专题设计。

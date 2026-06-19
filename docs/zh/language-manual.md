# Aurex 语言参考手册

日期：2026-06-18

本文是当前仓库的语言参考。它描述已经进入 parser、AST、sema、IR/backend 或测试样例的语言表面；没有落地的设计不写成已支持语法。

## 1. 程序结构

```text
SourceFile = ModuleHeader ModulePreamble* Item*
ModuleHeader = "module" ModulePath ";"
```

```aurex
module hello;

fn main() -> i32 {
    return 0;
}
```

入口支持 `fn main() -> i32` 和 `fn main() -> void`。私有普通函数可以推导返回类型；`pub fn`、`extern c fn`、`export c fn` 和函数原型必须显式返回类型。

## 2. 词法

标识符当前使用 ASCII：

```text
Identifier = [A-Za-z_][A-Za-z0-9_]*
```

注释：

- `//` 行注释。
- `/* ... */` 块注释，支持嵌套。

当前主要关键字：

```text
module import as pub priv extern export unsafe
fn struct opaque enum trait const type impl where dyn
match let var if else for in is while break continue defer return
true false null
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str char
mut cast ptrcast bitcast sizeof alignof ptraddr ptrat strvalid strfromutf8 strraw
```

## 3. 类型

基础类型：

```aurex
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
char
str
```

复合类型：

```aurex
*const T
*mut T
&T
&mut T
[N]T
[]T
[]mut T
(A, B)
fn(A, B) -> R
```

`[]T` 是 shared slice view，`[]mut T` 是 writable slice view。当前不使用 `[]const T`。

## 4. Item

函数：

```aurex
fn add(a: i32, b: i32 = 1) -> i32 {
    return a + b;
}
```

Struct：

```aurex
struct Point {
    x: i32;
    y: i32;
}
```

Enum：

```aurex
enum OptionI32 {
    Some(i32),
    None,
}
```

Trait / impl：

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;
}

impl Source for Point {
    type Item = i32;

    fn get(self: &Point) -> i32 {
        return self.x;
    }
}
```

当前内建 item attribute：

```aurex
#[derive(Copy, Eq, Hash)]
struct Key {
    value: i32;
}
```

`derive` 只支持编译器内建能力，不是完整用户宏系统。

## 5. 表达式

当前支持：

- 字面量、数组、repeat array、tuple、struct literal、enum case。
- block expression、if expression、match expression。
- 一元、二元、赋值和 compound assignment。
- field、index、slice、call、method call、generic call。
- cast：`value as T`。
- 语言内建：`sizeof<T>()`、`alignof<T>()`、pointer builtin、string builtin。
- lambda expression。

示例：

```aurex
let values: [3]i32 = [1, 2, 3];
let view: []i32 = values[:];
let first: i32 = values[0];
let text: str = strfromutf8(bytes);
let size: usize = sizeof<i32>();
```

## 6. 语句和控制流

```aurex
let value: i32 = 1;
var total: i32 = 0;

if value > 0 {
    total += value;
} else {
    total = 0;
}

while total < 10 {
    total += 1;
}

for var i: i32 = 0; i < 10; i += 1 {
    total += i;
}

for i in range(0, 10, 2) {
    total += i;
}

let range_values = range(1, 5);
for item in range_values {
    total += item;
}

let values: [3]i32 = [1, 2, 3];
for item in values {
    total += item;
}

let text: str = "abc";
for byte in text {
    total += ((byte) as i32);
}

struct Counter {
    current: i32;
    end: i32;
}

impl Counter {
    fn has_next(self: &mut Counter) -> bool {
        return self.current < self.end;
    }

    fn next(self: &mut Counter) -> i32 {
        let value: i32 = self.current;
        self.current = self.current + 1;
        return value;
    }
}

let counter: Counter = Counter { current: 0, end: 3 };
for item in counter {
    total += item;
}
```

`for i in range(...)` 是 counted range loop。普通表达式位置的 `range(...)` 会生成 `range<T>` value，记录 `start`、`end`、`step` 三个整数字段；该值可以绑定到局部并用 `for item in range_value` 迭代。当前源码类型语法还不能直接书写 `range<T>`，通常依赖 `let values = range(...)` 推断。

`for item in expr` 支持 range value iteration、array/slice value iteration、str byte iteration 和 protocol iterator iteration。

array/slice value iteration 每轮按值读取元素，元素类型必须满足 `Copy`，不会 move array/slice 本身。

str iteration 每轮读取一个 UTF-8 存储字节，loop item 类型是 `u8`。它不做 Unicode scalar / `char` 解码；字符级迭代留给后续标准库 adapter。

protocol iterator 可以是表达式本身，也可以由 `expr.iter()` 产生。iterator 必须提供：

```aurex
fn has_next(self: &mut Iterator) -> bool;
fn next(self: &mut Iterator) -> Item;
```

`next()` 的返回值成为 loop item，protocol item 不要求 `Copy`。协议方法可以来自 inherent method 或静态 trait dispatch；dyn trait vtable-slot dispatch 暂不作为 for-in protocol 来源。

## 7. Pattern 和 match

当前支持 local pattern、let-else、if/while pattern condition、match pattern、tuple/struct/enum payload destructuring、or-pattern 和 guard。

```aurex
match value {
    OptionI32.Some(x) => x,
    OptionI32.None => 0,
}
```

## 8. 泛型

Aurex 泛型统一使用 `<...>`：

```aurex
fn id<T>(value: T) -> T {
    return value;
}

struct Box<T> {
    value: T;
}

fn len<T, const N: usize>(values: [N]T) -> usize {
    return N;
}
```

支持 type parameter、origin/reference 相关表面和 typed scalar const generic check-only 子集。`[]` 不作为泛型语法。

`where` 支持内建 capability 和 nominal trait predicate：

```aurex
fn same<T>(a: T, b: T) -> bool where T: Eq {
    return a == b;
}
```

## 9. Lambda

Lambda 使用 C++ 风格 capture-list：

```aurex
let base: i32 = 2;
let by_value = [base](value: i32) -> i32 => value + base;
let by_ref = [&base](value: i32) -> i32 => value + base;
let by_mut = [&mut base](value: i32) -> i32 => value + base;
let default_value = [=](value: i32) -> i32 => value + base;
let default_ref = [&](value: i32) -> i32 => value + base;
let override_ref = [=, &base](value: i32) -> i32 => value + base;
let override_value = [&, base](value: i32) -> i32 => value + base;
let init = [captured = base + 1](value: i32) -> i32 => value + captured;
let moved = [owned = move base](value: i32) -> i32 => value + owned;
```

规则：

- 默认捕获只能写在 capture-list 第一项。
- 只能有一个默认捕获。
- `[=, x]` 和 `[&, &x]` 这类和默认模式重复的显式捕获会诊断。
- 显式捕获可以覆盖默认模式，例如 `[=, &x]`、`[&, x]`。
- `[x = expr]` 是 init-capture，initializer 在闭包创建点求值。
- `[x = move expr]` 和 `[move x]` 是 move capture，创建闭包时消费源值。
- 普通值捕获源必须满足 `Copy`；捕获泛型依赖类型或 borrowed-view 类型当前仍会诊断。

## 10. Trait 和 dyn

当前支持 nominal static trait、显式 impl、trait method dispatch、associated type、associated-type equality、trait default method 和 borrowed dyn view：

```aurex
trait Draw {
    fn draw(self: &Self) -> i32;
}

fn render(value: &dyn Draw) -> i32 {
    return value.draw();
}
```

`&dyn Trait` / `&mut dyn Trait` 是 borrowed view。当前支持 borrowed dyn composition 和 supertrait upcast。owning dyn、`Box<dyn Trait>`、allocator API 和 trait-object Drop runtime 不属于当前语言表面。

## 11. Unsafe 和内建

`unsafe` 允许使用 raw pointer dereference、unsafe function call 和 unsafe-only builtin，但类型、borrow、move 和 ABI 检查仍继续运行。

当前内建：

```aurex
value as T
sizeof<T>()
alignof<T>()
ptraddr(value)
ptrat<T>(addr)
ptrcast<T>(ptr)
bitcast<T>(value)
strvalid(bytes)
strfromutf8(bytes)
strraw(data, len)
```

`ptrat`、`ptrcast`、`bitcast` 和 `strraw` 需要 unsafe context。本版本不引入新的 `intrinsic.*` 名字。

## 12. 当前不支持

- 标准库 API 和拥有型容器。
- mutable/reference item iteration、range literal、标准库 iterable adapter 和 Unicode scalar / char iteration adapter。
- closure trait、borrowed-view capture 和完整 escaping closure lifetime。
- owning dyn 和 `Box<dyn Trait>`。
- 完整 macro/proc-macro/用户 derive lowering。
- generic associated type、associated const、specialization、generic const arithmetic。
- 完整 lifetime surface、raw pointer alias safe proof、语言级并发/atomic memory model。

## 13. 测试入口

语言手册中的能力主要由这些入口覆盖：

```text
tests/gtest/frontend/parse/parser_tests.cpp
tests/gtest/frontend/sema/functions_tests.cpp
tests/gtest/integration/sample_suite/
tests/samples/positive/**
tests/samples/negative/**
```

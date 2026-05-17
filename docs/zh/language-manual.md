# Aurex M2 语言语法说明书

日期：2026-05-17
阶段：M2 language-core-no-std
状态：按当前仓库实现编写的使用者语法说明，不描述尚未落地的未来功能。

本文说明当前 Aurex 能写什么、怎么写、哪些地方会被拒绝。语法以 `docs/spec/m2_grammar.md`、`include/aurex/syntax/token.hpp`、`src/parse/*`、`src/sema/*`、`tests/samples/**` 和 `examples/**` 的当前实现为准。

## 1. 最小完整程序

```aurex
module hello;

fn main() -> i32 {
    return 0;
}
```

规则：

- 一个源文件属于一个模块，通常以 `module path;` 开头。
- 可执行程序入口是 `main`。常见形式是 `fn main() -> i32` 或 `fn main() -> void`。
- 语句以 `;` 结束，块用 `{ ... }`。
- 私有普通函数可以推导返回类型；`pub fn`、`extern c fn`、`export c fn` 和函数原型必须显式写返回类型。

## 2. 词法

### 2.1 关键字

当前关键字：

```text
module import as pub priv extern export unsafe
fn struct opaque enum const type impl where
match let var if else for in is while break continue defer return
true false null
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str char
mut
cast ptrcast bitcast sizeof alignof ptraddr ptrat
strptr strblen strvalid strfromutf8 strraw
```

`c` 不是全局关键字。它只在 `extern c`、`export c fn` 和 `extern c fn(...) -> T` 这类 ABI 语法中作为上下文标记；在参数名、局部名、函数名和模块路径中仍可作为普通标识符。

### 2.2 标识符

```text
Identifier = [A-Za-z_][A-Za-z0-9_]*
```

当前实现按 ASCII 标识符处理，不支持 Unicode 标识符。单独的 `_` 是合法标识符，也可在 pattern 中表示通配。

### 2.3 注释

```aurex
// 行注释

/* 块注释 */

/* 块注释可以
   /* 嵌套 */
*/
```

### 2.4 标点和操作符 token

```text
( ) { } [ ] , . ... ; : :: -> => @ ?
+ ++ += - -- -= * *= / /= % %=
& &= | |= ^ ^= ~ !
= == != < <= > >= << <<= >> >>= && ||
```

注意：

- `::`、`++`、`--` 会被词法器识别，但 M2 源码语法不支持这些写法，解析或语义阶段会诊断。
- 泛型使用 `[]`，不使用 `< >`。
- 模块、类型、字段、方法、枚举 case 都使用 `.` 选择，不使用 `::`。

## 3. 字面量

### 3.1 整数

```aurex
0
123
1_000
0xff
0Xff
0b1010
0B1010
1i8
42usize
```

规则：

- `_` 只能放在两个合法数字之间。
- `1_`、`1__2`、`0x_FF`、`0b_1010` 会被拒绝。
- 后缀支持 `i8`、`i16`、`i32`、`i64`、`isize`、`u8`、`u16`、`u32`、`u64`、`usize`。
- 无后缀整数字面量可根据上下文推导并做范围检查。

### 3.2 浮点

```aurex
1.0
0.5
.5
1.
1e3
1.0e-3
1.0f32
1.0f64
```

规则：

- 没有期望类型时默认 `f64`。
- 后缀只支持 `f32` 和 `f64`。
- 整数后缀不能用于浮点，浮点后缀不能用于整数。

### 3.3 字符串、C 字符串、byte、char

```aurex
"hello"        // str，UTF-8 文本
c"hello"       // *const u8，C FFI 字符串，拒绝内部 NUL
r"raw\n"       // str，不解释 escape，可跨行
b"abc\n"       // [N]u8，ASCII byte string
b'a'           // u8 byte literal
'\n'           // char
'λ'            // char，Unicode scalar value
'\u{03BB}'     // char
```

普通字符串和 `char` 支持常见 escape，例如 `\0`、`\n`、`\r`、`\t`、`\\`、`\"` 和 `\u{...}`。byte string 支持简单 byte escape，拒绝非 ASCII raw byte 和 Unicode escape。

### 3.4 布尔和空指针

```aurex
true
false
null
```

`null` 只能在期望类型是 raw pointer 的位置使用。

## 4. 模块、导入和可见性

### 4.1 模块声明

```aurex
module app.main;
```

模块路径用 `.` 分隔。导入其他模块时，编译器会在当前文件相对目录和命令行 `-I` import path 下查找。

### 4.2 导入

```aurex
import common.status;
import common.result as result;
pub import common.metrics as metrics;
priv import internal.detail as detail;
```

规则：

- `import path;` 默认 alias 是最后一个路径段。
- `import path as name;` 显式指定 alias。
- `pub import` 可 re-export 给其他模块。
- `priv import` 明确表示私有导入。
- 当前不支持 glob import、selective import 或 package manifest。
- 未限定名字不会自动搜索导入模块，跨模块成员需要写 `alias.name` 或可见完整模块路径。

### 4.3 可见性

```aurex
pub fn api() -> i32 { return 1; }
priv fn helper() -> i32 { return 2; }

pub struct PublicBox {
    pub value: i32;
    priv secret: i32;
}
```

规则：

- 顶层 item 默认 private。
- struct field 默认 private。
- impl method 默认 private。
- import 默认 private。
- 跨模块 API、字段和方法必须显式 `pub`。
- `export c fn` 是对外 ABI 符号，不能作为 private API。

## 5. 类型语法

### 5.1 基础类型

```text
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
str char
```

`str` 是借用的 UTF-8 文本值，当前 ABI 表示为数据指针加 byte length。它不是拥有型 `String`。

### 5.2 命名类型和泛型类型参数

```aurex
Point
Box[i32]
Pair[i32, bool]
common.status.Health
```

规则：

- 泛型实参使用 `[]`。
- `Box[]` 会被拒绝。
- `Box<i32>` 不是 Aurex 泛型语法。
- 类型路径也使用 `.`。

### 5.3 Raw pointer

```aurex
*const i32
*mut i32
*mut *const u8
```

规则：

- `*` 后必须写 `const` 或 `mut`。
- raw pointer 解引用、raw pointer 字段/索引投影、`ptrcast`、`ptrat` 等操作受 `unsafe` 约束。
- `*i32` 不是合法类型。

### 5.4 Safe reference

```aurex
&i32
&mut i32
&Pair
&mut Pair
```

示例：

```aurex
fn read(value: &i32) -> i32 {
    return *value;
}

fn write(value: &mut i32, replacement: i32) -> void {
    *value = replacement;
}
```

规则：

- `&T` 是共享引用，`&mut T` 是可写引用。
- 当前有 reference 类型和基本可写性检查，但没有 borrow checker、lifetime、alias model。
- `&mut place` 要求 `place` 可写。

### 5.5 数组和 slice

```aurex
[4]i32
[]const i32
[]mut u8
```

数组长度只能是整数字面量，不是 const 表达式：

```aurex
let values: [4]i32 = [1, 2, 3, 4];
let all: []const i32 = values[:];
let middle: []const i32 = values[1:3];
```

规则：

- `[N]T` 是固定长度数组。
- `[]const T` / `[]mut T` 是 borrowed slice。
- `[]T` 会被拒绝，必须写 `const` 或 `mut`。
- `[]mut T` 可赋给 `[]const T`；反过来不行。
- slice 是 fat value：data pointer + length。

### 5.6 Tuple

```aurex
(i32, bool)
(i32,)
```

示例：

```aurex
let pair = (1, true);
let (value, flag) = pair;
```

规则：

- `(A, B)` 是二元 tuple。
- `(A,)` 是一元 tuple。
- `()` 暂不属于 M2。
- 匿名 tuple 不能用 `value.0` 或 `value.first` 访问字段；需要解构或使用 named struct。

### 5.7 函数类型

```aurex
fn(i32, i32) -> i32
unsafe fn(*const i32) -> i32
extern c fn(*const u8, ...) -> i32
unsafe extern c fn(*const u8) -> i32
```

规则：

- 函数类型是非捕获函数指针类型。
- `unsafe fn` 函数值调用也需要 unsafe context。
- `extern c fn` 使用 C ABI。
- variadic `...` 只支持 `extern c`。

## 6. 顶层 item

### 6.1 常量

```aurex
const ANSWER: i32 = 42;
const TEXT: str = "ok";
```

当前 const initializer 支持字面量、部分标量运算、struct/enum 常量等已落地路径，但不是完整 comptime。

### 6.2 类型别名

```aurex
type Count = i32;
type Bytes4 = [4]u8;
type BoxPtr[T] = *mut T;
```

泛型别名可以使用 `where` 约束：

```aurex
type Ptr[T] where T: Sized = *const T;
```

### 6.3 Struct

```aurex
struct Point {
    x: i32;
    pub y: i32;
}

struct Box[T] {
    value: T;
}
```

规则：

- 字段以 `;` 分隔。
- 字段默认 private。
- payload 或字段类型必须是有效 storage type。

### 6.4 Opaque struct

```aurex
extern c {
    opaque struct FILE;
}
```

opaque struct 只能通过指针使用，不能按值构造、取 `sizeof` 或访问字段。

### 6.5 Enum

```aurex
enum OptionI32 {
    some(i32),
    none,
}

enum Status: u8 {
    ok = 0,
    err = 1,
}

enum Result[T] {
    ok(T),
    err(i32),
}
```

规则：

- enum 是 ADT-first：可有 payload，可泛型，可省略底层整数类型。
- 显式 `: u8` / `: u16` 等可用于 C-like/repr enum。
- enum case 是类型成员，不进入普通 value namespace。写 `OptionI32.some(1)`，不要写 `some(1)`。
- payload 中包含数组存储目前会被拒绝。

### 6.6 Function

```aurex
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn exported(value: i32) -> i32 {
    return value + 1;
}

fn inferred(value: i32) {
    return value + 1;
}
```

规则：

- private 普通函数可推导返回类型。
- public、extern C、export C、prototype 必须显式返回类型。
- 参数必须写类型。
- 函数原型用 `;` 结束，定义用 block。

### 6.7 Extern C 和 Export C

```aurex
extern c {
    fn strlen(text: *const u8) -> usize @name("strlen");
    fn snprintf(buffer: *mut u8, size: usize, format: *const u8, ...) -> i32 @name("snprintf");
}

export c fn aurex_add(a: i32, b: i32) -> i32 @name("aurex_add") {
    return a + b;
}
```

规则：

- `@name("...")` 指定 ABI symbol。
- variadic 只支持 `extern c` 函数。
- C ABI 函数不支持泛型。

### 6.8 Impl 和方法

```aurex
struct Counter {
    value: i32;
}

impl Counter {
    pub fn new(value: i32) -> Counter {
        return Counter { value: value };
    }

    pub fn add(self: &mut Counter, delta: i32) -> i32 {
        self.value = self.value + delta;
        return self.value;
    }
}
```

规则：

- impl target 必须解析为 named struct、enum 或 opaque struct。
- receiver 参数必须是第一个参数，并命名为 `self`。
- 泛型 impl 支持 `impl[T] Box[T] { ... }` 这种 owner 泛型。
- method-local 独立泛型目前不支持。

## 7. 语句

### 7.1 局部变量

```aurex
let value: i32 = 1;
let inferred = value + 1;
var count: i32 = 0;
```

规则：

- `let` 不可重新赋值。
- `var` 可重新赋值。
- 类型可省略，由 initializer 推导。

### 7.2 Pattern 局部绑定和 let-else

```aurex
let (left, right) = pair;

let .some(value) = option else {
    return 0;
};
```

规则：

- 没有 `else` 时，pattern 必须 irrefutable。
- 有 `else` 时可用 refutable pattern，但 `else` block 不能 fall through。

### 7.3 赋值和复合赋值

```aurex
value = 1;
value += 2;
value <<= 1;
```

支持：

```text
= += -= *= /= %= <<= >>= &= ^= |=
```

赋值是语句，不是表达式。

### 7.4 If statement

```aurex
if value > 0 {
    return 1;
} else if value == 0 {
    return 0;
} else {
    return -1;
}
```

statement-form `if` 可以没有 `else`。

### 7.5 While

```aurex
while i < 10 {
    i += 1;
}
```

Pattern condition 也支持：

```aurex
while current is .next(value) {
    current = Step.next(value - 1);
}
```

### 7.6 C-style for

```aurex
for var i: i32 = 0; i < 10; i += 1 {
    sum += i;
}

for ; ; {
    break;
}
```

### 7.7 Range for

```aurex
for i in range(4) {
    sum += i;
}

for i in range(2, 5) {
    sum += i;
}

for i in range(1, 6, 2) {
    sum += i;
}
```

规则：

- `range(end)` 从 0 到 end，不含 end。
- `range(start, end)` 不含 end。
- `range(start, end, step)` 使用显式步长。
- 这不是通用 iterator 语法；`for x in values {}` 不属于 M2。

### 7.8 Break、continue、return、defer

```aurex
break;
continue;
return;
return value;
defer cleanup();
```

`defer` 后面必须是 call expression。

### 7.9 Unsafe block

```aurex
unsafe {
    *ptr = *ptr + 1;
}

let value = unsafe {
    *ptr
};
```

unsafe block 可作为语句，也可作为带 tail expression 的表达式。

## 8. 表达式

### 8.1 字面量、名字、调用

```aurex
let a = 1;
let b = add(a, 2);
let c = module_name.function_name(3);
```

函数名可作为非捕获函数指针值：

```aurex
type Op = fn(i32) -> i32;
let op: Op = add_one;
let value = op(41);
```

### 8.2 泛型调用

```aurex
fn id[T](value: T) -> T {
    return value;
}

let value = id[i32](1);
```

显式泛型调用使用 `name[T](...)` 或 `module.name[T](...)`。`name::[T](...)` 不支持。

### 8.3 字段、方法、索引、slice

```aurex
point.x
counter.add(1)
values[index]
values[start:end]
values[:end]
values[start:]
values[:]
text[start:end]
```

规则：

- array/slice 索引返回元素。
- raw pointer 字段/索引投影要求 unsafe context。
- `str` 不支持 `text[i]` 单字节索引；使用 `text[l:r]` 做 checked byte range slicing。
- `str` slice 按 byte offset 检查边界和 UTF-8 code point boundary；失败返回空 `str`。

### 8.4 Array、tuple、struct literal

```aurex
let values = [1, 2, 3];
let zeroes: [4]u8 = [0; 4];
let pair = (1, true);
let single = (1,);
let point = Point { x: 1, y: 2 };
let box = Box[i32] { value: 42 };
```

### 8.5 Block expression

```aurex
let value = {
    let base = 40;
    base + 2
};
```

最后一个无分号表达式是 block 的值。空 block 或只有语句的 block 在需要值的位置会被拒绝。

### 8.6 If expression

```aurex
let sign = if value < 0 {
    -1
} else {
    1
};
```

expression-form `if` 必须有 `else`，两个分支类型必须兼容。

### 8.7 Match expression

```aurex
let score = match choice {
    .yes => 1,
    .no => 0,
};
```

match 支持 enum、bool/integer literal、tuple/struct/array/slice 等 pattern。语义分析会做一定范围的 exhaustiveness 和 unreachable arm 检查。

### 8.8 Try expression

```aurex
let parsed = parse_digit(value)?;
```

`?` 只对结构上识别为 result-like 或 option-like 的 enum 生效：

- result-like：恰好有 `ok(payload)` 和 `err(payload)`。
- option-like：恰好有 `some(payload)` 和 payload-free `none`。

enum 名字不特殊，形状才重要。

## 9. 操作符

常用二元操作：

```text
* / %
+ -
<< >>
< <= > >=
== !=
&
^
|
&&
||
```

常用一元操作：

```text
-x
!flag
~bits
&place
&mut place
*ptr_or_ref
```

规则要点：

- 算术和位运算要求相应数值类型。
- 逻辑运算要求 `bool`。
- `&place` 产生 safe reference，除少数 legacy raw-pointer context 兼容路径外，不等同于 raw pointer。
- `*raw_pointer` 要求 unsafe；`*reference` 是 safe projection。
- 不支持 `++` / `--`。

## 10. 内建函数和内建操作

当前内建函数不是普通库函数，而是 parser/semantic/IR/backend 共同支持的语言内建。建议把它们当作语法的一部分。

| 写法 | 安全性 | 结果 | 说明 |
| --- | --- | --- | --- |
| `cast[T](x)` | safe | `T` | 数值/布尔转换。支持整数、浮点、bool 之间转换。 |
| `ptrcast[T](p)` | unsafe | `T` | raw pointer / reference 到 raw pointer 的指针转换，`T` 必须是 pointer。 |
| `bitcast[T](x)` | unsafe | `T` | 等 ABI size 的 bit reinterpret。标量对标量、pointer 对 pointer。 |
| `sizeof[T]` | safe | `usize` | 查询有效 storage type 的 ABI size。 |
| `alignof[T]` | safe | `usize` | 查询有效 storage type 的 ABI alignment。 |
| `ptraddr(p_or_ref)` | safe | `usize` | 读取 raw pointer 或 reference 的地址值。 |
| `ptrat[T](addr)` | unsafe | `T` | 从整数地址构造 raw pointer，`T` 必须是 pointer。 |
| `strptr(s)` | safe | `*const u8` | 返回 `str` 底层 byte pointer。 |
| `strblen(s)` | safe | `usize` | 返回 `str` byte length。 |
| `strvalid(bytes)` | safe | `bool` | 检查 `[]const u8` 或 `[]mut u8` 是否是有效 UTF-8。 |
| `strfromutf8(bytes)` | safe | `str` | 成功时借用 byte slice 作为 `str`；失败返回空 `str`。 |
| `strraw(data, len)` | unsafe | `str` | 不检查 UTF-8，从 `*const u8` 和 length 构造 `str`。 |

示例：

```aurex
fn from_bytes(bytes: []const u8) -> str {
    if !strvalid(bytes) {
        return "";
    }
    return strfromutf8(bytes);
}

fn raw_roundtrip(text_value: str) -> str {
    return unsafe { strraw(strptr(text_value), strblen(text_value)) };
}

fn pointer_roundtrip(value: &mut i32) -> *mut i32 {
    let address: usize = ptraddr(value);
    return unsafe { ptrat[*mut i32](address) };
}
```

注意：

- `strfromutf8` 失败返回空 `str`，所以需要区分“合法空输入”和“非法输入”时先调用 `strvalid`。
- `strraw` 不做 UTF-8 检查，必须包在 `unsafe` 内。
- `sizeof[void]`、`sizeof[opaque]`、`sizeof` 非 storage type 会被拒绝。
- `ptraddr(1)` 会被拒绝，因为参数不是 pointer/reference。

## 11. Pattern

### 11.1 基础 pattern

```aurex
_
name
42
true
false
const ANSWER
```

规则：

- 裸 identifier pattern 永远是绑定，不是 enum case。
- 常量 pattern 使用 `const NAME`，当前主要用于整数和 bool 常量。

### 11.2 Enum case pattern

```aurex
match option {
    .some(value) => value,
    .none => 0,
};

match option {
    OptionI32.some(value) => value,
    OptionI32.none => 0,
};
```

规则：

- 已知被匹配 enum 类型时可用 `.case`。
- 显式写法是 `Type.case` 或 `Type[Args].case`。
- 不支持裸 `some(value)`。

### 11.3 Tuple、struct、slice pattern

```aurex
let (left, right) = pair;

match point {
    Point { x, y } => x + y,
};

match values {
    [head, .., tail] => head + tail,
    [single] => single,
    _ => 0,
};
```

规则：

- struct pattern 支持字段 shorthand 和 `field: pattern`。
- slice pattern 最多一个 `..`。
- 动态 slice 的 match 通常需要 `_` 或 `[..]` 这类覆盖剩余长度的 arm。

### 11.4 Or-pattern 和 guard

```aurex
match value {
    1 | 2 => true,
    _ => false,
};

match packet {
    .int(value) if value > 0 => value,
    .int(value) => -value,
    .none => 0,
};
```

Or-pattern 每个分支若绑定名字，必须绑定相同名字和类型。

## 12. 泛型和 where

```aurex
fn id[T](value: T) -> T {
    return value;
}

struct Box[T] {
    value: T;
}

enum Option[T] {
    some(T),
    none,
}

impl[T] Box[T] {
    fn get(self: &Box[T]) -> T {
        return self.value;
    }
}
```

`where` 当前支持内建非资源 capability：

```aurex
fn same[T](left: T, right: T) -> bool where T: Eq {
    return left == right;
}
```

支持：

- `Sized`
- `Eq`
- `Ord`
- `Hash`

暂不支持：

- 用户自定义 trait
- associated type
- const generic
- trait object
- `Copy` / `Drop` 资源能力
- method-local 独立泛型
- `<T>` 风格泛型

## 13. Unsafe 边界

需要 unsafe context 的操作：

- raw pointer 解引用：`*ptr`
- raw pointer 字段/索引投影：`ptr.field`、`ptr[index]`
- `ptrcast[T](p)`
- `bitcast[T](x)`
- `ptrat[T](addr)`
- `strraw(data, len)`
- 调用 `unsafe fn` 或 `unsafe fn(...) -> T` 函数值

不需要 unsafe 的操作：

- `&place`
- `&mut place`
- safe reference 解引用：`*reference`
- `ptraddr(pointer_or_reference)`
- `strptr(s)`
- `strblen(s)`
- `strvalid(bytes)`
- `strfromutf8(bytes)`
- `sizeof[T]`
- `alignof[T]`
- `cast[T](x)`

示例：

```aurex
unsafe fn read_raw(value: *const i32) -> i32 {
    return *value;
}

fn read(value: &i32) -> i32 {
    return *value;
}

fn call_raw(value: *const i32) -> i32 {
    return unsafe { read_raw(value) };
}
```

## 14. 当前明确不支持的语法

```aurex
*i32                 // pointer 必须写 *const / *mut
[]i32                // slice 必须写 []const / []mut
[N]i32               // 数组长度不是标识符
[1 + 2]i32           // 数组长度不是 const expr
Box[]                // 泛型实参不能为空
Box<i32>             // 泛型不用 <>
()
let () = value;
foo::bar             // selector 不用 ::
id::[i32](1)
fn add[T: Add](a: T, b: T) -> T { return a; }
for value in values {}
text[0]              // str 不支持单 index
value++              // 不支持自增
```

## 15. 可直接运行的正则库案例

仓库现在包含一个用当前 Aurex 语言写的多模块编译型正则库。它不是 `text`
子模块，而是独立的 `regex` 模块目录：

- 入口模块：`examples/libs/regex/api.ax`
- 内部模块：`regex.ascii`、`regex.alloc`、`regex.types`、`regex.program`、`regex.parser`、`regex.engine`
- 演示程序：`examples/regex_demo.ax`
- 详细语法/API/模块说明：`docs/zh/regex.md`

编译方式：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
```

最小调用示例：

```aurex
module regex_minimal;

import regex.api as regex;

fn main() -> i32 {
    if !regex.fullmatch("^(cat|dog)s?$", "dogs") {
        return 1;
    }
    if !regex.search("\\w+@\\w+\\.com", "mail: team@example.com;") {
        return 2;
    }

    var compiled: regex.Regex = regex.compile("^\\d{2,4}$");
    defer regex.destroy(&mut compiled);
    if !regex.is_valid(&compiled) {
        return 3 + regex.status_code(compiled.status);
    }
    let result: regex.MatchResult = regex.fullmatch_compiled(&compiled, "2026");
    if !result.ok() {
        return 10 + regex.status_code(result.status);
    }
    return 0;
}
```

这个库用来验证当前语言的实际工程表达能力：

- 模块和 import：`import regex.api as regex;`
- 多模块拆分：API facade、ASCII 常量、FFI 分配、程序表示、parser、VM engine、类型定义。
- public/private API：外部只需要 `regex.api`，内部 helper 保持 private。
- type alias：`pub type Regex = t.Regex`、`pub type MatchResult = t.MatchResult`。
- enum 和 struct：`RegexStatus`、`Regex`、`MatchResult`、`State`、`ClassRange` 等。
- impl 方法：`Regex.valid`、`MatchResult.ok`。
- FFI 和 unsafe：通过 `calloc/free` 分配状态表、字符类表和 VM 工作区。
- raw pointer 字段/索引投影：编译程序和 VM 列表都用指针数组访问。
- `defer`：示例和 API convenience 函数用它保证释放。
- match：状态码和演示结果判定使用 enum pattern。
- 函数类型：演示中使用 `type Matcher = fn(str, str) -> bool`。
- `str` 内建：`strblen`、`strptr` 参与 byte 级解析和匹配。

当前正则语法是一个明确的 ASCII byte 级语法，详细定义见
`docs/zh/regex.md`。概要如下：

- 字面量：普通 byte 字面量按自身匹配。
- 转义：`\n`、`\r`、`\t`、`\0`、`\x` 形式的转义字面量。
- 预定义类：`\d`、`\D`、`\w`、`\W`、`\s`、`\S`。
- 通配：`.` 匹配任意一个 byte。
- 锚点：`^` 匹配输入开头，`$` 匹配输入结尾。
- 字符类：`[abc]`、`[^abc]`、`[a-z]`，类内支持 `\d`、`\w`、`\s`。
- 分组：`(...)` 用于组合表达式，不产生捕获。
- 交替：`a|b`，支持在分组内使用。
- 量词：`*`、`+`、`?`、`{m}`、`{m,n}`、`{m,}`。

当前正则库的有意边界：

- 不支持捕获组、反向引用、lookaround、替换模板、命名组。
- 不支持 Unicode scalar、grapheme、Unicode property；匹配单位是 `str` 的 UTF-8 byte。
- 不支持懒惰量词或 possessive 量词。
- `{m,n}` 展开上限是实现常量，超出返回 `RegexStatus.repeat_too_large`。
- 没有标准库 RAII，`compile` 得到的 `Regex` 必须手动 `destroy` 或 `defer destroy`。

## 16. 内建函数测试位置

内建函数集中运行样例位于：

```text
tests/samples/positive/core/builtins.ax
```

它覆盖：

```text
cast ptrcast bitcast sizeof alignof ptraddr ptrat
strptr strblen strvalid strfromutf8 strraw
```

集成测试会把该样例编译为可执行文件并运行：

```text
tests/gtest/integration/sample_suite_tests.cpp
```

正则库案例也有 examples 集成测试：

```text
tests/gtest/integration/examples_tests.cpp
```

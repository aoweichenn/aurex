# Aurex 当前语言特性清单

日期：2026-06-18

本文只记录当前仓库真实支持的语言和编译器能力。历史阶段、旧路线和已废弃设计不在本文保存。

## 总体状态

Aurex 当前已经具备一个无标准库依赖的小型系统语言核心：

- 模块、import、可见性、re-export。
- 函数、method、C FFI、默认参数、命名参数。
- 标量、raw pointer、safe reference、array、slice、tuple、`str`、struct、opaque struct、ADT enum、type alias。
- 泛型函数、泛型类型、泛型 impl、method-local 泛型、typed scalar const generic check-only 子集。
- `where` capability、nominal static trait、显式 trait impl、associated type、associated-type equality、trait default method。
- borrowed dyn trait、borrowed dyn composition、supertrait upcast和 checked vtable dispatch。
- `let` / `var`、pattern、let-else、block expression、if/match expression、`while`、C-style `for`、counted `range(...)`、array/slice value for-in、protocol iterator for-in、`defer`、`?`、`unsafe`。
- compiler-owned `Copy` capability、move/reinit 检查、cleanup/drop flag、borrow summary、local loan checking 和 lifetime/origin 诊断。
- Aurex IR、IR verifier/pass pipeline、LLVM backend 和 clang native 输出。

## 当前语法库存

### 文件结构

```aurex
module app.main;

import app.util;

fn main() -> i32 {
    return 0;
}
```

模块文件必须声明 `module`。import 查找只使用导入者目录和显式 `-I`。

### 绑定和控制流

```aurex
let value: i32 = 1;
var total: i32 = 0;

if value > 0 {
    total += value;
}

while total < 10 {
    total += 1;
}

for i in range(0, 10, 2) {
    total += i;
}

let values: [3]i32 = [1, 2, 3];
for item in values {
    total += item;
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

`for item in expr` 当前支持三类来源：

- array/slice 按值迭代。每轮从元素地址 load，元素类型必须满足 `Copy`，不会 move array/slice 本身。
- 表达式本身是 protocol iterator。iterator 必须提供 `has_next(self: &mut Iterator) -> bool` 和 `next(self: &mut Iterator) -> Item`。
- 表达式提供 `iter()`，且 `iter()` 返回 protocol iterator。`iter()` receiver 可以是 by-value、`&self` 或 `&mut self`，按普通 receiver 规则匹配。

protocol iterator 的 `Item` 由 `next()` 按值返回，不要求 `Copy`。协议方法可以来自 inherent method，也可以来自静态 trait dispatch；generic `where T: Trait` 下的静态 trait dispatch 已进入 IR lowering。dyn trait vtable-slot dispatch 暂不作为 for-in protocol 来源。

mutable/reference item iteration、str iteration、range value 和标准库 iterable adapter 仍未进入当前语言表面。

### 类型

```aurex
i32
usize
*const i32
*mut i32
&i32
&mut i32
[4]i32
[]i32
[]mut i32
(i32, bool)
fn(i32) -> i32
str
```

slice 当前使用 `[]T` / `[]mut T`。旧 `[]const T` 不属于当前语言表面。

### 泛型

泛型统一使用 C++ 风格尖括号：

```aurex
struct Box<T> {
    value: T;
}

fn id<T>(value: T) -> T {
    return value;
}

let value: i32 = id<i32>(1);
```

`[]` 不作为泛型语法。`sizeof<T>()`、`alignof<T>()`、`ptrat<T>(addr)`、`ptrcast<T>(ptr)` 和 `bitcast<T>(value)` 使用同一套 type argument 表面。

### 函数和闭包

```aurex
fn add(a: i32, b: i32 = 1) -> i32 {
    return a + b;
}

fn main() -> i32 {
    let base: i32 = 2;
    let by_value = [base](value: i32) -> i32 => value + base;
    let by_ref = [&base](value: i32) -> i32 => value + base;
    let by_init = [captured = base + 1](value: i32) -> i32 => value + captured;
    let by_default = [=](value: i32) -> i32 => value + base;
    return by_value(1) + by_ref(1) + by_init(1) + by_default(1);
}
```

lambda capture-list 当前支持：

- `[]`
- `[x]`
- `[&x]`
- `[&mut x]`
- `[=]`
- `[&]`
- `[=, &x]`
- `[&, x]`
- `[x = expr]`
- `[x = move expr]`
- `[move x]`

普通值捕获仍要求源值满足 `Copy`。init-capture 在闭包创建点求值并进入环境；move capture 会消费源值，捕获后继续使用源值会触发 move 诊断。当前 closure trait、borrowed-view capture 和完整 escaping lifetime 仍未进入语言表面。

### Trait 和 dyn view

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;
}

struct Cell {
    value: i32;
}

impl Source for Cell {
    type Item = i32;

    fn get(self: &Cell) -> i32 {
        return self.value;
    }
}

fn read<T>(value: &T) -> i32 where T: Source<Item = i32> {
    return value.get();
}
```

当前支持 nominal static trait、显式 impl、trait predicate、associated type、associated-type equality、default method body、borrowed dyn trait view 和 borrowed dyn composition/supertrait upcast。owning dyn、`Box<dyn Trait>`、allocator API 和 trait-object Drop runtime 不属于当前语言能力。

### 内建和 unsafe

当前低层内建保留短名字：

```aurex
value as i32
sizeof<i32>()
alignof<i32>()
ptraddr(&value)
unsafe { ptrat<*const i32>(addr) }
unsafe { ptrcast<*const u8>(ptr) }
unsafe { bitcast<u32>(bits) }
strvalid(bytes)
strfromutf8(bytes)
unsafe { strraw(data, len) }
```

本版本不引入新的 `intrinsic.*` 表面。`ptrat`、`ptrcast`、`bitcast` 和 `strraw` 只能在 unsafe context 中使用。

### 宏表面

当前 parser/AST/query 可以识别宏声明、macro call 和 user derive target schema 的 admission-only 表面，但不执行展开、不生成用户代码、不消费 generated token buffer，也不修改 AST。完整 macro/proc-macro/derive lowering 后续单独设计。

## 当前测试入口

主要测试位置：

- `tests/gtest/frontend/parse/parser_tests.cpp`
- `tests/gtest/frontend/sema/functions_tests.cpp`
- `tests/gtest/integration/sample_suite_tests.cpp`
- `tests/samples/positive/**`
- `tests/samples/negative/**`

本轮语法相关新增样例：

- `tests/samples/positive/functions/lambda_default_capture.ax`
- `tests/samples/negative/functions/lambda_capture_default_duplicate.ax`
- `tests/samples/negative/functions/lambda_capture_default_order.ax`
- `tests/samples/negative/functions/lambda_capture_default_redundant.ax`
- `tests/samples/positive/control_flow/for_in_array_slice.ax`
- `tests/samples/negative/control_flow/for_in_non_copy_element.ax`

## 当前非目标

- 标准库 API 和拥有型容器。
- range value、str iteration、mutable/reference item iteration 和标准库 iterable adapter。
- 低层 builtin 长命名空间或新 intrinsic 表面。
- owning dyn runtime。
- 完整宏展开和 proc-macro。
- closure trait、borrowed-view capture 和完整 escaping lifetime。
- 完整 lifetime surface 和 raw pointer alias safe proof。

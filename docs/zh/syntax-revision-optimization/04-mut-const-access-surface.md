# Mut / Const Access Surface：把可写权限从深不可变里拆开

日期：2026-06-13
状态：语法修正优化第四手改动设计稿；建议硬切，不保留成功兼容路径
关联问题：`mut` / `const` 位置不统一

本文固定 Aurex `mut` / `const` 语法表面的第一阶段修正方向。核心结论不是“`mut` 放前面还是放后面”，而是先把概念拆干净：Aurex 当前已经有 `let` / `var`、`&T` / `&mut T`、`*const T` / `*mut T`、`[]const T` / `[]mut T` 四组表面，如果继续把它们都叫“const/mutability”，用户和实现都会把 binding、view、object immutability 和 aliasing 证明混成一坨。

这套设计必须保持一个底线：Aurex 不做空格敏感语法。`&mut T` 和 `& mut T` 在 token 层只要能组成同一条 grammar，就不应因为空格改变语义；`[]T`、`[]mut T` 也是普通 type constructor 规则，不允许靠排版制造例外。

## 当前现状

当前类型表面：

```aurex
*const T
*mut T

&T
&mut T

[]const T
[]mut T
```

当前 binding 表面：

```aurex
let x: T = value;
var y: T = value;
```

当前实现入口：

- `src/frontend/parse/grammar/parser_type.cpp`
  - `*` 后必须写 `mut` 或 `const`。
  - `&` 后默认 shared/read-only view，可选 `mut`。
  - `[]` 后必须写 `mut` 或 `const`。
  - pointer/reference/slice type constructor 会先入栈，再从右向左套到元素类型上。
- `include/aurex/frontend/syntax/ast/type_nodes.hpp`
  - `PointerTypePayload`、`ReferenceTypePayload`、`SliceTypePayload` 都保存 `PointerMutability`。
- `include/aurex/frontend/sema/type.hpp`
  - `PointerMutability` 只有 `mut` 和 `const_` 两态。
  - `TypeInfo` 对 pointer、reference、slice 分别保存可写权限。
- `src/frontend/sema/types/type.cpp`
  - 当前 display name 会输出 `*mut T`、`*const T`、`&mut T`、`&T`、`[]mut T`、`[]const T`。

这说明一个重要事实：实现里没有深不可变对象类型，也没有 C++ 式完整 cv-qualified type 系统。`const/mut` 现在实际表达的是“通过这个 pointer/reference/slice view 能不能写”。

## 必须拆开的四个概念

### 1. Binding 是否可重绑定

`let` / `var` 管的是名字本身能不能重新赋值：

```aurex
let x: i32 = 1; // x 不能被重新赋值
var y: i32 = 1; // y 可以被重新赋值
```

这和 `x` 指向的对象是否深不可变不是一回事。把 binding 不可变叫成 object const，是非常危险的误导。

### 2. View 是否允许写入

`&T`、`&mut T`、slice、raw pointer 管的是“通过这条访问路径能不能写”：

```aurex
let shared: &i32 = &count;
let writable: &mut i32 = &mut count;
```

`shared` 不能写，不代表原始 storage 永久不可变；`writable` 能写，也不代表 binding 自己一定能被重绑。

### 3. Object / deep immutability

深不可变表示对象自身以及可达对象在某种语义边界内都不能被修改。这不是当前 Aurex 已经建模的东西。

如果未来要做深不可变，需要单独设计：

- 对 struct field、array element、slice element、owned resource 的传递规则。
- 对 interior mutability / unsafe / FFI 的边界规则。
- 对 generic type parameter 的约束传播。
- 对 drop、move、aliasing、borrow summary 的交互。

第一阶段不要把 `const` 偷偷解释成深不可变，否则用户以为拿到了强保证，编译器实际给不了。

### 4. Alias uniqueness / exclusivity

`&mut T` 不只是“能写”，还通常意味着可写 view 在 safe 语义内需要排他性证明。Aurex 当前 borrow checker 已经有 shared/mutable loan 规则，所以这个概念应继续放在 safe reference / borrowed view 系统里，而不是塞进一个泛泛的 `const` 关键字。

## 其他语言和学术资料给出的结论

这里不照搬任何语言，只看它们把问题切在哪里。

### C / C++

C++ 的 `const` 很强，也很乱。它能表达多层 pointer constness：

```cpp
T const* p;       // pointer to const T
T* const p;       // const pointer to mutable T
T const* const p; // const pointer to const T
```

优点是表达力高，尤其适合老式 ABI、裸指针和库边界；缺点是阅读方向和修饰层级很难稳定，初学者长期会混淆“pointer 本身 const”和“pointee const”。这不是 Aurex 应该复制的用户表面。

对 Aurex 的启发：

- raw pointer 可以保留 `*const T` / `*mut T`，因为 unsafe/FFI 边界需要显式噪音。
- 不要引入 `T const`、`const T`、`* const` 这种完整 cv 组合，除非类型系统真的准备好支持 pointer object constness、pointee constness 和多层 cv 传播。

### Rust

Rust 把 binding mutability 和 borrowed access 分开：`let` 默认不可重绑，`let mut` 允许重绑或直接写本地 storage；`&T` 是 shared borrow，`&mut T` 是可写且排他的 borrow；raw pointer 又有 `*const T` / `*mut T`。

值得吸收的是概念切分，不是照抄语法。Aurex 已经选择 `let` / `var`，所以没必要改成 `let mut`。但 `&T` / `&mut T` 这组 safe reference 表面本身是短、清楚、和 borrow checker 语义贴合的。

对 Aurex 的启发：

- safe reference 保持 `&T` / `&mut T`。
- shared view 默认短写，writable view 显式写 `mut`。
- raw pointer 可以继续显式区分 `*const T` / `*mut T`。

### Zig

Zig 明确区分 `const` / `var` binding 和 pointer 指向数据是否可写。它的一个重要经验是：名字是否能重新赋值，不能替代通过 pointer/slice view 的写权限。

对 Aurex 的启发：

- `let` / `var` 的分工应写进规范和诊断。
- `let s: []mut u8` 应表示 binding 不能重绑，但可以通过这个 slice view 写元素。
- `var s: []u8` 应表示 binding 可以重绑到另一个 shared slice，但不能通过这个 view 写元素。

### D

D 设计了 `const`、`immutable`、`shared`，其中 `immutable` 更接近可共享的深不可变，`const` 更像只读 view。这说明语义空间远大于一个 `const` 单词。

对 Aurex 的启发：

- 如果未来要做 deep immutable，应该另起语义层，例如 `immutable`、`frozen` 或 capability，而不是复用当前 view-level `const`。
- 当前阶段不要把 slice/reference/pointer 的 read-only view 文档写成对象不可变。

### Swift / Kotlin

Swift 的 `let` / `var`、Kotlin 的 `val` / `var` 都说明同一个事实：binding 不可重绑不等于对象深不可变。集合、类对象、属性、方法是否能改，通常另有类型和 API 规则。

对 Aurex 的启发：

- `let` / `var` 是绑定层语法，不是类型 qualifier。
- `let` 不应该被解释成“这个值递归不可变”。

### C#

C# 有 `readonly`、`in`、`ref readonly` 等一组机制，用来区分字段写入限制、只读引用参数、返回只读引用等。这能表达很多低层场景，但关键字和规则会迅速变复杂。

对 Aurex 的启发：

- Aurex 第一阶段不要扩展成一套 `readonly/ref readonly/in` 大系统。
- 先把 view access 权限和 binding 重绑定讲清楚，后续再根据真实需求加能力。

### Pony / reference capabilities

Pony 用 `iso`、`trn`、`ref`、`val`、`box`、`tag` 这类 reference capability 精细表达 aliasing、读写和共享能力。这证明“能不能写”和“有没有别名”本来就是更大的能力系统。

对 Aurex 的启发：

- 如果未来 Aurex 要做并发安全、对象冻结、唯一所有权和能力传播，可以单独设计 capability。
- 当前 `mut/const` 不应该伪装成完整 capability system。

### 学术界：reference immutability

Javari、IGJ 等 reference immutability 研究反复强调一个区分：

- immutable reference / read-only view：通过这条引用不能改。
- immutable object：对象自身在某个语义范围内不能被改。
- ownership / aliasing：有没有其他路径能改，以及能否证明没有。

这正好对应 Aurex 当前要避开的坑：如果 `[]const T` 被用户读成“元素对象是 const”，但实际只是“这个 slice view 不给写权限”，语法就在撒谎。

## 当前问题

### 1. `const` 这个词太容易被误读成深不可变

下面这个类型：

```aurex
[]const u8
```

直觉上像“slice of const u8”，也像“元素是 const”。但当前实际语义只是“这个 borrowed slice view 不能写元素”。底层 storage 可能来自 mutable buffer，也可能存在其他 writable view，只要 borrow checker 允许。

所以文档和诊断不能继续粗暴说“const slice”或“const object”。应该说：

- shared slice view
- read-only slice view
- writable slice view
- shared reference
- writable reference

### 2. `[]const T` 对常用只读 slice 太吵

只读 slice 是函数参数和字符串/byte API 的高频形态：

```aurex
fn parse(bytes: []const u8) -> Result { ... }
fn write_all(out: []mut u8) -> usize { ... }
```

`[]const T` 的问题不是不能解释，而是高频代码噪音太重。safe reference 已经选择了 `&T` 默认 shared、`&mut T` 显式 writable。slice 也是 safe borrowed view，默认 shared 才更自然：

```aurex
fn parse(bytes: []u8) -> Result { ... }
fn write_all(out: []mut u8) -> usize { ... }
```

这里 `[]u8` 不是“owned dynamic array”，而是 borrowed shared slice view。Aurex 当前没有拥有型 dynamic array 语法，所以这个读法不会和已有 owned container 冲突。

### 3. Pointer、reference、slice 的视觉模型不统一

当前三组表面：

```aurex
*const T
*mut T

&T
&mut T

[]const T
[]mut T
```

reference 的 shared 默认最短，slice 却要求 shared 也显式写 `const`。这导致用户必须记两个不同默认策略：

- reference：不写就是 shared。
- slice：不写是错误。

没有必要。slice 和 reference 都是 safe borrowed view，应该有同样默认策略：shared/read-only view 是短写，writable view 写 `mut`。

### 4. `let` / `var` 和 view mutability 需要硬性讲清楚

必须允许并解释下面两种组合：

```aurex
let out: []mut u8 = buffer[:];
out[0] = 1;        // ok：binding 不可重绑，但 view 可写
out = other[:];    // error：let binding 不可重绑

var input: []u8 = first[:];
input = second[:]; // ok：var binding 可重绑
input[0] = 1;      // error：shared slice view 不可写
```

这不是边角，是用户理解 `let` / `var` / `mut` 的核心例子。

## 决策

### 决策 1：语言概念改名为 shared / writable access

规范、文档、诊断中的用户概念改成：

```text
shared view / read-only view
writable view
shared reference
writable reference
shared slice
writable slice
raw read-only pointer
raw writable pointer
```

实现内部暂时仍可继续使用 `PointerMutability`，因为这是机械命名问题，不要求第一阶段大重构。但用户表面不要把它宣传成“对象 constness”。

### 决策 2：保留 `let` / `var`

不改成 `let mut`。Aurex 已经有清楚的二分：

```aurex
let value: i32 = 1;
var count: i32 = 0;
```

规则：

- `let`：binding 不可重绑。
- `var`：binding 可重绑。
- binding 是否可重绑，不决定通过该值内部 view 能不能写。

### 决策 3：保留 safe reference 表面

保留：

```aurex
&T
&mut T
&[origin] T
&mut [origin] T
```

理由：

- `&T` 作为 shared view 短写非常高频。
- `&mut T` 显式表达 writable/exclusive safe view。
- 和当前 borrow checker 的 shared/mutable loan 模型一致。
- 不需要引入新的关键字。

### 决策 4：raw pointer 保持显式 `*const T` / `*mut T`

保留：

```aurex
*const T
*mut T
```

理由：

- raw pointer 是 unsafe/FFI 边界，显式噪音是好事。
- `*T` 如果默认 shared，会让 unsafe 代码过于安静。
- `*const T` / `*mut T` 对系统语言用户和 C ABI 边界是熟悉表面。
- 第一阶段不引入 pointer object 自身 constness，所以不支持 `* const T`、`T const*`、`*T const` 等 C++ cv 组合。

这里的 `const` 仍解释为 raw read-only pointer view，不解释为深不可变对象。

### 决策 5：slice shared 形态改成 `[]T`

目标 slice 表面：

```aurex
[]T       // shared/read-only borrowed slice view
[]mut T   // writable borrowed slice view
```

迁移前：

```aurex
[]const T
[]mut T
```

迁移后：

```aurex
[]T
[]mut T
```

示例：

```aurex
fn parse(bytes: []u8) -> bool {
    return bytes.len() != 0usize;
}

fn fill(out: []mut u8, value: u8) -> void {
    out[0] = value;
}
```

`[]T` 必须被规范明确写成 borrowed slice，不是 owned array，不是 dynamic array，不是 `Vec<T>`。

### 决策 6：`[]const T` 作为 legacy error，不进入成功路径

开发期语言还没稳定，第一阶段就应该硬切：

```aurex
let old: []const u8 = data[:]; // error: write []u8
let new: []u8 = data[:];       // target
```

`[]const T` 不应该作为别名、warning path 或第二套成功语法存在。原因是它会继续把用户引向“元素是 const”的读法，也会让同一个概念有两种写法。parser 可以识别这个旧 token 序列并给出定向诊断，但不能把它构造成合法 shared slice type 继续通过编译。

## 最终表面表

| 写法 | 概念 | 能否重绑 binding | 能否通过该 view 写 |
|---|---|---:|---:|
| `let x: T` | 不可重绑 binding | 否 | 由 `T` 和操作决定 |
| `var x: T` | 可重绑 binding | 是 | 由 `T` 和操作决定 |
| `&T` | shared safe reference view | 不适用 | 否 |
| `&mut T` | writable/exclusive safe reference view | 不适用 | 是 |
| `[]T` | shared borrowed slice view | 不适用 | 否 |
| `[]mut T` | writable borrowed slice view | 不适用 | 是 |
| `*const T` | raw read-only pointer view | 不适用 | 否 |
| `*mut T` | raw writable pointer view | 不适用 | 是 |

关键例子：

```aurex
let value: i32 = 1;
var count: i32 = 0;

let shared_ref: &i32 = &count;
let writable_ref: &mut i32 = &mut count;

let bytes: []u8 = data[:];
let out: []mut u8 = buffer[:];

let text_ptr: *const u8 = c"hello";
let raw_out: *mut u8 = unsafe { ptrat<*mut u8>(addr) };
```

## Parser 规则

目标 grammar：

```text
PointerType   = "*" ("const" | "mut") Type
ReferenceType = "&" "mut"? OriginQualifier? Type
SliceType     = "[]" "mut"? Type
```

说明：

- `*T` 继续非法，raw pointer 必须显式写 `const` 或 `mut`。
- `&T` 是 shared reference。
- `&mut T` 是 writable reference。
- `[]T` 是 shared slice。
- `[]mut T` 是 writable slice。
- `[]const T` 是 legacy error；诊断指向 `[]T`。

parser 修改点非常局部：

```text
当前：
    "[]" 后要求 "const" 或 "mut"

目标：
    "[]" 后如果是 "mut"，解析 writable slice
    "[]" 后如果是 "const"，报 legacy syntax error，提示改成 []T
    否则直接解析元素类型，得到 shared slice
```

这不是空格特例。`[]T`、`[] T`、`[]mut T`、`[] mut T` 在 token 序列能匹配 grammar 时都按同一规则解析；`[]const T` 和 `[] const T` 也是同一个 legacy error。parser 为了恢复可以在报错后继续消费 `const` 并解析后续 `T`，但这个文件必须保持有诊断，不能被当作合法程序接受。

## AST / Sema 影响

第一阶段可以复用现有数据结构：

- `SliceTypePayload::mutability`
- `TypeInfo::slice_mutability`
- `TypeTable::slice(PointerMutability, TypeHandle)`

`[]T` 解析后生成：

```text
slice_mutability = PointerMutability::const_
slice_element = T
```

`[]mut T` 解析后生成：

```text
slice_mutability = PointerMutability::mut
slice_element = T
```

也就是说，语法修正不要求新增 `TypeKind`，不要求改 checked type canonical key，不要求改 IR slice fat value 表示。

建议后续内部重命名，但不作为第一阶段阻塞项：

```text
PointerMutability -> AccessMutability / ViewAccess
const_            -> shared / read_only
mut               -> writable
```

这个重命名会触及很多代码，应该单独做机械重构。

## 诊断规则

需要避免的诊断措辞：

```text
cannot assign to const slice
cannot mutate const object
expected const or mut after []
```

目标措辞：

```text
cannot write through shared slice view
slice element assignment requires []mut T
raw pointer type requires 'const' or 'mut' after '*'
legacy []const T is no longer supported; write []T for a shared slice view
```

不要先同时接受 `[]T` 和 `[]const T`。这会制造中间态，并把文档已经指出的误读继续留在用户表面。旧写法只允许出现在负例测试、迁移说明和诊断样例里。

## 类型转换规则

保留当前方向：

```aurex
[]mut T -> []T      // ok，writable view 可以降级成 shared view
[]T -> []mut T      // error，shared view 不能升级成 writable view

&mut T -> &T        // ok
&T -> &mut T        // error
```

raw pointer 是否允许 `*mut T -> *const T` 需要单独确认当前实现和 unsafe coercion 策略；本文不新增 raw pointer 隐式转换。

## 不采用的方案

### 不采用 C++ 式完整 cv 语法

不引入：

```aurex
const T
T const
* const T
*mut const T
```

原因：

- Aurex 现在没有 deep const/object const 语义。
- 多层 pointer constness 会显著增加类型显示、canonicalization、coercion 和诊断复杂度。
- 用户最容易混淆 pointer 本身 const 和 pointee const。

### 不在第一阶段引入 deep immutable

不引入：

```aurex
immutable T
frozen T
```

这类能力有价值，但它不是 `[]const T` 丑的问题的直接解。deep immutable 需要完整语义设计，不应该用语法清理的机会硬塞。

### 不把 slice 改成 `&[T]` / `&mut [T]`

这个方案语义上也说得通：

```aurex
&[T]
&mut [T]
```

优点是 slice 明确像“引用到连续序列”。但第一阶段不建议采用：

- Aurex 已有 `&[origin] T`，`&[T]` 会和 origin qualifier 视觉冲突。
- 当前 `[N]T` 是 array type，`[T]` 又会增加 bracket 负担。
- 迁移面比 `[]const T -> []T` 大得多。
- 用户已经明确要求不靠空格和特例解析，`&[T]` 周边会让 parser/lookahead 讨论变多。

### 不引入 `read T` / `view T` 这类新 qualifier

例如：

```aurex
[]read T
[]write T
view T
```

这会多造关键字，还会让高频类型更长。当前没有必要。

## 落地计划

### Stage 1：硬切 parser / display

- parser 接受 `[]T`。
- parser 接受 `[]mut T`。
- parser 拒绝 `[]const T`，并给出 `write []T` 的 legacy diagnostic。
- type display 优先输出 `[]T` 而不是 `[]const T`。

### Stage 2：更新文档、样例和测试

- `docs/zh/language-manual.md` 中 `SliceType` 改成 `[] "mut"? Type`。
- 所有 `[]const T` 样例迁移到 `[]T`。
- builtin/intrinsic、str/slice API 文档跟随改名。
- type display golden test 更新成 `[]T`。

### Stage 3：内部命名清理

- 视改动规模单独把 `PointerMutability` 机械重命名为 `AccessMutability` / `ViewAccess`。
- 把用户诊断中的 “const slice/object” 全部换成 shared/read-only view 表述。
- 保留 raw pointer 的 `*const T` 命名，不把 raw pointer 和 safe slice 的展示策略混在一起。

## 测试要求

Parser：

- 接受 `[]u8`。
- 接受 `[]mut u8`。
- 拒绝 `[]const u8`，诊断提示 `write []u8`。
- `*u8` 继续拒绝。
- `*const u8` / `*mut u8` 继续接受。
- `&u8` / `&mut u8` 继续接受。

Sema：

- `[]mut T` 可以赋给 `[]T`。
- `[]T` 不能赋给 `[]mut T`。
- `[]T` 的 element assignment 被拒绝，诊断说 shared slice view 不可写。
- `[]mut T` 的 element assignment 被接受。
- `let s: []mut T` 可以写元素，但不能重绑 `s`。
- `var s: []T` 可以重绑 `s`，但不能写元素。

Display / diagnostics：

- shared slice display name 输出 `[]T`。
- writable slice display name 输出 `[]mut T`。
- 不再在用户诊断里把 shared slice 叫成 const object。

## 参考资料

这些资料只用来校验概念边界，不代表 Aurex 要照搬任何一门语言。

- C++ draft `[dcl.type.cv]`：`const` / `volatile` 是类型系统里的 cv qualifier；同一个访问路径是否 cv-qualified 和对象本身是否原本 cv-qualified 需要区分。参考：https://eel.is/c++draft/dcl.type.cv
- Rust Reference pointer/reference types：区分 shared reference、mutable reference、raw const pointer 和 raw mutable pointer。参考：https://doc.rust-lang.org/reference/types/pointer.html
- Zig Language Reference：`const` / `var` binding 和 pointer/slice 指向数据的 constness 是不同层级。参考：https://ziglang.org/documentation/master/
- D Type Qualifiers：`const`、`immutable`、`shared`、`inout` 是传递性 type qualifier，说明深不可变和只读 view 不是一个小语法问题。参考：https://dlang.org/spec/const3.html
- Swift The Basics：`let` 声明 constant，`var` 声明 variable，主要管名字绑定。参考：https://docs.swift.org/swift-book/documentation/the-swift-programming-language/thebasics/
- Kotlin Basic Syntax / Collections：`val` / `var` 管变量重赋值；collections 又单独区分 read-only interface 和 mutable interface。参考：https://kotlinlang.org/docs/basic-syntax.html 与 https://kotlinlang.org/docs/collections-overview.html
- C# `readonly` / `ref readonly`：只读引用、只读成员和 binding 不是同一层规则。参考：https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/readonly
- Pony Reference Capabilities：`iso`、`trn`、`ref`、`val`、`box`、`tag` 展示了 aliasing、读写权限和并发共享可以形成完整 capability system。参考：https://tutorial.ponylang.io/reference-capabilities/reference-capabilities.html
- Javari：`Javari: Adding reference immutability to Java`，OOPSLA 2005。参考：https://homes.cs.washington.edu/~mernst/pubs/ref-immutability-oopsla2005-abstract.html
- IGJ：`Object and reference immutability using Java generics`，ESEC/FSE 2007。参考：https://homes.cs.washington.edu/~mernst/pubs/immutability-generics-fse2007-abstract.html
- `A Practical Type System and Language for Reference Immutability`：把 immutable reference 的约束定义为不能通过该引用修改对象抽象状态。参考：https://homes.cs.washington.edu/~mernst/pubs/ref-immutability-oopsla2004-abstract.html

## 结论

第一阶段应该做一个小而硬的修正：

```aurex
// 保留
let x: T
var x: T
&T
&mut T
*const T
*mut T

// 修改
[]const T  ->  []T
[]mut T    ->  []mut T
```

这能解决当前最丑、最高频、最容易误导的 slice 表面，同时不引入完整 deep immutability、C++ cv 组合或 capability system。Aurex 后续如果要做真正不可变对象或并发 capability，应在这个干净边界上另起设计，而不是继续让 `const` 一个词背所有语义。

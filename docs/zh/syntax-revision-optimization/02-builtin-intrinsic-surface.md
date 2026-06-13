# Builtin / Intrinsic Surface：删除硬关键字污染，收束内建能力

日期：2026-06-13
状态：语法修正优化第二手改动设计稿
关联问题：`docs/zh/m27c-syntax-ergonomics-review.md` 中的 P1 builtin 名字和关键字污染

本文固定 Aurex 第二项语法修正方向：把当前直接暴露给用户的 builtin keyword 表面收束掉。第一批泛型修正已经把 builtin type operand 统一成 `<...>`，但这只是换括号；真正的问题是这些名字不应该全部进入语言关键字层，尤其 `strfromutf8`、`strblen`、`sliceptr`、`ptrat` 这种名字像编译器内部 helper，不像一门语言的长期用户表面。

## 现状

当前 lexer 把下面这些名字直接 token 化为 keyword：

```text
cast ptrcast bitcast sizeof alignof
ptraddr ptrat sliceptr slicelen
strptr strblen strvalid strfromutf8 strraw
```

相关实现入口：

- `include/aurex/frontend/syntax/core/token.hpp`：`kw_cast` 到 `kw_strraw`。
- `src/frontend/lex/sources/keyword.cpp`：builtin 名字进入 keyword table。
- `src/frontend/parse/grammar/parser_primary.cpp`：primary expression 直接识别 builtin keyword。
- `src/frontend/parse/grammar/parser_builtin_expr.cpp`：每种 builtin 有专门 parser shape。
- `src/frontend/sema/internal/expressions/sources/sema_builtin_expression_analyzer.cpp`：builtin 类型规则和 unsafe 规则。
- `src/midend/ir/lowering/sources/lower_ast_expr.cpp`：builtin expression lowering 到 checked IR。

样例里的实际形态：

```aurex
let text: str = strfromutf8(bytes);
return cast<i32>(strblen(text));

let addr: usize = ptraddr(&pair);
let pair_ptr: *mut Pair = unsafe { ptrat<*mut Pair>(addr) };
let pointer_size: usize = sizeof<*mut Pair>;
let erased: *const void = unsafe { ptrcast<*const void>(&pair) };
let zero_bits: f32 = unsafe { bitcast<f32>(cast<u32>(0)) };

if slicelen(ascii) != 2usize {
    return 11;
}
let ascii_ptr: *const u8 = sliceptr(ascii);

let raw: str = unsafe { strraw(strptr("raw"), strblen("raw")) };
```

从当前样例和文档粗略统计，出现频率最高的是：

```text
cast         247
ptraddr      130
ptrat        114
strblen      114
sizeof        97
ptrcast       47
strptr        37
strraw        30
strfromutf8   28
slicelen      28
sliceptr      22
strvalid      22
alignof       20
bitcast       17
```

这说明问题不是边角。`cast`、`ptraddr`、`ptrat`、`strblen`、`sizeof` 已经是用户代码高频噪音。

## 问题

### 1. 名字丑，内部味太重

`strfromutf8`、`strblen`、`sliceptr`、`ptrat` 都是压缩过的内部 helper 名字。它们短，但不是好打；用户需要记缩写规则：

```text
strblen      // string byte length?
sliceptr     // slice data pointer?
ptrat        // pointer at address?
strraw       // raw string from parts?
```

短不等于美。长期语言表面应该让常用代码接近自然读法：

```aurex
text.byte_len()
slice.len()
slice.data()
str.from_utf8_or_empty(bytes)
intrinsic.ptr_at<*mut Pair>(addr)
```

### 2. 关键字空间被低层 helper 污染

`strfromutf8` 这种名字一旦是 keyword，用户不能把它当普通函数名、变量名或模块成员。语言关键字应该少而稳，不能把当前实现阶段的后端 helper 名字冻结进去。

当前做法还让 parser 直接知道所有 builtin 名字。parser 本来应该关心表达式形状，比如 call、generic apply、field access；现在却在 primary expression 入口维护一张 builtin keyword shape 表。

### 3. 安全能力和危险能力视觉层级不清

下面这些操作危险级别明显不同：

```aurex
cast<i32>(x)                 // safe numeric/bool conversion
sizeof<T>                    // safe layout query
strfromutf8(bytes)           // checked UTF-8 boundary
ptraddr(&value)              // 低层地址观察
ptrat<*mut T>(addr)          // 从整数构造 pointer，unsafe
ptrcast<*const void>(ptr)    // pointer cast，unsafe
bitcast<f32>(bits)           // bit reinterpret，unsafe
strraw(ptr, len)             // unchecked UTF-8，unsafe
```

但它们现在长得都像同一类 builtin call。`unsafe { ... }` 虽然能圈出部分危险操作，但名字本身没有形成层级。

### 4. 只是迁移到 `<...>` 不够

下面这种改法只修了泛型括号，没有修语言气质：

```aurex
cast<i32>(x)
strblen(text)
sliceptr(bytes)
ptrat<*mut Pair>(addr)
```

`ptrat<*mut Pair>(addr)` 虽然已经没有 `[]` 冲突，仍然不像稳定用户语法。

## 决策

### 决策 1：safe scalar cast 改成 `as`

高频 `cast<T>(x)` 改为表达式 cast：

```aurex
let y: i32 = x as i32;
let n: usize = value as usize;
let b: bool = code as bool;
```

旧写法：

```aurex
cast<i32>(x)
cast<usize>(value)
cast<bool>(code)
```

迁移目标：

```aurex
x as i32
value as usize
code as bool
```

`as` 只承载 safe scalar conversion：整数、浮点、bool 之间当前已经允许的转换。raw pointer cast、bit reinterpret、integer-to-pointer 不进入 `as` 的 safe 集合。

这样做有三个收益：

- 高频转换少打字。
- `cast` 不再作为硬关键字。
- 危险 cast 不会和普通数值转换长得一样。

复杂表达式需要靠括号表达意图：

```aurex
(a + b) as i32
value.method() as usize
```

parser 不靠空格判断。`as` 是明确 token，右侧解析 type。

### 决策 2：`sizeof` / `alignof` 保留名字，但不再是 hard keyword

layout query 是系统语言常用能力，`sizeof` / `alignof` 这两个名字可以保留；问题是它们不应该继续使用 `[]`，也不应该必须作为 lexer keyword。

目标写法：

```aurex
let size: usize = sizeof<T>();
let align: usize = alignof<T>();
let pointer_size: usize = sizeof<*mut Pair>();
```

它们在语法上是普通 zero-argument generic call：

```text
Name GenericArgs "(" ")"
```

也就是说，`sizeof<T>()` 不需要 parser 里的 builtin primary expression 特例。parser 只产出普通 call / generic apply；sema 通过预定义 compiler intrinsic symbol 把它解析成 layout query。

保留 `()` 是有意设计。泛型表达式调用统一成 `callee<Args...>(...)`，不额外开 `sizeof<T>` 这种无括号特例。

### 决策 3：危险低层能力进入 `intrinsic` namespace

下面这些能力不应该伪装成普通函数，也不应该占用全局 keyword：

```aurex
ptrcast<T>(p)
bitcast<T>(x)
ptrat<T>(addr)
strraw(data, len)
```

目标写法：

```aurex
let erased: *const void = unsafe { intrinsic.ptr_cast<*const void>(&pair) };
let bits: f32 = unsafe { intrinsic.bit_cast<f32>(0u32) };
let ptr: *mut Pair = unsafe { intrinsic.ptr_at<*mut Pair>(addr) };
let text: str = unsafe { intrinsic.str_from_raw(data, len) };
```

`intrinsic` 是 compiler-reserved root namespace，不是普通用户模块。parser 不需要为它开 keyword；name resolution / sema 负责识别 `intrinsic.*` 的目标。

危险性继续由 sema 检查：

- `intrinsic.ptr_cast<T>(p)` 需要 `unsafe`。
- `intrinsic.bit_cast<T>(x)` 需要 `unsafe`。
- `intrinsic.ptr_at<T>(addr)` 需要 `unsafe`。
- `intrinsic.str_from_raw(data, len)` 需要 `unsafe`。

`ptraddr` 当前是 safe，但它仍然是低层地址观察能力，不适合放在全局短名字里。目标写法：

```aurex
let addr: usize = intrinsic.addr(&pair);
```

第一阶段保持它 safe，只迁移名字和 namespace。后续如果 Aurex 引入更严格的 pointer provenance 模型，再单独讨论它是否也需要 unsafe。

### 决策 4：slice / str projection 改成方法表面

`slicelen`、`sliceptr`、`strblen`、`strptr` 本质是对值的 projection，不应该长成全局 builtin function。

目标写法：

```aurex
let n: usize = bytes.len();
let p: *const u8 = bytes.data();

let n: usize = text.byte_len();
let p: *const u8 = text.data();
```

旧写法：

```aurex
slicelen(bytes)
sliceptr(bytes)
strblen(text)
strptr(text)
```

迁移目标：

```aurex
bytes.len()
bytes.data()
text.byte_len()
text.data()
```

`str` 使用 `byte_len()` 而不是 `len()` 是刻意的。Aurex 当前字符串 slicing 和 regex 代码都按 UTF-8 byte offset 工作，`len()` 对字符串会让用户误以为是 Unicode scalar count 或 grapheme count。`byte_len()` 明确表达单位。

这些方法可以先作为 compiler-known primitive methods，不要求标准库和 trait 系统一次性成熟。parser 仍按普通 method call 解析，sema 对 slice / str 类型的特定 method 做内建解析并 lowering 到现有 IR。

### 决策 5：UTF-8 构造进入 `str` namespace

当前：

```aurex
strvalid(bytes)
strfromutf8(bytes)
```

当前语义下的迁移目标：

```aurex
str.is_utf8(bytes)
str.from_utf8_or_empty(bytes)
```

长期建议不要让 `from_utf8` 失败时静默返回空字符串。更好的长期语义是：

```aurex
str.from_utf8(bytes) -> Result<str, Utf8Error>
```

或者在错误模型还没稳定时先用：

```aurex
str.from_utf8(bytes) -> Option<str>
```

这属于语义层优化，不要求和本轮语法迁移同一步完成。但命名必须提前留好坑位：`from_utf8` 不应该长期表示“失败给你空串”。

## 目标表面总览

| 当前写法 | 目标写法 | 层级 |
| --- | --- | --- |
| `cast<i32>(x)` | `x as i32` | safe scalar cast |
| `sizeof<T>` | `sizeof<T>()` | safe layout query |
| `alignof<T>` | `alignof<T>()` | safe layout query |
| `ptraddr(p)` | `intrinsic.addr(p)` | low-level address query |
| `ptrat<*mut T>(addr)` | `intrinsic.ptr_at<*mut T>(addr)` | unsafe intrinsic |
| `ptrcast<*const T>(p)` | `intrinsic.ptr_cast<*const T>(p)` | unsafe intrinsic |
| `bitcast<T>(x)` | `intrinsic.bit_cast<T>(x)` | unsafe intrinsic |
| `slicelen(xs)` | `xs.len()` | primitive method |
| `sliceptr(xs)` | `xs.data()` | primitive method |
| `strblen(s)` | `s.byte_len()` | primitive method |
| `strptr(s)` | `s.data()` | primitive method |
| `strvalid(bytes)` | `str.is_utf8(bytes)` | str namespace |
| `strfromutf8(bytes)` | `str.from_utf8_or_empty(bytes)` | str namespace |
| `strraw(data, len)` | `intrinsic.str_from_raw(data, len)` | unsafe intrinsic |

## Parser 规则

### 规则 1：builtin 名字不再进入 primary-expression keyword 表

目标不是扩张 `parse_builtin_expr()`，而是逐步删除它。

parser 应该把下面这些都当普通表达式形状处理：

```aurex
sizeof<T>()
intrinsic.ptr_at<*mut T>(addr)
xs.len()
text.byte_len()
str.from_utf8_or_empty(bytes)
```

也就是说：

- `sizeof<T>()` 是 name + generic args + call。
- `intrinsic.ptr_at<*mut T>(addr)` 是 qualified path + generic args + call。
- `xs.len()` 是 field/member + call。
- `str.from_utf8_or_empty(bytes)` 是 namespace path + call。

具体是不是 compiler intrinsic，由 sema / name resolution 决定。

### 规则 2：`as` 是表达式 cast operator

语法草案：

```text
CastExpr = PrefixOrPostfixExpr ("as" Type)*
```

示例：

```aurex
x as i32
value.method() as usize
(a + b) as i32
```

`as` 右侧必须是 type，不是 expression。parser 不靠空格，不靠名字大小写。

### 规则 3：generic intrinsic 全部使用 `<...>(...)`

配合泛型第一手改动，所有带 type argument 的 intrinsic 都走统一写法：

```aurex
sizeof<T>()
alignof<T>()
intrinsic.ptr_at<T>(addr)
intrinsic.ptr_cast<T>(p)
intrinsic.bit_cast<T>(x)
```

不新增 `sizeof<T>`、`ptr_at[T]`、`ptr_at T` 等额外表面。

### 规则 4：`intrinsic` 是保留 root namespace，不是 keyword

`intrinsic` 不需要 lexer keyword。它应在 name resolution 层作为 compiler-reserved root path 处理：

```aurex
intrinsic.ptr_at<*mut Pair>(addr)
intrinsic.bit_cast<f32>(bits)
```

这样 parser 不需要特殊判断，但用户也不能用普通模块假冒 `intrinsic` 根路径。

## 不采用的方向

### 只把 `[]` 换成 `<...>`

不够。

```aurex
strblen(text)
ptrat<*mut Pair>(addr)
strraw(data, len)
```

这些名字的问题仍然存在。

### 全部放进 `intrinsic`

不建议把所有能力都写成：

```aurex
intrinsic.cast<i32>(x)
intrinsic.sizeof<T>()
intrinsic.slice_len(xs)
intrinsic.str_byte_len(s)
```

这会把高频安全操作也写得很重。`x as T`、`xs.len()`、`text.byte_len()` 更适合用户代码。

### C 风格 cast

不采用：

```aurex
(i32)x
```

它和 grouping、tuple、function call 视觉冲突，也不适合 Aurex 现有 parser 形态。

### 类型构造式 cast

不采用把普通 cast 写成：

```aurex
i32(x)
```

它容易和 constructor、call、newtype conversion 混在一起。`as` 明确表达“转换”，语义边界更好。

### `@builtin(...)`

不采用：

```aurex
@builtin("ptr_at", *mut Pair, addr)
```

这更像 compiler test hook，不适合作为用户语法。

## 迁移计划

### 阶段 1：支持新表面，旧表面继续可用但诊断

新增：

```aurex
x as i32
sizeof<T>()
alignof<T>()
intrinsic.addr(p)
intrinsic.ptr_at<T>(addr)
intrinsic.ptr_cast<T>(p)
intrinsic.bit_cast<T>(x)
xs.len()
xs.data()
text.byte_len()
text.data()
str.is_utf8(bytes)
str.from_utf8_or_empty(bytes)
intrinsic.str_from_raw(data, len)
```

旧表面发迁移诊断：

```text
cast<T>(x) is deprecated; use x as T
sizeof<T> is deprecated; use sizeof<T>()
ptrat<T>(addr) is deprecated; use intrinsic.ptr_at<T>(addr)
strblen(s) is deprecated; use s.byte_len()
```

### 阶段 2：删除旧 builtin type operand 表面

配合泛型迁移，删除：

```aurex
cast<T>(x)
sizeof<T>
alignof<T>
ptrat<T>(addr)
ptrcast<T>(p)
bitcast<T>(x)
```

如果迁移期还保留旧名字，也只能接受 `<...>`：

```aurex
ptrat<T>(addr)      // compatibility only
ptrcast<T>(p)       // compatibility only
bitcast<T>(x)       // compatibility only
```

但这只是过渡，最终不作为推荐语法。

### 阶段 3：删除 builtin keyword token

从 keyword table 删除：

```text
kw_cast kw_ptrcast kw_bitcast kw_sizeof kw_alignof
kw_ptraddr kw_ptrat kw_sliceptr kw_slicelen
kw_strptr kw_strblen kw_strvalid kw_strfromutf8 kw_strraw
```

这些名字如果仍出现在源码里，应按普通 identifier 解析，然后由 name resolution 给出迁移诊断或未定义符号诊断。

### 阶段 4：删除 parser builtin primary expression 分支

目标是让 parser 不再维护 builtin shape 表：

```text
BuiltinExprShape::CAST
BuiltinExprShape::TYPE
BuiltinExprShape::PTRADDR
BuiltinExprShape::PTRAT
...
```

parser 只负责普通 expression grammar。compiler intrinsic 的识别和检查进入 sema。

## 影响范围

### Lexer

短期保留旧 keyword 以支持迁移诊断。

长期删除 builtin keyword entries，使这些文本重新变成 identifier。`as` 仍保留 keyword，因为 import alias 已经使用它，expression cast 只是复用同一个稳定关键字。

### Parser

需要新增：

- `as` cast expression。
- 泛型第一手改动里的 `<...>` generic call。

需要逐步删除：

- `parse_builtin_expr()` 的 keyword primary 入口。
- `parser_builtin_expr.cpp` 中按 builtin 名字拆出来的特殊括号 parser。

### AST

短期可以继续用现有 `ExprKind::cast`、`ExprKind::pcast`、`ExprKind::bcast`、`ExprKind::size_of` 等节点，以降低实现风险。

中期更合理的形态是：

- parser 产出普通 call / method / generic apply / cast-as AST。
- sema 把命中特定 compiler intrinsic symbol 的 call 转成 checked builtin operation。

也就是说，raw syntax AST 不应该长期保存“这是 `strblen` keyword parse 出来的东西”这种实现痕迹。

### Sema

需要把 builtin 检查从“语法节点种类驱动”逐步迁移到“resolved intrinsic symbol 驱动”：

- `x as T` 调用 safe scalar conversion checker。
- `sizeof<T>()` / `alignof<T>()` 调用 layout query checker。
- `intrinsic.*` 调用 unsafe / low-level checker。
- primitive methods 调用 slice / str projection checker。
- `str.from_utf8*` 调用 UTF-8 checker。

### IR / Backend

IR 不需要因为语法表面大改。现有 IR value kind 可以继续承载：

- cast / pointer cast / bitcast / pointer address / pointer from address
- size_of / align_of
- slice_data / slice_len
- str_data / str_byte_len
- str_is_valid_utf8 / str_from_utf8_checked / str_from_bytes_unchecked

改变的是 frontend 如何产生这些 checked operations。

### Docs / Samples / Tests

需要更新：

- `docs/zh/language-manual.md`
- `docs/zh/language-feature-inventory.md`
- `tests/samples/**`
- `examples/**`
- parser keyword tests
- parser builtin recovery tests
- sema unsafe tests
- IR lowering golden / integration tests

## 必须覆盖的测试

safe cast：

```aurex
let a: i32 = value as i32;
let b: usize = (x + y) as usize;
let c: bool = code as bool;
```

unsafe cast / pointer intrinsic：

```aurex
let erased: *const void = unsafe { intrinsic.ptr_cast<*const void>(&value) };
let bits: f32 = unsafe { intrinsic.bit_cast<f32>(0u32) };
let ptr: *mut Pair = unsafe { intrinsic.ptr_at<*mut Pair>(addr) };
```

unsafe requirement：

```aurex
let bad = intrinsic.ptr_at<*mut Pair>(addr);      // diagnostic
let bad2 = intrinsic.bit_cast<f32>(0u32);         // diagnostic
let bad3 = intrinsic.str_from_raw(data, len);     // diagnostic
```

layout query：

```aurex
let a: usize = sizeof<i32>();
let b: usize = alignof<Pair>();
let c: usize = sizeof<Vec<i32>>();
```

slice / str methods：

```aurex
let n: usize = bytes.len();
let p: *const u8 = bytes.data();
let m: usize = text.byte_len();
let q: *const u8 = text.data();
```

UTF-8 namespace：

```aurex
if str.is_utf8(bytes) {
    let text: str = str.from_utf8_or_empty(bytes);
}
```

旧语法迁移诊断：

```aurex
cast<i32>(x)
sizeof<i32>
ptrat<*mut Pair>(addr)
strblen(text)
slicelen(bytes)
strfromutf8(bytes)
strraw(data, len)
```

## 成功标准

这次语法修正完成后应满足：

- `strblen`、`strfromutf8`、`sliceptr`、`ptrat` 等不再是长期用户语法。
- 高频 safe cast 用 `as`，不再写 `cast<T>(x)`。
- safe layout query 使用普通 generic call：`sizeof<T>()` / `alignof<T>()`。
- 危险低层能力集中到 `intrinsic` namespace，并继续要求 `unsafe`。
- slice / str projection 看起来像值的方法，而不是全局 compiler helper。
- parser 不再靠 builtin keyword table 解析用户表达式。
- compiler intrinsic 识别下沉到 sema / name resolution。
- `[]` 从 builtin type argument 里完全退出。

# Angle Bracket Generics：泛型统一改为 `<...>`

日期：2026-06-13
状态：语法修正优化第一批落地设计记录
关联问题：泛型 `[]` 过载和大小写启发式影响 parsing

本文固定 Aurex 泛型语法的第一项修正：泛型参数、泛型实参、associated type equality、type-operand builtin 和泛型表达式调用统一使用 C++ 风格的 `<...>` 表面。

这里采用的是 C++ 泛型写法的可读外形，不采用 C++ template 的历史语义包袱。Aurex 仍保持自己的泛型、trait/capability、where constraint、associated type 和 const generic 语义。

## 决策

采用 `<...>` 作为 Aurex 泛型统一表面：

```aurex
struct Box<T> {
    value: T;
}

fn id<T>(x: T) -> T {
    return x;
}

fn map<T, U>(value: T) -> U
where T: Convert<U> {
    ...
}

impl<T> Box<T> {
    fn value(self: &Box<T>) -> &T {
        return &self.value;
    }
}

trait Source<T> {
    type Item;
}

fn use_source<T>(value: T) -> i32
where T: Source<Item = i32> {
    ...
}
```

泛型实例化也统一使用 `<...>`：

```aurex
let x: Box<i32> = make_box<i32>(1);
let xs: Vec<Box<i32>> = make_vec<Box<i32>>();
let y: Result<Vec<i32>, Error> = parse(input);
```

`[]` 不属于 Aurex 泛型语法。下面这些 token 序列不定义为泛型参数或泛型实参：

```aurex
Box[T]
fn f[T](x: T) -> T
Source[Item = i32]
cast[i32](x)
sizeof[*mut Pair]
```

当前泛型唯一合法写法：

```aurex
Box<T>
fn f<T>(x: T) -> T
Source<Item = i32>
cast<i32>(x)
sizeof<*mut Pair>
```

`sizeof<T>` / `alignof<T>` 当前保持 builtin type operand 形态，不追加 `()`；`cast<T>(x)`、`ptrat<T>(addr)`、`ptrcast<T>(p)`、`bitcast<T>(x)` 是带 value argument 的 builtin call。后续若 builtin 降级为 intrinsic namespace，可自然变成：

```aurex
intrinsic.sizeof<*mut Pair>()
intrinsic.alignof<Pair>()
intrinsic.cast<i32>(x)
intrinsic.ptr_at<*mut Pair>(addr)
```

## 不采用的部分

不采用 C++ template 语义：

- 不引入 `typename`。
- 不引入 `template` disambiguator。
- 不引入 SFINAE。
- 不引入 dependent name lookup。
- 不引入 C++ two-phase lookup。
- 不把约束塞进模板参数列表里写类型体操。
- 不用空格、换行或 comment 的有无来决定 `<...>` 是泛型还是比较表达式。

Aurex 的约束仍由 `where`、trait/capability 和 associated type equality 表达：

```aurex
fn f<T>(x: T) -> T
where T: Copy + Eq {
    return x;
}
```

## 需要解决的歧义

`<...>` 的核心风险是和比较表达式冲突：

```aurex
a < b > c
```

如果 parser 无脑把 `<...>` 当泛型，就会重走 C++ 老问题。Aurex 应该用更严格的语法规则避免猜测。

## Parser 规则

### 规则 1：Aurex 不做空格敏感语法

空格、换行、comment 只负责分隔 token 和保留源码格式，不参与语法含义判断。下面这些写法在 parser 层必须等价：

```aurex
make<T>(x)
make < T >(x)
make < T > (x)
make
<
T
>
(x)
```

类型、声明、where constraint、表达式泛型调用都遵守同一原则。formatter 可以把它们统一格式化成紧凑风格：

```aurex
make<T>(x)
Box<T>
fn id<T>(x: T) -> T
```

但这只是代码风格，不是语言规则。parser 不允许读取 trivia 来决定语义。

### 规则 2：类型上下文和声明上下文中，`<...>` 就是泛型

这些上下文没有表达式比较歧义：

```aurex
let x: Vec<i32>;
fn f<T>(x: T) -> T;
struct Map<K, V> {}
impl<T> Box<T> {}
where T: Source<Item = i32>
```

parser 在这些位置已经知道自己正在解析 type、generic parameter list 或 where constraint，因此不需要启发式判断。

### 规则 3：`<...>` 是统一 postfix generic suffix，不看空格

表达式里的 `<...>` 不是靠“写得像不像泛型”来猜，也不是靠空格来猜。它是一个正式的 postfix suffix：

```text
ExprPath GenericArgs GenericContinuation
```

parser 看到 `ExprPath <` 时，可以试探解析 generic arg list；只有同时满足下面两个条件才接受为泛型后缀：

1. `<...>` 内部能按 generic args 解析并正常关闭。
2. 关闭 `>` 后的下一个有效 token 是 `(`、`{` 或 `.`。

允许：

```aurex
make<T>(x)
make < T > (x)
Option<T>.some(x)
Option < T > . some(x)
Box<T>{ value: x }
Box < T > { value: x }
```

不把下面形式解释为泛型：

```aurex
foo < T > x
a < b > c
```

因为 `>` 后面不是 `(` / `{` / `.`。

这个规则的本质是 grammar production 优先，而不是空白优先。`make<T>(x)` 和 `make < T > (x)` 完全等价。

这不是为了某一种书写习惯开的特例，而是把泛型实参设计成 postfix grammar 的一部分。所有合法排版都走同一条 grammar，不存在“紧贴能过、加空格不能过”的情况。

必须承认一个代价：下面这种 token 序列会按泛型调用解析：

```aurex
a < b > (c)
```

它等价于：

```aurex
a<b>(c)
```

如果用户真的要写比较表达式，必须加括号表达意图：

```aurex
(a < b) > (c)
```

这是 `<...>` 泛型在表达式里不可避免的代价。Aurex 的选择是：不让空格改变语义，用明确的括号处理少数歧义表达式。

### 规则 4：generic suffix 的 continuation 集合固定

`callee<Args...>` 只有在后续 token 是以下之一时才构成完整 postfix generic suffix：

- `(`：泛型函数调用。
- `{`：泛型 struct literal。
- `.`：泛型类型的 enum case、associated item 或后续 member path。

示例：

```aurex
make<T>(x)          // ok
make < T > (x)      // ok, same parse
Box<T>{ value: x }  // ok
Box < T > { value: x } // ok, same parse
Option<T>.some(x)   // ok
Option < T > . some(x) // ok, same parse
```

下面不接受为泛型：

```aurex
a < b > c
```

因为 `>` 后面是 `c`，不是 `(` / `{` / `.`。

### 规则 5：不靠大小写判断语法

删除当前 bracket suffix classifier 中基于首字母大写的 type-like 判断。类型名大写可以作为 lint 或 style guide，但不能影响 parser 对 token stream 的解释。

目标不变量：

```text
foo[bar] 永远是 index/slice 相关。
Foo<Bar> 永远是 generic 相关。
```

### 规则 6：泛型上下文支持拆 right-angle token

lexer 保持 maximal munch，继续产生：

```text
>
>=
>>
>>=
```

parser 在泛型关闭上下文中拆分这些 token：

```text
>     -> 关闭一层泛型
>>    -> 关闭两层泛型
>=    -> 关闭一层泛型，然后留下 =
>>=   -> 关闭两层泛型，然后留下 =
```

这样用户可以自然书写：

```aurex
let a: Vec<Box<i32>> = value;
let b: Map<str, Vec<Option<i32>>> = value;
let c: Vec<i32>=make_vec();
```

不要求用户写老式空格：

```aurex
Vec<Box<i32> >
```

实现建议：不要为此破坏 lexer maximal munch。应在 token cursor 或 parser helper 层提供 `consume_generic_right_angle()` 之类的能力，在泛型上下文里消费或拆分 `greater`、`greater_greater`、`greater_equal`、`greater_greater_equal`。普通表达式上下文仍把 `>>` 当 shift operator。

### 规则 7：const generic 第一批只支持 atom

第一批 const generic 保持当前 M16 check-only 子集：const argument 只接受 integer / bool / char scalar literal，或当前 generic context 中同类型 const parameter name。

```aurex
struct Array<T, const N: usize> {
    data: [N]T;
}

let xs: Array<i32, 4>;
let ys: Array<i32, N>;
```

复杂 const 表达式不属于第一批落地范围，下面写法是后续设计候选，不是当前可用语法：

```aurex
Array<i32, (N >> 1)>
```

未来也可以考虑显式 const block：

```aurex
Array<i32, const { N >> 1 }>
```

后续若打开 generic const arithmetic，再单独设计 `>` / `>>` 与 const expression 的恢复、诊断和 formatter 规则。

## 语法草案

近似 EBNF：

```text
GenericParams        = "<" GenericParam ("," GenericParam)* ","? ">"
GenericParam         = Identifier | "const" Identifier ":" Type | "origin" Identifier

GenericArgs          = "<" GenericArg ("," GenericArg)* ","? ">"
GenericArg           = Type | ConstGenericAtom
ConstGenericAtom     = IntegerLiteral | "true" | "false" | CharLiteral | Identifier

NamedType            = TypePath GenericArgs?
TypePath             = Identifier ("." Identifier)*

ExpressionGeneric    = ExprPath GenericArgs GenericContinuation
GenericContinuation  = "(" | "{" | "."
```

`ExpressionGeneric` 不直接独立成完整 expression，它只作为 postfix suffix 参与 call / struct literal / associated path。

`ExprPath` 表示 identifier、qualified path、member path 或其它明确按名字寻址的 callee/path。泛型实参绑定的是名字，不绑定任意 runtime expression；如果未来需要支持更宽的 callable expression generic instantiation，必须单独设计，不能靠空格区分。

## 第一批落地策略

### 当前语法只定义 `<...>`

parser 只接受：

```aurex
Box<T>
fn f<T>(...)
impl<T> Box<T> { ... }
trait Source<T> { ... }
foo<T>(bar)
```

`foo[bar]` 只解析为 index/slice。泛型函数调用只能写：

```aurex
foo<T>(bar)
```

保留 `[]` 给 array / slice / index / literal / pattern / attribute / origin 等非泛型用途。

## 影响范围

### Lexer

无需改变基本 token set。当前已有 `less`、`greater`、`greater_equal`、`less_less`、`greater_greater`、`greater_greater_equal` 等 token。

lexer 不需要为泛型提供空格敏感信息。trivia 仍应保留给 formatter、diagnostic 和 source map，但 parser 不应读取 trivia 来决定 `<...>` 的语义。

### Parser

需要新增或调整：

- generic parameter list parser：只接受 `<` / generic right-angle close。
- named type generic args parser：只接受 `<` / generic right-angle close。
- dyn trait associated type constraints：只接受 `dyn Trait<Item = T>`。
- expression postfix parser：新增 angle generic suffix；删除 `[]` generic apply 分类。
- expression postfix parser：对 `ExprPath <` 做可回滚的 generic suffix 试探解析；试探成功只取决于 generic args 是否闭合，以及后续 token 是否为 `(` / `{` / `.`。
- expression postfix parser：不得用 whitespace、newline、comment 或 identifier 首字母大小写参与 `<...>` 判定。
- token cursor 或 parser helper：支持在泛型上下文拆 `>>`、`>=`、`>>=`。
- recovery context：泛型参数和实参的 closing delimiter 从 `]` 调整为 generic right-angle close。

### AST / Sema / IR

AST 里的 `GenericParamDecl`、`GenericArgDecl`、`TypeNode::generic_args`、`GenericApplyExprPayload` 等语义结构不需要改模型，只需要更新 source range 和 parser 构造路径。

Sema / IR 不应依赖 delimiter 文本。若有诊断文本、dump golden 或 syntax fixture，需同步更新。

### Docs / Samples / Tests

需要更新：

- `docs/zh/language-manual.md`
- `docs/zh/language-feature-inventory.md`
- 当前语法修正优化目录
- `tests/samples/**`
- `tests/gtest/frontend/**`
- `examples/**`

## 必须覆盖的测试

正例：

```aurex
struct Box<T> { value: T; }
struct Map<K, V> { key: K; value: V; }
struct Array<T, const N: usize> { data: [N]T; }

fn id<T>(x: T) -> T { return x; }
fn pair<T, U>(x: T, y: U) -> (T, U) { return (x, y); }

let a: Vec<Box<i32>> = value;
let b: Map<str, Vec<Option<i32>>> = value;
let c: Array<i32, 4> = value;

let x = make_box<i32>(1);
let y = Option<i32>.some(1);
let z = Box<i32>{ value: 1 };
```

比较表达式不应误判：

```aurex
let ok = a < b > c;
let ok2 = foo < bar;
let ok3 = foo < bar > baz;
```

空格非敏感：

```aurex
let a = make<i32>(1);      // generic call
let b = make < i32 >(1);   // same generic call
let c = make < i32 > (1);  // same generic call
let d = Box < i32 > { value: 1 };
let e = Option < i32 > . some(1);
```

表达式歧义的固定优先级：

```aurex
let a = make < i32 > x;  // comparison expression or diagnostic, not generic call
let b = a < b > c;       // comparison expression or diagnostic, not generic call
let c = a < b > (x);     // generic call: a<b>(x)
let d = (a < b) > (x);   // comparison expression
```

right-angle 拆分：

```aurex
let a: Vec<Vec<i32>> = value;
let b: Vec<Vec<Vec<i32>>> = value;
let c: Vec<i32>=make_vec();
```

const generic 当前子集：

```aurex
let a: Array<i32, N> = value;
let b: Array<i32, 4> = value;
```

复杂 const generic 表达式暂不属于第一批：

```aurex
let c: Array<i32, (N >> 1)> = value; // future, not first batch
```

非泛型 bracket 形式：

```aurex
struct Box[T] { value: T; }
fn id[T](x: T) -> T { return x; }
let x: Box[i32] = value;
```

这些形式不是泛型。parser 不提供兼容分支，也不把它们恢复成泛型 AST。

## 成功标准

这次语法修正完成后应满足：

- `[]` 不再承担泛型职责。
- `foo[bar]` 不可能被 parser 猜成 generic apply。
- 类型名是否大写不影响 parsing。
- 空格、换行、comment 不影响 `<...>` 的语义。
- `Vec<Box<i32>>` 不需要写成 `Vec<Box<i32> >`。
- `a < b > c` 不会被误判为泛型。
- `make<T>(x)`、`make < T >(x)`、`make < T > (x)` 解析结果一致。
- 泛型写法在 type、fn、trait、impl、where、builtin/intrinsic call 中一致。

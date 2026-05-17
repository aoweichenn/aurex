# Aurex 正则库语法和实现说明

日期：2026-05-17
阶段：M2 language-core-no-std
状态：按当前仓库 `examples/libs/regex` 实现编写。本文是正则库的语法、API、模块和实现说明，避免后续只靠上下文记忆查范围。

## 1. 位置和导入

正则库是独立模块目录，不放在 `text` 下：

```text
examples/libs/regex/api.ax
examples/libs/regex/alloc.ax
examples/libs/regex/ascii.ax
examples/libs/regex/engine.ax
examples/libs/regex/parser.ax
examples/libs/regex/program.ax
examples/libs/regex/types.ax
```

外部调用只导入 facade：

```aurex
import regex.api as regex;
```

编译示例：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
```

## 2. 设计目标

这个库的目标不是复刻 PCRE，而是在当前 Aurex M2 能力范围内实现一个结构完整、可编译、可运行、可测试的正则库：

- API 独立：调用方只面对 `regex.api`。
- 实现分层：parser、program、engine、types、alloc、ascii 各自负责一类问题。
- 编译型：`compile` 把 pattern 编译成 NFA 状态表，匹配阶段运行 VM。
- 可释放：通过 FFI `calloc/free` 分配和释放状态表、字符类表、VM 工作区。
- 无魔法数字：ASCII byte、opcode、flag、容量策略和 repeat 上限都使用命名常量。
- 语法边界明确：支持完整的 ASCII byte 级子集，并清楚记录不支持项。

## 3. 模块职责

`regex.api`

- 公开 facade。
- 导出 `Regex`、`MatchResult`、`RegexStatus` 类型别名。
- 提供 `compile`、`destroy`、`is_valid`、`search_compiled`、`fullmatch_compiled`、`search`、`fullmatch`、`status_code`。
- `Regex.valid` 和 `MatchResult.ok` 是定义在 `regex.types` 中的类型方法，
  通过 `regex.api` 暴露出的类型别名使用。

`regex.types`

- 定义 `RegexStatus`。
- 定义 VM 状态 `State`、字符类 range `ClassRange`、编译片段 `Fragment`、持有型 `Regex`、匹配结果 `MatchResult`。
- 定义 opcode、flag 和 `NO_STATE` 等程序表示常量。

`regex.ascii`

- 集中定义 ASCII byte 常量，例如 `CARET`、`DOLLAR`、`DOT`、`BACKSLASH`、`DIGIT_0`、`LOWER_D`。
- 提供 `byte_at`、`is_digit`、`decimal_value`、`escaped_byte`。

`regex.alloc`

- 声明 C ABI `calloc` 和 `free`。
- 提供 `alloc_zeroed_bytes` 和 `free_bytes`。

`regex.program`

- 管理编译后 NFA 程序。
- 分配/扩容/释放状态表和字符类表。
- 提供构造片段的 helper：literal、any、class、assert、split、epsilon、concat、alternate、repeat。

`regex.parser`

- 递归下降解析 pattern。
- 支持分组、交替、字符类、转义、量词、bounded repeat。
- 将 pattern 编译为 `regex.types.Regex`。

`regex.engine`

- 运行 NFA VM。
- 使用当前列表、下一列表、visited marks 和显式 stack。
- `search_compiled` 扫描所有起点；`fullmatch_compiled` 要求从 0 到输入结尾。
- 在同一起点下记录可到达的最长匹配终点。

## 4. Public API

```aurex
pub type Regex = regex.types.Regex;
pub type MatchResult = regex.types.MatchResult;
pub type RegexStatus = regex.types.RegexStatus;

pub fn compile(pattern: str) -> Regex;
pub fn destroy(compiled: &mut Regex) -> void;
pub fn is_valid(compiled: &Regex) -> bool;

pub fn search_compiled(compiled: &Regex, input: str) -> MatchResult;
pub fn fullmatch_compiled(compiled: &Regex, input: str) -> MatchResult;

pub fn search(pattern: str, input: str) -> bool;
pub fn fullmatch(pattern: str, input: str) -> bool;

pub fn status_code(status: RegexStatus) -> i32;
```

`RegexStatus`：

```aurex
pub enum RegexStatus: u8 {
    ok = 0,
    no_match = 1,
    syntax_error = 2,
    unsupported = 3,
    out_of_memory = 4,
    repeat_too_large = 5,
}
```

`MatchResult` 字段：

```aurex
pub struct MatchResult {
    pub matched: bool;
    pub start: usize;
    pub end: usize;
    pub status: RegexStatus;
}
```

调用规则：

- `search` / `fullmatch` 是 convenience API：内部 compile、match、destroy，返回 bool。
- `compile` 返回持有 FFI 分配内存的 `Regex`，调用方必须 `destroy(&mut compiled)`。
- 推荐写 `defer regex.destroy(&mut compiled);`，避免早返回泄漏。
- `compiled.valid()` 等价于 `regex.is_valid(&compiled)`。
- `result.ok()` 表示 `result.status == RegexStatus.ok && result.matched`。
- `compiled.status != RegexStatus.ok` 时，匹配函数直接返回对应状态。
- 没有匹配是正常运行结果：`MatchResult { matched: false, status: no_match }`。

## 5. 正则语法

本库匹配单位是 UTF-8 `str` 的 byte，不是 Unicode scalar 或 grapheme。

### 5.1 顶层文法

```text
regex       = alternation
alternation = sequence ("|" sequence)*
sequence    = piece*
piece       = atom quantifier?
atom        = literal
            | escaped
            | "."
            | "^"
            | "$"
            | char_class
            | "(" alternation ")"
quantifier  = "*"
            | "+"
            | "?"
            | "{" decimal "}"
            | "{" decimal "," decimal "}"
            | "{" decimal "," "}"
```

空 sequence 合法，用 epsilon 表示。因此 `a|`、`|a` 和 `()` 都能表达空分支或空分组。

### 5.2 字面量

没有特殊含义的 byte 按自身匹配：

```text
abc
hello
```

这些字符在 atom 位置有特殊含义，需要转义成字面量：

```text
^ $ . * + ? { } ( ) [ ] | \
```

示例：

```text
\.
\*
\(
\\
```

### 5.3 转义

字面量转义：

```text
\n    line feed
\r    carriage return
\t    tab
\0    NUL byte
\x    其他 x 按 x 自身匹配
```

预定义 ASCII 类：

```text
\d    [0-9]
\D    [^0-9]
\w    [0-9A-Za-z_]
\W    [^0-9A-Za-z_]
\s    [ \t\n\r\f\v]
\S    [^ \t\n\r\f\v]
```

### 5.4 通配和锚点

```text
.     匹配任意一个 byte
^     仅在当前位置是输入开头时成功
$     仅在当前位置是输入结尾时成功
```

锚点是零宽断言，不消耗输入。

### 5.5 字符类

支持普通集合、范围和取反：

```text
[abc]
[a-z]
[A-Za-z0-9_]
[^0-9]
```

类内支持：

```text
\d
\w
\s
\]
\-
\\
```

类内范围两端必须是 byte 字面量，不能用预定义类当范围端点：

```text
[a-z]      // 合法
[\d-z]     // 非法
```

当前类内不支持 `\D`、`\W`、`\S` 作为单项；需要写外层取反，例如 `[^0-9]`。

### 5.6 分组和交替

分组只做组合，不捕获：

```text
(cat|dog)s?
^(ab|cd){2,3}$
```

交替优先级低于连接：

```text
ab|cd      // 等价于 (ab)|(cd)
a(b|c)d   // 匹配 abd 或 acd
```

### 5.7 量词

支持：

```text
a*        0 次或多次
a+        1 次或多次
a?        0 次或 1 次
a{3}      恰好 3 次
a{2,4}    2 到 4 次
a{2,}     至少 2 次
```

量词只作用于前一个 atom。前一个 atom 可以是 literal、class、`.`、锚点或分组。

当前 bounded repeat 通过复制 atom 片段展开，展开上限由实现常量控制。超过上限会返回 `RegexStatus.repeat_too_large`，避免无界膨胀。

## 6. 匹配语义

`fullmatch(pattern, input)`：

- 必须从 input byte offset `0` 开始。
- 必须在 input byte length 处结束。
- pattern 自身不需要显式写 `^...$`。

`search(pattern, input)`：

- 从 byte offset `0` 到 `strblen(input)` 逐个尝试起点。
- 返回第一个能匹配的起点。
- 在同一个起点下，VM 记录能到达的最长匹配终点。

`MatchResult.start` 和 `MatchResult.end` 都是 byte offset。由于 `str` 是 UTF-8 文本，offset 不一定是 Unicode 字符下标。

## 7. 示例矩阵

| Pattern | Input | API | 结果 | 说明 |
| --- | --- | --- | --- | --- |
| `^h[ae]llo+$` | `hellooo` | `fullmatch` | true | 锚点、字符类、`+` |
| `colou?r` | `the color token` | `search` | true | `?` 可选 |
| `a.+z` | `aaaz` | `search` | true | `.` 和 `+` |
| `\d{2,4}` | `2026` | `fullmatch` | true | 预定义类和 bounded repeat |
| `\d{2,4}` | `7` | `fullmatch` | false | repeat 下限 |
| `(cat|dog)s?` | `dogs` | `fullmatch` | true | 分组、交替、可选 |
| `[^0-9]+` | `abc_` | `fullmatch` | true | 取反字符类 |
| `\w+@\w+\.com` | `mail: team@example.com;` | `search` | true | 转义点和 word class |
| `[abc` | `a` | `search` | false | convenience API 对 syntax error 返回 false |
| `a{33}` | `a` | `compile` | `repeat_too_large` | repeat 展开保护 |

## 8. 完整演示

`examples/regex_demo.ax` 展示了：

- import alias：`import regex.api as regex;`
- 函数类型：`type Matcher = fn(str, str) -> bool;`
- struct literal：`Case { code, passed }`
- enum match：匹配 `RegexStatus`
- compiled API：`compile`、`fullmatch_compiled`、`destroy`
- `defer`：释放 compiled regex
- 方法调用：`compiled.valid()`、`result.ok()`

## 9. 已知不支持项

这些语法不是遗漏，而是当前实现边界：

- 捕获组、命名组、捕获结果数组。
- 反向引用。
- lookahead/lookbehind。
- 懒惰量词 `*?`、`+?`、`??`。
- possessive 量词。
- inline flags，例如 `(?i)`。
- Unicode property，例如 `\p{L}`。
- 多行模式、dotall 模式。
- 替换模板。
- 标准库风格 RAII；当前必须手动 destroy。

## 10. 测试入口

正则示例测试位于：

```text
tests/gtest/integration/examples_tests.cpp
```

手动验证命令：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
```

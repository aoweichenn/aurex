# Aurex 正则库语法和实现说明

日期：2026-05-17
阶段：M2 language-core-no-std，安全工业路线语法面扩展已实现
状态：按当前仓库 `examples/libs/regex` 实现编写。本文是正则库的语法、API、模块、性能和工业级差距说明，避免后续只靠上下文记忆查范围。

## 1. 位置和导入

正则库是独立模块目录，不放在 `text` 下：

```text
examples/libs/regex/api.ax
examples/libs/regex/compile/parser.ax
examples/libs/regex/compile/program.ax
examples/libs/regex/config/limits.ax
examples/libs/regex/core/results.ax
examples/libs/regex/core/types.ax
examples/libs/regex/ops/iter.ax
examples/libs/regex/ops/replace.ax
examples/libs/regex/ops/split.ax
examples/libs/regex/runtime/alloc.ax
examples/libs/regex/syntax/ascii.ax
examples/libs/regex/vm/engine.ax
```

外部调用只导入 facade：

```aurex
import regex.api as regex;
```

编译示例：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
build/bin/aurexc -I examples/libs examples/regex_phase1.ax -o build/tests/regex_phase1
build/tests/regex_phase1
build/bin/aurexc -I examples/libs examples/regex_industrial.ax -o build/tests/regex_industrial
build/tests/regex_industrial
build/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

## 2. 设计目标

这个库的目标不是复刻 PCRE，而是在当前 Aurex M2 能力范围内实现一个结构完整、可编译、可运行、可测试，并能继续向工业级推进的正则库：

- API 独立：调用方只面对 `regex.api`。
- 实现分层：parser、program、engine、types、results、ops、alloc、ascii 各自负责一类问题。
- 编译型：`compile` 把 pattern 编译成 NFA 状态表，匹配阶段运行 VM。
- 显式所有权：`Regex` 和 `Captures` 都持有 FFI 堆内存，分别用 `destroy` 和 `destroy_captures` 释放。
- 无魔法数字：ASCII byte、opcode、flag、错误 kind、容量策略、资源上限和 repeat 上限都使用命名常量。
- 资源可见：公开状态数、range 数、捕获数、编译后程序内存估算和 VM 工作区内存估算。
- API 可组合：提供 compiled API、便利 API、find/captures 游标、split 游标、替换到调用方 buffer 的接口。
- 语法面走 RE2/Rust regex 风格的安全路线：补齐 inline flags、scoped flags、lazy/ungreedy、word boundary、absolute anchors、hex/unicode byte encoding escapes、quoted literal、newline escape、POSIX/ASCII property classes，同时继续拒绝反向引用和 lookaround。
- 向工业级演进：当前实现明确约束 pattern/program/workspace/capture 上限，避免灾难性回溯，并用 demo、phase1、industrial、stress 样例锁住行为。

当前版本仍是 Aurex 语言能力验证库，不声称达到 RE2、Rust regex、PCRE2 或 Hyperscan 的生产成熟度。本文后面单独列出工业级目标和对比。

## 3. 模块职责

`regex.api`

- 公开 facade。
- 导出 `Regex`、`MatchResult`、`RegexStatus`、`Captures`、`CaptureSpan`、`FindIter`、`CaptureIter`、`SplitIter`、`SplitPart`、`ReplaceResult` 类型别名。
- 提供编译、释放、匹配、捕获、迭代、替换、分割、错误诊断和资源查询 API。
- `Regex.valid`、`MatchResult.ok`、`Captures.ok`、`SplitPart.ok`、`ReplaceResult.ok` 是定义在 `regex.core.types` 中的类型方法，通过 facade 暴露出的类型别名使用。

`regex.core.types`

- 定义 `RegexStatus`。
- 定义 VM 状态 `State`、字符类 range `ClassRange`、捕获元信息 `CaptureInfo`、捕获 span `CaptureSpan`、编译片段 `Fragment`、持有型 `Regex`、匹配结果 `MatchResult`、捕获结果 `Captures`、游标和替换结果。
- 定义 opcode、flag、错误 kind、`NO_STATE`、`NO_CAPTURE`、`CAPTURE_MISSING` 等程序表示常量。

`regex.core.results`

- 管理 `Captures` 的分配、释放和查询。
- 提供 `capture_at`、`capture_text`、`capture_name`、`capture_index`。
- `capture_name` 返回 pattern 中命名捕获组名字的 borrowed `str`。
- `capture_text` 返回 input 中捕获 span 对应的 borrowed `str`。

`regex.config.limits`

- 集中定义容量策略和资源上限。
- 包含 `MAX_PATTERN_BYTES`、`MAX_STATE_CAPACITY`、`MAX_RANGE_CAPACITY`、`MAX_WORKSPACE_STATES`、`MAX_BOUNDED_REPEAT`、`MAX_CAPTURE_GROUPS`。
- 包含工作区数组数量 `WORKSPACE_LIST_COUNT` 和捕获 row 数量 `WORKSPACE_CAPTURE_ROW_COUNT`，用于内存估算。

`regex.syntax.ascii`

- 集中定义 ASCII byte 常量，例如 `CARET`、`DOLLAR`、`DOT`、`BACKSLASH`、`LESS`、`GREATER`、`DIGIT_0`、`LOWER_D`。
- 提供 `byte_at`、`is_digit`、`is_alpha`、`is_name_start`、`is_name_continue`、`decimal_value`、`escaped_byte`。

`regex.runtime.alloc`

- 声明 C ABI `calloc` 和 `free`。
- 提供 `alloc_zeroed_bytes` 和 `free_bytes`。

`regex.compile.program`

- 管理编译后 NFA 程序。
- 分配/扩容/释放状态表、字符类表和捕获元信息表。
- 检查 pattern bytes、state capacity、range capacity 和 capture count 上限。
- 提供构造片段的 helper：literal、any、class、assert、split、epsilon、save、match、concat、alternate、repeat、capture。
- 提供 `state_count`、`range_count`、`program_bytes`。

`regex.compile.parser`

- 递归下降解析 pattern。
- 支持捕获组、命名捕获组、非捕获组、inline flags/scoped flags、交替、字符类、POSIX/ASCII property 类、转义、绝对锚点、word boundary、greedy/lazy/ungreedy 量词、bounded repeat。
- 将 pattern 编译为 `regex.core.types.Regex`。
- 将语法错误写入 `Regex.status`、`Regex.error_offset`、`Regex.error_kind`。

`regex.vm.engine`

- 运行 NFA VM。
- 使用 current/next state 列表、visited marks、显式 stack，以及对应的 capture snapshot row。
- `search_compiled` 扫描所有起点；`fullmatch_compiled` 要求从 0 到输入结尾。
- `captures_compiled` 和 `captures_fullmatch_compiled` 返回 owning `Captures`。
- 在同一起点下使用有序 Thompson 线程。greedy 分支会保留后备较长结果，lazy/ungreedy 分支可提前接受较短结果。
- 对非 multiline 的 `^` 和 `\A` 开头 pattern 做 anchored 起点优化，只尝试 offset `0`；`(?m)^` 仍会扫描行首。
- 检查工作区容量，提供 `workspace_bytes`。

`regex.ops.iter`

- 提供 C++ iterator 风格的显式游标 API。
- `find_iter` / `find_next` 返回连续 `MatchResult`。
- `captures_iter` / `captures_next` 返回连续 `Captures`，每个返回值需要调用 `destroy_captures`。
- 对零长度匹配做 1 byte 前进，避免无限循环。

`regex.ops.replace`

- 实现 `replace_all`。
- 不创建拥有型字符串；调用方传入 `*mut u8` 和容量。
- 返回 `ReplaceResult { status, written, required, replacements }`。
- 支持 literal replacement、`$$`、`$0`..`$9`、`${name}`。

`regex.ops.split`

- 实现 `split_iter` / `split_next`。
- 返回 input 的 byte span，不分配字符串。
- 对零长度分隔符做 1 byte 前进，避免无限循环。

## 4. Public API

```aurex
pub type Regex = regex.core.types.Regex;
pub type MatchResult = regex.core.types.MatchResult;
pub type RegexStatus = regex.core.types.RegexStatus;
pub type CaptureSpan = regex.core.types.CaptureSpan;
pub type Captures = regex.core.types.Captures;
pub type FindIter = regex.core.types.FindIter;
pub type CaptureIter = regex.core.types.CaptureIter;
pub type SplitIter = regex.core.types.SplitIter;
pub type SplitPart = regex.core.types.SplitPart;
pub type ReplaceResult = regex.core.types.ReplaceResult;

pub fn compile(pattern: str) -> Regex;
pub fn destroy(compiled: &mut Regex) -> void;
pub fn is_valid(compiled: &Regex) -> bool;

pub fn state_count(compiled: &Regex) -> usize;
pub fn range_count(compiled: &Regex) -> usize;
pub fn program_bytes(compiled: &Regex) -> usize;
pub fn workspace_bytes(compiled: &Regex) -> usize;
pub fn capture_count(compiled: &Regex) -> usize;

pub fn max_pattern_bytes() -> usize;
pub fn max_state_capacity() -> usize;
pub fn max_range_capacity() -> usize;
pub fn max_workspace_states() -> usize;
pub fn max_bounded_repeat() -> usize;
pub fn max_capture_groups() -> usize;

pub fn search_compiled(compiled: &Regex, input: str) -> MatchResult;
pub fn search_compiled_from(compiled: &Regex, input: str, start_offset: usize) -> MatchResult;
pub fn fullmatch_compiled(compiled: &Regex, input: str) -> MatchResult;
pub fn search(pattern: str, input: str) -> bool;
pub fn is_match(pattern: str, input: str) -> bool;
pub fn find(pattern: str, input: str) -> MatchResult;
pub fn fullmatch(pattern: str, input: str) -> bool;
pub fn captures(pattern: str, input: str) -> Captures;
pub fn replace(pattern: str, input: str, replacement: str, out: *mut u8, out_capacity: usize) -> ReplaceResult;

pub fn captures_compiled(compiled: &Regex, input: str) -> Captures;
pub fn captures_fullmatch_compiled(compiled: &Regex, input: str) -> Captures;
pub fn destroy_captures(captures: &mut Captures) -> void;
pub fn capture_at(captures: &Captures, index: usize) -> CaptureSpan;
pub fn capture_text(input: str, captures: &Captures, index: usize) -> str;
pub fn capture_name(compiled: &Regex, index: usize) -> str;
pub fn capture_index(compiled: &Regex, name: str) -> usize;
pub fn no_capture() -> usize;

pub fn find_iter(compiled: &Regex, input: str) -> FindIter;
pub fn find_next(iter: &mut FindIter) -> MatchResult;
pub fn captures_iter(compiled: &Regex, input: str) -> CaptureIter;
pub fn captures_next(iter: &mut CaptureIter) -> Captures;

pub fn split_iter(compiled: &Regex, input: str) -> SplitIter;
pub fn split_next(iter: &mut SplitIter) -> SplitPart;

pub fn replace_all(compiled: &Regex, input: str, replacement: str, out: *mut u8, out_capacity: usize) -> ReplaceResult;

pub fn error_offset(compiled: &Regex) -> usize;
pub fn error_kind(compiled: &Regex) -> u8;
pub fn status_code(status: RegexStatus) -> i32;
pub fn error_kind_code(kind: u8) -> i32;
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
    pattern_too_large = 6,
    program_too_large = 7,
    workspace_too_large = 8,
    capture_too_large = 9,
    invalid_group_name = 10,
    buffer_too_small = 11,
    replacement_error = 12,
}
```

核心结果结构：

```aurex
pub struct MatchResult {
    pub matched: bool;
    pub start: usize;
    pub end: usize;
    pub status: RegexStatus;
}

pub struct CaptureSpan {
    pub matched: bool;
    pub start: usize;
    pub end: usize;
}

pub struct Captures {
    pub matched: bool;
    pub start: usize;
    pub end: usize;
    pub status: RegexStatus;
    pub spans: *mut CaptureSpan;
    pub span_len: usize;
    pub span_capacity: usize;
}

pub struct ReplaceResult {
    pub status: RegexStatus;
    pub written: usize;
    pub required: usize;
    pub replacements: usize;
}
```

调用规则：

- `search` / `is_match` / `fullmatch` 是 bool convenience API：内部 compile、match、destroy。
- `find` 是 span convenience API，返回 `MatchResult`。
- `captures` 是 owning capture convenience API，调用方仍必须 `destroy_captures`。
- `replace` 是 buffer replacement convenience API，内部 compile、replace、destroy。
- `compile` 返回持有 FFI 分配内存的 `Regex`，调用方必须 `destroy(&mut compiled)`。
- `captures_compiled`、`captures_fullmatch_compiled` 和 `captures_next` 返回持有 FFI 分配内存的 `Captures`，调用方必须 `destroy_captures(&mut captures)`。
- 推荐写 `defer regex.destroy(&mut compiled);` 和 `defer regex.destroy_captures(&mut captures);`，避免早返回泄漏。
- `compiled.valid()` 等价于 `regex.is_valid(&compiled)`。
- `result.ok()` 表示 `result.status == RegexStatus.ok && result.matched`。
- `captures.ok()` 表示 `captures.status == RegexStatus.ok && captures.matched`。
- `replace_result.ok()` 表示输出缓冲区容量足够且状态为 `ok`。
- `compiled.status != RegexStatus.ok` 时，匹配函数直接返回对应状态。
- 没有匹配是正常运行结果：`MatchResult { matched: false, status: no_match }` 或 `Captures { matched: false, status: no_match }`。
- `program_bytes` 是状态表容量、字符类 range 表容量、捕获元信息表容量的估算字节数。
- `workspace_bytes` 是一次匹配需要的 VM 工作区估算字节数，包括 capture snapshot row。
- convenience API 每次调用都会 compile/destroy；循环、大量输入或服务端路径应优先使用 compiled API。

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
            | "(?:" alternation ")"
            | "(?<" name ">" alternation ")"
            | "(?" flags ")"
            | "(?" flags ":" alternation ")"
quantifier  = "*"
            | "+"
            | "?"
            | "{" decimal "}"
            | "{" decimal "," decimal "}"
            | "{" decimal "," "}"
            | "*?"
            | "+?"
            | "??"
            | "{" decimal "}" "?"
            | "{" decimal "," decimal "}" "?"
            | "{" decimal "," "}" "?"
flags       = flag* ("-" flag*)?
flag        = "i" | "m" | "s" | "x" | "U" | "u"
name        = [A-Za-z_][A-Za-z0-9_]*
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
\a    bell
\e    escape
\f    form feed
\n    line feed
\r    carriage return
\t    tab
\v    vertical tab
\0    NUL byte
\xNN        2 位 hex byte/codepoint
\x{H...}    braced hex codepoint，编译成 UTF-8 byte 序列
\uNNNN      4 位 hex codepoint，编译成 UTF-8 byte 序列
\u{H...}    braced Unicode codepoint，编译成 UTF-8 byte 序列
\Q...\E     quoted literal，区间内元字符按字面量处理
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

零宽断言和换行相关转义：

```text
\A    输入绝对开头
\z    输入绝对结尾
\Z    当前实现等同 \z
\b    ASCII word boundary，word = [0-9A-Za-z_]
\B    非 word boundary
\N    任意非 LF byte
\R    LF、CRLF 或 CR
```

未知字母转义会被诊断为 `unsupported` + `invalid_escape`，避免 typo 被静默当字面量。未知标点转义仍按标点字面量处理，例如 `\.`。

### 5.4 通配和锚点

```text
.     默认匹配除 LF 外任意一个 byte；`(?s)` dotall 下匹配任意 byte
^     默认仅在输入开头成功；`(?m)` multiline 下也在 LF 后成功
$     默认仅在输入结尾成功；`(?m)` multiline 下也在 LF 前成功
```

锚点是零宽断言，不消耗输入。

### 5.5 Inline flags

支持全局后续生效和 scoped 两种形式：

```text
(?i)abc          // 从当前位置开始忽略 ASCII 大小写
(?i:a)b          // 只在 scoped group 内忽略大小写
(?i)abc(?-i:de)  // 局部关闭 i
```

当前 flag：

| flag | 语义 |
| --- | --- |
| `i` | ASCII case-insensitive，用 `A-Z`/`a-z` 折叠 literal 和 class |
| `m` | multiline，使 `^`/`$` 识别行首/行尾 |
| `s` | dotall，使 `.` 匹配 LF |
| `x` | extended，忽略 pattern 中 ASCII 空白，`#` 到行尾为注释；转义空格 `\ ` 仍是字面量 |
| `U` | ungreedy，默认量词改为 lazy，显式 `?` 后缀会反转回来 |
| `u` | 保留 flag；当前匹配模型仍是 UTF-8 byte 级，不启用完整 Unicode 属性语义 |

### 5.6 字符类

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
\D
\w
\W
\s
\S
\p{Alpha}
\P{Digit}
[[:alpha:]]
[[:^digit:]]
\]
\-
\\
```

类内范围两端必须是 byte 字面量，不能用预定义类当范围端点：

```text
[a-z]      // 合法
[a-\d]     // 非法
```

如果预定义类后面跟 `-`，该 `-` 会作为普通字面量处理，而不是把预定义类作为 range 起点。

当前 POSIX/property 名称是 ASCII-mapped 子集，不是完整 Unicode property 数据库。已支持：

```text
digit, nd, decimal, decimal_number
w, word
s, space, whitespace, white_space
l, letter, alpha, alphabetic
alnum, alphanumeric
ascii
blank
cntrl, control
graph
lower, lowercase
print
punct, punctuation
upper, uppercase
xdigit, hex, hex_digit
```

### 5.7 分组、捕获和交替

普通分组默认捕获：

```text
(cat|dog)s?
```

非捕获分组：

```text
(?:com|net)
```

命名捕获：

```text
(?<user>\w+)@(?<domain>\w+\.(?:com|net))
```

捕获编号从 `0` 开始：

- `0` 是整体匹配。
- `1..capture_count - 1` 是按左括号出现顺序编号的捕获组。
- 非捕获组 `(?:...)` 不占编号。
- `capture_index(compiled, "name")` 返回命名捕获组编号；找不到返回 `no_capture()`。

交替优先级低于连接：

```text
ab|cd      // 等价于 (ab)|(cd)
a(b|c)d   // 匹配 abd 或 acd
```

### 5.8 量词

支持：

```text
a*        0 次或多次
a+        1 次或多次
a?        0 次或 1 次
a{3}      恰好 3 次
a{2,4}    2 到 4 次
a{2,}     至少 2 次
a*?       lazy 0 次或多次
a+?       lazy 1 次或多次
a??       lazy 0 次或 1 次
a{2,4}?   lazy bounded repeat
(?U)a*    ungreedy flag 下默认 lazy
```

量词只作用于前一个 atom。前一个 atom 可以是 literal、class、`.`、锚点或分组。

当前 bounded repeat 通过复制 atom 片段展开，展开上限由实现常量控制。超过上限会返回 `RegexStatus.repeat_too_large`，避免无界膨胀。

## 6. 匹配和捕获语义

`fullmatch(pattern, input)`：

- 必须从 input byte offset `0` 开始。
- 必须在 input byte length 处结束。
- pattern 自身不需要显式写 `^...$`。

`search(pattern, input)`：

- 从 byte offset `0` 到 `strblen(input)` 逐个尝试起点。
- 返回第一个能匹配的起点。
- 在同一个起点下，VM 按 NFA split 顺序维护有序线程。greedy 量词优先继续消费，并保存后备接受点；lazy/ungreedy 量词优先退出，可在更短位置接受。

`search_compiled_from(compiled, input, start_offset)`：

- 从指定 byte offset 开始搜索。
- `start_offset > strblen(input)` 时返回 `no_match`。

`MatchResult.start`、`MatchResult.end`、`CaptureSpan.start`、`CaptureSpan.end` 都是 byte offset。由于 `str` 是 UTF-8 文本，offset 不一定是 Unicode 字符下标。

捕获实现说明：

- 编译期为捕获组插入 `OP_SAVE` 状态，保存 start/end slot。
- VM 每个活动 state 携带一行 capture snapshot。
- 到达 accept 时复制当前 capture row 为 best capture row。
- 被 bounded repeat 复制的捕获组复用同一个 capture slot；多次迭代时保留最后一次参与匹配的 span。
- 当前 VM 仍按 state id 去重活动列表，并用捕获 row 记录可接受结果。多数常规模式能得到稳定结果，greedy/lazy 的外部 span 已被测试锁住；复杂交替中的完整 submatch precedence 还没有做到 PCRE/Rust/RE2 级精确兼容。后续若要工业级精确兼容，需要定义并测试完整子匹配优先级。

## 7. Iterator、replace、split

`find_iter` / `find_next`：

```aurex
var iter: regex.FindIter = regex.find_iter(&compiled, "a1 b22");
let first: regex.MatchResult = regex.find_next(&mut iter);
let second: regex.MatchResult = regex.find_next(&mut iter);
```

- 每次返回下一个 match span。
- 结束时返回 `status == no_match`。
- 零长度匹配会至少前进 1 byte。

`captures_iter` / `captures_next`：

```aurex
var iter: regex.CaptureIter = regex.captures_iter(&compiled, input);
var captures: regex.Captures = regex.captures_next(&mut iter);
defer regex.destroy_captures(&mut captures);
```

- 每次返回一个 owning `Captures`。
- 调用方必须释放每个成功或失败返回的 `Captures`；空结果释放是安全的。

`replace_all`：

```aurex
var buffer: [64]u8 = [0; 64];
let out: *mut u8 = unsafe { ptrcast[*mut u8](ptrat[*mut [64]u8](ptraddr(&mut buffer))) };
let result: regex.ReplaceResult = regex.replace_all(&compiled, input, "${domain}/$1", out, 64usize);
```

- 输出写入调用方 buffer。
- `required` 是完整输出所需 byte 数。
- `written` 是实际写入 byte 数；当 buffer 太小时，状态为 `buffer_too_small`，但仍会尽量写入前缀。
- replacement 模板支持：
  - `$$`：字面量 `$`
  - `$0`：整体匹配
  - `$1`..`$9`：编号捕获
  - `${name}`：命名捕获
- 无效模板或未知 name 返回 `replacement_error`。

`split_iter` / `split_next`：

```aurex
var iter: regex.SplitIter = regex.split_iter(&comma, "a, b,c");
let part: regex.SplitPart = regex.split_next(&mut iter);
```

- pattern 是分隔符。
- 返回 input 的 byte span，不分配字符串。
- 结束时返回 `status == no_match`。
- 零长度分隔符会至少前进 1 byte。

## 8. 错误诊断

`compile` 失败时可读：

```aurex
compiled.status
regex.error_offset(&compiled)
regex.error_kind(&compiled)
regex.error_kind_code(regex.error_kind(&compiled))
```

`error_offset` 是 pattern 的 byte offset。`error_kind_code` 当前映射：

| code | kind | 说明 |
| ---: | --- | --- |
| 0 | none | 无错误 |
| 1 | unexpected_token | 非法 token 或重复量词 |
| 2 | unclosed_group | 分组未闭合 |
| 3 | unclosed_class | 字符类未闭合或空字符类 |
| 4 | invalid_repeat | `{m,n}` 语法错误 |
| 5 | repeat_too_large | bounded repeat 超过上限 |
| 6 | invalid_range | 字符类 range 非法 |
| 7 | unsupported_escape | 当前不支持的扩展语法或属性类 |
| 8 | trailing_input | parser 完成后仍有未消费输入 |
| 9 | capture_too_large | 捕获组数超过上限 |
| 10 | invalid_group_name | 命名捕获组名字非法 |
| 11 | unexpected_eof | 转义或扩展语法遇到 EOF |
| 12 | program_too_large | 程序容量超限 |
| 13 | invalid_escape | 非法 hex/unicode escape 或未知字母转义 |

## 9. 性能和内存要求

当前实现采用 Thompson 风格 NFA 程序和列表式 VM，不使用递归回溯，因此不会出现 PCRE 风格回溯引擎常见的指数级灾难性回溯。

资源上限集中在 `regex.config.limits`：

| 名称 | 当前值 | 作用 |
| --- | ---: | --- |
| `MAX_PATTERN_BYTES` | 4096 | 单个 pattern 的 UTF-8 byte 长度上限 |
| `MAX_STATE_CAPACITY` | 65536 | 编译后 NFA state 容量上限 |
| `MAX_RANGE_CAPACITY` | 32768 | 字符类 range 容量上限 |
| `MAX_WORKSPACE_STATES` | 65536 | 单次 VM 工作区可容纳的 state 上限 |
| `MAX_BOUNDED_REPEAT` | 32 | `{m,n}` 展开上限 |
| `MAX_CAPTURE_GROUPS` | 64 | 捕获组上限，不含 `$0` |
| `WORKSPACE_LIST_COUNT` | 4 | current、next、marks、stack 四组 VM state 数组 |
| `WORKSPACE_CAPTURE_ROW_COUNT` | 4 | current、next、stack、best 四组捕获 row |

复杂度和内存模型：

- 编译时间目标：`O(pattern_bytes + emitted_states + class_ranges + capture_groups)`。
- 编译期内存：`program_bytes = state_capacity * sizeof[State] + range_capacity * sizeof[ClassRange] + capture_capacity * sizeof[CaptureInfo]`。
- 匹配工作区：`workspace_bytes = state_count * 4 * sizeof[usize] + state_count * capture_slots * 4 * sizeof[usize]`。
- `capture_slots = capture_count * 2`，每个捕获含 start/end 两个 slot，`capture_count` 包含 `$0`。
- `fullmatch_compiled` 匹配时间：`O(input_bytes * active_state_count * capture_slots)`，capture row copy 是常数上限内的线性 slot 拷贝。
- `search_compiled` 匹配时间：当前会尝试多个起点，最坏是 `O(input_bytes * input_bytes * active_state_count * capture_slots)`；当 pattern 以非 multiline `^` 或 `\A` 开头时只尝试起点 `0`。
- 同一起点采用有序线程接受策略：greedy 量词保留后备较长结果，lazy/ungreedy 量词可优先接受较短结果。
- `search` / `fullmatch` convenience API 会每次重新编译，性能敏感路径必须使用 `compile` 后复用。
- `replace_all` 只写调用方 buffer，不分配输出字符串；捕获结果当前逐 match 分配，后续可优化为复用 scratch captures。

压力样例：

- `examples/regex_demo.ax`：基础语法和 compiled API。
- `examples/regex_phase1.ax`：捕获、命名捕获、find/captures iterator、replace、split、错误 offset/kind。
- `examples/regex_industrial.ax`：flags、lazy、边界断言、扩展 escape、POSIX/property class、便利 API 和非法 escape 诊断。
- `examples/regex_stress.ax`：数百次重复 compiled search/fullmatch、资源预算和错误路径。

后续向工业级继续推进时，优先级如下：

- 将 `search_compiled` 从多起点 NFA 扫描推进到真正线性时间搜索，加入 literal prefix / start byte 加速。
- 给捕获优先级定义完整 submatch precedence，并增加差分测试。
- `replace_all` 增加 scratch capture 复用，避免每个 match 分配。
- 扩展 Unicode 时需要明确 byte、scalar、grapheme 三层语义，避免和当前 byte offset API 混淆。
- 增加 fuzz、差分测试和长输入基准，把正确性、峰值内存和吞吐纳入自动化。

## 10. 与工业级引擎对比

| 引擎 | 核心定位 | 复杂度/性能特点 | 功能范围 | Aurex 当前差距 |
| --- | --- | --- | --- | --- |
| RE2 | 生产级 C++ 正则引擎，偏有限自动机语义 | 以避免回溯爆炸为核心设计，匹配时间受输入规模约束 | 捕获、命名捕获、替换、迭代、flags、边界、丰富 ASCII/Unicode 语法；不支持反向引用、lookaround 等会破坏线性时间保证的特性 | Aurex 也避免回溯，并补齐安全语法面核心子集，但缺少成熟 DFA/NFA 混合优化、Unicode 数据库、完整 submatch 语义和多年工程验证 |
| Rust `regex` crate | Rust 生态常用正则库 | 文档承诺搜索复杂度可界定，通常用 NFA/DFA/literal 加速组合 | captures、find_iter、captures_iter、replace、split、flags、Unicode/bytes API | Aurex API 方向接近，已补 flags/边界/类/escape/lazy 子集，但目前只有 byte 级语义，没有 literal acceleration、完整 Unicode、bytes/text 双 API 和成熟差分测试 |
| PCRE2 | Perl-compatible 功能优先引擎 | 功能很全，可使用 JIT；回溯模型需要限制资源避免病态 pattern | 捕获、命名组、反向引用、lookaround、条件、丰富选项 | Aurex 不追求 PCRE 兼容，暂不支持这些会明显增加复杂度或破坏线性保证的语法 |
| Hyperscan | 高吞吐多 pattern 扫描库 | 面向 block/stream/vectored 扫描和预编译 database，适合 IDS/日志类高吞吐场景 | 支持 PCRE-like 子集和多 pattern 批量匹配 | Aurex 没有多 pattern database、SIMD、streaming 状态和平台级优化 |
| Aurex regex | Aurex M2 写成的独立多模块正则库 | 编译 NFA + 有序 Thompson VM，无灾难性回溯；有资源上限和 stress/industrial 样例 | ASCII byte 子集、flags、lazy/ungreedy、边界、锚点、捕获/命名捕获、迭代、替换、分割、错误 offset/kind | 目标是验证语言工程能力并逐步演进，不把当前实现包装成工业级完成品 |

这个对比用于确定路线：Aurex 当前应优先学习 RE2/Rust regex 的“可界定复杂度、拒绝破坏线性时间的语法、显式资源预算、稳定 API”路线，而不是先追 PCRE2 的全部语法。

## 11. 示例矩阵

| Pattern | Input | API | 结果 | 说明 |
| --- | --- | --- | --- | --- |
| `^h[ae]llo+$` | `hellooo` | `fullmatch` | true | 锚点、字符类、`+` |
| `colou?r` | `the color token` | `search` | true | `?` 可选 |
| `a.+z` | `aaaz` | `search` | true | `.` 和 `+` |
| `\d{2,4}` | `2026` | `fullmatch` | true | 预定义类和 bounded repeat |
| `\d{2,4}` | `7` | `fullmatch` | false | repeat 下限 |
| `(cat|dog)s?` | `dogs` | `fullmatch` | true | 捕获分组、交替、可选 |
| `(?:cat|dog)s?` | `dogs` | `fullmatch` | true | 非捕获分组 |
| `(?<user>\w+)@(?<domain>\w+\.(?:com|net))` | `mail: team@example.com` | `captures_compiled` | `$1=team`, `${domain}=example.com` | 命名捕获 |
| `[^0-9]+` | `abc_` | `fullmatch` | true | 取反字符类 |
| `\w+@\w+\.com` | `mail: team@example.com;` | `search` | true | 转义点和 word class |
| `\s*,\s*` | `alpha, beta,gamma` | `split_iter` | `alpha`、`beta`、`gamma` | 分割 |
| `(?i)abc` | `ABC` | `fullmatch` | true | inline case-insensitive |
| `(?m)^warn$` | `info\nwarn\nend` | `find` | span `5..9` | multiline anchor |
| `a.*?b` | `axxbxxb` | `find` | span `0..4` | lazy 量词 |
| `\bword\b` | `a word!` | `find` | span `2..6` | word boundary |
| `\x41+` | `AAA` | `fullmatch` | true | hex escape |
| `\Q.^$*\E` | `.^$*` | `fullmatch` | true | quoted literal |
| `[[:alpha:]]+` | `abcXYZ` | `fullmatch` | true | POSIX ASCII class |
| `\p{Alpha}+` | `abcXYZ` | `fullmatch` | true | ASCII-mapped property class |
| `[abc` | `a` | `compile` | `syntax_error`, offset `0` | 未闭合 class |
| `abc[` | `a` | `compile` | `syntax_error`, offset `3` | 错误 offset |
| `a{33}` | `a` | `compile` | `repeat_too_large` | repeat 展开保护 |
| `\y` | `a` | `compile` | `unsupported`, kind `invalid_escape` | 未知字母转义 |

## 12. 完整演示

`examples/regex_demo.ax` 展示了：

- import alias：`import regex.api as regex;`
- 函数类型：`type Matcher = fn(str, str) -> bool;`
- struct literal：`Case { code, passed }`
- enum match：匹配 `RegexStatus`
- compiled API：`compile`、`fullmatch_compiled`、`destroy`
- `defer`：释放 compiled regex
- 方法调用：`compiled.valid()`、`result.ok()`

`examples/regex_phase1.ax` 展示了：

- 命名捕获：`(?<user>...)`、`(?<domain>...)`
- 非捕获组：`(?:com|net)`
- 捕获结果：`captures_compiled`、`capture_at`、`capture_text`、`capture_name`、`capture_index`
- `find_iter` / `find_next`
- `captures_iter` / `captures_next`
- `replace_all`：`${domain}/$1` 模板写入固定 buffer
- `split_iter` / `split_next`
- 错误诊断：`error_offset`、`error_kind_code`

`examples/regex_industrial.ax` 展示了：

- inline flags：`(?i)`、`(?m)`、`(?s)`、`(?x)`、`(?U)`。
- scoped flags：`(?i:a)` 和 `(?-i:...)`。
- greedy/lazy/ungreedy span 行为：`a.*b`、`a.*?b`、`(?U)a.*b`。
- 边界断言：`\A`、`\z`、`\b`、`\B`。
- 扩展 escape：`\xNN`、`\x{...}`、`\uNNNN`、`\u{...}`、`\Q...\E`、`\N`、`\R`。
- 类语法：类内 `\D`、POSIX class、ASCII property class、case-insensitive class。
- convenience API：`is_match`、`find`、`captures`、`replace`。
- invalid escape 诊断：`\y`、不完整 `\x`。

`examples/regex_stress.ax` 展示了：

- compiled API 在重复匹配中的复用。
- 资源预算查询：`state_count`、`range_count`、`program_bytes`、`workspace_bytes`。
- 上限查询：`max_state_capacity`、`max_range_capacity`、`max_bounded_repeat`。
- 大规模循环压力：固定迭代次数重复 search/fullmatch。
- 错误状态验证：repeat 上限和非法/不支持字符类。

## 13. 已知不支持项

这些语法不是遗漏，而是当前实现边界：

- 反向引用。
- lookahead/lookbehind。
- possessive 量词。
- 原子组、条件组、递归/子例程调用等 PCRE-only 高复杂度语法。
- 完整 Unicode property 数据库，例如真正的 `\p{Greek}`、`\p{Script=Han}`；当前 `\p{Alpha}` 等是 ASCII-mapped 子集。
- Unicode scalar/grapheme 级匹配；当前 offset 和匹配单位都是 UTF-8 byte。
- replacement 中的复杂转义、条件替换、函数式替换。
- 标准库风格 RAII；当前必须手动 destroy。

## 14. 测试入口

正则示例测试位于：

```text
tests/gtest/integration/examples_tests.cpp
```

手动验证命令：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
build/bin/aurexc -I examples/libs examples/regex_phase1.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_phase1.ax -o build/tests/regex_phase1
build/tests/regex_phase1
build/bin/aurexc -I examples/libs examples/regex_industrial.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_industrial.ax -o build/tests/regex_industrial
build/tests/regex_industrial
build/bin/aurexc -I examples/libs examples/regex_stress.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

## 15. 参考资料

工业级对比参考的是这些官方资料：

- RE2 README：说明 RE2 面向安全、线性匹配时间、可配置内存预算，并明确不支持反向引用和 lookaround。
  <https://raw.githubusercontent.com/google/re2/main/README.md>
- Rust `regex` crate 文档：说明性能建议、避免循环内重复编译、Unicode 成本、literal 加速和不可信输入边界。
  <https://docs.rs/regex/latest/regex/>
- PCRE2 syntax / JIT 文档：说明 PCRE2 的 Perl-compatible 语法面和 JIT 支持。
  <https://pcre2project.github.io/pcre2/doc/pcre2syntax/>
  <https://pcre2project.github.io/pcre2/doc/pcre2jit/>
- Hyperscan developer reference：说明编译成 database、block/vectored/streaming 扫描和运行期内存模型。
  <https://intel.github.io/hyperscan/dev-reference/intro.html>

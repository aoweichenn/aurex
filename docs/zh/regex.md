# Aurex 正则库语法和实现说明

日期：2026-05-17
阶段：M2 language-core-no-std，安全工业路线语法面扩展已实现
状态：按当前仓库 `examples/libs/regex` 实现编写。本文是正则库的语法、API、模块、性能和工业级差距说明，避免后续只靠上下文记忆查范围。

## 1. 位置和导入

正则库是独立模块目录，不放在 `text` 下：

```text
examples/libs/regex/api.ax
examples/libs/regex/bytes.ax
examples/libs/regex/compile/database.ax
examples/libs/regex/compile/parser.ax
examples/libs/regex/compile/program.ax
examples/libs/regex/compile/set.ax
examples/libs/regex/config/limits.ax
examples/libs/regex/core/results.ax
examples/libs/regex/core/types.ax
examples/libs/regex/ops/iter.ax
examples/libs/regex/ops/replace.ax
examples/libs/regex/ops/split.ax
examples/libs/regex/ops/stream.ax
examples/libs/regex/runtime/alloc.ax
examples/libs/regex/syntax/ascii.ax
examples/libs/regex/vm/engine.ax
```

外部调用只导入 facade：

```aurex
import regex.api as regex;
```

需要 raw byte 语义时导入 bytes facade：

```aurex
import regex.bytes as bytes;
```

编译示例：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
build/bin/aurexc -I examples/libs examples/regex_phase1.ax -o build/tests/regex_phase1
build/tests/regex_phase1
build/bin/aurexc -I examples/libs examples/regex_industrial.ax -o build/tests/regex_industrial
build/tests/regex_industrial
build/bin/aurexc -I examples/libs examples/regex_advanced.ax -o build/tests/regex_advanced
build/tests/regex_advanced
build/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

## 2. 设计目标

这个库的目标不是复刻 PCRE，而是在当前 Aurex M2 能力范围内实现一个结构完整、可编译、可运行、可测试，并能继续向工业级推进的正则库：

- API 独立：调用方只面对 `regex.api`。
- 实现分层：parser、program、engine、types、results、ops、alloc、ascii 各自负责一类问题；Unicode 标量、属性和 case fold 复用独立的 `unicode.ucd` 模块。
- 编译型：`compile` 把 pattern 编译成 NFA 状态表，匹配阶段运行 VM。
- 显式所有权：`Regex` 和 `Captures` 都持有 FFI 堆内存，分别用 `destroy` 和 `destroy_captures` 释放。
- 无魔法数字：ASCII byte、opcode、flag、错误 kind、容量策略、资源上限和 repeat 上限都使用命名常量。
- 资源可见：公开状态数、range 数、捕获数、编译后程序内存估算和 VM 工作区内存估算。
- API 可组合：提供 compiled API、便利 API、text/bytes 双入口、find/captures 游标、split/splitn 游标、模板替换、回调替换、RegexSet 多模式扫描、可序列化 set database 和文本 stream 状态接口。
- 语法面走 RE2/Rust regex 风格的安全路线：补齐 inline flags、scoped flags、lazy/ungreedy、word boundary、absolute anchors、hex/unicode escapes、quoted literal、newline escape、POSIX/Unicode property classes，同时继续拒绝反向引用和 lookaround。
- 向工业级演进：当前实现明确约束 pattern/program/workspace/capture/set 上限，避免灾难性回溯，并用 demo、phase1、industrial、advanced、stress 样例锁住行为。

当前版本仍是 Aurex 语言能力验证库，不声称达到 RE2、Rust regex、PCRE2 或 Hyperscan 的生产成熟度。本文后面单独列出工业级目标和对比。

## 3. 模块职责

`regex.api`

- 公开 facade。
- 导出 `Regex`、`MatchResult`、`RegexStatus`、`Captures`、`CaptureSpan`、`FindIter`、`CaptureIter`、`SplitIter`、`SplitPart`、`ReplaceResult`、`ReplaceCallback`、`RegexSet`、`SetMatchesResult`、`RegexStream`、`DatabaseResult` 类型别名。
- 提供编译、释放、匹配、捕获、迭代、替换、分割、多模式 set、database 序列化、stream、错误诊断和资源查询 API。
- `Regex.valid`、`RegexSet.valid`、`MatchResult.ok`、`Captures.ok`、`SplitPart.ok`、`ReplaceResult.ok`、`DatabaseResult.ok` 是定义在 `regex.core.types` 中的类型方法，通过 facade 暴露出的类型别名使用。

`regex.bytes`

- text facade 的 raw byte 版本。
- `compile` 等价于 `regex.api.compile_bytes`，匹配函数接受 `[]const u8`。
- 支持 bytes compiled regex、bytes captures 和 bytes RegexSet 扫描；`.` 和 literal 消费一个 raw byte，`\xNN` 在 bytes 模式匹配单个 byte。

`regex.core.types`

- 定义 `RegexStatus`。
- 定义 VM 状态 `State`、字符类 range `ClassRange`、捕获元信息 `CaptureInfo`、捕获 span `CaptureSpan`、编译片段 `Fragment`、持有型 `Regex`、持有型 `RegexSet`、stream 状态、匹配结果 `MatchResult`、捕获结果 `Captures`、游标、替换结果和 database 结果。
- 定义 opcode、flag、错误 kind、`NO_STATE`、`NO_CAPTURE`、`CAPTURE_MISSING` 等程序表示常量。

`regex.core.results`

- 管理 `Captures` 的分配、释放和查询。
- 提供 `capture_at`、`capture_text`、`capture_name`、`capture_index`。
- `capture_name` 返回 pattern 中命名捕获组名字的 borrowed `str`。
- `capture_text` 返回 input 中捕获 span 对应的 borrowed `str`。

`regex.config.limits`

- 集中定义容量策略和资源上限。
- 包含 `MAX_PATTERN_BYTES`、`MAX_STATE_CAPACITY`、`MAX_RANGE_CAPACITY`、`MAX_WORKSPACE_STATES`、`MAX_BOUNDED_REPEAT`、`MAX_CAPTURE_GROUPS`。
- 包含工作区数组数量 `WORKSPACE_LIST_COUNT`、按 state 分配的捕获 row 数量 `WORKSPACE_STATE_CAPTURE_ROW_COUNT` 和固定捕获 row 数量 `WORKSPACE_FIXED_CAPTURE_ROW_COUNT`，用于内存估算。

`regex.syntax.ascii`

- 集中定义 ASCII byte 常量，例如 `CARET`、`DOLLAR`、`DOT`、`BACKSLASH`、`LESS`、`GREATER`、`DIGIT_0`、`LOWER_D`。
- 提供 `byte_at`、`is_digit`、`is_alpha`、`is_name_start`、`is_name_continue`、`decimal_value`、`escaped_byte`。

`unicode.ascii` / `unicode.ucd`

- 独立于 regex 的可复用 Unicode 底层模块。
- `unicode.ascii` 提供基础 ASCII byte helper；`unicode.ucd` 提供 UTF-8 scalar 解码、前后边界推进、Unicode 17.0 属性表、script/script extensions、simple case folding 和 Unicode word 判断。
- regex 只保存 byte offset，对 literal、`.`、量词、字符类、`\b` / `\B` 的消费单位则统一使用 Unicode scalar value。

`regex.runtime.alloc`

- 声明 C ABI `calloc` 和 `free`。
- 提供 `alloc_zeroed_bytes` 和 `free_bytes`。

`regex.compile.program`

- 管理编译后 NFA 程序。
- 分配/扩容/释放状态表、字符类表和捕获元信息表。
- 检查 pattern bytes、state capacity、range capacity 和 capture count 上限。
- 提供构造片段的 helper：literal、any、class、assert、split、epsilon、save、match、concat、alternate、repeat、capture。
- 提供 `state_count`、`range_count`、`program_bytes`。

`regex.compile.set`

- 将 `[]const str` 多个 pattern 编译为共享 `RegexSet` 状态表。
- 每个 pattern 单独解析成安全 NFA，再复制到同一个 set program；set 起点用 split 串联多个 pattern 起点，accept state 的 `value` 保存 pattern id。
- 支持 text set 和 bytes set；释放使用 `destroy_set`。

`regex.compile.database`

- 将 `RegexSet` 序列化为稳定 little-endian byte database，字段逐项编码，不依赖 C struct ABI。
- `deserialize_set` 从 `[]const u8` 还原 `RegexSet`，验证 magic、version、mode、容量、state 引用、class range 边界和 match pattern id。

`regex.compile.parser`

- 递归下降解析 pattern。
- 支持捕获组、命名捕获组、非捕获组、inline flags/scoped flags、交替、Unicode 字符类、POSIX/Unicode property 类、转义、绝对锚点、word boundary、greedy/lazy/ungreedy 量词、bounded repeat。
- 将 pattern 编译为 `regex.core.types.Regex`。
- 将语法错误写入 `Regex.status`、`Regex.error_offset`、`Regex.error_kind`。

`regex.vm.engine`

- 运行 NFA VM。
- 使用 current/next state 列表、visited marks、显式 stack，以及对应的 capture snapshot row。
- `search_compiled` 对未锚定 pattern 使用单次共享 active-list 扫描：每个输入边界只注入一个新起点 closure；`fullmatch_compiled` 要求从 0 到输入结尾。
- `captures_compiled` 和 `captures_fullmatch_compiled` 返回 owning `Captures`。
- 在同一起点下使用有序 Thompson 线程。greedy 分支会保留后备较长结果，lazy/ungreedy 分支可提前接受较短结果。
- 对非 multiline 的 `^` 和 `\A` 开头 pattern 做 anchored 起点优化，只尝试 offset `0`；`(?m)^` 仍会扫描行首。
- 检查工作区容量，提供 `workspace_bytes` 和 `set_workspace_bytes`。
- `matches_set_compiled` / `matches_bytes_set_compiled` 在共享 set program 上扫描输入，把匹配到的 pattern id 以升序写入调用方 `usize` buffer。

`regex.ops.iter`

- 提供 C++ iterator 风格的显式游标 API。
- `find_iter` / `find_next` 返回连续 `MatchResult`。
- `captures_iter` / `captures_next` 返回连续 `Captures`，每个返回值需要调用 `destroy_captures`。
- 对零长度匹配按下一个 Unicode scalar boundary 前进，避免无限循环。

`regex.ops.replace`

- 实现 `replace_all`、`replace_first`、`replace_n` 和 `replace_*_with` 回调替换。
- 不创建拥有型字符串；调用方传入 `*mut u8` 和容量。
- 返回 `ReplaceResult { status, written, required, replacements }`。
- 模板替换支持 literal replacement、`$$`、`$0`、多位编号 `$10`、braced 编号 `${10}` 和 `${name}`。

`regex.ops.split`

- 实现 `split_iter` / `splitn_iter` / `split_next`。
- 返回 input 的 byte span，不分配字符串。
- 对零长度分隔符按下一个 Unicode scalar boundary 前进，避免无限循环。

`regex.ops.stream`

- 实现文本流式扫描状态。
- `open_stream` 绑定一个 text `Regex`，`stream_feed` 追加 `str` chunk，`stream_next` 返回绝对 byte offset span，`stream_finish` 标记 EOF。
- 未 finish 时，结束在当前 buffer 尾部的潜在可扩展 match 会延后，避免把跨 chunk match 截断。

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
pub type ReplaceCallback = regex.core.types.ReplaceCallback;
pub type RegexSet = regex.core.types.RegexSet;
pub type SetMatchesResult = regex.core.types.SetMatchesResult;
pub type RegexStream = regex.core.types.RegexStream;
pub type DatabaseResult = regex.core.types.DatabaseResult;

pub fn compile(pattern: str) -> Regex;
pub fn compile_bytes(pattern: str) -> Regex;
pub fn compile_set(patterns: []const str) -> RegexSet;
pub fn compile_set_bytes(patterns: []const str) -> RegexSet;
pub fn destroy(compiled: &mut Regex) -> void;
pub fn destroy_set(compiled: &mut RegexSet) -> void;
pub fn is_valid(compiled: &Regex) -> bool;
pub fn set_is_valid(compiled: &RegexSet) -> bool;

pub fn state_count(compiled: &Regex) -> usize;
pub fn range_count(compiled: &Regex) -> usize;
pub fn program_bytes(compiled: &Regex) -> usize;
pub fn workspace_bytes(compiled: &Regex) -> usize;
pub fn capture_count(compiled: &Regex) -> usize;
pub fn set_state_count(compiled: &RegexSet) -> usize;
pub fn set_range_count(compiled: &RegexSet) -> usize;
pub fn set_program_bytes(compiled: &RegexSet) -> usize;
pub fn set_workspace_bytes(compiled: &RegexSet) -> usize;
pub fn database_bytes(compiled: &RegexSet) -> usize;

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
pub fn search_bytes_compiled(compiled: &Regex, input: []const u8) -> MatchResult;
pub fn fullmatch_bytes_compiled(compiled: &Regex, input: []const u8) -> MatchResult;
pub fn captures_bytes_compiled(compiled: &Regex, input: []const u8) -> Captures;
pub fn matches_set_compiled(compiled: &RegexSet, input: str, out_pattern_ids: *mut usize, out_capacity: usize) -> SetMatchesResult;
pub fn matches_bytes_set_compiled(compiled: &RegexSet, input: []const u8, out_pattern_ids: *mut usize, out_capacity: usize) -> SetMatchesResult;
pub fn serialize_set(compiled: &RegexSet, out: *mut u8, out_capacity: usize) -> DatabaseResult;
pub fn deserialize_set(bytes: []const u8) -> RegexSet;
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
pub fn splitn_iter(compiled: &Regex, input: str, limit: usize) -> SplitIter;
pub fn split_next(iter: &mut SplitIter) -> SplitPart;

pub fn replace_all(compiled: &Regex, input: str, replacement: str, out: *mut u8, out_capacity: usize) -> ReplaceResult;
pub fn replace_first(compiled: &Regex, input: str, replacement: str, out: *mut u8, out_capacity: usize) -> ReplaceResult;
pub fn replace_n(compiled: &Regex, input: str, replacement: str, limit: usize, out: *mut u8, out_capacity: usize) -> ReplaceResult;
pub fn replace_all_with(compiled: &Regex, input: str, callback: ReplaceCallback, out: *mut u8, out_capacity: usize) -> ReplaceResult;
pub fn replace_first_with(compiled: &Regex, input: str, callback: ReplaceCallback, out: *mut u8, out_capacity: usize) -> ReplaceResult;
pub fn replace_n_with(compiled: &Regex, input: str, callback: ReplaceCallback, limit: usize, out: *mut u8, out_capacity: usize) -> ReplaceResult;

pub fn open_stream(compiled: &Regex) -> RegexStream;
pub fn stream_feed(stream: &mut RegexStream, chunk: str) -> RegexStatus;
pub fn stream_next(stream: &mut RegexStream) -> MatchResult;
pub fn stream_finish(stream: &mut RegexStream) -> RegexStatus;
pub fn destroy_stream(stream: &mut RegexStream) -> void;
pub fn stream_buffer_bytes(stream: &RegexStream) -> usize;

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
    database_error = 13,
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

pub struct RegexSet {
    pub mode: u8;
    pub states: *mut State;
    pub state_len: usize;
    pub state_capacity: usize;
    pub ranges: *mut ClassRange;
    pub range_len: usize;
    pub range_capacity: usize;
    pub pattern_count: usize;
    pub start: usize;
    pub status: RegexStatus;
    pub error_index: usize;
}

pub struct SetMatchesResult {
    pub status: RegexStatus;
    pub matched: bool;
    pub count: usize;
    pub written: usize;
}

pub struct RegexStream {
    pub compiled: *const Regex;
    pub buffer: *mut u8;
    pub len: usize;
    pub capacity: usize;
    pub base_offset: usize;
    pub next_start: usize;
    pub finished: bool;
    pub status: RegexStatus;
}

pub struct DatabaseResult {
    pub status: RegexStatus;
    pub written: usize;
    pub required: usize;
}
```

调用规则：

- `search` / `is_match` / `fullmatch` 是 bool convenience API：内部 compile、match、destroy。
- `find` 是 span convenience API，返回 `MatchResult`。
- `captures` 是 owning capture convenience API，调用方仍必须 `destroy_captures`。
- `replace` 是 buffer replacement convenience API，内部 compile、replace、destroy。
- `compile` 返回持有 FFI 分配内存的 `Regex`，调用方必须 `destroy(&mut compiled)`。
- `compile_set` / `compile_set_bytes` 返回持有 FFI 分配内存的 `RegexSet`，调用方必须 `destroy_set(&mut compiled_set)`。
- `deserialize_set` 返回持有 FFI 分配内存的 `RegexSet`，调用方同样必须 `destroy_set`。
- `open_stream` 返回持有 FFI buffer 的 `RegexStream`，调用方必须 `destroy_stream(&mut stream)`。
- `captures_compiled`、`captures_fullmatch_compiled` 和 `captures_next` 返回持有 FFI 分配内存的 `Captures`，调用方必须 `destroy_captures(&mut captures)`。
- 推荐写 `defer regex.destroy(&mut compiled);` 和 `defer regex.destroy_captures(&mut captures);`，避免早返回泄漏。
- `compiled.valid()` 等价于 `regex.is_valid(&compiled)`。
- `compiled_set.valid()` 等价于 `regex.set_is_valid(&compiled_set)`。
- `result.ok()` 表示 `result.status == RegexStatus.ok && result.matched`。
- `captures.ok()` 表示 `captures.status == RegexStatus.ok && captures.matched`。
- `replace_result.ok()` 表示输出缓冲区容量足够且状态为 `ok`。
- `database_result.ok()` 表示 database 序列化容量足够且状态为 `ok`。
- `compiled.status != RegexStatus.ok` 时，匹配函数直接返回对应状态。
- 没有匹配是正常运行结果：`MatchResult { matched: false, status: no_match }` 或 `Captures { matched: false, status: no_match }`。
- `program_bytes` 是状态表容量、字符类 range 表容量、捕获元信息表容量的估算字节数。
- `workspace_bytes` 是一次匹配需要的 VM 工作区估算字节数，包括 capture snapshot row。
- `set_program_bytes` 是 `RegexSet` 状态表和字符类表容量估算；`set_workspace_bytes` 包含 set VM 工作区和 pattern id bitset。
- convenience API 每次调用都会 compile/destroy；循环、大量输入或服务端路径应优先使用 compiled API。

## 5. 正则语法

Text regex 的匹配消费单位是 UTF-8 `str` 的 Unicode scalar value，公开 span 仍是 byte offset；bytes regex 的匹配消费单位是 `[]const u8` 的 raw byte。两者都不是 grapheme cluster 级 API。

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
            | "(?P<" name ">" alternation ")"
            | "(?#" comment ")"
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
\UNNNNNNNN  8 位 Unicode codepoint
\U{H...}    braced Unicode codepoint
\Q...\E     quoted literal，区间内元字符按字面量处理
```

在 `compile_bytes` / `regex.bytes.compile` 中，literal、`.` 和字符类都按 raw byte 消费；`\xNN` 对 `0x00..0xff` 匹配单个 byte，超过 `0xff` 的 braced Unicode codepoint 会按 UTF-8 byte 序列展开。

预定义 Unicode 类：

```text
\d    Unicode Decimal_Number / Nd
\D    非 Unicode Decimal_Number
\w    Unicode word
\W    非 Unicode word
\s    Unicode White_Space
\S    非 Unicode White_Space
```

零宽断言和换行相关转义：

```text
\A    输入绝对开头
\z    输入绝对结尾
\Z    当前实现等同 \z
\b    Unicode word boundary
\B    非 word boundary
\b{start} Unicode word start
\b{end}   Unicode word end
\N    任意非 Unicode line break scalar
\R    CRLF 或单个 Unicode line break scalar
```

未知字母转义会被诊断为 `unsupported` + `invalid_escape`，避免 typo 被静默当字面量。未知标点转义仍按标点字面量处理，例如 `\.`。

### 5.4 通配和锚点

```text
.     默认匹配除 Unicode line break 外任意一个 scalar；`(?s)` dotall 下匹配任意 scalar
^     默认仅在输入开头成功；`(?m)` multiline 下也在 Unicode line break 后成功
$     默认仅在输入结尾成功；`(?m)` multiline 下也在 Unicode line break 前成功
```

锚点是零宽断言，不消耗输入。

### 5.5 Inline flags

支持全局后续生效和 scoped 两种形式：

```text
(?i)abc          // 从当前位置开始使用 Unicode simple case folding
(?i:a)b          // 只在 scoped group 内忽略大小写
(?i)abc(?-i:de)  // 局部关闭 i
```

当前 flag：

| flag | 语义 |
| --- | --- |
| `i` | Unicode simple case-insensitive，用 Unicode 17.0 simple case folding 比较 literal 和 class |
| `m` | multiline，使 `^`/`$` 识别行首/行尾 |
| `s` | dotall，使 `.` 匹配任意 Unicode scalar |
| `x` | extended，忽略 pattern 中 Unicode `White_Space`，`#` 到行尾为注释；转义空格 `\ ` 仍是字面量 |
| `U` | ungreedy，默认量词改为 lazy，显式 `?` 后缀会反转回来 |
| `u` | text regex 兼容 flag；当前实现本来就固定启用 UTF-8 Unicode scalar 语义 |

### 5.6 字符类

支持普通集合、范围和取反：

```text
[abc]
[a-z]
[A-Za-z0-9_]
[^0-9]
```

支持嵌套字符类和 set algebra，表达式以 postfix class program 保存，不展开成巨型 range 表：

```text
[a-z&&[aeiou]]        intersection
[a-z--[aeiou]]        difference
[a-f~~[d-z]]          symmetric difference
[[\p{L}]--[\p{Greek}]] nested Unicode class difference
```

取反 `[^...]` 会编译成 class expression complement；在 `(?i)` 下，case folding 先应用到 primitive class，再应用 complement，因此 `(?i)[^a-f]` 不会匹配 `F`。

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

类内范围两端必须是 scalar 字面量，不能用预定义类当范围端点：

```text
[a-z]      // 合法
[\u{0400}-\u{04ff}] // 合法
[a-\d]     // 非法
```

如果预定义类后面跟 `-`，该 `-` 会作为普通字面量处理，而不是把预定义类作为 range 起点。

POSIX 名称会映射到 Unicode 属性；`\p{...}` / `\P{...}` 支持 Unicode 17.0 general category、binary property、script 和 keyed 语法，例如：

```text
\p{L}
\p{Lu}
\p{Alphabetic}
\p{White_Space}
\p{Greek}
\p{gc=Nd}
\p{sc=Greek}
\p{scx=Hira}
```

兼容别名仍支持 `digit`、`word`、`space`、`alpha`、`alnum`、`ascii`、`blank`、`cntrl`、`graph`、`lower`、`print`、`punct`、`upper`、`xdigit` 等传统名称，但它们现在按 Unicode 属性工作，不再退化成 ASCII-only。

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
(?P<user>\w+)
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
- 匹配消费单位是 UTF-8 Unicode scalar value，不是 raw byte；例如 `.` 对 `"é"` 只消费一次，`".."` 不会把它拆成两个 byte。

`search(pattern, input)`：

- 从 byte offset `0` 到 `strblen(input)` 按 UTF-8 scalar boundary 尝试起点。
- 返回第一个能匹配的起点。
- 在同一个起点下，VM 按 NFA split 顺序维护有序线程。greedy 量词优先继续消费，并保存后备接受点；lazy/ungreedy 量词优先退出，可在更短位置接受。

`search_compiled_from(compiled, input, start_offset)`：

- 从指定 byte offset 开始搜索。
- 如果 `start_offset` 落在 UTF-8 continuation byte 中，会先前进到下一个 scalar boundary，不会在字符中间启动匹配。
- `start_offset > strblen(input)` 时返回 `no_match`。

`MatchResult.start`、`MatchResult.end`、`CaptureSpan.start`、`CaptureSpan.end` 都是 byte offset。这个 API 选择与 C++ `std::regex` / iterator 风格相近：位置仍指向原始输入序列，消费语义则由 regex locale/traits 层决定；在 Aurex 文本 regex 中，这一层固定为 UTF-8 scalar value。由于 `str` 是 UTF-8 文本，offset 不一定是 Unicode 字符下标。

Unicode 语义：

- literal、`.`、量词和字符类都按 Unicode scalar value 工作；`\x{...}`、`\uNNNN`、`\u{...}` 直接编译成一个 scalar literal。
- `.` 默认拒绝 Unicode line break scalar，`(?s)` 下匹配任意 scalar。
- `\d` / `\D` 使用 Unicode `Nd`；`\w` / `\W` 使用 Unicode word 定义；`\s` / `\S` 使用 Unicode `White_Space`。
- `\p{...}` / `\P{...}` 支持 Unicode general category、binary property、script、`gc=...`、`sc=...` 和 `scx=...`；POSIX 名称会映射到 Unicode 属性而不是旧的 ASCII 子集。
- `(?i)` 使用 Unicode simple case folding，覆盖 Unicode 17.0 `CaseFolding.txt` 中的一对一 `C` / `S` fold；多 scalar full fold 不在单 scalar regex 语义里展开。
- `\b` / `\B` 基于 Unicode word 判断；public span 继续保留 byte offset，便于对原始 UTF-8 `str` 做切片。

捕获实现说明：

- 编译期为捕获组插入 `OP_SAVE` 状态，保存 start/end slot。
- VM 每个活动 state 携带一行 capture snapshot。
- 到达 accept 时复制当前 capture row 为 best capture row。
- 被 bounded repeat 复制的捕获组复用同一个 capture slot；多次迭代时保留最后一次参与匹配的 span。
- 子匹配优先级采用左最先、同一起点下按 NFA split 顺序的 ordered Thompson 语义：每个 state id 只保留最高优先级线程，greedy split 先继续消费，lazy/ungreedy split 先退出。`regex_advanced.ax` 锁住了 `(a*)(a*)`、`(a|ab)(b?)` 等子匹配 precedence 回归。

## 7. Iterator、replace、split

`find_iter` / `find_next`：

```aurex
var iter: regex.FindIter = regex.find_iter(&compiled, "a1 b22");
let first: regex.MatchResult = regex.find_next(&mut iter);
let second: regex.MatchResult = regex.find_next(&mut iter);
```

- 每次返回下一个 match span。
- 结束时返回 `status == no_match`。
- 零长度匹配会至少前进到下一个 Unicode scalar boundary。

`captures_iter` / `captures_next`：

```aurex
var iter: regex.CaptureIter = regex.captures_iter(&compiled, input);
var captures: regex.Captures = regex.captures_next(&mut iter);
defer regex.destroy_captures(&mut captures);
```

- 每次返回一个 owning `Captures`。
- 调用方必须释放每个成功或失败返回的 `Captures`；空结果释放是安全的。

`replace_all` / `replace_first` / `replace_n`：

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
  - `$1`、`$10`：编号捕获，支持多位编号
  - `${10}`：braced 编号捕获
  - `${name}`：命名捕获
- 无效模板或未知 name 返回 `replacement_error`。
- `replace_first` 等价于 `replace_n(..., 1usize, ...)`。
- `replace_all_with` / `replace_first_with` / `replace_n_with` 接受 `ReplaceCallback = fn(str, &Captures, *mut u8, usize) -> ReplaceResult`，用于函数式替换。callback 负责写入提供的 replacement 子 buffer，并用 `required` 报告完整 replacement byte 数；外层仍会累计总 `required` 和 `replacements`。

`split_iter` / `split_next`：

```aurex
var iter: regex.SplitIter = regex.split_iter(&comma, "a, b,c");
let part: regex.SplitPart = regex.split_next(&mut iter);
```

- pattern 是分隔符。
- 返回 input 的 byte span，不分配字符串。
- `splitn_iter(compiled, input, limit)` 最多返回 `limit` 个 part；`limit == 1` 时直接返回剩余 tail。
- 结束时返回 `status == no_match`。
- 零长度分隔符会至少前进到下一个 Unicode scalar boundary。

## 8. Bytes、RegexSet、Database、Stream

Bytes facade：

```aurex
import regex.bytes as bytes;

let raw: [2]u8 = [0xc3u8, 0xa9u8];
let view: []const u8 = raw[:];
let ok: bool = bytes.fullmatch("..", view);
```

- `regex.api.compile_bytes` 和 `regex.bytes.compile` 产生 bytes mode `Regex`。
- bytes mode 只接受 `[]const u8` 输入；text mode API 调用 bytes regex 会返回 `unsupported`。
- bytes mode 中 `.` 消费一个 byte，`\xNN` 匹配一个 byte；Unicode 属性仍可解析，但输入值只有 `0..255`。

RegexSet：

```aurex
let patterns: [3]str = ["error:\\s+\\w+", "warn:\\s+\\w+", "\\p{Greek}+"];
var set: regex.RegexSet = regex.compile_set(patterns[:]);
defer regex.destroy_set(&mut set);

var ids: [8]usize = [0usize; 8];
let result: regex.SetMatchesResult =
    regex.matches_set_compiled(&set, "trace Ω warn: ops", ids_ptr, 8usize);
```

- `RegexSet` 是多 pattern 共享 NFA program，不是循环调用多个 `Regex`。
- `matches_set_compiled` 在一次 set VM 扫描中标记所有匹配的 pattern id，并按升序写入调用方 `usize` buffer。
- `result.count` 是匹配到的 pattern 总数，`result.written` 是实际写入的 id 数；buffer 小时仍可通过 `count` 判断还有多少结果。
- `compile_set_bytes` / `matches_bytes_set_compiled` 提供 raw byte 版本。

Database：

```aurex
let bytes_needed: usize = regex.database_bytes(&set);
let serialized: regex.DatabaseResult = regex.serialize_set(&set, out, cap);
var loaded: regex.RegexSet = regex.deserialize_set(serialized_bytes);
defer regex.destroy_set(&mut loaded);
```

- database 格式有 magic/version/mode/header，并以 little-endian 编码 state/range 字段。
- `deserialize_set` 会验证 mode、容量、state 引用、class range 边界和 match pattern id；格式错误返回 `RegexStatus.database_error`。
- database 表示的是 `RegexSet`，用于多模式预编译产物传递；单 pattern `Regex` 如需复用仍直接持有 compiled object。

Stream：

```aurex
var stream: regex.RegexStream = regex.open_stream(&compiled);
defer regex.destroy_stream(&mut stream);
regex.stream_feed(&mut stream, chunk);
let found: regex.MatchResult = regex.stream_next(&mut stream);
regex.stream_finish(&mut stream);
```

- 当前 stream API 是 text regex stream，输入 chunk 类型为 `str`。
- `stream_next` 返回从 stream 开头计算的绝对 byte offset。
- 未 `stream_finish` 时，如果最优 match 正好结束在当前 buffer 尾部，会返回 `no_match` 并等待后续 chunk，以避免截断可继续扩展的 match。

## 9. 错误诊断

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

## 10. 性能和内存要求

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
| `MAX_SET_PATTERNS` | 1024 | 单个 `RegexSet` 的 pattern 数上限 |
| `MIN_STREAM_CAPACITY` | 64 | stream 初始 buffer 容量 |
| `WORKSPACE_LIST_COUNT` | 5 | current、next、marks、stack、start_states 五组 VM state 数组 |
| `WORKSPACE_STATE_CAPTURE_ROW_COUNT` | 3 | current、next、stack 三组按 state 分配的捕获 row |
| `WORKSPACE_FIXED_CAPTURE_ROW_COUNT` | 2 | best、seed 两组固定捕获 row |

复杂度和内存模型：

- 编译时间目标：`O(pattern_bytes + emitted_states + class_ranges + capture_groups)`。
- 编译期内存：`program_bytes = state_capacity * sizeof[State] + range_capacity * sizeof[ClassRange] + capture_capacity * sizeof[CaptureInfo]`。
- 匹配工作区：`workspace_bytes = state_count * 5 * sizeof[usize] + state_count * capture_slots * 3 * sizeof[usize] + capture_slots * 2 * sizeof[usize] + MAX_RANGE_CAPACITY * sizeof[bool]`。
- `capture_slots = capture_count * 2`，每个捕获含 start/end 两个 slot，`capture_count` 包含 `$0`。
- `fullmatch_compiled` 匹配时间：`O(input_scalars * active_state_count * capture_slots)`，capture row copy 是常数上限内的线性 slot 拷贝。
- `search_compiled` 匹配时间：未锚定 pattern 采用共享 active-list search，每个 scalar boundary 注入一次起点 closure，最坏为 `O(input_scalars * active_state_count * capture_slots)`；同一位置同一 state 只保留最高优先级线程，继续保持 leftmost + ordered Thompson 子匹配语义。以非 multiline `^` 或 `\A` 开头时仍只尝试起点 `0`。
- `matches_set_compiled` 使用共享 set NFA 和同样的单次 active-list 扫描，同时推进所有 pattern；结果去重后升序写出 pattern id。
- `serialize_set` 时间和输出大小都是 `O(state_len + range_len)`；deserialize 额外做同阶结构校验。
- stream 当前保留内部 buffer，并在已消费前缀足够大时压缩；它是增量 API，不是 Hyperscan 级别的 bounded-history streaming automaton。
- 同一起点采用有序线程接受策略：greedy 量词保留后备较长结果，lazy/ungreedy 量词可优先接受较短结果。
- `search` / `fullmatch` convenience API 会每次重新编译，性能敏感路径必须使用 `compile` 后复用。
- `replace_all` 只写调用方 buffer，不分配输出字符串；捕获结果当前逐 match 分配，后续可优化为复用 scratch captures。

压力样例：

- `examples/regex_demo.ax`：基础语法和 compiled API。
- `examples/regex_phase1.ax`：捕获、命名捕获、find/captures iterator、replace、split、错误 offset/kind。
- `examples/regex_industrial.ax`：flags、lazy、边界断言、扩展 escape、Unicode scalar/property/case-fold、便利 API 和非法 escape 诊断。
- `examples/regex_advanced.ax`：nested class set algebra、bytes API、RegexSet、database roundtrip、replace callback、splitn、stream、submatch precedence 和线性 search 语义。
- `examples/regex_stress.ax`：数百次重复 compiled search/fullmatch、长前缀线性 search、资源预算和错误路径。

后续向工业级继续推进时，优先级如下：

- 在单次 active-list search 之上增加 literal prefix / start byte prefilter，降低 literal-heavy 输入的常数。
- `replace_all` 增加 scratch capture 复用，避免每个 match 分配。
- `RegexSet` 后续可继续增加 literal prefilter 和 bytes set 的专用 SIMD/bitset 加速。
- stream 后续可演进为 bounded-history automaton，减少长流上内部 buffer 保留。
- 如果后续增加 grapheme API，需要继续把 byte offset、scalar 消费和 grapheme cluster 三层语义分开，不把当前 text regex 的 scalar 规则偷偷改成用户可见的 grapheme 规则。
- 增加 fuzz、差分测试和长输入基准，把正确性、峰值内存和吞吐纳入自动化。

## 11. 与工业级引擎对比

| 引擎 | 核心定位 | 复杂度/性能特点 | 功能范围 | Aurex 当前差距 |
| --- | --- | --- | --- | --- |
| RE2 | 生产级 C++ 正则引擎，偏有限自动机语义 | 以避免回溯爆炸为核心设计，匹配时间受输入规模约束 | 捕获、命名捕获、替换、迭代、flags、边界、丰富 ASCII/Unicode 语法；不支持反向引用、lookaround 等会破坏线性时间保证的特性 | Aurex 也避免回溯，并有 Unicode 17.0 scalar/property/case-fold、ordered Thompson 子匹配语义和资源预算，但仍缺少成熟 DFA/NFA 混合优化和多年工程验证 |
| Rust `regex` crate | Rust 生态常用正则库 | 文档承诺搜索复杂度可界定，通常用 NFA/DFA/literal 加速组合 | captures、find_iter、captures_iter、replace、split、flags、Unicode/bytes API、RegexSet | Aurex API 方向接近，已补 text/bytes、RegexSet、splitn、replace callback、database roundtrip 和线性 active-list search，但还没有 literal acceleration、完整 fuzz/差分测试和成熟优化器 |
| PCRE2 | Perl-compatible 功能优先引擎 | 功能很全，可使用 JIT；回溯模型需要限制资源避免病态 pattern | 捕获、命名组、反向引用、lookaround、条件、丰富选项 | Aurex 不追求 PCRE 兼容，暂不支持这些会明显增加复杂度或破坏线性保证的语法 |
| Hyperscan | 高吞吐多 pattern 扫描库 | 面向 block/stream/vectored 扫描和预编译 database，适合 IDS/日志类高吞吐场景 | 支持 PCRE-like 子集和多 pattern 批量匹配 | Aurex 现在已有共享 RegexSet、序列化 database 和 text stream API，但没有 SIMD、平台级向量化、vectored scan 或 bounded-history streaming automaton |
| Aurex regex | Aurex M2 写成的独立多模块正则库 | 编译 NFA + 有序 Thompson VM，无灾难性回溯；有资源上限和 stress/industrial/advanced 样例 | UTF-8 scalar text regex、raw bytes regex、Unicode 17.0 属性、simple case folding、flags、lazy/ungreedy、边界、锚点、nested class algebra、捕获/命名捕获、迭代、替换、分割、RegexSet、database、stream、错误 offset/kind | 目标是验证语言工程能力并逐步演进，不把当前实现包装成已经经受多年生产流量验证的工业完成品 |

这个对比用于确定路线：Aurex 当前应优先学习 RE2/Rust regex 的“可界定复杂度、拒绝破坏线性时间的语法、显式资源预算、稳定 API”路线，而不是先追 PCRE2 的全部语法。

## 12. 示例矩阵

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
| `\p{Alpha}+` | `éΩЖ汉` | `fullmatch` | true | Unicode property class |
| `\p{sc=Greek}+` | `Ωω` | `fullmatch` | true | Unicode script property |
| `.` | `é` | `fullmatch` | true | 单个 UTF-8 scalar，不按 byte 拆分 |
| `[a-z&&[aeiou]]+` | `aei` | `fullmatch` | true | class intersection |
| `[a-z--[aeiou]]+` | `bcdf` | `fullmatch` | true | class difference |
| `(?P<word>\w+)` | `word` | `captures_compiled` | `${word}=word` | Python 风格命名捕获兼容 |
| `..` bytes mode | `[0xc3, 0xa9]` | `bytes.fullmatch` | true | raw byte 消费 |
| `["alpha", "beta"]` set | `xx beta yy` | `matches_set_compiled` | id `1` | 多模式扫描 |
| `[abc` | `a` | `compile` | `syntax_error`, offset `0` | 未闭合 class |
| `abc[` | `a` | `compile` | `syntax_error`, offset `3` | 错误 offset |
| `a{33}` | `a` | `compile` | `repeat_too_large` | repeat 展开保护 |
| `\y` | `a` | `compile` | `unsupported`, kind `invalid_escape` | 未知字母转义 |

## 13. 完整演示

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
- 类语法：类内 `\D`、POSIX class、Unicode general category / script property、case-insensitive class。
- Unicode 语义：scalar `.`、Unicode range、`\p{L}` / `\p{sc=Greek}`、Unicode `\d`、simple case folding、Unicode word boundary 和 byte span 保持。
- convenience API：`is_match`、`find`、`captures`、`replace`。
- invalid escape 诊断：`\y`、不完整 `\x`。

`examples/regex_advanced.ax` 展示了：

- nested class set algebra：`&&`、`--`、`~~` 和 Unicode class 差集。
- 兼容语法：`(?P<name>...)`、`(?#...)`、`\U00000041` / `\U{...}`、`\b{start}` / `\b{end}`。
- `regex.bytes`：raw byte 模式下 `.` 不按 UTF-8 scalar 合并。
- `RegexSet`：共享多 pattern program、升序 pattern id 输出、set 资源查询。
- database：`serialize_set` / `deserialize_set` roundtrip。
- replacement：`$10`、`${10}`、`replace_first`、`replace_n` 和 `replace_all_with`。
- `splitn_iter`。
- `RegexStream`：分块 feed、finish 前延后尾部 match。
- submatch precedence：ordered Thompson 捕获行为。
- linear search：leftmost、greedy deferred accept、失败早起点让位和 `search_compiled_from`。

`examples/regex_stress.ax` 展示了：

- compiled API 在重复匹配中的复用。
- 资源预算查询：`state_count`、`range_count`、`program_bytes`、`workspace_bytes`。
- 上限查询：`max_state_capacity`、`max_range_capacity`、`max_bounded_repeat`。
- 大规模循环压力：固定迭代次数重复 search/fullmatch。
- 长前缀线性 search：验证共享 active-list 路径不依赖“每个起点重新跑一遍”的旧模型。
- 错误状态验证：repeat 上限和非法/不支持字符类。

## 14. 已知不支持项

这些语法不是遗漏，而是当前实现边界：

- 反向引用。
- lookahead/lookbehind。
- possessive 量词。
- 原子组、条件组、递归/子例程调用等 PCRE-only 高复杂度语法。
- full case folding 的多 scalar 展开，例如 `ß` 到 `ss`；当前 `(?i)` 固定使用一对一 simple fold。
- grapheme cluster 级匹配；当前 text regex 的消费单位是 Unicode scalar，公开 offset 仍是 UTF-8 byte offset。
- replacement 中的复杂转义和条件替换；当前函数式替换已支持非捕获函数指针 callback，但没有 closure 捕获。
- Hyperscan 级 vectored scan、SIMD codegen/JIT 和 bounded-history streaming automaton。
- 标准库风格 RAII；当前必须手动 destroy。

## 15. 测试入口

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
build/bin/aurexc -I examples/libs examples/regex_advanced.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_advanced.ax -o build/tests/regex_advanced
build/tests/regex_advanced
build/bin/aurexc -I examples/libs examples/regex_stress.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

## 16. 参考资料

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

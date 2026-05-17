# Aurex 正则库语法和实现说明

日期：2026-05-17
阶段：M2 language-core-no-std
状态：按当前仓库 `examples/libs/regex` 实现编写。本文是正则库的语法、API、模块和实现说明，避免后续只靠上下文记忆查范围。

## 1. 位置和导入

正则库是独立模块目录，不放在 `text` 下：

```text
examples/libs/regex/api.ax
examples/libs/regex/compile/parser.ax
examples/libs/regex/compile/program.ax
examples/libs/regex/config/limits.ax
examples/libs/regex/core/types.ax
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
build/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

## 2. 设计目标

这个库的目标不是复刻 PCRE，而是在当前 Aurex M2 能力范围内实现一个结构完整、可编译、可运行、可测试的正则库：

- API 独立：调用方只面对 `regex.api`。
- 实现分层：parser、program、engine、types、alloc、ascii 各自负责一类问题。
- 编译型：`compile` 把 pattern 编译成 NFA 状态表，匹配阶段运行 VM。
- 可释放：通过 FFI `calloc/free` 分配和释放状态表、字符类表、VM 工作区。
- 无魔法数字：ASCII byte、opcode、flag、容量策略、资源上限和 repeat 上限都使用命名常量。
- 资源可见：公开状态数、range 数、编译后程序内存估算和 VM 工作区内存估算。
- 向工业级演进：当前实现明确约束 pattern/program/workspace 上限，避免灾难性回溯，并用压力样例锁住大规模重复匹配行为。
- 语法边界明确：支持完整的 ASCII byte 级子集，并清楚记录不支持项。

当前版本仍然是 Aurex 语言能力验证库，不声称达到 RE2、Rust regex、PCRE2 或 Hyperscan 的生产成熟度。本文后面单独列出工业级目标和对比。

## 3. 模块职责

`regex.api`

- 公开 facade。
- 导出 `Regex`、`MatchResult`、`RegexStatus` 类型别名。
- 提供 `compile`、`destroy`、`is_valid`、`search_compiled`、`fullmatch_compiled`、`search`、`fullmatch`、`status_code`。
- 提供资源查询：`state_count`、`range_count`、`program_bytes`、`workspace_bytes`。
- 提供资源上限查询：`max_pattern_bytes`、`max_state_capacity`、`max_range_capacity`、`max_workspace_states`、`max_bounded_repeat`。
- `Regex.valid` 和 `MatchResult.ok` 是定义在 `regex.core.types` 中的类型方法，
  通过 `regex.api` 暴露出的类型别名使用。

`regex.core.types`

- 定义 `RegexStatus`。
- 定义 VM 状态 `State`、字符类 range `ClassRange`、编译片段 `Fragment`、持有型 `Regex`、匹配结果 `MatchResult`。
- 定义 opcode、flag 和 `NO_STATE` 等程序表示常量。

`regex.config.limits`

- 集中定义容量策略和资源上限。
- 包含 `MAX_PATTERN_BYTES`、`MAX_STATE_CAPACITY`、`MAX_RANGE_CAPACITY`、`MAX_WORKSPACE_STATES`、`MAX_BOUNDED_REPEAT`。
- 包含工作区数组数量 `WORKSPACE_LIST_COUNT`，用于内存估算。

`regex.syntax.ascii`

- 集中定义 ASCII byte 常量，例如 `CARET`、`DOLLAR`、`DOT`、`BACKSLASH`、`DIGIT_0`、`LOWER_D`。
- 提供 `byte_at`、`is_digit`、`decimal_value`、`escaped_byte`。

`regex.runtime.alloc`

- 声明 C ABI `calloc` 和 `free`。
- 提供 `alloc_zeroed_bytes` 和 `free_bytes`。

`regex.compile.program`

- 管理编译后 NFA 程序。
- 分配/扩容/释放状态表和字符类表。
- 检查 pattern bytes、state capacity 和 range capacity 上限。
- 提供构造片段的 helper：literal、any、class、assert、split、epsilon、concat、alternate、repeat。
- 提供 `state_count`、`range_count`、`program_bytes`。

`regex.compile.parser`

- 递归下降解析 pattern。
- 支持分组、交替、字符类、转义、量词、bounded repeat。
- 将 pattern 编译为 `regex.core.types.Regex`。

`regex.vm.engine`

- 运行 NFA VM。
- 使用当前列表、下一列表、visited marks 和显式 stack。
- `search_compiled` 扫描所有起点；`fullmatch_compiled` 要求从 0 到输入结尾。
- 在同一起点下记录可到达的最长匹配终点。
- 对 `^` 开头的 pattern 做 anchored 起点优化，只尝试 offset `0`。
- 检查工作区容量，提供 `workspace_bytes`。

## 4. Public API

```aurex
pub type Regex = regex.core.types.Regex;
pub type MatchResult = regex.core.types.MatchResult;
pub type RegexStatus = regex.core.types.RegexStatus;

pub fn compile(pattern: str) -> Regex;
pub fn destroy(compiled: &mut Regex) -> void;
pub fn is_valid(compiled: &Regex) -> bool;

pub fn state_count(compiled: &Regex) -> usize;
pub fn range_count(compiled: &Regex) -> usize;
pub fn program_bytes(compiled: &Regex) -> usize;
pub fn workspace_bytes(compiled: &Regex) -> usize;

pub fn max_pattern_bytes() -> usize;
pub fn max_state_capacity() -> usize;
pub fn max_range_capacity() -> usize;
pub fn max_workspace_states() -> usize;
pub fn max_bounded_repeat() -> usize;

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
    pattern_too_large = 6,
    program_too_large = 7,
    workspace_too_large = 8,
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
- `program_bytes` 是状态表容量和字符类 range 表容量的估算字节数。
- `workspace_bytes` 是一次匹配需要的 VM 工作区估算字节数。
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

## 7. 性能和内存要求

当前实现采用 Thompson 风格 NFA 程序和列表式 VM，不使用递归回溯，因此不会出现 PCRE 风格回溯引擎常见的指数级灾难性回溯。

资源上限集中在 `regex.config.limits`：

| 名称 | 当前值 | 作用 |
| --- | ---: | --- |
| `MAX_PATTERN_BYTES` | 4096 | 单个 pattern 的 UTF-8 byte 长度上限 |
| `MAX_STATE_CAPACITY` | 65536 | 编译后 NFA state 容量上限 |
| `MAX_RANGE_CAPACITY` | 32768 | 字符类 range 容量上限 |
| `MAX_WORKSPACE_STATES` | 65536 | 单次 VM 工作区可容纳的 state 上限 |
| `MAX_BOUNDED_REPEAT` | 32 | `{m,n}` 展开上限 |
| `WORKSPACE_LIST_COUNT` | 4 | current、next、marks、stack 四组 VM 数组 |

复杂度和内存模型：

- 编译时间目标：`O(pattern_bytes + emitted_states + class_ranges)`。
- 编译期内存：`program_bytes = state_capacity * sizeof[State] + range_capacity * sizeof[ClassRange]`。
- 匹配工作区：`workspace_bytes = state_count * WORKSPACE_LIST_COUNT * sizeof[usize]`。
- `fullmatch_compiled` 匹配时间：`O(input_bytes * active_state_count)`。
- `search_compiled` 匹配时间：当前会尝试多个起点，最坏是 `O(input_bytes * input_bytes * active_state_count)`；当 pattern 以 `^` 开头时只尝试起点 `0`。
- 同一起点采用 leftmost-longest：找到最左起点后，在该起点记录可到达的最长结束位置。
- `search` / `fullmatch` convenience API 会每次重新编译，性能敏感路径必须使用 `compile` 后复用。

压力样例 `examples/regex_stress.ax` 固定验证：

- compiled regex 在数百次重复 search/fullmatch 中复用。
- pattern 覆盖锚点、字符类、取反类、预定义类、转义、分组、交替和 bounded repeat。
- 校验 `state_count`、`range_count`、`program_bytes`、`workspace_bytes` 不超过预算。
- 校验 `repeat_too_large`、`syntax_error` / `unsupported` 等错误路径。

后续向工业级继续推进时，优先级如下：

- 将 `search_compiled` 从多起点 NFA 扫描推进到真正线性时间搜索，加入 literal prefix / start byte 加速。
- 增加捕获组需要先设计稳定的 capture storage 和 API，不能把捕获状态塞进当前 `MatchResult`。
- 扩展 Unicode 时需要明确 byte、scalar、grapheme 三层语义，避免和当前 byte offset API 混淆。
- 增加 fuzz、差分测试和长输入基准，把正确性、峰值内存和吞吐纳入自动化。

## 8. 与工业级引擎对比

| 引擎 | 核心定位 | 复杂度/性能特点 | 功能范围 | Aurex 当前差距 |
| --- | --- | --- | --- | --- |
| RE2 | 生产级 C++ 正则引擎，偏有限自动机语义 | 以避免回溯爆炸为核心设计，匹配时间受输入规模约束 | 不支持反向引用、lookaround 等会破坏线性时间保证的特性 | Aurex 也避免回溯，但缺少成熟 DFA/NFA 混合优化、Unicode 生态、完整 API 和多年工程验证 |
| Rust `regex` crate | Rust 生态常用正则库 | 文档承诺搜索复杂度可界定，通常用 NFA/DFA/literal 加速组合 | 不支持 look-around 和 backreference，支持更完整的 Unicode/bytes API | Aurex 目前只有 byte 级子集，没有 literal acceleration、captures、迭代器和 Unicode 支持 |
| PCRE2 | Perl-compatible 功能优先引擎 | 功能很全，可使用 JIT；回溯模型需要限制资源避免病态 pattern | 捕获、反向引用、lookaround、条件、丰富选项 | Aurex 不追求 PCRE 兼容，暂不支持这些会明显增加复杂度的语法 |
| Hyperscan | 高吞吐多 pattern 扫描库 | 面向 block/stream/vectored 扫描和预编译 database，适合 IDS/日志类高吞吐场景 | 支持 PCRE-like 子集和多 pattern 批量匹配 | Aurex 没有多 pattern database、SIMD、streaming 状态和平台级优化 |
| Aurex regex | Aurex M2 写成的独立多模块正则库 | 编译 NFA + VM，无灾难性回溯；有资源上限和 stress 样例 | ASCII byte 子集、无捕获、无 lookaround、无 backreference | 目标是验证语言工程能力并逐步演进，不把当前实现包装成工业级完成品 |

这个对比用于确定路线：Aurex 当前应优先学习 RE2/Rust regex 的“可界定复杂度、拒绝破坏线性时间的语法、显式资源预算”路线，而不是先追 PCRE2 的全部语法。

## 9. 示例矩阵

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

## 10. 完整演示

`examples/regex_demo.ax` 展示了：

- import alias：`import regex.api as regex;`
- 函数类型：`type Matcher = fn(str, str) -> bool;`
- struct literal：`Case { code, passed }`
- enum match：匹配 `RegexStatus`
- compiled API：`compile`、`fullmatch_compiled`、`destroy`
- `defer`：释放 compiled regex
- 方法调用：`compiled.valid()`、`result.ok()`

`examples/regex_stress.ax` 展示了：

- compiled API 在重复匹配中的复用。
- 资源预算查询：`state_count`、`range_count`、`program_bytes`、`workspace_bytes`。
- 上限查询：`max_state_capacity`、`max_range_capacity`、`max_bounded_repeat`。
- 大规模循环压力：固定迭代次数重复 search/fullmatch。
- 错误状态验证：repeat 上限和非法/不支持字符类。

## 11. 已知不支持项

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

## 12. 测试入口

正则示例测试位于：

```text
tests/gtest/integration/examples_tests.cpp
```

手动验证命令：

```sh
build/bin/aurexc -I examples/libs examples/regex_demo.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
build/bin/aurexc -I examples/libs examples/regex_stress.ax --emit=checked
build/bin/aurexc -I examples/libs examples/regex_stress.ax -o build/tests/regex_stress
build/tests/regex_stress
```

## 13. 参考资料

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

# Aurex 正则库工业化现状报告

日期：2026-05-18
分支：`m2`
基线提交：`6a6f8e1 Complete regex industrial API surface`
范围：`examples/libs/regex`、`examples/libs/unicode` 中被正则库依赖的 Unicode 支撑、正则样例和测试脚本。

本文是正则库的阶段性总报告，重点回答四件事：

1. 当前这个库到底已经实现了什么。
2. 使用方应该怎么正确使用它。
3. 它和 RE2、Rust regex、PCRE2、Hyperscan、C++ `std::regex` 等工业实现相比还差什么。
4. 当前测试、覆盖率、编译耗时、内存占用和代码量数据是什么，下一步怎么推进。

已有 `docs/zh/regex.md` 更偏语法、API 和实现手册；本文更偏工程状态、工业差距和路线图。

## 1. 总体结论

Aurex 正则库当前已经不是一个 toy demo。它具备独立 facade、编译型 NFA 程序、文本和 bytes 双入口、Unicode 17.0 语义、RegexSet、多模式 database、stream、vectored scan、replace、split、iterator、结构化错误诊断和资源上限。语法路线更接近 RE2 / Rust `regex` 的安全线性路线，而不是 PCRE2 那种功能优先的回溯兼容路线。

当前最重要的结论：

- 已完成的核心方向：安全正则、Unicode 17.0 scalar/property/case-fold、`\X` grapheme cluster、text/bytes API parity、RegexSet exact-literal Aho-Corasick fast path、database roundtrip、stream/vectored API、replacement escaping、match options、结构化 diagnostics。
- 当前性能模型：Thompson NFA + active-list VM，不使用递归回溯，因此不会出现典型灾难性回溯。未锚定 search 已有共享 active-list、start-byte bitset、literal prefix 预过滤；exact-literal RegexSet 已走标量 Aho-Corasick。
- 当前工程短板：还没有完整的 regex 专用 Google Benchmark 吞吐基线，还没有和 RE2/Rust regex/PCRE2/Hyperscan 的同机运行时吞吐对比，还没有 capture-free lazy DFA / dense DFA 快路径，也没有混合 RegexSet 的工业级 literal extraction + verify planner。
- 当前功能短板：故意不支持会破坏线性时间保证的 PCRE 类功能，例如反向引用、lookaround、递归、条件、控制动词、callout、atomic group、possessive quantifier 等。若未来必须兼容 PCRE，需要做独立可限额的回溯引擎，不能污染默认安全引擎。
- 当前文档中的编译耗时和 RSS 是样例编译 / native 生成路径数据，不等同于正则运行时吞吐。运行时吞吐必须由下一步 Google Benchmark + 对照工业库补齐。

一句话定位：当前实现已经具备“安全工业路线”的结构和大部分 API 面，但还不是 RE2/Rust regex/Hyperscan 那种经多年生产流量和性能工程打磨的成熟引擎。下一步的关键不是再补零散 API，而是建立工业基准、引入 DFA/literal planner、把混合 RegexSet 和 stream 做成真正的高吞吐路径。

## 2. 代码位置和模块结构

正则库位于：

```text
examples/libs/regex
```

Unicode 支撑位于：

```text
examples/libs/unicode
```

当前正则模块分层如下：

| 模块 | 职责 |
| --- | --- |
| `regex.api` | 对外 text facade，导出编译、匹配、捕获、替换、分割、RegexSet、database、stream、diagnostic、builder、match options 等 API |
| `regex.bytes` | raw byte facade，API 面和 text facade 对齐，但输入是 `[]u8` |
| `regex.core.types` | Regex、RegexSet、Captures、MatchResult、RegexOptions、RegexMatchOptions、状态机 opcode、错误码、database/stream/replace 结果类型 |
| `regex.core.results` | 捕获结果的分配、释放、查询和命名捕获索引 |
| `regex.config.limits` | pattern/state/range/workspace/capture/set/stream 等资源上限 |
| `regex.syntax.ascii` | ASCII byte 常量和 parser helper |
| `regex.runtime.alloc` | C ABI `calloc/free` 包装 |
| `regex.compile.parser` | 正则语法解析和编译入口 |
| `regex.compile.program` | NFA 状态表、字符类表、捕获元信息和程序构造 helper |
| `regex.compile.set` | RegexSet 编译，共享 NFA 和 exact-literal Aho-Corasick 自动机 |
| `regex.compile.database` | Regex / RegexSet database 序列化、反序列化、shape validation |
| `regex.vm.engine` | Thompson VM、search/fullmatch/captures、RegexSet scan、vectored scan、start-byte/prefix fast path |
| `regex.ops.iter` | find/captures iterator |
| `regex.ops.replace` | replace all/first/n、callback replace、append-style replace、replacement 模板解析 |
| `regex.ops.split` | split/splitn iterator |
| `regex.ops.stream` | text/bytes streaming buffer 和增量 scan |
| `regex.ops.escape` | pattern quote、replacement escape |
| `unicode.ucd` | UTF-8 scalar 解码、Unicode 17.0 属性、script/scx、case folding、Grapheme_Cluster_Break |

## 3. 当前能力清单

### 3.1 语法能力

当前支持：

- 字面量、连接、交替、空分支、普通分组、非捕获分组。
- 捕获组、命名捕获组：`(?<name>...)`、`(?P<name>...)`、`(?'name'...)`。
- 注释分组：`(?#...)`。
- inline flags 和 scoped flags：`i`、`m`、`s`、`x`、`U`、`u`、`a`。
- greedy / lazy / ungreedy 量词：`*`、`+`、`?`、`{m}`、`{m,n}`、`{m,}`、`{,n}` 及 `?` 后缀。
- `^`、`$`、`\A`、`\z`、`\Z`、`\G`。
- `\b`、`\B`、`\b{start}`、`\b{end}`。
- `.`、`\N`、`\R`、`\X`。
- 常见转义：`\n`、`\r`、`\t`、`\f`、`\v`、`\a`、`\e`、`\0`、`\xNN`、`\x{...}`、`\uNNNN`、`\u{...}`、`\UNNNNNNNN`、`\U{...}`、`\o{...}`、`\cX`、`\Q...\E`。
- 预定义类：`\d`、`\D`、`\w`、`\W`、`\s`、`\S`。
- Unicode property：`\p{...}`、`\P{...}`、`gc=...`、`sc=...`、`scx=...`。
- POSIX class：`[[:alpha:]]`、`[[:^digit:]]` 等，当前按 Unicode 属性处理。
- 字符类 range、取反、嵌套和集合代数：union、intersection `&&`、difference `--`、symmetric difference `~~`、complement。

当前明确不支持：

- 反向引用：`\1`、`\k<name>`。
- lookahead/lookbehind：`(?=...)`、`(?!...)`、`(?<=...)`、`(?<!...)`。
- atomic group：`(?>...)`。
- possessive quantifier：`*+`、`++`、`?+`、`{m,n}+`。
- 条件正则：`(?(cond)yes|no)`。
- 子程序递归和 subroutine call。
- PCRE control verbs：`(*SKIP)`、`(*PRUNE)`、`(*ACCEPT)` 等。
- callout、branch reset、duplicate named groups 的 PCRE 兼容语义。

这些缺口不是遗漏式小 bug，而是默认安全路线的边界。Aurex 当前选择的是“默认线性、可限额、可验证”的工业路线，而不是把所有 PCRE 功能塞进同一个引擎。

### 3.2 Unicode 能力

当前正则 text 模式按 UTF-8 `str` 的 Unicode scalar value 消费，public span 保留 byte offset。

已实现：

- Unicode 17.0 属性表。
- Unicode general category、binary property、script、script extensions。
- Unicode `White_Space`、`Decimal_Number`、word 判定。
- simple case folding 和 full case folding。
- literal full fold：例如 `(?i)ß` 可匹配 `ss` / `SS`，`(?i)ss` 可匹配 `ß` / `ẞ`。
- 字符类单 scalar fold 语义。
- UAX #29 extended grapheme cluster 的 `\X` atom。
- Unicode line break 相关的 `.`、`\N`、`\R`。
- bytes mode 下保留 raw byte 语义，避免把二进制输入误当 UTF-8 text。

仍缺：

- Unicode canonical equivalence / normalization matching，例如 NFC/NFD 等价匹配。
- `\b{gcb}`、`\b{wb}`、`\b{sb}` 这类更完整的 UAX #29 边界断言。
- locale/collation-sensitive matching。
- 针对大 Unicode property class 的专用压缩表和 DFA transition 加速。

### 3.3 API 能力

当前 API 面已经覆盖常用工业用法：

- 编译和释放：`compile`、`compile_bytes`、`compile_with_options`、`destroy`。
- 便利 API：`search`、`is_match`、`find`、`fullmatch`、`captures`、`replace`。
- compiled API：`search_compiled`、`fullmatch_compiled`、`captures_compiled`。
- match options：`not_bol`、`not_eol`、`not_bow`、`not_eow`、`not_null`、`continuous`、`anchored`。
- builder：case-insensitive、multiline、dotall、extended、ungreedy、ascii/unicode、newline、workspace/program limit、stream history limit。
- iterator：find iterator、capture iterator、bytes iterator。
- split：`split_iter`、`splitn_iter` 和 bytes 版本。
- replace：all/first/n、callback replace、append-style replace、bytes replace。
- escaping：`escape_pattern` / `quote` / `escape_replacement`。
- RegexSet：matches、find、scan、all spans、overlap、vectored、bytes set。
- database：单 Regex database、RegexSet database、deserialize validation。
- stream：text/bytes stream feed、next、finish、buffer budget。
- diagnostic：status、error offset、error kind、single regex diagnostic、set diagnostic。
- 资源查询：state count、range count、program bytes、workspace bytes、capture count、database bytes、limits。

### 3.4 性能和安全能力

当前引擎是 Thompson NFA 风格，不使用递归回溯。重要属性：

- 不会出现 PCRE 回溯模型常见的指数级灾难性回溯。
- pattern、state、range、workspace、repeat、capture、set pattern、stream history 都有显式上限。
- search 已经不是每个起点重新完整跑一次，它使用共享 active-list。
- 对非 multiline `^` / `\A` 开头 pattern 只尝试 offset 0。
- start-byte bitset 会在 active-list 为空时跳过明显不可能的候选起点。
- deterministic literal prefix 会进一步校验候选，减少误启动。
- exact-literal RegexSet 走持久标量 Aho-Corasick 自动机。
- replace 循环复用 scratch captures 和 VM workspace，避免每个 match 重复分配。
- zero-length match 的 iterator/split/replace 路径会推进到下一个 scalar boundary，避免死循环。

仍缺的性能关键点：

- capture-free search/fullmatch 的 lazy DFA / dense DFA cache。
- one-pass NFA 快路径。
- general literal extraction planner，不只是当前 literal prefix 和 exact-literal set。
- mixed RegexSet planner，也就是 literal prefilter + AC candidate + NFA verify 的组合。
- 运行时吞吐 Google Benchmark 和工业库对照。
- 更低常数的 Unicode property transition 和 class evaluation。
- 真正的 streaming automaton，而不是当前有预算的 buffer-based stream。

## 4. 使用方式

### 4.1 基本导入

Text regex：

```aurex
import regex.api as regex;
```

Raw bytes regex：

```aurex
import regex.bytes as bytes;
```

编译样例：

```sh
build/full-llvm/bin/aurexc -I examples/libs examples/regex_demo.ax -o build/tests/regex_demo
build/tests/regex_demo
```

下面片段沿用样例文件里的 `buffer_ptr` / `ids_ptr` 这类指针 helper 写法；真实调用时只需要把它们替换成项目自己的 buffer 指针获取方式。

### 4.2 compiled API

循环、服务端路径和性能敏感路径必须优先使用 compiled API。便利 API 每次都会 compile/destroy。

```aurex
import regex.api as regex;

fn main() -> i32 {
    var compiled: regex.Regex = regex.compile("(?<user>\\w+)@(?<host>[\\w.]+)");
    defer regex.destroy(&mut compiled);

    if !compiled.valid() {
        return regex.status_code(compiled.status);
    }

    let found: regex.MatchResult = regex.search_compiled(&compiled, "contact dev@example.com");
    if !found.ok() {
        return regex.status_code(found.status);
    }

    var captures: regex.Captures = regex.captures_compiled(&compiled, "contact dev@example.com");
    defer regex.destroy_captures(&mut captures);

    let user: str = regex.capture_text("contact dev@example.com", &captures, regex.capture_index(&compiled, "user"));
    let host: str = regex.capture_text("contact dev@example.com", &captures, regex.capture_index(&compiled, "host"));
    return 0;
}
```

### 4.3 builder 和资源限制

```aurex
var builder: regex.RegexBuilder = regex.builder("^error:\\s+\\w+");
builder = regex.builder_case_insensitive(builder, true);
builder = regex.builder_multiline(builder, true);
builder = regex.builder_workspace_limit(builder, 4096usize);

var compiled: regex.Regex = regex.builder_compile(&builder);
defer regex.destroy(&mut compiled);
```

### 4.4 match options

`RegexMatchOptions` 对应 C++ `std::regex_constants::match_flag_type` 方向的调用控制，但当前只实现核心 flags：

```aurex
var options: regex.RegexMatchOptions = regex.default_match_options();
options = regex.match_options_continuous(options, true);
options = regex.match_options_not_null(options, true);

let result: regex.MatchResult =
    regex.search_compiled_with_match_options(&compiled, input, start_offset, options);
```

### 4.5 replace 和 escaping

```aurex
var out: [256]u8 = [0u8; 256];
let result: regex.ReplaceResult =
    regex.replace_all(&compiled, "a=1 b=2", "${name}:$1", buffer_ptr(&mut out), 256usize);
```

用户输入作为 pattern 字面量时，用 `escape_pattern` 或 `quote`。用户输入作为 replacement 字面量时，用 `escape_replacement`，避免 `$1`、`${name}`、`\n` 等被解释。

### 4.6 RegexSet

```aurex
let patterns: [3]str = ["error", "warning", "\\p{Greek}+"];
var set: regex.RegexSet = regex.compile_set(patterns[:]);
defer regex.destroy_set(&mut set);

var ids: [8]usize = [0usize; 8];
let result: regex.SetMatchesResult =
    regex.matches_set_compiled(&set, "warning Ω", ids_ptr(&mut ids), 8usize);
```

RegexSet 不是循环调用多个 Regex。exact-literal set 会构建共享 Aho-Corasick 自动机；非 literal set 会进入共享 NFA。

### 4.7 Database

```aurex
let needed: usize = regex.regex_database_bytes(&compiled);
let serialized: regex.DatabaseResult = regex.serialize_regex(&compiled, out, cap);
var loaded: regex.Regex = regex.deserialize_regex(bytes_view);
defer regex.destroy(&mut loaded);
```

RegexSet 也支持 database：

```aurex
let needed: usize = regex.database_bytes(&set);
let serialized: regex.DatabaseResult = regex.serialize_set(&set, out, cap);
var loaded: regex.RegexSet = regex.deserialize_set(bytes_view);
defer regex.destroy_set(&mut loaded);
```

database 格式做 magic、version、mode、容量、state/range/capture 引用和 RegexSet AC metadata 校验。格式错误返回 `database_error`。

### 4.8 bytes API

```aurex
import regex.bytes as bytes;

let raw: [2]u8 = [0xc3u8, 0xa9u8];
let view: []u8 = raw[:];

var compiled: bytes.Regex = bytes.compile("\\xc3\\xa9");
defer bytes.destroy(&mut compiled);

let ok: bytes.MatchResult = bytes.fullmatch_compiled(&compiled, view);
```

bytes mode 中 `.` 消费一个 byte，`\xNN` 匹配一个 byte。text API 调用 bytes regex 会返回 `unsupported`。

### 4.9 stream

```aurex
var stream: regex.RegexStream = regex.open_stream(&compiled);
defer regex.destroy_stream(&mut stream);

let feed_status: regex.RegexStatus = regex.stream_feed(&mut stream, chunk);
let found: regex.MatchResult = regex.stream_next(&mut stream);
let finish_status: regex.RegexStatus = regex.stream_finish(&mut stream);
```

当前 stream 是有预算的增量 buffer API。它会避免把跨 chunk 的潜在 match 截断，但还不是 Hyperscan 级别的 streaming automaton。

## 5. 与工业级实现的对比

### 5.1 RE2

RE2 的核心目标是安全、可预测、避免回溯爆炸。Aurex 当前路线和 RE2 最接近：

| 项目 | RE2 | Aurex 当前状态 |
| --- | --- | --- |
| 灾难性回溯 | 默认避免 | 默认避免 |
| 反向引用/lookaround | 不支持 | 不支持 |
| NFA/DFA 混合优化 | 成熟 | 尚缺 capture-free lazy DFA / dense DFA |
| Unicode | 成熟但语义边界与版本取决于实现 | Unicode 17.0 scalar/property/full fold/`\X` 已实现 |
| API 成熟度 | 生产多年验证 | API 面较完整，但生产流量验证不足 |
| 性能基准 | 工业成熟 | 缺同机 Google Benchmark 对照 |

Aurex 与 RE2 的主要差距不是“还能不能匹配更多语法”，而是工程成熟度、DFA 快路径、literal extraction、缓存策略、长期 fuzz 和跨平台 benchmark。

### 5.2 Rust `regex`

Rust `regex` 是安全正则路线的另一个重要参考：

| 项目 | Rust regex | Aurex 当前状态 |
| --- | --- | --- |
| text/bytes 双 API | 完整 | 已有 text / bytes facade |
| RegexSet | 完整 | 已有 RegexSet，exact-literal set 有 AC fast path |
| replace/split/iter | 完整 | 已有 replace/split/iter/callback/append |
| Unicode | 完整且长期维护 | Unicode 17.0 已接入，仍缺 normalization/canonical equivalence |
| literal planner | 成熟 | 当前只有 prefix/start-byte/exact-literal set，缺通用 planner |
| DFA cache | 成熟 | 尚缺 |
| fuzz/quickcheck 长期积累 | 成熟 | 当前有 deterministic differential/conformance，长期 fuzz 还不足 |

Aurex API 面已经接近 Rust regex 的方向，但运行时优化层和测试生态还落后。

### 5.3 PCRE2 / Oniguruma / .NET / Java

这些实现偏功能完整和兼容性，支持很多非 regular 或接近非 regular 的扩展。

Aurex 当前不支持的典型功能：

- 反向引用和命名反向引用。
- lookahead / lookbehind。
- atomic group 和 possessive quantifier。
- 条件表达式。
- 递归、subroutine call。
- branch reset。
- duplicate named capture 的复杂兼容规则。
- callout。
- PCRE control verbs。
- 更完整的 replacement / format flag 生态。

这些功能会显著增加引擎复杂度，有些会破坏线性时间保证。若产品目标需要 PCRE 兼容，应当新增独立模式，例如 `compile_pcre_compatible_with_limits`，并强制 timeout、step budget、heap budget 和回溯深度限制。默认 `regex.api.compile` 不应改变安全路线。

### 5.4 Hyperscan

Hyperscan 的核心强项是高吞吐多模式扫描、block/stream/vectored、预编译 database 和平台级优化。

Aurex 已经具备相似概念的雏形：

- RegexSet。
- exact-literal set Aho-Corasick。
- database serialize/deserialize。
- text/bytes stream。
- vectored scan。

但差距仍然很大：

- 没有 SIMD/vectorized transition。
- 没有 dense DFA / hybrid automata compiler。
- 没有真正 resumable streaming state。
- 没有大规模 pattern database 编译优化。
- 没有跨平台吞吐 benchmark 和 CPU feature dispatch。

因此当前不能把 Aurex regex 描述成 Hyperscan 替代品。比较合理的目标是先达到 RE2/Rust regex 风格的安全通用正则，再针对 RegexSet 做专门高吞吐路线。

### 5.5 C++ `std::regex`

用户之前要求 Unicode 语义“按照 C++ 那个类似的来”。当前 Aurex 更像 C++ 标准库中“编译对象 + match flags + iterator/result span”的 API 风格，但语法和语义不是完整 `std::regex` 方言兼容。

已接近的部分：

- `Regex` 是可复用编译对象。
- `MatchResult` / `Captures` 返回原输入中的位置 span。
- `RegexMatchOptions` 已有 `not_bol`、`not_eol`、`not_bow`、`not_eow`、`not_null`、`continuous`、`anchored`。
- find iterator、capture iterator、split、replace 与 C++ 常见使用模型类似。

仍缺：

- ECMAScript/basic/extended/awk/grep/egrep 多方言。
- locale traits、collating element、equivalence class。
- 完整 `match_flag_type` 和 `format_flag_type`。
- `sub_match` / `match_results` 风格的完整容器语义。
- C++ 标准库实现可能支持的实现相关优化和 locale 行为。

Aurex 不建议以“完全兼容 C++ `std::regex`”作为短期目标。更合理的目标是借鉴 C++ 的 match/result/options 形态，同时保持 RE2/Rust regex 的安全语义和明确 Unicode 版本。

## 6. 当前测试和数据

### 6.1 测试命令和结果

最近一次正则线验证基线如下：

| 命令 | 结果 | 备注 |
| --- | --- | --- |
| `tools/run_tests.sh` | 11/11 passed | regex conformance 慢测耗时 223.78s |
| `tools/check_coverage.sh` | 11/11 passed | regex conformance 慢测耗时 222.34s；source totals、`aurexc` 入口和 parser/sema focused gate 均通过 |
| `python3 tools/regex_differential.py --fold-end 32 --grapheme-end 32` | passed | 生成并运行 `build/regex_differential_generated.ax` |
| `build/full-llvm/bin/aurexc -I examples/libs examples/regex_advanced.ax -o build/tests/regex_advanced && build/tests/regex_advanced` | passed | 高级 API/Unicode/RegexSet/database/stream 回归样例 |
| `git diff --check` | passed | 代码提交前空白检查 |

覆盖率数据来自 `tools/check_coverage.sh`。主 source totals 排除 CLI 入口后统计 `src/**/*.cpp`，CLI 入口由独立 `aurexc --help` 覆盖 lane 统计；M2.1 还对 `parser_postfix.cpp`、`match.cpp`、`sema_expr.cpp`、`sema_types.cpp` 保留 focused 95% 门禁。

| 指标 | 当前值 |
| --- | ---: |
| Source line coverage | 95.75% |
| Source function coverage | 98.56% |
| Source region coverage | 95.38% |
| `aurexc` entrypoint line/function/region coverage | 100.00% / 100.00% / 100.00% |
| Focused parser/sema gate | 全部 >= 95.00% |

测试覆盖内容：

- `regex_demo.ax`：基础语法和 compiled API。
- `regex_phase1.ax`：捕获、命名捕获、iterator、replace、split、错误 offset/kind。
- `regex_industrial.ax`：flags、lazy、边界、扩展 escape、Unicode scalar/property/case-fold、非法 escape。
- `regex_advanced.ax`：nested class algebra、options/builder、bytes parity、Unicode full fold、`\X`、RegexSet span/callback/all-span/overlap/vectored、database、replace callback、splitn、stream、submatch precedence、线性 search、literal prefix、start-byte prefilter。
- `regex_stress.ax`：重复 compiled search/fullmatch、长前缀 search、RegexSet prefilter、资源预算和错误路径。
- `tools/regex_differential.py`：固定用例、deterministic property、Python `re` 差分、RegexSet exact-literal property、Unicode 17.0 `CaseFolding.txt`、Unicode 17.0 `GraphemeBreakTest.txt`。

### 6.2 样例编译耗时和内存

下面数据是本机样例编译数据，单位为 wall time 和 peak RSS。`--check` 表示前端检查路径；`native` 表示生成本地可执行文件路径。它们能反映当前 Aurex 编译这些正则库样例的成本，但不能替代正则运行时吞吐 benchmark。

| 样例 | `--check` 耗时 | `--check` peak RSS | `native` 耗时 | `native` peak RSS |
| --- | ---: | ---: | ---: | ---: |
| `examples/regex_demo.ax` | 0.33s | 123.7 MiB | 1.16s | 189.4 MiB |
| `examples/regex_phase1.ax` | 0.33s | 125.0 MiB | 1.16s | 189.7 MiB |
| `examples/regex_industrial.ax` | 0.33s | 123.9 MiB | 1.16s | 190.1 MiB |
| `examples/regex_advanced.ax` | 0.35s | 131.3 MiB | 1.59s | 941.5 MiB |
| `examples/regex_stress.ax` | 0.33s | 123.9 MiB | 1.16s | 190.3 MiB |

注意：

- `regex_advanced.ax` native RSS 941.5 MiB 是 compiler/native pipeline 的峰值，不是正则库运行时匹配 RSS。
- 其他样例 native RSS 稳定在约 190 MiB，说明普通正则样例没有触发同等规模的 native/backend 峰值。
- 这组数据说明当前“编译正则样例”的成本可见，但还不能回答 RE2/Rust regex/PCRE2 在同一批 pattern/input 上的运行时吞吐差距。

### 6.3 代码量

当前 `examples/libs/regex` 总代码量是 12,752 行：

| 文件 | 行数 |
| --- | ---: |
| `examples/libs/regex/bytes.ax` | 438 |
| `examples/libs/regex/syntax/ascii.ax` | 196 |
| `examples/libs/regex/ops/replace.ax` | 1,003 |
| `examples/libs/regex/ops/split.ax` | 184 |
| `examples/libs/regex/ops/stream.ax` | 301 |
| `examples/libs/regex/ops/iter.ax` | 173 |
| `examples/libs/regex/ops/escape.ax` | 150 |
| `examples/libs/regex/api.ax` | 965 |
| `examples/libs/regex/compile/set.ax` | 928 |
| `examples/libs/regex/compile/parser.ax` | 1,900 |
| `examples/libs/regex/compile/program.ax` | 527 |
| `examples/libs/regex/compile/database.ax` | 1,243 |
| `examples/libs/regex/vm/engine.ax` | 4,018 |
| `examples/libs/regex/runtime/alloc.ax` | 16 |
| `examples/libs/regex/config/limits.ax` | 26 |
| `examples/libs/regex/core/types.ax` | 525 |
| `examples/libs/regex/core/results.ax` | 159 |
| **合计** | **12,752** |

这个行数不包含 `examples/libs/unicode/ucd.ax` 的 Unicode 数据和 helper。正则库实际运行时能力依赖该 Unicode 模块。

## 7. 当前工业差距清单

### 7.1 功能缺口

| 缺口 | 当前影响 | 建议策略 |
| --- | --- | --- |
| PCRE 非 regular 功能 | 无法兼容依赖反向引用/lookaround/条件/递归的 pattern | 不进默认引擎；如必须支持，做独立 bounded backtracking engine |
| Unicode normalization | NFC/NFD 等价文本不会自动互相匹配 | 新增 `unicode.normalize` 可复用模块，再在 regex options 中开关 |
| 完整 UAX #29 边界 | 只有 `\X`，缺 word/sentence/grapheme boundary assertions | 在 Unicode 模块补 boundary iterator，再映射到 regex assertion |
| locale/collation | 不支持 C++ locale 风格排序、等价类、collating element | 短期不做，除非语言标准库明确需要 locale regex |
| 完整 C++ 方言 | 不支持 ECMAScript/basic/extended/awk/grep/egrep 多方言 | 不作为短期目标，优先安全单方言 |
| Replacement format 生态 | 已有 `$name`/`${name}`/escape，但未覆盖所有工业库格式细节 | 根据实际用户 API 继续补齐 |

### 7.2 性能缺口

| 缺口 | 当前影响 | 建议策略 |
| --- | --- | --- |
| regex 专用 Google Benchmark | 无法量化运行时吞吐和工业库差距 | 先建 bench harness，再做优化 |
| capture-free lazy DFA | 简单 search/fullmatch 常数不够低 | 对无捕获/无复杂子匹配路径引入 lazy DFA cache |
| one-pass NFA | 一部分确定性 pattern 仍走通用 VM | 编译期识别 one-pass program，运行时用专门 executor |
| 通用 literal extraction | 当前只有 prefix/start-byte/exact-literal set | 做 required literal、prefix/suffix、rare byte、multi-literal planner |
| mixed RegexSet planner | 非纯 literal set 仍偏通用 NFA | AC prefilter 先找候选，再 NFA verify |
| Unicode class transition 常数 | 大 Unicode 属性匹配仍会拉高常数 | 压缩 range table、cache property predicate、可选 DFA transition |
| 真 streaming automaton | 当前 stream 仍保留 buffer | 增加 resumable VM/DFA state 和 bounded lookbehind/history 模型 |
| SIMD / vectorization | 高吞吐扫描不如 Hyperscan | 不是下一步第一优先级，等 benchmark 和 planner 稳定后再做 |

### 7.3 工程缺口

| 缺口 | 当前影响 | 建议策略 |
| --- | --- | --- |
| 长期 fuzz | 当前有 deterministic differential/conformance，但随机语料还不够 | 增加 grammar-aware generator、libFuzzer/AFL 风格输入、database roundtrip fuzz |
| 多平台 perf baseline | 只有当前机器数据 | 保存机器信息、CPU、编译器版本、benchmark JSON |
| 线程安全文档 | 当前手动所有权和 borrowed input 规则需要更明确 | 明确 `Regex` 只读匹配是否可并发、哪些 API 会分配/写状态 |
| RAII/生命周期包装 | 调用方必须手动 destroy | 后续语言能力允许后提供 safer wrapper |
| database zero-copy/mmap | 当前 deserialize 会重建 owning 对象 | database v4 可增加 mmap/borrowed view |
| diagnostics 文本 | 当前有 status/kind/offset/code，但缺更友好的 message/help | 增加 diagnostic message table 和修复建议 |

## 8. 下一步计划

### P0：建立工业级 benchmark 基线

目标：先把“正则库运行时性能到底差多少”量清楚。

要做：

- 新增 Google Benchmark 目标，例如 `aurex_regex_bench`。
- 覆盖 compile time、is_match/search/fullmatch、captures、replace、split、RegexSet matches/find/scan、database deserialize、stream。
- 输入矩阵至少包含：
  - 短 literal。
  - 长 literal。
  - alternation。
  - Unicode property。
  - case-insensitive Unicode。
  - captures。
  - zero-length。
  - long haystack no-match。
  - long haystack late-match。
  - exact-literal RegexSet。
  - mixed RegexSet。
  - bytes mode。
- 同机对照：
  - RE2。
  - Rust `regex`。
  - PCRE2。
  - Hyperscan，仅对多模式/block/stream/vectored 合适项对照。
  - C++ `std::regex`，作为功能/API 参考和性能下限参考，不作为安全引擎目标。
- 输出 JSON，记录 wall time、CPU time、alloc、RSS、pattern/input 参数、库版本、编译选项和机器信息。

验收：

- 能回答每类用例 Aurex 比 RE2/Rust regex 慢多少。
- 能区分 compile time 和 match throughput。
- 能区分 text Unicode 成本和 bytes 成本。
- 能把后续优化结果写成可复现基线，而不是凭主观感觉判断。

### P1：capture-free lazy DFA / one-pass 快路径

目标：让最常见的无捕获 search/fullmatch 走更低常数路径。

要做：

- 编译期标记 program 是否 capture-free、是否包含需要 ordered submatch 的结构、是否可 one-pass。
- 对 capture-free 路径建立 lazy DFA cache，key 是 NFA state set + input class。
- 为 fullmatch/search 分别接入，不改变 captures 的语义。
- 保留资源预算，DFA cache 超限时回退 Thompson VM。

验收：

- 无捕获 literal/property/alternation 用例明显接近 RE2/Rust regex。
- captures 和 lazy/greedy 子匹配 precedence 不回归。
- DFA cache 超限不影响正确性。

### P2：通用 literal extraction 和 mixed RegexSet planner

目标：把当前 exact-literal set 的优势推广到混合 pattern。

要做：

- 从 AST/NFA 编译阶段抽取 required literals、prefix literals、suffix literals、rare byte。
- 为 RegexSet 构建 AC prefilter。
- 命中候选后按 pattern id 做 NFA verify。
- 对纯 literal 继续使用当前 AC fast path。
- 对没有有效 literal 的 pattern 保留通用 NFA set 路径。

验收：

- 混合 RegexSet 在长 no-match haystack 上不再按每个起点启动通用 NFA。
- exact-literal set 不退化。
- overlap/all-span/vectored/database roundtrip 行为不变。

### P3：Unicode normalization 和 boundary 模块

目标：把 Unicode 能力从 regex 私有逻辑提升为标准库可复用模块。

要做：

- 新增可复用 `unicode.normalize` 模块，支持 NFC/NFD/NFKC/NFKD 所需基础表和 iterator。
- 新增 word/grapheme/sentence boundary iterator。
- regex options 增加可选 normalization matching。默认不开，避免隐式成本。
- `\b{...}` 扩展到更多 UAX #29 边界。

验收：

- Unicode 官方 normalization/boundary 数据通过。
- 默认 regex 性能不因 normalization 模块引入明显成本。
- `\X` 现有行为不回归。

### P4：stream 和 database 工业化

目标：把当前“可用的 stream/database API”推进到生产服务可用。

要做：

- stream 从 buffer-based scan 演进到 resumable VM/DFA state。
- RegexSet streaming 支持多模式跨 chunk。
- database v4 增加 manifest、checksum、feature flags、endianness/version validation。
- 增加 zero-copy/mmap borrowed view，避免 deserialize 全量重建。

验收：

- 长流扫描 RSS 只和状态规模及 bounded history 有关。
- database 反序列化错误有稳定诊断。
- database roundtrip、corruption fuzz 和跨版本拒绝策略明确。

### P5：可选 PCRE-compatible bounded engine

目标：如果产品确实需要 PCRE 功能，另开隔离模式。

原则：

- 默认 `regex.compile` 继续保持安全线性路线。
- PCRE-compatible 模式必须显式 opt-in。
- 必须有 timeout、step budget、recursion/stack budget、heap budget。
- 必须在 API 和文档里标明复杂度不再等同默认引擎。

短期建议：暂不做。先把 RE2/Rust regex 这条安全工业路线跑扎实。

## 9. 风险和注意事项

1. 不要把样例 native 编译 RSS 当作正则运行时 RSS。当前 `regex_advanced.ax` 的 941.5 MiB 是编译器/native pipeline 峰值。
2. 不要继续补零散小 API 伪装成性能优化。下一步性能必须由 benchmark、planner、DFA 快路径驱动。
3. 不要把 PCRE 功能混入默认引擎。反向引用/lookaround 等功能要么不做，要么单独 bounded engine。
4. 不要在没有 benchmark 的情况下做 SIMD。SIMD 是后续高吞吐阶段，不是当前最大瓶颈。
5. 不要把 Unicode normalization 默认开启。它应当是显式选项，否则会把所有普通匹配拖慢。
6. 不要忘记手动释放。当前 `Regex`、`RegexSet`、`Captures`、`RegexStream` 都有显式所有权。

## 10. 当前阶段判断

按工业正则库的标准看，Aurex regex 当前处在“安全路线 API 面和正确性面基本成型，性能优化层和工业验证层仍需集中补齐”的阶段。

已经完成的东西足够支撑继续做真性能优化：

- 语法和 API 面不是临时 demo。
- Unicode 语义已经足够具体。
- RegexSet/database/stream/vectored 已经有结构，不需要重写 API 才能扩展。
- 测试和覆盖率已经能防止大量行为回归。

下一阶段不应该再以“小修小补”推进，而应该围绕三条主线：

1. 正则 Google Benchmark 和工业库对照。
2. capture-free DFA / one-pass / literal planner。
3. mixed RegexSet + stream + database 的高吞吐工业化。

这三条完成后，Aurex regex 才能从“功能和结构接近工业路线”推进到“性能和验证也真正接近工业实现”。

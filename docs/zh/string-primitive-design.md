# Aurex 字符串基础类型设计草案

版本：0.1.7 设计草案
日期：2026-05-08
状态：设计冻结前草案；Phase 1、Phase 2、Phase 3、Phase 4 C FFI 边界、Phase 5 scalar API、raw `Bytes` / bytes-backed `Path` 风险收口已落地。

本文目标是把 Aurex 的字符串基础类型设计清楚，再继续改标准库和 M1 样例。这里的“基础类型”指和 `int`、`usize`、`bool` 同一层级的语言内建类型，而不是 C 的 `char*` 包装，也不是某个需要 allocator 的拥有型容器。

## 结论先行

Aurex 应把 `str` 定义为内建、不可变、借用的 UTF-8 文本切片：

```aurex
// 语义模型，不要求暴露字段名
builtin str = {
    data: *const u8,
    len: usize, // byte length
}
```

核心决策：

- `str` 是文本，不是字节数组；所有 `str` 值必须是有效 UTF-8。
- `str` 不拥有内存，不分配、不释放，按值复制只复制两字机器字。
- `str` 的长度单位是字节；命名必须写成 `byte_len()`，避免 `len()` 被误解成字符数。
- `str` 允许内部 `\0`，没有尾随 NUL 保证，也不能隐式转成 `*const u8`。
- 普通字符串字面量 `"..."` 的类型是 `str`。
- C 字符串字面量 `c"..."` 只能作为显式 FFI 逃生口，长期应收敛到 `CStr` / `CString`，不能参与普通文本 API。
- 拥有型字符串放在标准库 `String`，它应成为“拥有 UTF-8 buffer”，并通过 `as_str()` 暴露 `str`。
- 原始字节必须使用 `Vec<u8>` / `Bytes` / `Span<u8>`；不要把任意 bytes 塞进 `String` 或 `str`。
- 不提供默认 `s[i]` 字符索引。必须通过 `bytes()`、`scalars()`、`graphemes()` 等显式视图操作。
- 默认相等、哈希、排序按 UTF-8 字节序列进行；Unicode normalization、locale collation、grapheme segmentation 全部显式调用。

这基本采用 Rust `str` 的系统级核心模型，吸收 Swift 现代 UTF-8 存储和 grapheme API 的经验，但不采用 Swift 的默认 grapheme collection 语义；同时避免 Go 的“字符串可含任意 bytes”、Java/JavaScript 的 UTF-16 surrogate 历史包袱，以及 C/C++ 的 C-string/text/bytes 混杂问题。

## 当前项目状态

当前 Aurex 已经具备 `str` 的雏形，但标准库语义还没有跟上：

- 编译器类型表已有 `BuiltinType::str`，`TypeTable::is_str()` 能识别内建 `str`。
- 语义分析中普通字符串字面量类型是 `str`，`c"..."` 类型是 `*const u8`。
- LLVM 后端把 `str` 降低为 `{ ptr, usize }`，普通字符串字面量降低为全局字节数据加长度。
- 测试已经锁住 `size_of(str) == 16`、`align_of(str) == 8` 这一 64-bit ABI 事实。
- `std.core.string.String` 仍以 `VecU8` 存储并维护尾随 `\0` 作为兼容层，但已经新增 `from_str`、`from_utf8`、`as_str`、`as_str_checked`、`as_bytes`、`append(str)`、`push_scalar`、`insert_scalar`、`pop_scalar`、`remove_scalar_at`、`byte_len`、`slice_bytes_checked`、`truncate_bytes_checked`、`equals(str)`、`starts_with(str)`、`ends_with(str)` 等 UTF-8/`str` surface；旧字节级 `push/insert/remove/truncate/append_span/from_c` 已开始收口到 UTF-8 边界检查，且不再暴露 `String.as_mut_span()`。
- `std.core.bytes.Bytes` 已作为拥有型 raw bytes 容器落地，提供 `from_span`、`as_span`、`as_mut_span`、`append`、`push/pop/remove/truncate/clear` 等 API；`Bytes.append` 已处理 self-alias，扩容时不会读失效源指针。
- `std.ffi.c.string` 已提供 `CStr` / `CString` 第一版，把 borrowed/owned NUL-terminated C 字符串从普通文本 API 中隔离出来。
- `std.core.text` 目前主要是 `SpanU8`、ASCII helper、`c_strlen`、`strcmp` 这类 C/bytes 工具。
- `std.fs.path.Path` 已从 `String` 语义中拆出，当前由 `Bytes` 存储平台路径 bytes；`from_span` 接受非 UTF-8 bytes 但拒绝内部 NUL，`from_str` 只是 UTF-8 文本便利入口，`as_c` 仍提供 NUL-terminated 兼容视图。
- `std.fs.file`、`std.fs.dir` 和 M1 样例仍有部分底层接口使用 `*const u8` / `c"..."`，后续应继续向 `Path`、`CStr` / `CString` 和 `str` 分层迁移。

因此 ABI 方向已经接近正确；真正要重构的是“类型边界”和“标准库 API 边界”：`str` 负责有效 UTF-8 借用文本，`String` 负责拥有文本，`Bytes` / `Span<u8>` 负责原始字节，`CStr` / `CString` 负责 C FFI。

## 业界设计对比

### C

C 的字符串是以 NUL 结尾的 `char*` 字节序列，没有长度字段，没有编码不变量，也没有所有权信息。它的优点是 ABI 极简、和 OS/ libc 兼容；代价是内部 NUL 截断、越界、生命周期、编码混乱都会泄漏到所有 API。Aurex 不能把这个模型作为普通字符串基础类型，只能把它放在 FFI 层。

### C++

`std::string` 本质是拥有字节 buffer，长期不承诺 UTF-8 文本语义；`std::u8string` 和 `char8_t` 改善了 UTF-8 code unit 的类型表达，但仍没有自动提供 Unicode scalar、grapheme、normalization 等文本语义。C++ 的经验说明：把“字节容器”叫字符串会长期造成 API 分裂，后面再补 `char8_t` 也只是部分修复。

### Rust

Rust 的 `str` 是最接近 Aurex 需求的模型：借用字符串切片、有效 UTF-8、不拥有内存、由 pointer + length 表示；`String` 是拥有 UTF-8 buffer。Rust 还明确把 `len()` 定义成字节长度，并要求切片发生在 UTF-8 code point 边界上。Aurex 应采用这个核心，但要避免把 `len()` 这个名字直接暴露为“看似字符长度”的 API，建议命名为 `byte_len()`。

### Swift

Swift 的 `String` 是 Unicode-correct 的值类型，面向用户感知字符，即 extended grapheme cluster；Swift 5 也把首选存储切到 UTF-8。这个模型用户体验很好，尤其适合 App/UI 层文本；但把 grapheme 当默认 collection 元素会让索引、切片、计数都依赖 Unicode segmentation，成本高、规则随 Unicode 版本演进。Aurex 可以学习 Swift 的显式 Unicode 视图和 UTF-8 存储方向，但不应把 grapheme 作为基础类型默认索引单位。

### Go

Go 的 `string` 是不可变 byte sequence，源码 UTF-8，`range` 可按 rune 解码；但 string 值可以包含任意 bytes，索引返回 byte。这很适合系统编程和网络协议，但它把“文本”和“bytes”揉在一起，导致无效 UTF-8 能在普通字符串里长期流动。Aurex 不应采用 Go 的任意 bytes string；原始 bytes 应使用 `Bytes` / `Span<u8>`。

### Java

Java `String` 不可变、可共享，API 历史上以 UTF-16 code unit 为索引单位；非 BMP 字符需要 surrogate pair，占两个 `char` 位置。Java 9 的 Compact Strings 改善了内部内存占用，但公开 API 的 UTF-16 code unit 语义无法撤回。Aurex 应避免 UTF-16 作为核心表示，避免 surrogate pair 成为永久语言包袱。

### JavaScript / ECMAScript

ECMAScript String 是 16-bit code unit 序列，规范允许 ill-formed UTF-16 子序列；`length` 和许多 API 都按 code unit 工作，normalization 也必须显式调用。这是 Web 兼容性下的合理历史设计，但对新系统语言来说是需要规避的反例：不要让普通字符串能表示半个 surrogate，也不要让索引单位和用户认知长期错位。

### Python 3

Python 3 的 `str` 是 Unicode 文本，CPython 通过 PEP 393 使用 1/2/4-byte flexible representation，兼顾 ASCII 内存效率和全 Unicode code point 访问。这对动态语言很好，但对象头、GC、可变内部表示不适合作为 Aurex 的固定 ABI primitive。Aurex 可以学习“内部表示不暴露”和“UTF-8 作为 C 交互推荐表示”的经验，但基础 `str` 需要固定两字布局。

### Zig

Zig 倾向用 `[]const u8` 表达字符串字面量和文本，语言保持极度显式。这符合系统语言透明性，但缺少“这是有效文本”的类型级不变量。Aurex 如果只用 `Span<u8>`，后续所有 parser、diagnostic、path、source manager 都会继续手写 UTF-8 检查。Aurex 需要 `str` 来把有效文本从原始 bytes 中分离出来。

### C# / Kotlin / JVM 家族

这些语言通常继承 UTF-16 字符串模型，API 成熟、生态强，但同样有 code unit 和 code point / grapheme 不一致的问题。它们的经验更适合说明“历史 ABI 一旦公开就很难修”，不适合作为 Aurex 新基础类型的方向。

## 学术与前沿方向

### UTF-8 everywhere 与 validate-once

现代系统软件、Web、源码、JSON/TOML/YAML、日志、CLI、路径显示层都大量使用 UTF-8。Swift 5 切到 UTF-8 首选存储，Rust 从一开始把 `str` 锁成有效 UTF-8，都说明“创建时验证一次，之后依赖不变量”是工程上最稳的路线。Aurex 应把 UTF-8 validity 作为 `str` 的核心不变量。

### Grapheme segmentation 不适合做 primitive 默认语义

Unicode UAX #29 定义 extended grapheme cluster，用于用户感知字符、光标移动、删除一个可见字符等操作。它非常重要，但规则复杂、版本化、locale 可裁剪，不适合塞进基础类型的 O(1) 语义里。Aurex 应提供 `unicode::graphemes(str)`，而不是让 `s[i]` 或默认 `len()` 悄悄变成 grapheme 语义。

### Normalization 必须显式

Unicode UAX #15 定义 NFC/NFD/NFKC/NFKD。Swift 默认相等会考虑 canonical representation；JavaScript 则把 normalization 放到显式 API。对 Aurex 这类系统语言，默认相等如果隐式 normalization，会让 `HashMap<str, V>`、build graph key、symbol table、文件名比较都承担隐藏 O(n) 成本和 Unicode 版本依赖。因此核心相等应是字节相等；需要 canonical equality 时调用显式 API。

### Rope / piece table 是编辑器文本结构，不是基础 `str`

Rope 论文指出传统连续字符串不适合大文本编辑、频繁拼接和共享切片。这个方向适合标准库后续提供 `Rope`、`TextBuffer`、`StringBuilder` 或 source file buffer；不适合替代 `str`。基础 `str` 应保持简单稳定，复杂文本结构通过库类型组合。

### Interning / atoms 是符号优化，不是字符串语义

编译器 identifier、module name、target name 可以用 interner 或 atom 表优化比较和存储，但 atom 不是文本本身。Aurex 应允许 `Symbol` / `Atom` 从 `str` 构造，不能让所有 `str` 默认 intern，否则会把 allocator、全局表、生命周期和线程安全问题塞进 primitive。

## Aurex `str` 规范草案

### 类型与布局

`str` 是内建 copyable 值类型，运行时语义等价于：

- `data: *const u8`
- `len: usize`

不变量：

- 若 `len > 0`，`data` 必须非 null，并且指向至少 `len` 个可读字节。
- 若 `len == 0`，`data` 不可解引用；实现可以使用 null 或静态空地址。
- `data[0..len]` 必须是有效 UTF-8。
- `data[0..len]` 可以包含 `0x00`。
- `data[len]` 不保证存在，也不保证是 `0x00`。
- `str` 不拥有 `data`，不会释放内存。

ABI：

- 在 64-bit target 上 `size_of(str) == 16`、`align_of(str) == 8`。
- 以值传参只传 `{ptr, len}`。
- 导出到 C ABI 时必须使用显式 ABI struct，例如 `aurex_str { const uint8_t* data; uintptr_t len; }`；不得隐式降级成 `char*`。

### 字面量

普通字符串字面量：

```aurex
let s: str = "hello";
```

规则：

- 字面量内容必须解码成有效 UTF-8。
- 支持 `\\`、`\"`、`\0`、`\n`、`\r`、`\t`。
- 应新增 `\u{...}`，表示 Unicode scalar value，排除 surrogate range 和超过 `U+10FFFF` 的值。
- 普通 `str` 字面量不应支持任意 raw byte escape；如果未来支持 `\xNN`，只允许产生 ASCII byte，或明确改成 bytes literal。
- 无效 escape 不能像当前 `decode_string_literal` 那样静默吞掉，必须诊断。

C 字符串字面量：

```aurex
let p: *const u8 = c"hello";
```

长期规则：

- `c"..."` 只用于 FFI。
- 输出带一个隐式尾随 NUL。
- 不隐式转成 `str`。
- 不接受内部 NUL；如需 raw bytes，未来使用 bytes literal。
- 更理想的长期类型是 `CStr`，当前 `*const u8` 作为兼容过渡。

### 长度与索引

不要提供默认 `s[i]`。所有单位必须显式：

- `byte_len(s: str) -> usize`：O(1)，返回 UTF-8 byte 数。
- `is_empty(s: str) -> bool`：O(1)。
- `as_bytes(s: str) -> Span<u8>`：O(1)，只读 byte view。
- `is_boundary(s: str, index: usize) -> bool`：检查 UTF-8 scalar 边界。
- `slice_bytes_checked(s: str, begin: usize, end: usize) -> Result<str, SliceError>`：begin/end 必须在边界上。
- `is_scalar_value(value: u32) -> bool`：检查 `u32` 是否是合法 Unicode scalar value，排除 surrogate 和超过 `U+10FFFF` 的值。
- `scalar_utf8_width(value: u32) -> Result<usize, ScalarError>`：返回合法 scalar 的 UTF-8 byte 宽度。
- `scalar_at(s: str, byte_index: usize) -> Result<u32, ScalarError>`：byte_index 必须在 scalar 边界上，返回 Unicode scalar value。
- `scalar_width_at(s: str, byte_index: usize) -> Result<usize, ScalarError>`：返回当前 scalar 的 UTF-8 byte 宽度。
- `next_boundary(s: str, byte_index: usize) -> Result<usize, ScalarError>` / `previous_boundary(s: str, byte_index: usize) -> Result<usize, ScalarError>`：显式边界移动。
- `scalar_count(s: str) -> usize`：O(n)，名字明确。
- 后续再提供 `scalars(s: str) -> ScalarIter`：按 Unicode scalar value 迭代，O(n)。
- `graphemes(s: str)` 和 `grapheme_count(s: str)`：放在 `std.unicode`，O(n)，依赖 Unicode 数据版本。

如果未来引入 `char`：

- `char` 必须表示 Unicode scalar value。
- `char` 不是 `u8`，不是 C `char`，也不是 grapheme cluster。
- `u8` 继续表示 byte。
- 单个用户感知字符可由 `unicode::Grapheme` 或 `str` slice 表示，不做 primitive。

### 相等、哈希、排序

默认：

- `==`：UTF-8 byte sequence 完全相同。
- `hash(str)`：按 bytes hash，和 `==` 一致。
- `<` / `cmp`：如果提供，只表示稳定 binary lexicographic order，不表示人类语言排序。

显式 Unicode API：

- `unicode::normalize_nfc(str) -> String`
- `unicode::normalize_nfd(str) -> String`
- `unicode::canonical_equal(left: str, right: str) -> bool`
- `unicode::collate(locale, left, right)` 后续再做，不进 core。

这样做的原因是：build target name、module name、symbol key、diagnostic label 等系统工程场景需要稳定、可预测、低成本的 key 语义；用户可见文本再通过显式 Unicode 模块获得更高级语义。

## 标准库类型边界

### `String`

`std.core.string.String` 应重新定义为拥有型 UTF-8 buffer：

- 内部可继续使用 `Vec<u8>`。
- 必须保持 UTF-8 validity。
- 可以维护尾随 NUL 作为优化，但这不是公开语义。
- `as_str(self: *const String) -> str`。
- `as_bytes(self: *const String) -> Span<u8>`。
- `from_str(text: str) -> Result<String, AllocError>`。
- `from_utf8(bytes: Vec<u8>) -> Result<String, Utf8Error>`，成功后接管 bytes。
- `try_from_bytes(bytes: Span<u8>) -> Result<String, Utf8Error | AllocError>`。
- `push_scalar(self, value: u32)`、`insert_scalar(self, byte_index: usize, value: u32)` 或 `append(self, text: str)`，而不是用 `push(u8)` 拼文本。
- 字节级可变视图不能出现在 safe `String` surface；当前已从 `String` 移除，raw mutation 使用 `Bytes.as_mut_span()`。

当前 `String.push(u8)`、`insert(index, u8)`、`remove(index)`、`truncate(byte_len)` 可以先保留兼容包装，但新 API 不应继续扩展它们。

### `Bytes`

新增或明确使用：

- `Bytes` / `Vec<u8>`：拥有原始 bytes。
- `Span<u8>`：借用原始 bytes。
- `Bytes` 不承诺 UTF-8。
- `Bytes.as_mut_span()` 是 raw byte mutation 的 safe surface；它不会影响 `String` / `str` 的 UTF-8 不变量。
- `Bytes.append(span)` 必须允许 span 指向自身 buffer，扩容后仍复制正确内容。
- 文本解析入口使用 `String.from_utf8` 或 `str.from_utf8` 验证后进入 `str` 世界。

### `CStr` / `CString`

FFI 层应有单独类型：

- `CStr`：借用 NUL-terminated C string，不拥有。
- `CString`：拥有 NUL-terminated C string，不允许内部 NUL。
- `CStr.as_str_utf8() -> Result<str, Utf8Error>`。
- `CString.from_str(str) -> Result<CString, InteriorNulError | AllocError>`。
- `CString.as_c() -> *const u8`。

普通 `str` 不提供 `.c_str()`。需要 C 指针时必须显式构造 `CString` 或进入 `with_c_str` 作用域，避免生命周期和内部 NUL 坑。

### `Path`

`Path` 不应等同于 `String`：

- POSIX path 是 bytes，通常但不保证 UTF-8。
- Windows path 需要宽字符/UTF-16 系统 API。
- Aurex 的 `Path` 应是独立类型，提供 `from_str`、`from_span` / future `from_bytes`、`display`、`as_platform` 等显式入口。
- 当前 POSIX 侧基线已经让 `Path` 由 `Bytes` 支撑：`from_span` 不验证 UTF-8，但拒绝内部 NUL；`from_str` 是便利构造；`as_span` 返回 raw bytes；`as_c` 返回内部 NUL-terminated 兼容指针。
- 标准库文件 API 应优先接收 `Path` 或 `str`，底层 FFI 再转换成平台需要的形式。

## 实施路线

### Phase 0：锁定设计和现状测试

- 保留现有 `str` ABI：两字 `{ptr, len}`。
- 增加/保留 `size_of(str)`、`align_of(str)`、普通字面量类型和 `c"..."` 类型测试。
- 写入本设计文档，后续代码改动以本文为准。

### Phase 1：编译器字面量与诊断

状态：已落地基线。

- 已完成：普通字符串字面量解码后验证 UTF-8。
- invalid escape 改为诊断，不再静默接受。
- 新增 `\u{...}` Unicode scalar escape。
- 明确普通 `str` 字面量不产生 trailing NUL。
- `c"..."` 仍生成 trailing NUL，但加入内部 NUL/无效 escape 诊断策略。
- 增加 negative tests：invalid UTF-8、invalid scalar、surrogate、bad escape。

落地文件：

- `include/aurex/base/string_literal.hpp` / `src/base/string_literal.cpp`：共享字符串字面量解码、UTF-8 validation、Unicode scalar validation。
- `src/lex/lexer.cpp`：lexer 在产出字符串 token 前执行 escape/UTF-8/C string NUL 诊断。
- `src/backend/llvm/llvm_backend_util.cpp`：LLVM 后端复用共享解码逻辑，避免 lexer/backend escape 语义分裂。
- `tests/gtest/frontend/lexer_tests.cpp`、`tests/gtest/backend/llvm_utility_tests.cpp`、`tests/samples/positive/types/string_unicode_escape.ax` 和三个 negative samples 覆盖 Phase 1 行为。

### Phase 2：`std.core.str` 基础 API

状态：已落地基线。

- `byte_len`
- `is_empty`
- `as_bytes`
- `is_boundary`
- `slice_bytes_checked`
- `starts_with`
- `ends_with`
- `equals`
- `from_utf8`

落地文件：

- `std/core/str.ax`：提供 `byte_len`、`is_empty`、`as_bytes`、`equals`、`starts_with`、`ends_with`、`is_boundary`、`slice_bytes_checked`、`is_valid_utf8`、`from_utf8`。
- `src/parse/parser_expr.cpp` / `src/sema/sema_expr.cpp` / `src/ir/lower_ast_expr.cpp` / `src/backend/llvm/llvm_backend_value.cpp`：提供低层 `str_data`、`str_byte_len`、`str_from_bytes_unchecked` 编译器内建，支撑标准库实现。
- `tests/samples/positive/std/std_str.ax` 和 `AurexIntegrationTest.SampleSuite_Std_std_str`：覆盖 checked surface、IR surface 和 native runtime。

注意：`str_from_bytes_unchecked` 当前是为了在缺少 unsafe 语义前支撑标准库 checked API 的低层内建。普通代码应使用 `std.core.str.from_utf8` 或 `slice_bytes_checked`，不要直接构造可能违反 UTF-8 不变量的 `str`。

### Phase 3：重构 `String`

状态：已落地第一批兼容重构。

- 已完成：`String` 新增 `from_str`、`from_utf8`、`from_c_utf8`、`as_str`、`as_str_checked`、`as_bytes`、`append(str)`、`byte_len`。
- 已完成：新增 `equals(str)`、`starts_with(str)`、`ends_with(str)`，让拥有型字符串可以直接对接 borrowed `str` API。
- 已完成：旧 `from_c` 改为 `from_c_utf8` 包装，`read_text` 改为通过 `String.from_utf8` 验证文件内容。
- 已完成：旧 `push(u8)` 仅接受 ASCII byte，`insert(index, u8)` 要求 UTF-8 边界和 ASCII byte，`remove/pop/truncate` 避免切断多字节 scalar；`append_span` / `append_c` 现在要求输入是合法 UTF-8。
- 已完成：新增 `slice_bytes_checked` 和 `truncate_bytes_checked`，owned `String` 的字节边界操作也必须显式检查 UTF-8 scalar 边界。
- 已完成：新增 `push_scalar`、`insert_scalar`、`pop_scalar`、`remove_scalar_at`，把 owned `String` 的文本 mutation 从 byte API 迁到 Unicode scalar value API。
- 已完成：新增 `tests/samples/positive/std/std_string.ax` 和 `SampleSuite_Std_std_string`，覆盖 `String.from_str`、`from_utf8`、`as_str`、checked view、invalid UTF-8 拒绝、旧 byte API 的 UTF-8 边界保护和 C 兼容入口。
- 已完成：移除 `String.as_mut_span()`，避免 safe API 直接破坏 UTF-8 不变量；需要 raw mutable bytes 时使用 `std.core.bytes.Bytes`。
- 已完成：`String.append(str)` 的 self-alias 场景已覆盖，扩容后不会读失效源指针。

### Phase 4：隔离 C FFI

状态：已落地第一批边界类型。

- 已完成：新增 `std.ffi.c.string.CStr`，表示 borrowed NUL-terminated C 字符串，不拥有内存。
- 已完成：新增 `std.ffi.c.string.CString`，表示 owned NUL-terminated C 字符串，`from_str` 拒绝内部 NUL，`from_utf8` 拒绝非法 UTF-8。
- 已完成：`CStr.as_str_utf8()` 和 `CString.as_str()` / `as_str_checked()` 把 C 边界显式转回 `str`。
- 已完成：新增 `tests/samples/positive/std/std_cstring.ax` 和 `SampleSuite_Std_std_cstring`，覆盖 borrowed/owned C string、UTF-8 conversion、内部 NUL 拒绝和 invalid UTF-8 拒绝。
- 未完成：文件、进程、console、host support 还没有整体改成接收 `CStr` / `CString`；`c"..."` 仍在底层 std 和部分测试中作为兼容过渡。

### Phase 5：Unicode 扩展

- 已完成：添加 `is_scalar_value`、`scalar_utf8_width`、`scalar_at`、`scalar_width_at`、`next_boundary`、`previous_boundary`、`scalar_count`，用 `u32` 显式表示 Unicode scalar value。
- 待完成：添加 UTF-8 scalar iterator。
- 后续再引入 UAX #29 grapheme iterator 和 UAX #15 normalization。
- Unicode 数据版本必须在模块中显式记录，不影响 core `str` ABI。

### Phase 5.5：raw bytes 与平台路径拆分

状态：已落地第一批风险收口。

- 已完成：新增 `std.core.bytes.Bytes`，作为拥有型 raw bytes 容器，不承诺 UTF-8。
- 已完成：`Bytes` 提供 `from_span`、`as_span`、`as_mut_span`、`append`、`push/pop/remove/truncate/clear` 和 method API。
- 已完成：`Bytes.append` 处理 self-alias，覆盖 `raw.append(raw.as_span())` 触发扩容的场景。
- 已完成：`std.fs.path.Path` 改为 bytes-backed，`from_span` 接受非 UTF-8 path bytes、拒绝内部 NUL，`from_str` 作为便利入口，`as_c` 提供 NUL-terminated 兼容视图。
- 已完成：新增 `tests/samples/positive/std/std_bytes.ax` 和 `SampleSuite_Std_std_bytes`，覆盖 raw bytes mutation、自别名 append、非 UTF-8 path bytes、NUL rejection 和 `Path.from_str`。
- 已完成：`std_collections_path` 样例不再通过 `String.as_mut_span` 编辑字符串，改用受控 `String` byte compatibility API，并新增 `Bytes` raw mutation 覆盖。

### Phase 6：高级文本结构

- `StringBuilder`：高效拼接。
- `Rope` / `TextBuffer`：大文本编辑、source manager、diagnostic buffer。
- `Atom` / `Symbol`：编译器标识符和 build target name interning。

这些都是库类型，不改变 primitive `str`。

## 必须避免的坑

- 不要把 `str` 做成 C string；不能要求 trailing NUL，不能隐式转 `char*`。
- 不要把 `str` 做成任意 bytes；否则 UTF-8 不变量失效，文本 bug 会扩散到所有 API。
- 不要把 UTF-16 作为核心 ABI；surrogate pair 问题会永久进入索引和长度语义。
- 不要默认 `s[i]` 返回 byte、scalar 或 grapheme；三个答案都容易被误解。
- 不要让默认 equality 隐式 normalization；系统 key 语义需要稳定低成本。
- 不要把 OS path 等同于 `String`；路径是平台对象，不是普通 Unicode 文本。
- 不要把 rope/small-string/interner 设计塞进 primitive；它们是库层优化。
- 不要在没有生命周期/借用检查前，让 API 长期保存从临时 `String.as_str()` 借来的 `str`；这需要后续资源/borrow 规则配合。

## 验收测试清单

后续实现时至少补这些测试；已完成的项保留在这里作为回归边界：

- `size_of(str)` 和 `align_of(str)` 在 64-bit 下保持 16/8。
- `"abc"` 类型是 `str`，`c"abc"` 类型是 `*const u8` 或未来 `CStr`，二者不能隐式互转。
- `"a\0b"` 是合法 `str`，`byte_len()` 为 3，不能被 C API 截断。
- 已完成：非 ASCII 文本如 `"ƒ"` 的 `byte_len()` 为 2，`scalar_count()` 为 1。
- 已完成：invalid UTF-8 字面量、invalid escape、surrogate escape 都给出稳定诊断。
- 已完成：`slice_bytes_checked("ƒ", 0, 1)` 失败，`slice_bytes_checked("ƒ", 0, 2)` 成功。
- 已完成：`String.from_utf8` 接受合法 UTF-8，拒绝非法 bytes。
- 已完成：`CString.from_str("a\0b")` 拒绝内部 NUL。
- 已完成：`String` safe surface 不再暴露 `as_mut_span`。
- 已完成：`Bytes` 覆盖 raw mutable bytes 和自别名 append。
- 已完成：`Path.from_span` 接受非 UTF-8 bytes 并拒绝内部 NUL。
- 标准库文件/进程/path API 的公开层继续减少业务代码传 `c"..."` 的需求。

## 参考资料

- Rust `str` primitive：<https://doc.rust-lang.org/std/primitive.str.html>
- Swift `String` 文档：<https://developer.apple.com/documentation/Swift/String>
- Swift UTF-8 String：<https://www.swift.org/blog/utf8-string/>
- Go strings / bytes / runes：<https://go.dev/blog/strings>
- Go string wiki：<https://go.dev/wiki/GoStrings>
- Java `String`：<https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/lang/String.html>
- OpenJDK JEP 254 Compact Strings：<https://openjdk.org/jeps/254>
- ECMAScript String Type：<https://tc39.es/ecma262/multipage/ecmascript-data-types-and-values.html#sec-ecmascript-language-types-string-type>
- Python PEP 393：<https://peps.python.org/pep-0393/>
- RFC 3629 UTF-8：<https://datatracker.ietf.org/doc/rfc3629/>
- Unicode UAX #29 Text Segmentation：<https://www.unicode.org/reports/tr29/>
- Unicode UAX #15 Normalization：<https://unicode.org/reports/tr15/>
- C++ `char8_t` proposal P0482：<https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0482r2.html>
- Boehm, Atkinson, Plass, “Ropes: an alternative to strings”：<https://www.sri.com/publication/ropes-an-alternative-to-strings/>

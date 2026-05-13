# Aurex 字符串基础类型设计草案

版本：M2 设计草案
日期：2026-05-10
状态：语言核心设计草案。M2 当前仓库已经冻结并删除标准库实现；旧 M1 标准库落地记录只作为历史输入，不再代表当前树的实现状态。后续应先稳定内建 `str` 的语言语义，再重新设计 `String` / `Bytes` / `Path` 等库类型。

本文目标是把 Aurex 的字符串基础类型设计清楚。这里的“基础类型”指和 `int`、`usize`、`bool` 同一层级的语言内建类型，而不是 C 的 `char*` 包装，也不是某个需要 allocator 的拥有型容器。

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
- 拥有型字符串未来应由库层提供 `String`，它应成为“拥有 UTF-8 buffer”，并通过 `as_str()` 暴露 `str`。
- 原始字节未来应使用 `Vec[u8]` / `Bytes` / `Span[u8]` 这类库类型；不要把任意 bytes 塞进 `String` 或 `str`。
- 不提供默认 `s[i]` 字符索引。必须通过 `bytes()`、`scalars()`、`graphemes()` 等显式视图操作。
- 默认相等、哈希、排序按 UTF-8 字节序列进行；Unicode normalization、locale collation、grapheme segmentation 全部显式调用。

这基本采用 Rust `str` 的系统级核心模型，吸收 Swift 现代 UTF-8 存储和 grapheme API 的经验，但不采用 Swift 的默认 grapheme collection 语义；同时避免 Go 的“字符串可含任意 bytes”、Java/JavaScript 的 UTF-16 surrogate 历史包袱，以及 C/C++ 的 C-string/text/bytes 混杂问题。

## 当前项目状态（M2）

当前 Aurex 已经具备 `str` 的语言核心雏形，但没有活跃标准库层：

- 编译器类型表已有 `BuiltinType::str`，`TypeTable::is_str()` 能识别内建 `str`。
- 语义分析中普通字符串字面量类型是 `str`，`c"..."` 类型是 `*const u8`。
- LLVM 后端把 `str` 降低为 `{ ptr, usize }`，普通字符串字面量降低为全局字节数据加长度。
- 测试已经锁住 `sizeof[str] == 16`、`alignof[str] == 8` 这一 64-bit ABI 事实。
- 字符串字面量解码已经集中到共享实现，普通字符串会做 UTF-8 / Unicode scalar / escape 诊断，C 字符串会拒绝内部 NUL。
- 编译器内建已经有 `strptr`、`strblen`、`strvalid`、`strfromutf8`、`strraw`；`strraw` 已被正式 `unsafe` 体系约束，checked UTF-8 构造不依赖旧 std。
- 当前仓库根目录没有 `std/` 或 `selfhost/`。不存在当前有效的 `std.core.string.String`、`std.core.bytes.Bytes`、`std.fs.path.Path`、`std.ffi.c.string.CString` 实现。

因此 M2 的判断是：`str` 的 ABI 方向已经接近正确，checked-vs-unchecked 构造边界和 checked byte-offset slicing 已在语言核心收口；真正后续要补的是未来库类型边界、Unicode text API 和拥有型资源语义。`str` 应负责有效 UTF-8 借用文本，`String` 负责拥有文本，`Bytes` / `Span[u8]` 负责原始字节，`CStr` / `CString` 负责 C FFI，`Path` 负责平台路径。旧 M1 的标准库 API 名字不能继续当成当前事实，只能作为未来库层重建设计素材。

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

Go 的 `string` 是不可变 byte sequence，源码 UTF-8，`range` 可按 rune 解码；但 string 值可以包含任意 bytes，索引返回 byte。这很适合系统编程和网络协议，但它把“文本”和“bytes”揉在一起，导致无效 UTF-8 能在普通字符串里长期流动。Aurex 不应采用 Go 的任意 bytes string；原始 bytes 应使用 `Bytes` / `Span[u8]`。

### Java

Java `String` 不可变、可共享，API 历史上以 UTF-16 code unit 为索引单位；非 BMP 字符需要 surrogate pair，占两个 `char` 位置。Java 9 的 Compact Strings 改善了内部内存占用，但公开 API 的 UTF-16 code unit 语义无法撤回。Aurex 应避免 UTF-16 作为核心表示，避免 surrogate pair 成为永久语言包袱。

### JavaScript / ECMAScript

ECMAScript String 是 16-bit code unit 序列，规范允许 ill-formed UTF-16 子序列；`length` 和许多 API 都按 code unit 工作，normalization 也必须显式调用。这是 Web 兼容性下的合理历史设计，但对新系统语言来说是需要规避的反例：不要让普通字符串能表示半个 surrogate，也不要让索引单位和用户认知长期错位。

### Python 3

Python 3 的 `str` 是 Unicode 文本，CPython 通过 PEP 393 使用 1/2/4-byte flexible representation，兼顾 ASCII 内存效率和全 Unicode code point 访问。这对动态语言很好，但对象头、GC、可变内部表示不适合作为 Aurex 的固定 ABI primitive。Aurex 可以学习“内部表示不暴露”和“UTF-8 作为 C 交互推荐表示”的经验，但基础 `str` 需要固定两字布局。

### Zig

Zig 倾向用 `[]const u8` 表达字符串字面量和文本，语言保持极度显式。这符合系统语言透明性，但缺少“这是有效文本”的类型级不变量。Aurex 如果只用 `Span[u8]`，后续所有 parser、diagnostic、path、source manager 都会继续手写 UTF-8 检查。Aurex 需要 `str` 来把有效文本从原始 bytes 中分离出来。

### C# / Kotlin / JVM 家族

这些语言通常继承 UTF-16 字符串模型，API 成熟、生态强，但同样有 code unit 和 code point / grapheme 不一致的问题。它们的经验更适合说明“历史 ABI 一旦公开就很难修”，不适合作为 Aurex 新基础类型的方向。

## 学术与前沿方向

### UTF-8 everywhere 与 validate-once

现代系统软件、Web、源码、JSON/TOML/YAML、日志、CLI、路径显示层都大量使用 UTF-8。Swift 5 切到 UTF-8 首选存储，Rust 从一开始把 `str` 锁成有效 UTF-8，都说明“创建时验证一次，之后依赖不变量”是工程上最稳的路线。Aurex 应把 UTF-8 validity 作为 `str` 的核心不变量。

### Grapheme segmentation 不适合做 primitive 默认语义

Unicode UAX #29 定义 extended grapheme cluster，用于用户感知字符、光标移动、删除一个可见字符等操作。它非常重要，但规则复杂、版本化、locale 可裁剪，不适合塞进基础类型的 O(1) 语义里。Aurex 应提供 `unicode::graphemes(str)`，而不是让 `s[i]` 或默认 `len()` 悄悄变成 grapheme 语义。

### Normalization 必须显式

Unicode UAX #15 定义 NFC/NFD/NFKC/NFKD。Swift 默认相等会考虑 canonical representation；JavaScript 则把 normalization 放到显式 API。对 Aurex 这类系统语言，默认相等如果隐式 normalization，会让 `HashMap[str, V]`、build graph key、symbol table、文件名比较都承担隐藏 O(n) 成本和 Unicode 版本依赖。因此核心相等应是字节相等；需要 canonical equality 时调用显式 API。

### Rope / piece table 是编辑器文本结构，不是基础 `str`

Rope 论文指出传统连续字符串不适合大文本编辑、频繁拼接和共享切片。这个方向适合未来库层提供 `Rope`、`TextBuffer`、`StringBuilder` 或 source file buffer；不适合替代 `str`。基础 `str` 应保持简单稳定，复杂文本结构通过库类型组合。

### Interning / atoms 是符号优化，不是字符串语义

编译器 identifier、module name、target name 可以用 interner 或 atom 表优化比较和存储，但 atom 不是文本本身。Aurex 应允许 `Symbol` / `Atom` 从 `str` 构造，不能让所有 `str` 默认 intern，否则会把 allocator、全局表、生命周期和线程安全问题塞进 primitive。

## Aurex `str` 规范草案

### 类型与布局

`str` 是内建普通值类型，运行时语义等价于：

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

- 在 64-bit target 上 `sizeof[str] == 16`、`alignof[str] == 8`。
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
- 支持 `\u{...}`，表示 Unicode scalar value，排除 surrogate range 和超过 `U+10FFFF` 的值。
- 普通 `str` 字面量不应支持任意 raw byte escape；如果未来支持 `\xNN`，只允许产生 ASCII byte，或明确改成 bytes literal。
- 无效 escape 必须诊断，不能静默吞掉。

raw string：

```aurex
let raw: str = r"C:\tmp\a";
```

规则：

- 使用 `r"..."`。
- escape 不解释，内容仍必须是有效 UTF-8。
- 允许跨行。
- 不支持 Rust 风格 `r#"..."#` 分隔符。

byte string：

```aurex
let bytes: [3]u8 = b"abc";
```

规则：

- 类型是固定长度 `[N]u8`，`N` 是解码后的 byte 数。
- 只接受 ASCII raw byte 和 `\0`、`\n`、`\r`、`\t`、`\\`、`\"` 等简单 byte escape。
- 不接受 `\u{...}` 或非 ASCII raw byte。

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

- 当前 M2 源码语法是 `s[l:r]` / `s[:r]` / `s[l:]` / `s[:]`：`l` 和 `r` 是 UTF-8 byte offset，返回 `str`，不分配。运行时检查 `l <= r`、`r <= strblen(s)`，并要求两个边界都落在 UTF-8 code point boundary 上；失败返回空 `str`。这不是 Unicode scalar 迭代、grapheme cluster 索引或 locale-aware text segmentation。

- `byte_len(s: str) -> usize`：O(1)，返回 UTF-8 byte 数。
- `is_empty(s: str) -> bool`：O(1)。
- `as_bytes(s: str) -> Span[u8]`：O(1)，只读 byte view。
- `is_boundary(s: str, index: usize) -> bool`：检查 UTF-8 scalar 边界。
- `slice_bytes_checked(s: str, begin: usize, end: usize) -> Result[str, SliceError]`：未来库层可在 `s[l:r]` 的基础上提供更细错误信息；begin/end 必须在边界上。
- `is_scalar_value(value: u32) -> bool`：检查 `u32` 是否是合法 Unicode scalar value，排除 surrogate 和超过 `U+10FFFF` 的值。
- `scalar_utf8_width(value: u32) -> Result[usize, ScalarError]`：返回合法 scalar 的 UTF-8 byte 宽度。
- `scalar_at(s: str, byte_index: usize) -> Result[u32, ScalarError]`：byte_index 必须在 scalar 边界上，返回 Unicode scalar value。
- `scalar_width_at(s: str, byte_index: usize) -> Result[usize, ScalarError]`：返回当前 scalar 的 UTF-8 byte 宽度。
- `next_boundary(s: str, byte_index: usize) -> Result[usize, ScalarError]` / `previous_boundary(s: str, byte_index: usize) -> Result[usize, ScalarError]`：显式边界移动。
- `scalar_count(s: str) -> usize`：O(n)，名字明确。
- 后续再提供 `scalars(s: str) -> ScalarIter`：按 Unicode scalar value 迭代，O(n)。
- `graphemes(s: str)` 和 `grapheme_count(s: str)`：放在未来显式 Unicode 模块，O(n)，依赖 Unicode 数据版本。

当前 `char`：

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

## 未来库层类型边界

### `String`

未来 `String` 应重新定义为拥有型 UTF-8 buffer：

- 内部可继续使用 `Vec[u8]`。
- 它是拥有型资源，不能隐式浅拷贝；M2 当前不设计拥有型字符串资源语义，后续需要单独的资源模型承接。
- 必须保持 UTF-8 validity。
- 可以维护尾随 NUL 作为优化，但这不是公开语义。
- `as_str(self: *const String) -> str`。
- `as_bytes(self: *const String) -> Span[u8]`。
- `from_str(text: str) -> Result[String, AllocError]`。
- `from_utf8(bytes: Vec[u8]) -> Result[String, Utf8Error]`，成功后接管 bytes。
- `try_from_bytes(bytes: Span[u8]) -> Result[String, Utf8Error | AllocError]`。
- `push_scalar(self, value: u32)`、`insert_scalar(self, byte_index: usize, value: u32)` 或 `append(self, text: str)`，而不是用 `push(u8)` 拼文本。
- 字节级可变视图不能出现在 safe `String` surface；raw mutation 应使用 `Bytes.as_mut_span()` 一类 raw bytes API。

如果以后必须保留旧兼容 API，`String.push(u8)`、`insert(index, u8)`、`remove(index)`、`truncate(byte_len)` 只能作为受控兼容层存在，新 API 不应继续扩展它们。

### `Bytes`

未来应明确使用：

- `Bytes` / `Vec[u8]`：拥有原始 bytes，未来应通过资源 capability 标记为不可隐式浅拷贝的 owner。
- `Span[u8]`：借用原始 bytes。
- `Bytes` 不承诺 UTF-8。
- `Bytes.as_mut_span()` 是 raw byte mutation 的 safe surface；它不会影响 `String` / `str` 的 UTF-8 不变量。
- `Bytes.append(span)` 必须允许 span 指向自身 buffer，扩容后仍复制正确内容。
- 文本解析入口使用 `String.from_utf8` 或 `str.from_utf8` 验证后进入 `str` 世界。

### `CStr` / `CString`

FFI 层应有单独类型：

- `CStr`：借用 NUL-terminated C string，不拥有。
- `CString`：拥有 NUL-terminated C string，不允许内部 NUL，未来应按资源 capability 管理复制和释放。
- `CStr.as_str_utf8() -> Result[str, Utf8Error]`。
- `CString.from_str(str) -> Result[CString, InteriorNulError | AllocError]`。
- `CString.as_c() -> *const u8`。

普通 `str` 不提供 `.c_str()`。需要 C 指针时必须显式构造 `CString` 或进入 `with_c_str` 作用域，避免生命周期和内部 NUL 坑。

### `Path`

`Path` 不应等同于 `String`：

- POSIX path 是 bytes，通常但不保证 UTF-8。
- Windows path 需要宽字符/UTF-16 系统 API。
- Aurex 的 `Path` 应是独立类型，提供 `from_str`、`from_span` / future `from_bytes`、`display`、`as_platform` 等显式入口。
- POSIX 侧可由 `Bytes` 支撑，`from_span` 不验证 UTF-8 但拒绝内部 NUL，`from_str` 只是便利构造。
- Windows 侧不应强行复用 POSIX byte path 模型，必须给宽字符 / UTF-16 系统 API 留出边界。

## 实施路线

### Phase 0：锁定 M2 现状

- 已完成：保留现有 `str` ABI：两字 `{ptr, len}`。
- 已完成：保留 `sizeof[str]`、`alignof[str]`、普通字面量类型和 `c"..."` 类型测试。
- 写入本设计文档，后续代码改动以本文为准。

### Phase 1：编译器字面量与诊断

状态：M2 当前基线。

- 已完成：普通字符串字面量解码后验证 UTF-8。
- 已完成：invalid escape 改为诊断，不再静默接受。
- 已完成：新增 `\u{...}` Unicode scalar escape。
- 已完成：普通 `str` 字面量不产生 trailing NUL。
- 已完成：`c"..."` 生成 trailing NUL，并加入内部 NUL / 无效 escape 诊断策略。
- 已完成：新增 raw/multiline raw string，类型为 `str`，escape 不解释。
- 已完成：新增 byte string，类型为 `[N]u8`，拒绝 Unicode escape 和非 ASCII raw byte。
- 已完成：新增 Unicode scalar `char` 字面量，类型为 `char`，LLVM 表示为 `i32`。
- 已完成：增加 negative tests：invalid UTF-8、invalid scalar、surrogate、bad escape、invalid byte string、invalid char literal。

落地文件：

- `include/aurex/base/string_literal.hpp` / `src/base/string_literal.cpp`：共享字符串字面量解码、UTF-8 validation、Unicode scalar validation。
- `src/lex/lexer.cpp`：lexer 在产出字符串 token 前执行 escape/UTF-8/C string NUL 诊断。
- `src/backend/llvm/llvm_backend_util.cpp`：LLVM 后端复用共享解码逻辑，避免 lexer/backend escape 语义分裂。
- `tests/gtest/frontend/lexer_tests.cpp`、`tests/gtest/backend/llvm_utility_tests.cpp`、`tests/gtest/sema/sema_whitebox_tests.cpp`、`tests/gtest/ir/lower_ast_whitebox_tests.cpp`、`tests/gtest/backend/llvm_constants_tests.cpp`、`tests/samples/positive/types/string_unicode_escape.ax`、`tests/samples/positive/types/literal_system.ax` 和 negative literal samples 覆盖 Phase 1 行为。

### Phase 2：补上 `unsafe` 边界

状态：M2 已完成最小边界。

`strraw(data, len)` 能绕过 UTF-8 不变量，是典型 unsafe 操作。当前 M2 已引入最小 `unsafe` 语法，并把它保留为 unsafe-only 内建：

```aurex
unsafe {
    let text = strraw(data, len);
}
```

已落地规则：

- safe context 下调用 unchecked 构造会诊断。
- `unsafe fn` 和 unsafe 函数指针调用必须发生在 unsafe context。
- `strptr` / `strblen` 可以继续是 safe 只读观察操作；`strvalid` / `strfromutf8` 是 safe checked 构造边界。
- 文档要明确：任何构造 `str` 的入口都必须证明 UTF-8 有效，或者被标记为 unsafe。
- 当前 unsafe 不包含 borrow checker、lifetime、unsafe trait/impl/extern block 或资源模型。

### Phase 3：定义最小 text API

状态：M2 已冻结 no-std checked 构造入口和 checked byte-offset slicing，库层 text API 仍后置。

旧 std 已经从当前树删除，不应急着重建完整 `std.core.str`。M2 当前先提供两个 compiler builtin 来锁住 safe/unsafe 边界：

- `strvalid(bytes: []const u8 | []mut u8) -> bool`：只验证 byte slice 是否为有效 UTF-8。
- `strfromutf8(bytes: []const u8 | []mut u8) -> str`：成功返回借用原 byte slice 的文本；失败返回空 `str`。需要区分“合法空输入”和“非法输入”时调用 `strvalid(bytes)`。失败路径不会把无效输入包装成 `str`，也不 trap。
- `s[l:r] -> str`：按 byte offset 切片，检查 bounds 和 UTF-8 code point boundary；失败返回空 `str`，不会 trap，也不会构造 invalid UTF-8。

未来库层的最小 text surface 可以在这个边界上继续设计：

- `byte_len`
- `is_empty`
- `as_bytes`
- `is_boundary`
- `slice_bytes_checked`
- `starts_with`
- `ends_with`
- `equals`
- `from_utf8`

验收重点不是 API 数量，而是边界：

- checked API 失败时不能把无效输入包装成 `str`。
- unchecked API 必须在 `unsafe` 内。
- API 不应依赖 M1 标准库名字特判。
- 正例和负例应放在 language-core 样例中，直到库层重新设计完成。

### Phase 4：重新设计拥有型文本和 raw bytes 类型

状态：M2 后置。

`String`、`Bytes`、`CStr`、`CString`、`Path` 应等基础语法、`unsafe`、slice/string 边界和后续资源语义收口后再重新设计。原因是这些类型会立刻触发：

- owner 如何转移、借用和释放。
- 自动释放如何表达。
- borrowed `str` 能否安全指向 owned `String`。
- `Bytes.as_mut_span()` 和 `String` UTF-8 不变量如何隔离。
- `Path` 是否按平台分层，而不是把所有路径伪装成 UTF-8 文本。

这部分不能再沿 M1 的“先把库 API 做出来，再反推语言规则”的路线推进。

### Phase 5：Unicode 扩展

状态：M2 之后。

- UTF-8 scalar iterator 可以作为 core text 扩展。
- UAX #29 grapheme iterator 和 UAX #15 normalization 应放在显式 Unicode 模块。
- Unicode 数据版本必须在模块中显式记录，不影响 core `str` ABI。

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

M2 当前 language-core 至少要覆盖：

- `sizeof[str]` 和 `alignof[str]` 在 64-bit 下保持 16/8。
- `"abc"` 类型是 `str`，`c"abc"` 类型是 `*const u8` 或未来 `CStr`，二者不能隐式互转。
- 普通字符串字面量允许内部 `\0`，C 字符串字面量拒绝内部 NUL。
- 非 ASCII 文本如 `"ƒ"` 按 UTF-8 byte length 表达，不能被截断成单字节字符。
- invalid UTF-8 字面量、invalid escape、surrogate escape 都给出稳定诊断。
- `strptr`、`strblen`、`strvalid`、`strfromutf8`、`strraw` 的类型检查稳定。
- `strvalid` 对合法/非法 UTF-8 byte slice 返回稳定 bool；`strfromutf8` 成功返回文本，失败返回空 `str`。
- checked slicing 对 `"ƒ"` 这类多字节文本遵守 UTF-8 边界：`text[0:1]` 失败并返回空 `str`，`text[0:2]` 成功。
- `strraw` 不能继续暴露在 safe context。

未来库层重建后再补这些验收项：

- `slice_bytes_checked("ƒ", 0, 1)` 返回带错误信息的失败结果，`slice_bytes_checked("ƒ", 0, 2)` 成功。
- `String.from_utf8` 接受合法 UTF-8，拒绝非法 bytes。
- `CString.from_str("a\0b")` 拒绝内部 NUL。
- `String` safe surface 不暴露 raw mutable byte view。
- `Bytes` 覆盖 raw mutable bytes 和自别名 append。
- `Path.from_span` 接受非 UTF-8 bytes 并拒绝内部 NUL。
- 文件、目录、进程和 console API 优先接收 `Path`、`str`、`CStr` / `CString` 等边界类型，不再要求业务代码直接传 `c"..."`。
- 拥有型库值的资源约束必须通过后续资源语义表达，而不是通过标准库名字特判。

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

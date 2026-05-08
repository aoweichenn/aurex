# 标准库 API 与命名空间设计

本文记录 M1 阶段标准库公开 API 的命名、模块限定和兼容迁移规则。目标不是复制某一种语言，而是采用已经在工业界验证过的组合：Rust 的短类型词汇和 `snake_case` 习惯、Zig 的显式命名空间纪律、C++ 的容器分类、Swift API Design Guidelines 对调用点可读性的要求。

## 设计目标

- 调用点短：模块名承担上下文，不在函数名里重复类型前缀。
- 类型稳定：容器和视图使用可长期演进的短类型名，如 `Vec<T>`、`Span<T>`、`MutSpan<T>`、`Bytes`、`String`、`Path`。
- 语义清晰：`new`、`with_capacity`、`from_*` 表示构造；`as_*` 表示借用视图；未来 `into_*` 表示消耗式转换。
- 兼容可控：旧的 `vec_u8_push`、`string_from_c`、`path_join_c` 等长名保留为包装层，但新文档和新样例优先使用短 API。
- 错误模型可演进：M1 允许底层内存 API 返回 `bool`，面向用户的构造和转换优先返回 `Result<T, E>`；后续应逐步把可恢复错误迁到 `Result`。

## 命名规则

类型使用 `UpperCamelCase`：

- `Vec<T>`：可增长连续存储容器。
- `Span<T>`：只读连续视图。
- `MutSpan<T>`：可写连续视图。
- `Bytes`：拥有的原始 bytes；不承诺 UTF-8，可暴露 `MutSpan<u8>`。
- `String`：拥有的 UTF-8 字符串；不暴露 raw mutable byte view。
- `Path`：拥有的文件路径值；语义是平台路径 bytes，不等同于普通 `String`。
- `Option<T>`、`Result<T, E>`：基础代数数据类型。

函数和方法使用 `snake_case`：

- 构造：`new`、`with_capacity`、`from_str`、`from_utf8`、`from_c_utf8`、`from_c`、`from_span`。
- 容量和长度：`len`、`byte_len`、`capacity`、`is_empty`、`reserve`、`clear`。
- 变更：`push_scalar`、`insert_scalar`、`pop_scalar`、`remove_scalar_at`、`append`、`truncate_bytes_checked`、`push`、`extend`、`insert`、`remove`、`swap_remove`、`pop`、`truncate`、`append_span`、`append_c`。
- 随机访问：`get`、`set`、`first`、`last`。
- 视图：`as_str`、`as_str_checked`、`as_bytes`、`as_span`、`as_mut_span`、`as_c`、`c_str`。其中 `as_mut_span` 只用于 `Vec<T>` / `Bytes` 这类原始可变存储，不属于 `String`。
- 查询：`bytes_equal`、`starts_with`、`file_name`。

避免把类型名重复编码进函数名。新代码应写 `vec::push(&items, value)`，而不是继续扩展 `vec_u8_push` 这类前缀函数。

## 命名空间语法

M1 引入显式 import 别名和模块限定：

```aurex
import std.core.text as text;
import std.core.vec as vec;

fn main() -> i32 {
    var bytes: vec::Vec<u8> = vec::new<u8>();
    if !vec::push<u8>(&bytes, b'a') {
        return 1;
    }
    let view: text::Span<u8> = vec::as_span<u8>(&bytes);
    return cast(i32, view.len);
}
```

语法规则：

- `import a.b.c as c;` 绑定当前模块的直接导入别名。
- `alias::name` 查找该直接导入模块中的公开类型、函数、泛型函数或全局常量。
- `::` 用于模块/命名空间限定；`.` 继续用于值字段和方法调用。
- 未限定 import 仍按旧规则工作，用于兼容旧样例和旧代码。
- 别名只绑定直接 import，不自动绑定 public re-export；这能避免 re-export 链造成难以解释的名称来源。

成员访问规则：

- `value.field` 访问结构体公开字段；跨模块访问会遵守 `pub` / `priv`。
- `value.method(args)` 调用 `impl Type` 中带显式 `self` 参数的公开实例方法；跨模块 private method 会给出专门诊断，而不是退化成未知方法。
- `impl<T> Type<T>` 可为泛型类型声明实例方法，接收者类型会驱动类型实参推断，例如 `items.push(value)` 会实例化 `Vec<T>.push`。
- 方法也可以声明自己的泛型参数，例如 `value.ok_or<E>(err)` 或 `box.pair_with(value)`；impl 目标先推导 `T`，方法实参或显式 `<E>` 再推导方法级参数。
- `Type.function(args)` 保留给当前已落地的关联函数调用模型。

后续关联项语法会继续沿用这条边界：`module::item` 是命名空间限定，`value.method()` 是值方法调用；是否增加 `Type::associated_item` 会在 enum 构造器和 impl 关联函数统一设计后再落地。

## M1 标准库形态

新的推荐调用方式如下：

```aurex
import std.core.result as result;
import std.core.str as strings;
import std.core.string as string;
import std.core.bytes as bytes;
import std.core.text as text;
import std.core.vec as vec;
import std.ffi.c.string as cstring;
import std.fs.dir as directory;
import std.fs.file as file;
import std.fs.path as path;
```

`std.core.vec`：

- 类型：`vec::Vec<T>`、兼容别名 `vec::VecU8`。
- 新 API：`vec::new<T>`、`vec::with_capacity<T>`、`vec::destroy<T>`、`vec::len<T>`、`vec::capacity<T>`、`vec::is_empty<T>`、`vec::reserve<T>`、`vec::push<T>`、`vec::insert<T>`、`vec::extend<T>`、`vec::pop<T>`、`vec::remove<T>`、`vec::swap_remove<T>`、`vec::get<T>`、`vec::set<T>`、`vec::first<T>`、`vec::last<T>`、`vec::truncate<T>`、`vec::clear<T>`、`vec::as_span<T>`、`vec::as_mut_span<T>`、`vec::from_span<T>`。
- Method API：`Vec<T>.destroy`、`len`、`capacity`、`is_empty`、`as_span`、`as_mut_span`、`reserve`、`push`、`insert`、`extend`、`pop`、`remove`、`swap_remove`、`get`、`set`、`first`、`last`、`clear`、`truncate`。
- 兼容 API：`vec::vec_u8_new`、`vec::vec_u8_push` 等保留，但不作为新文档的主路径。

`std.core.bytes`：

- 类型：`bytes::Bytes`，拥有型 raw bytes buffer。
- 新 API：`bytes::new`、`bytes::with_capacity`、`bytes::from_span`、`bytes::destroy`、`bytes::len`、`bytes::is_empty`、`bytes::capacity`、`bytes::reserve`、`bytes::push`、`bytes::append`、`bytes::pop`、`bytes::remove`、`bytes::truncate`、`bytes::clear`、`bytes::as_span`、`bytes::as_mut_span`、`bytes::equals_span`。
- Method API：`Bytes.new`、`with_capacity`、`from_span`、`destroy`、`len`、`is_empty`、`capacity`、`reserve`、`push`、`append`、`pop`、`remove`、`truncate`、`clear`、`as_span`、`as_mut_span`、`equals_span`。
- 约束：`Bytes` 不验证 UTF-8，允许任意 byte，包括 `0x00`；`as_mut_span` 是 raw byte mutation 的公开位置。`Bytes.append` 支持 self-alias，`raw.append(raw.as_span())` 触发扩容时仍保持正确复制。

`std.core.string`：

- 类型：`string::String`。
- 新 API：`string::new`、`string::from_str`、`string::from_utf8`、`string::from_c_utf8`、`string::destroy`、`string::len`、`string::byte_len`、`string::is_empty`、`string::reserve`、`string::append`、`string::push_scalar`、`string::insert_scalar`、`string::pop_scalar`、`string::remove_scalar_at`、`string::as_str`、`string::as_str_checked`、`string::as_bytes`、`string::slice_bytes_checked`、`string::truncate_bytes_checked`、`string::is_valid_utf8`、`string::equals`、`string::starts_with`、`string::ends_with`。
- 兼容/过渡 API：`string::from_c`、`string::push`、`string::insert`、`string::append_span`、`string::append_c`、`string::pop`、`string::remove`、`string::truncate`、`string::clear`、`string::as_span`、`string::c_str`、`string::equals_span`、`string::ends_with_byte`。这些 API 当前会尽量维护 UTF-8 不变量；raw mutable byte view 已迁到 `Bytes.as_mut_span`。
- 兼容 API：`string::string_new`、`string::string_from_c` 等保留。

`std.core.str`：

- 类型：内建 borrowed UTF-8 文本切片 `str`。
- API：`strings::byte_len`、`strings::is_empty`、`strings::as_bytes`、`strings::equals`、`strings::starts_with`、`strings::ends_with`、`strings::is_boundary`、`strings::slice_bytes_checked`、`strings::is_scalar_value`、`strings::scalar_utf8_width`、`strings::scalar_at`、`strings::scalar_width_at`、`strings::next_boundary`、`strings::previous_boundary`、`strings::scalar_count`、`strings::is_valid_utf8`、`strings::from_utf8`。
- 约束：`strings::as_bytes` 只返回只读 `SpanU8`；构造 `str` 必须通过 UTF-8 validation 或已验证边界的切片。低层 `str_from_bytes_unchecked` 是编译器/标准库支撑点，不是普通业务 API。

`std.ffi.c.string`：

- 类型：`cstring::CStr`、`cstring::CString`。
- `CStr` API：`cstring::CStr.from_c`、`byte_len`、`as_c`、`as_bytes`、`as_str_utf8`。
- `CString` API：`cstring::CString.from_str`、`from_utf8`、`from_c_utf8`、`destroy`、`byte_len`、`as_c`、`as_cstr`、`as_str`、`as_str_checked`。
- 约束：`CStr` 是 borrowed copyable 视图；`CString` 是 `noncopy` 拥有型 C 字符串，必须显式 `destroy()`，不能隐式拷贝。`CString.from_str` 拒绝内部 NUL；`CString.from_utf8` 同时验证 UTF-8 和内部 NUL。普通 `str` / `String` 不隐式转成 C string，需要 FFI 指针时显式构造 `CString` 或借用 `CStr`。

`std.fs.path`：

- 类型：`path::Path`。
- 新 API：`path::from_c`、`path::from_span`、`path::from_str`、`path::destroy`、`path::as_c`、`path::as_span`、`path::is_absolute`、`path::parent`、`path::file_name`、`path::file_stem`、`path::extension`、`path::join_c`、`path::join_span`、`path::with_extension`。
- Method API：`Path.from_c`、`from_span`、`from_str`、`destroy`、`as_c`、`as_span`、`file_name`、`is_absolute`、`parent`、`extension`、`file_stem`、`join_c`、`join_span`、`with_extension`。
- 约束：`Path` 存储平台路径 bytes，不验证 UTF-8；`from_span` 和 `join_span` 拒绝内部 NUL，因为当前 POSIX/C FFI 兼容视图需要 NUL-terminated buffer；`from_str` 只是从 UTF-8 文本构造路径的便利入口。
- 兼容 API：`path::path_from_c`、`path::path_join_c` 等保留。

`std.fs.file`：

- 类型：`file::FileBytes`、`file::FileMetadata`。
- C 兼容 API：`file::metadata`、`file::read_bytes`、`file::read_text`、`file::write_bytes`、`file::write_text`、`file::file_exists`、`file::remove_file`、`file::rename_file` 继续接收 `*const u8` path，作为底层兼容入口保留。
- `Path` API：`file::metadata_path`、`file::read_bytes_path`、`file::read_text_path`、`file::write_bytes_path`、`file::write_text_path`、`file::file_exists_path`、`file::remove_file_path`、`file::rename_file_path` 接收 `*const path::Path`，空指针返回 `Result.err(1)` 或 `false`。
- 文本 API：`file::write_str(path, text: str)` 和 `file::write_str_path(path, text: str)` 按 `str` 的 byte length 写入，允许内部 `\0`，不经过 `c_strlen`。
- 约束：`Path.from_span` 已拒绝内部 NUL，因此 `*_path` 包装可以安全通过 `Path.as_c()` 调用当前 POSIX/C FFI；新业务代码优先使用 `Path` 和 `str` 入口，只有 FFI 或旧兼容代码继续直接传 `c"..."`。

`std.fs.dir`：

- 类型：`directory::DirectoryEntry`、`directory::DirectoryEntryKind`。
- C 兼容 API：`directory::create_directory`、`directory::read_entries`、`directory::read_entries_recursive`、`directory::count_files_with_suffix`、`directory::has_file_with_suffix`、`directory::count_files_with_suffix_recursive`、`directory::has_file_with_suffix_recursive` 继续接收 `*const u8` path / suffix，作为底层兼容入口保留。
- `Path` API：`directory::create_directory_path`、`directory::read_entries_path`、`directory::read_entries_recursive_path`、`directory::count_files_with_suffix_path`、`directory::has_file_with_suffix_path`、`directory::count_files_with_suffix_recursive_path`、`directory::has_file_with_suffix_recursive_path` 接收 `*const path::Path`，空 path 指针返回 `false` 或 `Result.err(1)`。
- suffix 文本 API：`directory::count_files_with_suffix_str`、`directory::count_files_with_suffix_path_str`、`directory::has_file_with_suffix_str`、`directory::has_file_with_suffix_path_str` 及递归版本接收 `str` suffix，内部通过 `CString.from_str` 构造 FFI 参数并拒绝内部 NUL。
- Directory entry API：`DirectoryEntry` 内部使用 bytes-backed `Path` 保存 host 返回的 name/path；`name_bytes()` / `path_bytes()` 返回原始路径 bytes，`name_utf8()` / `path_utf8()` 返回 `Result<str, i32>` checked 文本视图；`name_c_data()` / `path_c_data()` 保留为 C 兼容入口。null entry 指针不会解引用：C 兼容入口返回空 C string，bytes 入口返回空 span，checked UTF-8 入口返回 `Result.err(1)`。
- 约束：目录项默认不暴露 unchecked `str`，因为 POSIX path bytes 不保证 UTF-8；业务代码只有在确知路径是文本时才调用 `name_utf8()` / `path_utf8()`，否则应使用 bytes 视图或 `Path`/C 兼容边界。

`std.core.text`：

- 类型：`text::Span<T>`、`text::MutSpan<T>`、兼容别名 `text::SpanU8`、`text::MutSpanU8`。
- API：`text::span<T>`、`text::mut_span<T>`、`text::c_span`、`text::bytes_equal`、`text::bytes_starts_with`、`text::bytes_find_byte`、`text::bytes_trim_ascii_space`、ASCII 分类和大小写转换函数。

`std.core.result`：

- 类型：`result::Option<T>`、`result::Result<T, E>`。
- Method API：`Option<T>.is_some`、`is_none`、`is_some_ref`、`is_none_ref`、`unwrap_or`、`ok_or<E>`，以及 `Result<T, E>.is_ok`、`is_err`、`is_ok_ref`、`is_err_ref`、`unwrap_or`。
- `*_ref` 状态检查只读 enum tag，不消费 payload；这是 noncopy payload 进入 `Result` / `Option` 后的低风险状态检查入口。
- `Option<T>.ok_or<E>` 是当前标准库里第一个公开使用方法级泛型参数的 API，用来验证 `impl<T>` 参数和方法自身参数可以在同一次调用里组合推导。

## 迁移策略

1. 新样例和文档使用 `import ... as ...` 与 `alias::item`。
2. 旧长名函数保留为包装层，保证已有 M1 样例继续编译。
3. 新功能优先加到短 API；旧长名只在必要时补兼容包装。
4. 当覆盖率和迁移样例稳定后，再考虑给旧长名加弃用诊断。
5. 标准库模块内部可以逐步切到别名导入，但不在同一个变更里做大规模机械迁移。

## 参考来源

- Rust API Guidelines, Naming: https://rust-lang.github.io/api-guidelines/naming.html
- Rust `std::vec` and slices: https://doc.rust-lang.org/std/vec/index.html, https://doc.rust-lang.org/std/primitive.slice.html
- Rust collections overview: https://doc.rust-lang.org/std/collections/index.html
- Zig language reference: https://ziglang.org/documentation/master/
- C++ containers draft: https://eel.is/c++draft/containers.general
- Swift API Design Guidelines: https://www.swift.org/documentation/api-design-guidelines/

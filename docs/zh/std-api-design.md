# 标准库 API 与命名空间设计

本文记录 M1 阶段标准库公开 API 的命名、模块限定和兼容迁移规则。目标不是复制某一种语言，而是采用已经在工业界验证过的组合：Rust 的短类型词汇和 `snake_case` 习惯、Zig 的显式命名空间纪律、C++ 的容器分类、Swift API Design Guidelines 对调用点可读性的要求。

## 设计目标

- 调用点短：模块名承担上下文，不在函数名里重复类型前缀。
- 类型稳定：容器和视图使用可长期演进的短类型名，如 `Vec<T>`、`Span<T>`、`MutSpan<T>`、`String`、`Path`。
- 语义清晰：`new`、`with_capacity`、`from_*` 表示构造；`as_*` 表示借用视图；未来 `into_*` 表示消耗式转换。
- 兼容可控：旧的 `vec_u8_push`、`string_from_c`、`path_join_c` 等长名保留为包装层，但新文档和新样例优先使用短 API。
- 错误模型可演进：M1 允许底层内存 API 返回 `bool`，面向用户的构造和转换优先返回 `Result<T, E>`；后续应逐步把可恢复错误迁到 `Result`。

## 命名规则

类型使用 `UpperCamelCase`：

- `Vec<T>`：可增长连续存储容器。
- `Span<T>`：只读连续视图。
- `MutSpan<T>`：可写连续视图。
- `String`：拥有的 UTF-8 字符串；原始 bytes 后续应收敛到 `Vec<u8>` / `Bytes`。
- `Path`：拥有的文件路径值。
- `Option<T>`、`Result<T, E>`：基础代数数据类型。

函数和方法使用 `snake_case`：

- 构造：`new`、`with_capacity`、`from_str`、`from_utf8`、`from_c_utf8`、`from_c`、`from_span`。
- 容量和长度：`len`、`byte_len`、`capacity`、`is_empty`、`reserve`、`clear`。
- 变更：`push`、`extend`、`insert`、`remove`、`swap_remove`、`pop`、`truncate`、`append`、`append_span`、`append_c`。
- 随机访问：`get`、`set`、`first`、`last`。
- 视图：`as_str`、`as_str_checked`、`as_bytes`、`as_span`、`as_mut_span`、`as_c`、`c_str`。
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
import std.core.text as text;
import std.core.vec as vec;
import std.fs.path as path;
```

`std.core.vec`：

- 类型：`vec::Vec<T>`、兼容别名 `vec::VecU8`。
- 新 API：`vec::new<T>`、`vec::with_capacity<T>`、`vec::destroy<T>`、`vec::len<T>`、`vec::capacity<T>`、`vec::is_empty<T>`、`vec::reserve<T>`、`vec::push<T>`、`vec::insert<T>`、`vec::extend<T>`、`vec::pop<T>`、`vec::remove<T>`、`vec::swap_remove<T>`、`vec::get<T>`、`vec::set<T>`、`vec::first<T>`、`vec::last<T>`、`vec::truncate<T>`、`vec::clear<T>`、`vec::as_span<T>`、`vec::as_mut_span<T>`、`vec::from_span<T>`。
- Method API：`Vec<T>.destroy`、`len`、`capacity`、`is_empty`、`as_span`、`as_mut_span`、`reserve`、`push`、`insert`、`extend`、`pop`、`remove`、`swap_remove`、`get`、`set`、`first`、`last`、`clear`、`truncate`。
- 兼容 API：`vec::vec_u8_new`、`vec::vec_u8_push` 等保留，但不作为新文档的主路径。

`std.core.string`：

- 类型：`string::String`。
- 新 API：`string::new`、`string::from_str`、`string::from_utf8`、`string::from_c_utf8`、`string::destroy`、`string::len`、`string::byte_len`、`string::is_empty`、`string::reserve`、`string::append`、`string::as_str`、`string::as_str_checked`、`string::as_bytes`、`string::is_valid_utf8`、`string::equals`、`string::starts_with`、`string::ends_with`。
- 兼容/过渡 API：`string::from_c`、`string::push`、`string::insert`、`string::append_span`、`string::append_c`、`string::pop`、`string::remove`、`string::truncate`、`string::clear`、`string::as_span`、`string::as_mut_span`、`string::c_str`、`string::equals_span`、`string::ends_with_byte`。这些 API 当前会尽量维护 UTF-8 不变量；`as_mut_span` 仍是后续迁到 `Bytes` 或 unsafe 边界的风险点。
- 兼容 API：`string::string_new`、`string::string_from_c` 等保留。

`std.core.str`：

- 类型：内建 borrowed UTF-8 文本切片 `str`。
- API：`strings::byte_len`、`strings::is_empty`、`strings::as_bytes`、`strings::equals`、`strings::starts_with`、`strings::ends_with`、`strings::is_boundary`、`strings::slice_bytes_checked`、`strings::is_valid_utf8`、`strings::from_utf8`。
- 约束：`strings::as_bytes` 只返回只读 `SpanU8`；构造 `str` 必须通过 UTF-8 validation 或已验证边界的切片。低层 `str_from_bytes_unchecked` 是编译器/标准库支撑点，不是普通业务 API。

`std.fs.path`：

- 类型：`path::Path`。
- 新 API：`path::from_c`、`path::from_span`、`path::destroy`、`path::as_c`、`path::as_span`、`path::is_absolute`、`path::parent`、`path::file_name`、`path::file_stem`、`path::extension`、`path::join_c`、`path::join_span`、`path::with_extension`。
- 兼容 API：`path::path_from_c`、`path::path_join_c` 等保留。

`std.core.text`：

- 类型：`text::Span<T>`、`text::MutSpan<T>`、兼容别名 `text::SpanU8`、`text::MutSpanU8`。
- API：`text::span<T>`、`text::mut_span<T>`、`text::c_span`、`text::bytes_equal`、`text::bytes_starts_with`、`text::bytes_find_byte`、`text::bytes_trim_ascii_space`、ASCII 分类和大小写转换函数。

`std.core.result`：

- 类型：`result::Option<T>`、`result::Result<T, E>`。
- Method API：`Option<T>.is_some`、`is_none`、`unwrap_or`、`ok_or<E>`，以及 `Result<T, E>.is_ok`、`is_err`、`unwrap_or`。
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

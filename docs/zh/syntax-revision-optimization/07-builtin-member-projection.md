# Builtin Member Projection：str / slice 的 `.len` 和 `.ptr`

日期：2026-06-13
状态：当前源码表面和实现已落地
关联实现：lexer / parser / AST dump / sema / IR dump / samples / gtest

## 结论

源码层不提供历史 helper 入口，当前唯一用户表面是成员投影。

新的源码写法统一为成员投影：

```aurex
bytes.len
bytes.ptr
text.len
text.ptr
```

其中：

- `slice.len` 返回 `usize`。
- `slice.ptr` 返回指向 slice 首元素的 pointer，mutability 跟 slice 类型一致。
- `str.len` 返回 UTF-8 byte length，类型是 `usize`。
- `str.ptr` 返回 `*const u8`。

这是 field-like projection，不是函数调用，所以不写 `()`。

## 为什么这样改

历史 helper 写法问题：

- 名字像内部 helper，不像用户语言表面。
- 会占用关键字空间。
- parser 为这些名字维护 builtin primary expression 分支，耦合过重。
- 这些操作本质是从 fat value 或 string view 里读取元信息，不是“全局函数调用”。

新写法优势：

- `x.len` 和 `x.ptr` 短、好打、可读。
- 和 tuple/struct field projection 共用表面，不引入空格规则。
- parser 只产出普通 field expression，sema 根据 object type 和 field name 决定是否 rewrite 成内建投影。
- 用户只看到 `.len` / `.ptr`。

## 语义

`x.len` / `x.ptr` 先按普通 field expression 解析。sema 分析 object type 后处理：

```text
str.len     -> internal str_byte_len
str.ptr     -> internal str_data
[]T.len     -> internal slice_len
[]T.ptr     -> internal slice_data
[]mut T.ptr -> internal slice_data with mutable pointer result
```

如果 object 不是 `str` 或 slice，则继续走普通 field projection 诊断。例如：

```aurex
let bad = 1.len; // 不是 builtin projection，会按普通字段访问报错
```

## 不做空格敏感

下面写法 token 语义一致：

```aurex
text.len
text . len
bytes[:].ptr
bytes[:] . ptr
```

parser 不根据空格判断 builtin。所有语义都来自 token 和 type。

## 实现落点

- lexer 删除旧 keyword。
- parser 删除旧 unary builtin 分支。
- AST dump 只保留普通 parse field；sema rewrite 后的内部节点 dump 为 `str.len` / `str.ptr` / `slice.len` / `slice.ptr`。
- sema 在 field analyzer 中识别 `len` / `ptr`，并把表达式原地 rewrite 成已有 internal ExprKind。
- lowering 继续复用已有 `str_data` / `str_byte_len` / `slice_data` / `slice_len` IR。
- IR dump 使用中性内部名 `str_data` / `str_byte_len`，不再输出旧源码 helper 名。
- 泛型样例覆盖 `fn slice_ops<T>(values: []T)` 和 `fn mut_slice_ops<T>(values: []mut T)` 中的 `.len` / `.ptr`。
- runtime sample 覆盖 slice window 的 `.ptr` 地址和 `.len` 长度。

## 迁移原则

语言还没发布第一个大版本，所以不保留旧语法入口：

- 旧名字不再是 keyword。
- 旧名字不再作为 parser builtin。
- samples / tests / docs 的当前规范示例不再使用旧名字。
- 不增加 legacy recovery 分支。

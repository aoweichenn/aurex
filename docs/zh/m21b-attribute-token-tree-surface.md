# Aurex M21b AttributeDecl / Token Tree Surface

日期：2026-06-12
阶段：M21b AttributeDecl / Token Tree Surface
状态：frontend syntax / parser / sema blocker / regression baseline

## 目标

M21b 把 M21a 选定的 `token_tree_and_attribute_surface` 从 design gate 落到 frontend AST 和 parser。这个阶段仍不是
完整宏系统：它只让编译器可以稳定解析、保存、dump 和诊断 item attribute token tree，为 M21c 的 early item
expansion query、generated module part 和 expansion source map 做输入面。

## 已实现

新增通用 item attribute surface：

- `syntax::AttributeDecl`
- `syntax::AttributeTokenDecl`
- `syntax::AttributeTokenTreeGroupKind`
- `ItemNode::attributes`
- 所有 item compact payload 的 `attributes` 保存 / load / copy / move / materialize 路径

parser 现在会接受通用 item attribute：

```aurex
#[builder(defaults(threads = 4), flag, nested[a + b])]
struct Config {
    threads: i32;
}
```

该 attribute 会保存为 `AttributeDecl{name="builder", token_tree=[...]}`。token tree 当前是 flat token stream，
每个 token 保存：

- token kind
- source text
- source range
- group kind：paren / bracket / brace / none
- group depth

这里刻意不用递归 tree object，是为了和现有 compact AST / arena vector 风格一致，并避免 M21b 提前绑定 M21c
expansion engine 的内部节点形状。M21c 可以直接用 flat token stream 构造 expansion query key、source map 和
debug trace。

## derive 兼容

M20e 的内建 derive 继续兼容：

```aurex
#[derive(Copy, Eq, Hash)]
struct Key {
    value: i32;
}
```

parser 会同时记录：

- 通用 `AttributeDecl{name="derive", token_tree=[...]}`
- 兼容 `DeriveDecl{name="Copy" | "Eq" | "Hash"}`

sema 仍只把 `DeriveDecl` 降成 checked capability facts；没有引入用户自定义 derive，也没有生成 impl/function/member。

## 语义边界

M21b 对非 `derive` item attribute 的处理是：

- parser 接受并保存 token tree。
- AST dump 展示 `#[attr ... tokens=N ...]`。
- sema 明确报错：`item attribute macros are parsed but macro expansion is not implemented yet: <name>`。

这样可以保证工具和后续 query 能索引新 surface，同时不会让用户误以为 `#[builder]`、`#[serde]` 或 external
procedural macro 已经执行。

## 非目标

M21b 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- hygiene mark / origin id / declared generated names。
- expansion source map / debug trace。
- generated module part。
- `--emit-expanded` 或 macro trace CLI。

## 测试入口

本阶段新增 / 更新：

- `ParserRecordsGeneralItemAttributeTokenTrees`
- `ParserRecoveryHandlesMalformedItemAttributes`
- `AstItemNodeListCopiesAndMovesEveryPayloadKind`
- `BuiltinDeriveCapabilityRegressions`

这些测试覆盖：

- 通用 attribute token-tree parse。
- 嵌套括号 token depth / group 保存。
- `derive` 兼容保存。
- AST compact payload copy / move / materialize。
- 非 `derive` attribute sema blocker。

## 后续

M21c 已继续把 M21b 的输入面接到 query-level early item expansion facts。后续真实展开仍需继续实现：

- Early Item Expansion Pipeline。
- Generated Module Part。
- Expansion Source Map。
- Expansion debug trace facts。
- query-backed macro expansion key / fingerprint。

M21c 仍不应打开 external procedural macro、typed expression macro 或标准库。

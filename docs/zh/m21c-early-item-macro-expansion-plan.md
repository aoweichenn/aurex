# Aurex M21c Early Item Macro Expansion Plan

日期：2026-06-12
阶段：M21c Early Item Macro Expansion Plan
状态：query facts / no-op early expansion plan / sema blocker source of truth

## 目标

M21c 把 M21b 的 `AttributeDecl` / `AttributeTokenDecl` 输入面接到 query-level early item macro expansion 地基。
本阶段仍不执行宏、不生成用户代码、不实现标准库，也不运行 external procedural macro。它固定的是下一步真实展开必须
遵守的 pipeline facts：

- attribute token-tree input。
- builtin derive passthrough。
- early item expansion query key / fingerprint。
- generated module part no-op plan。
- expansion source map / debug trace stub。
- unimplemented item attribute sema blocker。
- external procedural macro future blocker。

## 已实现

新增 query facts：

- `query::MacroExpansionFact`
- `query::MacroExpansionSummary`
- `query::MacroExpansionPlan`
- `query::MacroExpansionFactKind`
- `query::MacroExpansionStage`
- `query::MacroExpansionPolicy`
- `m21c_macro_expansion_plan_baseline()`
- `is_valid_m21c_macro_expansion_plan()`
- `macro_expansion_plan_fingerprint()`
- `summarize_macro_expansion_plan()`
- `dump_macro_expansion_plan()`

M21c baseline 名称固定为：

```text
M21c Early Item Macro Expansion Plan
```

该 plan 含 7 个事实，分别约束：

- `attribute_token_tree_input`：M21b 的 `ItemNode::attributes` 和 flat `AttributeTokenDecl` 是宏输入面。
- `builtin_derive_passthrough`：`#[derive(Copy, Eq, Hash)]` 继续走现有 `DeriveDecl` 兼容路径。
- `early_item_expansion_query_key`：真实展开必须由 macro definition identity、attached item stable key 和 token-tree fingerprint 构成 query key。
- `generated_module_part_noop`：未来生成代码必须挂到 `SourceRole::generated` 和 `ModulePartKind::generated`。
- `expansion_source_map_stub`：生成代码必须有 expansion origin / debug trace，但 M21c 只固定 stub facts。
- `unimplemented_item_attribute_blocker`：非 `derive` item attribute 继续由 sema 明确拒绝。
- `external_procedural_macro_blocked`：external procedural macro 仍是 future stage，必须等待 sandbox / manifest / permission / implementation fingerprint。

## Sema 边界

M21b 的非 `derive` attribute blocker 现在改由 M21c query facts 提供消息来源：

```text
item attribute macros are parsed but macro expansion is not implemented yet: <name>
```

这让 sema 不再拥有一份孤立字符串。后续真正引入 early expansion output 时，可以让 sema 查询同一套 expansion facts，
决定某个 attribute 是已展开、builtin passthrough，还是仍需 blocker。

## 不变量

M21c validation 会拒绝以下漂移：

- plan 名称不是 `M21c Early Item Macro Expansion Plan`。
- 7 类 fact kind 不完整或重复。
- generated module part 不是 `SourceRole::generated` / `ModulePartKind::generated`。
- 任一事实声称 M21c 已经产生 user-generated code。
- 任一事实要求标准库或 runtime。
- summary 与 facts 不一致。
- fingerprint 与 facts / summary 不一致。

## 非目标

M21c 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- 真实 hygiene resolution。
- 真实 expansion source map。
- 真实 generated module part parse / merge。
- `--emit-expanded` 或 macro trace CLI。

## 测试入口

新增 `MacroExpansion*` query tests，覆盖：

- enum name / invalid fallback。
- `m21c_macro_expansion_plan_baseline()` validation。
- summary / dump / fingerprint 稳定性。
- generated module part identity guard。
- standard library / runtime / user-generated-code drift rejection。
- sema blocker message source。

## 下一步

下一步应进入 M21d：把 query facts 接到真实 no-op early expansion pass / driver pipeline boundary。
M21d 仍应保持 no-op：可以建立 expansion result container、generated part placeholder 和 source map record，但仍不打开
external procedural macro、typed expression macro、标准库或真实用户代码生成。

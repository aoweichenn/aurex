# Aurex M21d No-op Early Item Macro Expansion Boundary

日期：2026-06-12
阶段：M21d No-op Early Item Macro Expansion Boundary
状态：frontend no-op expansion pass / driver pipeline boundary / deterministic expansion facts

## 目标

M21d 把 M21c 的 early item expansion plan 接到真实 frontend pipeline，但仍保持 no-op。它运行在 module loading /
AST combine 之后、sema 之前，用已解析的 `ItemNode::attributes` 生成 deterministic expansion result。这个阶段的目的不是
让宏生成用户代码，而是固定后续真实 item macro / derive codegen 必须穿过的边界：

- 宏输入来自 AST 中已经保存的 item attributes。
- 每个 attribute 都有稳定 query-key fingerprint。
- 每个需要宏边界的 source module part 都有 generated module part placeholder。
- 每个 attribute 都有 expansion source map placeholder。
- profile 中有独立 `macro.expand_items` 阶段。
- AST 不被修改，sema 仍看到原始 AST。

## 已实现

新增 frontend macro API：

- `frontend::macro::EarlyItemMacroInput`
- `frontend::macro::GeneratedModulePartPlaceholder`
- `frontend::macro::ExpansionSourceMapPlaceholder`
- `frontend::macro::EarlyItemExpansionSummary`
- `frontend::macro::EarlyItemExpansionResult`
- `frontend::macro::EarlyItemExpansionDisposition`
- `expand_early_item_macros_noop()`
- `early_item_expansion_fingerprint()`
- `summarize_early_item_expansion()`
- `dump_early_item_expansion()`

新增 `aurex_macro` target，当前只包含 no-op early item expansion boundary。`aurex_driver` 链接该 target，并在
`FrontendPipeline::load_modules()` 中于 `module.append` 之后运行：

```text
module.read / module.lex / module.parse / module.append
macro.expand_items
ast.dump 或 sema.analyze
```

新增 pipeline stage：

```text
early_item_macro_expand -> macro.expand_items
```

该 stage 的输入是 `combined AST module + macro expansion plan`，输出是
`no-op early item expansion result`。

## Expansion Result

`EarlyItemExpansionResult` 是 M21d 的核心产物。它包含：

- `name`：固定为 `M21d No-op Early Item Macro Expansion Boundary`。
- `plan`：必须是有效的 `M21c Early Item Macro Expansion Plan`。
- `inputs`：每个 parsed item attribute 对应一个 `EarlyItemMacroInput`。
- `generated_parts`：每个带 attribute 的 `(module, source_part_index)` 对应一个 generated module part placeholder。
- `source_maps`：每个 macro input 对应一个 source map placeholder。
- `summary`：记录 attribute、builtin derive passthrough、blocked attribute、placeholder 和 no-op blocker 计数。
- `fingerprint`：由 plan、inputs、generated placeholders、source maps 和 summary 共同决定。

### EarlyItemMacroInput

每个 input 固定以下数据：

- attached `ItemId`。
- owning `ModuleId`。
- source module part index。
- attribute index。
- attribute name。
- attribute range 和 token-tree range。
- token-tree token count。
- attached `ModulePartKey`。
- token-tree fingerprint。
- query-key fingerprint。
- disposition。

当前 disposition 只有两类：

- `builtin_derive_passthrough`：仅用于 `derive` attribute，保持 M20e 内建 derive 兼容路径。
- `blocked_unimplemented_attribute`：用于其他 item attribute，表示已经被 no-op expansion boundary 记录，但仍未生成代码。

### GeneratedModulePartPlaceholder

M21d 会为带 attribute 的 source module part 生成 placeholder，但不会把它 parse / merge 到 AST。placeholder 明确固定：

- `source_role = SourceRole::generated`
- `part_kind = ModulePartKind::generated`
- `parsed = false`
- `merged = false`
- `produced_user_generated_code = false`

generated part stable index 使用保留偏移区间，避免和真实 source part stable index 混淆。它只是 deterministic identity
和 future merge point，不代表当前已经有生成文件。

### ExpansionSourceMapPlaceholder

每个 attribute input 都会生成一个 source-map placeholder。当前它只记录 expansion origin fingerprint 和源范围：

- `real_source_map = false`
- `debug_trace_available = false`

这表示 source map/debug trace 的身份边界已经存在，但真实映射和调试追踪仍留给后续阶段。

## Validation

M21d validation 会拒绝以下漂移：

- expansion result 名称不是 `M21d No-op Early Item Macro Expansion Boundary`。
- plan 不是有效 M21c plan。
- AST 的 `item_modules` / `item_part_indices` 与 `items` 数量不一致。
- 带 attribute 的 item 缺少有效 `ModulePartKey`。
- input 没有 attribute name、range、attached part、token-tree fingerprint 或 query-key fingerprint。
- generated placeholder 不是 `SourceRole::generated` / `ModulePartKind::generated`。
- generated placeholder 声称已经 parsed、merged 或 produced user-generated code。
- source-map placeholder 声称已经有 real source map 或 debug trace。
- summary 和 result 不一致。
- fingerprint 和 result 不一致。

## 当前能做什么

当前用户可见能力仍是：

- `#[derive(Copy, Eq, Hash)]` 继续走内建 derive path。
- 其他 `#[attr(...)]` 可以被 parser 保存为 `ItemNode::attributes`。
- driver profile 会显示 `macro.expand_items` 阶段。
- compiler 内部可以 dump / summarize M21d no-op expansion result。
- 单元测试可以用 `EarlyItemExpansionResult` 验证 attribute token tree、query-key fingerprint、generated placeholder 和
  source-map placeholder。

当前不能做：

- 不生成用户代码。
- 不修改 AST。
- 不把 generated module part parse / merge 回 combined module。
- 不执行 external procedural macro。
- 不实现 typed expression macro。
- 不实现用户自定义 derive。
- 不实现文本替换宏。
- 不实现标准库。
- 不实现 runtime helper。
- 不提供 `--emit-expanded` 或 macro trace CLI。

## 与后续阶段的关系

M21d 是真实宏展开前的 pipeline boundary。后续应继续沿这个边界扩展，而不是绕过它：

- M21e 可以把 generated module part parse / merge 的 stub contract 固定下来，但仍不生成用户代码。
- M21f 可以引入 hygienic expansion id / name resolution skeleton。
- M21g 可以加入 debug trace / source-map 真实数据结构和 dump/CLI 入口。
- 第一条真实代码生成主线应优先做 compiler-owned derive / attached item codegen，而不是直接打开 external
  procedural macro。

所有后续阶段仍应保持：宏输出必须走 parser、sema、borrow checker、visibility、trait solver、IR verifier 和 normal
diagnostics，不能让 macro output 绕过语言规则。


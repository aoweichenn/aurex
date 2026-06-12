# Aurex M21l Parser Admission Diagnostic Report Projection

日期：2026-06-12
阶段：M21l Parser Admission Diagnostic Report Projection
状态：parser admission diagnostic report / query projection hardening / parser-blocked / no user-generated code

## 阶段定位

M21l 继续沿用 M21d-M21k 已接入真实 frontend pipeline 的 `macro.expand_items` 边界。本阶段仍不打开
parser consumption，也不 parse / merge generated module part，而是把 M21k 的 per-input parser admission
diagnostic projection 汇总成 tooling/query 可复用的 report surface。

M21l 的核心目标：

- 保留 M21k 的每个 macro input 一个 `ParserAdmissionDiagnosticProjectionStub`。
- 新增每个 diagnostic 对应的 `ParserAdmissionDiagnosticReportEntry`。
- 新增每个 generated module part 对应的 `ParserAdmissionDiagnosticReport`。
- 固定 report entry identity、report identity、report anchor identity 和 report grouping identity。
- 固定 category totals、source-anchor ordering、query projection name 和 report policy。
- 继续让 parser、generated source text、debug trace、source map 和用户代码生成保持 blocked。
- 把 report / entry 纳入 summary、dump、fingerprint 和 validation。

M21l 不是宏展开执行阶段。它只是把“为什么 generated token buffer 仍不能进入 parser”从单个 diagnostic
projection 升级为可缓存、可索引、可比对的 report/query projection。

## 当前入口

M21l result 名称固定为：

```text
M21l Parser Admission Diagnostic Report Projection
```

实现入口仍是：

```text
frontend::macro::expand_early_item_macros_noop()
```

driver 仍只运行同一个 frontend profile stage：

```text
macro.expand_items
```

这点保持不变：M21l 不是 parser 的新入口，也不是 AST mutation pass。

## 新增公共结构

M21l 给 `EarlyItemExpansionResult` 新增：

```cpp
std::vector<ParserAdmissionDiagnosticReportEntry> parser_admission_report_entries;
std::vector<ParserAdmissionDiagnosticReport> parser_admission_reports;
```

### ParserAdmissionDiagnosticReportEntry

`ParserAdmissionDiagnosticReportEntry` 是 M21k diagnostic projection 的 report/query 行视图。每个
`ParserAdmissionDiagnosticProjectionStub` 都会派生出一个 entry。

关键字段：

- `item` / `module` / `part_index` / `attribute_index`：继承 diagnostic 的 input identity。
- `report_index`：当前 expansion result 内的稳定 diagnostic/report-entry 顺序。
- `attached_part` / `generated_part`：绑定 source module part 和 generated module part。
- `primary_anchor` / `token_tree_anchor`：继承 diagnostic 的 source anchor。
- `diagnostic_identity` / `diagnostic_anchor_identity`：继承 M21k diagnostic identity。
- `report_entry_identity`：M21l 新增 entry identity。
- `parse_gate_identity`：绑定 M21j parser admission gate。
- `blocker_category`：继承 M21k derive / empty category。
- `debug_projection_name`：继承 M21k debug projection name。
- `query_projection_name`：固定为 `m21l-parser-admission-report:<module>:<part>`。
- `token_count` / `token_records_available`：继承 M21j/M21k token availability。
- `report_visible = true` / `query_reusable = true`：允许 tooling/query 消费。
- `parser_admitted = false` / `parser_consumable = false` / `emit_expanded_available = false` /
  `produced_user_generated_code = false`：继续保持 parser-blocked。

`report_entry_identity` 混入 input identity、source anchors、diagnostic identity、diagnostic anchor identity、
parse gate identity、category、debug projection name、query projection name、token count 和 blocked flags。
这保证 entry 不是简单拷贝字符串，而是可稳定重算的 query row。

## Report 分组模型

`ParserAdmissionDiagnosticReport` 按 generated module part 分组。当前 no-op early item expansion 中，每个
source module part 至多有一个 generated module part，因此常见单文件用例会得到一个 report；多 module part
时会按 `(module, source_part_index)` 分别生成 report。

关键字段：

- `module` / `source_part_index`：report 所属 source module part。
- `attached_part` / `generated_part`：source part 与 generated part key。
- `report_identity`：完整 report identity。
- `report_anchor_identity`：source-anchor ordering identity。
- `report_grouping_identity`：entry grouping identity。
- `parse_config_fingerprint` / `generated_buffer_identity`：继承 M21e parse/merge stub。
- `report_policy = parser_admission_blocked_report_query_projection_v1`。
- `report_query_name = m21l-parser-admission-report:<module>:<part>`。
- `blocked_reason = parser admission diagnostic report remains parser-blocked in M21l`。
- `entry_count` / `blocked_entry_count` / `derive_entry_count` / `empty_entry_count` /
  `token_record_available_entry_count`：从 entries 重算的 totals。
- `query_reusable = true` / `report_visible = true` / `source_anchor_ordered = true`。
- `parser_admitted = false` / `parse_ready = false` / `parser_consumable = false` /
  `emit_expanded_available = false` / `debug_trace_available = false` /
  `source_map_available = false` / `produced_user_generated_code = false`。

`report_grouping_identity` 混入 generated part key 和该 part 下所有 entry 的 entry identity、diagnostic identity、
anchor identity、report index 和 category。`report_anchor_identity` 混入同一分组内的 source anchor 和 diagnostic
anchor identity。`report_identity` 再混入 M21e generated buffer identity、parse config、grouping identity、
anchor identity、policy、query name、blocker 和 blocked flags。

## 当前能做什么

M21l 当前能做到：

- 继续收集 item attributes 并生成 M21d-M21k 的所有 no-op macro facts。
- 为每个 macro input 生成 deterministic parser admission diagnostic projection。
- 为每个 diagnostic projection 生成 deterministic report entry。
- 为每个 generated module part 生成 deterministic parser admission report。
- 将 report entries 按 source anchor 保持稳定顺序。
- 对每个 report 汇总 blocked / derive / empty / token-record-available totals。
- 通过 `parser_admission_report_entries` 和 `parser_admission_reports` 暴露 query/debug 可复用投影。
- 在 summary 中统计 report entry 数、report 数、category totals、report visibility、query reusable、unordered
  anchor 和 parser-consumable drift。
- 在 dump 中输出 report entry identity、report identity、report anchor identity、report grouping identity、
  query projection name 和 blocked flags。
- 在 fingerprint 中混入 report entries 和 reports，使缓存能感知 report surface 漂移。

## 当前仍不实现

M21l 仍不实现：

- 标准库。
- runtime helper。
- 文本替换宏。
- 用户自定义 derive。
- external procedural macro 执行。
- typed expression macro。
- macro-generated user code lowering。
- AST mutation。
- parser consumption of generated token buffers。
- generated source text。
- generated module part parse / merge。
- 真实 hygiene resolution。
- declared generated names lookup。
- generated item visibility / export。
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

这些不是遗漏，而是本阶段的边界。M21l 的价值是把 parser admission blocker 做成强类型、可查询、可校验的
report layer，为后续真正打开 parser consumption 做准备。

## Validation 矩阵

M21l validation 会拒绝以下漂移：

- expansion result 名称不是 `M21l Parser Admission Diagnostic Report Projection`。
- 缺失 `parser_admission_report_entries` 或数量不等于 M21k diagnostics。
- 缺失 `parser_admission_reports` 或数量不等于 generated parts。
- report entry 不能按 M21k diagnostic 反算。
- report entry 的 `report_entry_identity`、`diagnostic_identity`、`diagnostic_anchor_identity` 或
  `parse_gate_identity` 漂移。
- report entry 的 source anchors 漂移。
- report entry 的 `blocker_category`、`debug_projection_name` 或 `query_projection_name` 漂移。
- report entry 的 `report_index` 与稳定 expansion order 不一致。
- report entry 打开 `parser_admitted`、`parser_consumable`、`emit_expanded_available` 或
  `produced_user_generated_code`。
- report entry 关闭 `report_visible` 或 `query_reusable`。
- report 不能按 generated part / parse-merge stub / entries 分组反算。
- report 的 `report_identity`、`report_anchor_identity` 或 `report_grouping_identity` 漂移。
- report 的 M21e `generated_buffer_identity` 或 `parse_config_fingerprint` 漂移。
- report 的 policy、query name 或 blocked reason 漂移。
- report 的 entry totals、blocked totals、derive / empty totals 或 token-record-available totals 漂移。
- report 的 source anchors 被标记为 unordered。
- report 打开 `parser_admitted`、`parse_ready`、`parser_consumable`、`emit_expanded_available`、
  `debug_trace_available`、`source_map_available` 或 `produced_user_generated_code`。
- summary 与 result 重算结果不一致。
- result fingerprint 与当前 result 重算结果不一致。

## 测试覆盖

新增和更新的测试集中在：

- `tests/gtest/frontend/macro/early_item_expansion_tests.cpp`

覆盖内容包括：

- M21l result name、summary、dump 和 fingerprint。
- 每个 M21k diagnostic 都有 report entry。
- 每个 generated module part 都有 report。
- report entry 继承 diagnostic identity、diagnostic anchor、parse gate identity、source anchors、category、
  debug projection 和 token availability。
- report entry 固定 `query_projection_name = m21l-parser-admission-report:<module>:<part>`。
- report 固定 `parser_admission_blocked_report_query_projection_v1`。
- report totals 会按 entries 汇总 blocked / derive / empty / token-record-available。
- report 绑定 M21e generated buffer identity 和 parse config。
- report 固定 `source_anchor_ordered=true`。
- validation 拒绝缺失 entries / reports。
- validation 拒绝 entry identity、anchor、category、query name、report index 和 blocked flags 漂移。
- validation 拒绝 report identity、grouping identity、anchor identity、parse config、generated buffer、policy、
  query name、blocked reason、totals 和 parser/debug/source-map/emit/user-code flags 漂移。
- fingerprint 会追踪 report entry identity、query projection name、report identity 和 report totals。

## 下一步

下一步建议继续 M21m：仍保持 parser-blocked / no user-code，做 generated token parser consumption readiness
preflight。M21m 不应直接 parse generated module part，而应先固定 parser 输入所需的 token stream shape、delimiter
balance、source-anchor coverage、hygiene/source-map prerequisites 和 failure diagnostics，让后续真正打开 parser
consumption 时不会猜 token buffer 是否已经满足 parser contract。

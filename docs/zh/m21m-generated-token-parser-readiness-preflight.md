# Aurex M21m Generated Token Parser Consumption Readiness Preflight

阶段：M21m Generated Token Parser Consumption Readiness Preflight

## 目标

M21m 继续沿用已经接入真实 frontend pipeline 的 `macro.expand_items` 边界。本阶段仍不打开 parser
consumption，也不 parse generated module part。它的目标是把“generated token buffer 是否具备进入 parser
之前的结构性前置条件”做成强类型、可查询、可校验的 preflight facts。

M21m 解决的是 parser consumption 之前的可审计性问题：

- token stream shape 是否明确。
- token index 是否连续。
- delimiter 是否平衡。
- source anchor 是否被原 attribute / token tree 覆盖。
- parse config、hygiene、source-map 和 diagnostic projection prerequisites 是否齐备。
- failure 状态是否仍保持 parser-blocked、no user-code。

这不是 parser 的新入口，也不是宏展开执行阶段。

## 新增 API

`EarlyItemExpansionResult` 新增：

```cpp
std::vector<GeneratedTokenParserReadinessPreflightEntry> parser_readiness_preflight_entries;
```

每个 parsed item attribute 对应一个 `GeneratedTokenParserReadinessPreflightEntry`。entry 绑定：

- 原始 macro input：`item`、`module`、`part_index`、`attribute_index`、`attached_part`。
- M21i token facts：`token_plan_identity`、`token_buffer_identity`、`materialization_identity`。
- M21e parser buffer facts：`generated_buffer_identity`、`parse_config_fingerprint`。
- M21j parser admission facts：`parse_gate_identity`。
- M21k/M21l diagnostic/report facts：`diagnostic_identity`、`diagnostic_anchor_identity`、`report_entry_identity`。
- M21f projection facts：`source_map_identity`、`hygiene_mark`、`trace_identity`。
- M21m 自身 identity：`preflight_identity`。

## Token Stream Shape

M21m 当前固定两类 shape：

- `derive_token_buffer_parser_input_candidate`：来自 compiler-owned builtin derive token prototype。
- `empty_token_stream_parser_input_blocked`：来自非 `derive` item attribute 的 empty generated token buffer。

这只是 parser 输入前的结构分类。`derive_token_buffer_parser_input_candidate` 也不代表可以被 parser 消费；
它只表示该 compiler-owned token buffer 有 records，可以被 preflight 审计。

## 结构性 Preflight

M21m 当前能做到：

- 对每个 generated token buffer 计算 `token_indices_contiguous`。
- 对 generated token records 计算 delimiter balance。
- 对 generated token records 的 `anchor_range` 计算 source-anchor coverage。
- 绑定 parse config、hygiene mark、source-map identity 和 diagnostic projection availability。
- 把结果写入 summary、dump、fingerprint 和 validation。

当前固定：

- `delimiter_balance_state = balanced`。
- `source_anchor_coverage_state = covered`。
- `parse_config_compatible = true`。
- `hygiene_prerequisite_available = true`。
- `source_map_prerequisite_available = true`。
- `diagnostic_projection_available = true`。

这些布尔值代表当前 compiler-owned no-op facts 已经具备“可审计 prerequisites”，不代表真实 source map、
真实 hygiene resolution 或 parser consumption 已经实现。

## 仍然阻塞

每个 preflight entry 固定：

- `readiness_policy = generated_token_parser_consumption_readiness_preflight_v1`。
- `blocker_reason = parser consumption readiness preflight remains parser-blocked in M21m`。
- `parser_admitted = false`。
- `parse_ready = false`。
- `parser_consumable = false`。
- `generated_part_parsed = false`。
- `generated_part_merged = false`。
- `produced_user_generated_code = false`。

M21m 仍不实现：

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
- 真实 expansion source map。
- debug trace CLI。
- `--emit-expanded`。

## Validation

M21m validation 会拒绝以下漂移：

- 缺失 `parser_readiness_preflight_entries`。
- entry 与 macro input、token buffer、parser admission gate、diagnostic projection 或 report entry 不能一一重算。
- `preflight_identity` 漂移。
- token stream shape 与 blocker category 不匹配。
- token index continuity、delimiter balance 或 source-anchor coverage 漂移。
- parse config、hygiene、source-map、diagnostic projection prerequisites 被关闭。
- `parser_admitted`、`parse_ready`、`parser_consumable`、`generated_part_parsed`、`generated_part_merged` 或
  `produced_user_generated_code` 被打开。

## 测试

当前测试覆盖：

- `derive` 和非 `derive` 两类 preflight entry。
- token stream shape、delimiter balance、source-anchor coverage、parse config compatibility。
- identity / shape / structural flags 的 negative validation matrix。
- summary、dump 和 fingerprint 对 preflight facts 的响应。

## 下一步

M21m 的输出由 M21n 消费：M21n 不直接 parse generated module part，而是把每个 generated part 下的 preflight
entries 汇总为 parser consumption contract gate。

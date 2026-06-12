# Aurex M22a Builtin Derive Expansion Admission Gate

阶段：M22a Builtin Derive Expansion Admission Gate
状态：已完成。

M22a 在 M21o macro expansion boundary release closure 之后，为每个 early item macro input 增加
`BuiltinDeriveExpansionAdmissionGate`。该阶段仍只产生 query/fingerprint/debug 可索引事实，不执行用户宏、
不生成 source text、不让 generated token buffer 进入 parser、不 parse / merge generated module part、不修改 AST、
不生成用户代码，也不引入标准库或 runtime helper。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_expansion_admissions`。
- `BuiltinDeriveExpansionAdmissionGate`。
- `is_valid(const BuiltinDeriveExpansionAdmissionGate&)`。
- `EarlyItemExpansionSummary` 中的 builtin derive admission 计数。
- fingerprint / dump / validation 对 admission facts 的混入和重算。

## 事实内容

每个 admission gate 绑定：

- macro input identity：item、module、part、attribute index 和 attached part。
- generated part identity。
- M21i `token_buffer_identity`。
- M21m `preflight_identity`。
- M21j `parse_gate_identity`。
- M21k `diagnostic_identity`。
- M21o `closure_identity`。
- M22a `admission_identity`。
- `admission_policy = builtin_derive_expansion_admission_gate_v1`。
- query name：`m22a-builtin-derive-admission:<module>:<part>:<item>:<attr>:<name>`。

`derive` input 的 `admission_kind` 固定为 `builtin_derive_expansion_candidate`；非 `derive` input 固定为
`non_derive_attribute_expansion_blocked`。`derive` input 会记录 token-tree 中可作为 Copy / Eq / Hash capability
candidate 的 identifier 数量，同时记录 unsupported / duplicate candidate 数量。非 `derive` input 的 capability
candidate 计数保持 0。

## 关闭边界

M22a 明确保持：

- `parser_consumption_enabled=false`。
- `external_process_required=false`。
- `standard_library_required=false`。
- `runtime_required=false`。
- `generated_source_text=false`。
- `produced_user_generated_code=false`。

`derive` input 的 blocker 为 `builtin derive expansion admission remains parser-blocked in M22a`。非 `derive`
input 的 blocker 为 `non-derive item attribute expansion remains blocked in M22a`。

## 不是宏执行

M22a 不把 `#[derive]` 展开成字段级方法，不创建新 item，不对 generated token buffer 进行 parser admission，
也不改变现有 sema 中内建 `#[derive(Copy, Eq, Hash)]` capability path。它只把后续 builtin derive expansion
需要的 admission 前置条件做成稳定事实。

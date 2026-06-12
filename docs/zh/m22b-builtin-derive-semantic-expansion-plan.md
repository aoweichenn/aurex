# Aurex M22b Builtin Derive Semantic Expansion Plan

阶段：M22b Builtin Derive Semantic Expansion Plan
状态：已完成。

M22b 在 M22a admission gate 之后，为每个 early item macro input 增加
`BuiltinDeriveSemanticExpansionPlan`。该阶段只描述 compiler-owned builtin derive 的语义计划，不生成 AST item、
不 materialize 用户代码、不打开 parser consumption，也不引入标准库或 runtime helper。

## 新增事实

- `EarlyItemExpansionResult::builtin_derive_semantic_plans`。
- `BuiltinDeriveSemanticExpansionPlan`。
- `is_valid(const BuiltinDeriveSemanticExpansionPlan&)`。
- `capability_set_identity`。
- `semantic_plan_identity`。
- `EarlyItemExpansionSummary` 中的 builtin derive semantic plan / capability 计数。

## 语义模型

M22b 的 semantic model 固定为 `capability_fact_lowering_plan`。它复用当前已有的内建 derive capability path：

- `Copy` 仍走现有资源语义和 drop/custom destructor 约束。
- `Eq` 仍进入 checked capability facts。
- `Hash` 仍进入 checked capability facts。

M22b 会读取 parser/AST 已经保留的 `ItemNode::derives`，记录：

- `target_kind = struct | enum | other`。
- `target_struct_or_enum`。
- `capability_count`。
- `copy_capability_count`。
- `eq_capability_count`。
- `hash_capability_count`。
- `uses_existing_builtin_derive_capability_path`。

非 `derive` input 仍会有一个 blocked semantic plan fact，便于 query/index 层做到一 input 一 plan；其 capability
计数保持 0。

## 关闭边界

M22b 明确保持：

- `requires_ast_mutation=false`。
- `requires_generated_items=false`。
- `requires_standard_library=false`。
- `requires_runtime=false`。
- `external_process_required=false`。
- `parser_consumption_enabled=false`。
- `produced_user_generated_code=false`。

blocker 固定为 `builtin derive semantic expansion remains capability-only and parser-blocked in M22b`。

## 与真实 derive 的关系

M22b 不是新 derive lowering，也不是标准库方法生成。它把现有 `#[derive(Copy, Eq, Hash)]` 的语义能力路径投影成
macro system 可索引、可 fingerprint、可 dump、可 validation 的计划事实，为后续真正打开 builtin derive parser
consumption 做准备。

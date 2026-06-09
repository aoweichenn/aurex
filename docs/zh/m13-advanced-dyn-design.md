# Aurex M13 Advanced Dyn Remaining Policy Design Baseline

日期：2026-06-09
状态：M13a design/query gate 已完成

## 目标

M13a 的目标不是继续扩大 runtime，而是在 M12 完成 direct composition dispatch 之后，重新审视 advanced dyn
剩余候选，并选择下一条可以在当前 compiler/runtime-core 阶段独立推进的主线。

M13a 的选择是：**borrowed composition-to-supertrait explicit projection**。

这条主线只允许在 borrowed principal-set composition view 上显式选择某个 principal，再沿用 M10 已完成的
supertrait upcast metadata，得到 borrowed supertrait view。M13a 不把这个能力实现为用户语法或 runtime lowering；
它只固定 policy、query gate、事实边界、diagnostics 计划、verifier/backend 测试计划和非目标。

## 当前输入事实

M13a 必须建立在现有代码事实上，而不是重新设计 dyn：

- M8/M9 已固定 borrowed dyn view：`&dyn Trait` / `&mut dyn Trait` 是 `{data*, vtable*}`，ABI policy 是
  `borrowed_view_v1`。
- M10 已完成 borrowed dyn supertrait upcasting：`&dyn Child -> &dyn Parent` 和 inherited parent method
  dispatch 会使用 `supertrait_vptr_metadata_v1`，lowering 为 `trait_object_upcast`。
- M11/M12 已完成 borrowed principal-set composition：`&dyn (A + B)` 是 `{data*, principal_set_metadata*}`，
  metadata policy 是 `principal_set_metadata_v1`，显式 principal projection 和无歧义 direct composition method
  dispatch 都会先生成 `trait_object_composition_project` 再复用 single-trait dyn dispatch。
- M12b 明确拒绝 `&dyn (Child + Debug)` 上隐式穿过 `Child -> Parent` 的 direct call；这个负例是 M13a 的设计输入。

## 候选比较

M13a 重新比较 M12 后剩余的 advanced dyn 候选：

| 候选 | M13a 结论 | 原因 |
| --- | --- | --- |
| borrowed composition-to-supertrait explicit projection | 选中 | 能复用 `principal_set_metadata_v1` 和 `supertrait_vptr_metadata_v1`，只需组合两条已验证 borrowed projection，不需要标准库、owner、allocator 或 drop runtime。 |
| dynamic Drop dispatch | 继续阻塞 | 需要 destructor slot、drop-glue metadata、dropck/tooling facts 和 runtime cleanup ABI；不应塞进 borrowed principal-set metadata。 |
| owning dyn / `Box<dyn Trait>` | 继续阻塞 | 需要 owner container、move/drop、allocator、layout、standard-library API 和 resource model 设计。 |
| allocator policy | 继续阻塞 | 需要标准库或 runtime ownership policy；borrowed view 不应携带 allocator metadata。 |
| auto trait / marker trait composition | 继续后置 | 需要 trait solver、object identity、negative/unsafe marker policy 和 diagnostics 的独立设计。 |

因此 M13a 不新增标准库，不新增 owning dyn，不新增 `Box<dyn Trait>`，不新增 allocator policy，不新增 dynamic
Drop dispatch，也不新增 auto trait composition。

## 选中语义

M13 后续实现包应采用显式 borrowed projection 语义：

```aurex
trait Parent { fn parent(self: &Self) -> i32; }
trait Child: Parent { fn child(self: &Self) -> i32; }
trait Debug { fn debug(self: &Self) -> i32; }

fn score(view: &dyn (Child + Debug)) -> i32 {
    // M13a 只设计这个能力；具体 syntax 和 lowering 留给 M13b。
    let parent: &dyn Parent = /* explicit projection from view through Child */;
    return parent.parent();
}
```

核心规则：

- Projection 必须是显式的。M13 不把 `view.parent()` 变成隐式 composition-to-supertrait direct dispatch。
- Projection 必须先选择一个 source principal，再沿该 principal 的 M10 supertrait path 投影到 target supertrait。
- 如果多个 principal 都能到达同一个 target supertrait，且用户没有显式选择 source principal，必须诊断 ambiguity。
- Projection 保持 data pointer 和 origin，不改变 borrow kind；`&mut dyn (A+B)` 可投影到 `&mut dyn Parent` 或
  `&dyn Parent`，但 `&dyn (A+B)` 不能投影到 `&mut dyn Parent`。
- Associated equality 仍来自选中的 source principal 和 M10 supertrait edge，不做跨 principal 的隐式 equality merge。
- 目标 supertrait view 仍是普通 single-trait dyn view，后续 method call 继续走已有 `vtable_slot` 或
  `trait_object_upcast` 事实链。

这不是新的 “composition supertrait metadata v1”。M13 后续实现应组合已有 metadata policy，而不是新增第三种
composition-supertrait runtime layout。

## Query Gate

M13a 在 query 层新增：

- `DynAdvancedCapability::borrowed_composition_supertrait_projection`
- `DynAdvancedPolicyDecision::composes_existing_metadata_policies`
- `m13a_dyn_advanced_design_gate_baseline()`
- `is_valid_m13a_dyn_advanced_design_gate()`

M13a gate 固定 6 个候选：

- `supertrait_upcasting`：`completed_release_baseline`，metadata policy 为 `supertrait_vptr_metadata_v1`。
- `multi_trait_composition`：`completed_release_baseline`，metadata policy 为 `principal_set_metadata_v1`。
- `borrowed_composition_supertrait_projection`：`ready_for_future_stage`，decision 为
  `composes_existing_metadata_policies`，不要求新 ABI policy 或新 metadata policy。
- `owning_dyn`：继续 `prototype_blocked`，需要 standard library stage。
- `dynamic_drop_dispatch`：继续 `prototype_blocked`，需要 runtime stage。
- `allocator_policy`：继续 `prototype_blocked`，需要 standard library stage。

新增候选的 required facts：

- `composition_to_supertrait_projection_fact`
- `principal_supertrait_path_fact`
- `composition_supertrait_ambiguity_fact`
- `composition_supertrait_projection_abi_descriptor`

新增 non-goals：

- `standard_library_runtime_not_in_m13a`
- `new_runtime_metadata_not_in_m13a`
- `owning_dyn_runtime_not_in_m13a`
- `do_not_make_composition_to_supertrait_direct_call_implicit`
- `do_not_add_new_principal_set_metadata_policy`

## M13b 实现建议

M13b 不应一次做到 release closure。推荐实现包：

1. Frontend/sema check-only explicit projection。
2. `CompositionProjectionFact` 记录 `composition_to_supertrait`，并补 `source_principal -> target supertrait` path。
3. `TypeCheckBodyAuthority` / checked dump / fingerprint 混入新的 projection fact。
4. Negative diagnostics：missing source principal、target 不在 selected source principal 的 supertrait closure、
   multiple source principals ambiguity、shared-to-mut projection、associated equality drift。
5. IR/backend 可先保持 blocked 或 explicit todo guard；等 M13c 再 lowering 为
   `trait_object_composition_project` + `trait_object_upcast` 组合，或单个 lowering helper 生成同等 IR。

M13c/M13d 再做 runtime lowering、ABI descriptor、query/cache/tooling、verifier negative matrix、native execution 和
release closure。

## 验证要求

M13 后续实现必须覆盖：

- query gate validation、summary、dump、fingerprint drift。
- explicit projection 正例：shared、mutable、mut-to-shared。
- source principal disambiguation。
- same target supertrait through multiple principals 的 ambiguity。
- target 不属于 selected source principal supertrait closure。
- direct `view.parent()` 继续拒绝，直到有独立设计明确改变该规则。
- IR verifier 不允许缺失 source principal、target supertrait、upcast path 或 metadata identity drift。
- lower-IR query fingerprint 对 projection target、source principal、borrow kind 和 supertrait path drift 敏感。
- coverage 继续满足 90% 门槛。

## 代码量预估

M13a design/query gate 预估 500-900 行新增/修改，主要来自 query DTO/baseline tests、设计文档和 documentation tests。

后续实现粗估：

| 阶段 | 内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M13b frontend/query/sema check-only | explicit composition-to-supertrait projection syntax/typing/facts/diagnostics/checked dump | 1,000-1,800 行 |
| M13c IR/backend runtime | composition project + supertrait upcast lowering、ABI descriptor、verifier、LLVM/native tests | 900-1,600 行 |
| M13d hardening/release | query/cache/tooling hover、negative matrix、docs/samples、coverage closure、代码量偏差分析 | 700-1,200 行 |

如果实际代码量偏离，主要原因应从 syntax 选择、diagnostic matrix、是否复用 M10/M11 lowering helper、documentation
tests 数量和 query/cache fingerprint 覆盖强度分析，不应通过降低测试质量来贴合预估。

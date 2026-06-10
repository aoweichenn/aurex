# Aurex M17 Dyn Ownership Runtime Preparation Release Baseline

状态：M17 已完成 dyn ownership runtime preparation 的 compiler/query/tooling 边界事实。M17 不实现标准库、
不实现 `Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop dispatch，也不做
runtime ABI lowering。

## 完成范围

M17 把 M15 选定的 owning dyn / dynamic Drop / allocator 后续路线从 design gate 推进到可验证的 preparation facts：

- 新增 `DynOwnershipRuntimeFacts`，作为 future owning dyn runtime 边界的独立 query DTO。
- 新增 `DynOwnedContainerBoundaryFact`，固定 `owning_dyn_container_v1` 和 `owning_dyn_metadata_v1` 只属于 future
  standard-library owner/container boundary。
- 新增 `DynErasedDropGlueBoundaryFact`，固定 `dynamic_drop_metadata_v1`、`erased_drop_glue_identity_fact`、
  `dynamic_drop_slot_layout_fact` 和 `dropck_erased_receiver_fact` 的事实身份。
- 新增 `DynAllocatorBoundaryFact`，固定 `allocator_placement_policy_v1`、`allocator_metadata_v1`、
  `allocator_identity_fact`、`allocator_placement_policy_fact` 和 `owned_dyn_deallocation_policy_fact`。
- 新增 `DynCleanupDropckBoundaryFact`，把 cleanup/resource/dropck 事实桥接到 future erased drop boundary，
  但仍明确阻塞 runtime lowering 和 dynamic Drop dispatch。
- 新增 `DynOwnershipRuntimeSummary`，统计 boundary、standard-library blocker、runtime-lowering blocker、
  `Box` surface blocker、allocator API blocker、dynamic-drop blocker、borrowed-vtable destructor-free 和
  cleanup/dropck bridge。
- 新增 `dyn_ownership_runtime_facts_fingerprint()`、
  `summarize_dyn_ownership_runtime_facts()`、`dump_dyn_ownership_runtime_facts()`、
  `m17_dyn_ownership_runtime_preparation_baseline()` 和
  `is_valid_m17_dyn_ownership_runtime_preparation_baseline()`。

这些事实位于独立 query 模块，不修改既有 borrowed dyn ABI：`FunctionDynAbiFacts`、`borrowed_view_v1`、
`borrowed_methods_only_v1`、principal-set composition metadata 和 supertrait upcast metadata 仍保持原有语义。

## 现在能做什么

M17 当前能做的是编译器内部和工具链层面的准备：

- 工具或测试可以调用 `m17_dyn_ownership_runtime_preparation_baseline()` 获取 M17 的稳定 baseline facts。
- Query/cache 可以通过 `dyn_ownership_runtime_facts_fingerprint()` 判断 future ownership/runtime boundary facts 是否漂移。
- Dump/summary 可以展示当前 boundary：owned container、erased drop glue、allocator、cleanup/dropck。
- Validation 会拒绝把 M17 事实伪装成已经实现标准库、已经实现 runtime lowering、已经打开 allocator API、已经支持
  `Box` surface，或把 borrowed vtable 变成携带 destructor slot 的 ABI。
- 文档和测试可以稳定引用 `owning_dyn_container_v1`、`owning_dyn_metadata_v1`、
  `dynamic_drop_metadata_v1`、`allocator_placement_policy_v1`、`allocator_metadata_v1` 和
  `cleanup_dropck_boundary_v1`。

换句话说，M17 的产物是“未来运行时/标准库阶段必须遵守的事实边界”，不是用户可写语言能力。

## 明确非目标

M17 仍然不实现：

- 标准库 API、`std` module、owned string/vector/container 或标准库 resource wrapper。
- `Box`、`Box<dyn Trait>`、owning dyn 用户值、owned trait-object coercion 或 owning trait-object method dispatch。
- allocator trait、allocator value、allocator placement expression、allocator selection API 或 deallocation runtime call。
- dynamic Drop dispatch、trait-object destructor slot、trait-object destructor ABI、unwind-aware cleanup runtime。
- 把 destructor slot 塞进 `borrowed_methods_only_v1` vtable。
- runtime ABI lowering、LLVM lowering、IR runtime call 或 backend emission。

M17 的原则是：先让 compiler facts 和 tooling boundary 稳定，再进入后续 runtime/standard-library 设计；不能用 borrowed dyn
metadata 临时冒充 owning dyn runtime。

## 边界不变量

M17 validation 固定以下不变量：

- owned container 和 allocator boundary 必须停在 future standard-library boundary；erased drop glue boundary 必须停在
  future runtime-lowering boundary；cleanup/dropck boundary 必须停在 preparation boundary。
- `DynOwnedContainerBoundaryFact` 必须保留 `standard_library_blocked=true`、`runtime_lowering_blocked=true`、
  `box_surface_blocked=true` 和 `user_value_surface_blocked=true`。
- `DynErasedDropGlueBoundaryFact` 必须保留 `runtime_lowering_blocked=true`、
  `dynamic_drop_dispatch_blocked=true` 和 `borrowed_vtable_destructor_free=true`。
- `DynAllocatorBoundaryFact` 必须保留 `standard_library_blocked=true`、`allocator_api_blocked=true` 和
  `runtime_lowering_blocked=true`。
- `DynCleanupDropckBoundaryFact` 必须记录 cleanup/dropck bridge，同时保留 runtime lowering 与 dynamic Drop dispatch blocker。
- `DynOwnershipRuntimeFacts::summary` 必须等于当前 facts 重新计算的 summary。
- `DynOwnershipRuntimeFacts::fingerprint` 为空或等于当前 facts 重新计算的 stable fingerprint。

M17 baseline 的 summary 应为：

```text
boundary_count=4
owned_container_boundary_count=1
erased_drop_glue_boundary_count=1
allocator_boundary_count=1
cleanup_dropck_boundary_count=1
standard_library_blocked_count=2
runtime_lowering_blocked_count=4
box_surface_blocked_count=1
allocator_api_blocked_count=1
dynamic_drop_dispatch_blocked_count=2
borrowed_vtable_destructor_free_count=1
cleanup_dropck_bridge_count=1
```

## 测试覆盖

M17 新增 focused query tests 覆盖：

- enum name / invalid fallback。
- `m17_dyn_ownership_runtime_preparation_baseline()` 的完整 baseline validation。
- record functions 对 summary 的增量更新。
- 空 subject、缺失 fact 名称、错误 policy、错误 stage、summary drift 和 fingerprint drift 的负例。
- 伪装成已实现标准库、已实现 allocator API、已实现 runtime lowering 或 borrowed vtable 携带 destructor slot 的负例。
- summary、dump 和 fingerprint 稳定性。

M17 release gate 仍要求 query tests、full gtest、documentation tests、coverage gate 和 `git diff --check` 通过。

## 后续阶段

下一阶段建议进入 **M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate**。M18 仍不应该直接实现标准库；
它应先把 M17 facts 接到更多 query/cache/tooling/reuse 场景，评估 runtime lowering 所需的 IR/verifier 形状，并继续保持
`Box`、allocator API 和 owning dyn user values 在 future standard-library/runtime 阶段之外。

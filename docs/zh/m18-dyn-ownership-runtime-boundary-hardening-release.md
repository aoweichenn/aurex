# Aurex M18 Dyn Ownership Runtime Boundary Hardening / Lowering Design Gate Release Baseline

状态：M18 已完成 dyn ownership runtime boundary hardening / lowering design gate。M18 不实现标准库、
不实现 `Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop dispatch，
也不做 runtime ABI lowering。M18 的目标是把 M17 的 dyn ownership runtime preparation facts 接到
query/cache/tooling/reuse/workspace 边界，并把 future IR/verifier/runtime lowering 的前置条件固化为可验证事实。

## 完成范围

M18 在 M17 `DynOwnershipRuntimeFacts` 的基础上新增项目级边界门：

- 新增 `DynOwnershipRuntimeBoundaryGate`，作为 project-level query DTO，表示当前项目的 dyn ownership runtime
  boundary hardening 状态。
- 新增 `DynOwnershipRuntimeBoundaryCheckpointFact`，记录 M18 六类 checkpoint：
  `query_cache_projection`、`tooling_projection`、`reuse_boundary`、`ir_verifier_planning`、
  `borrowed_abi_guard` 和 `runtime_lowering_gate`。
- 新增 `DynOwnershipRuntimeLoweringDesignGateFact`，记录 future IR/verifier/runtime lowering 前置条件：
  owned dyn object placeholder、erased drop identity、allocator identity、borrowed vtable destructor guard、
  erased receiver guard 和 stdlib-before-runtime-lowering guard。
- 新增 `DynOwnershipRuntimeBoundarySummary`，统计 checkpoint、M17 引用、standard-library blocker、
  runtime-lowering blocker、`Box` surface blocker、owning dyn user value blocker、allocator API blocker、
  dynamic-drop blocker、borrowed metadata destructor-free 和 lowering design gate。
- 新增 `m18_dyn_ownership_runtime_boundary_gate_baseline()`、
  `dyn_ownership_runtime_boundary_gate_fingerprint()`、
  `dyn_ownership_runtime_boundary_gate_result_fingerprint()`、
  `summarize_dyn_ownership_runtime_boundary_gate()` 和
  `dump_dyn_ownership_runtime_boundary_gate()`。
- 新增 query kind：`dyn_ownership_runtime_boundary_gate`。它使用 `ProjectKey` stable identity，
  provider 输出恰好依赖同一个 `ProjectKey` 的 `project_graph`。
- Query graph、stable-key decoder、edge verifier、provider set、query context、query executor 和 query record
  helpers 已接入该 query kind。
- Incremental cache 已把 M18 gate 作为 project-level subject 收集、排序、评估、统计、复用和 profile 输出。
- IDE snapshot 会生成 M18 semantic fact；tooling session reuse 和 workspace semantic index 会保留
  `dyn_ownership_runtime_boundary_gate` 的稳定 fact kind。

这些改动只把 runtime boundary 变成可缓存、可索引、可复用、可验证的事实，不改变用户可写语法，也不改变现有
borrowed dyn runtime ABI。

## 现在能做什么

M18 当前能做的是编译器基础设施和工具链层面的 hardening：

- Query/cache 可以通过 `dyn_ownership_runtime_boundary_gate` 追踪项目级 runtime boundary gate。
- Query edge verifier 会验证 `dyn_ownership_runtime_boundary_gate -> project_graph` 使用同一个 `ProjectKey`
  stable bytes。
- Incremental cache 可以把 M18 gate 和普通 query records 一起写入、复用、重算、统计和 profile。
- IDE semantic facts 可以展示 M18 gate 的 summary，包括 `standard_library_blocked=6`、
  `runtime_lowering_blocked=6`、`box_surface_blocked=6`、`allocator_api_blocked=6` 和
  `lowering_runtime_implemented=0`。
- Workspace semantic index 可以索引 `dyn_ownership_runtime_boundary_gate`，tooling reuse 会把它当作
  project/signature-level fact，而不是 body-local fact。
- Future IR/verifier 设计可以稳定引用 M18 的 lowering design gate facts，明确哪些 runtime lowering 前置条件尚未实现。

换句话说，M18 的产物是“future runtime lowering 和标准库阶段进入前的事实门禁”，不是用户可写 owning dyn 能力。

## 明确非目标

M18 仍然不实现：

- 标准库 API、`std` module、owned string/vector/container 或标准库 resource wrapper。
- `Box`、`Box<dyn Trait>`、owning dyn 用户值、owned trait-object coercion 或 owning trait-object method dispatch。
- allocator trait、allocator value、allocator placement expression、allocator selection API 或 deallocation runtime call。
- dynamic Drop dispatch、trait-object destructor slot、trait-object destructor ABI、unwind-aware cleanup runtime。
- 把 destructor slot 塞进 borrowed vtable 或把 `borrowed_methods_only_v1` 扩展成 owning/drop ABI。
- IR owned dyn object lowering、runtime ABI lowering、LLVM runtime call、allocator lowering 或 backend emission。

M18 只允许记录 future IR/verifier prerequisites，不能把这些 prerequisites 当成已经实现的 runtime surface。

## 边界不变量

M18 validation 固定以下不变量：

- M18 gate 必须引用有效的 M17 dyn ownership runtime preparation baseline。
- M18 baseline 必须恰好有 6 个 checkpoint，且六类 checkpoint 各出现一次。
- 每个 checkpoint 必须保留 `references_m17_facts=true`。
- 每个 checkpoint 必须保留 `standard_library_blocked=true`、`runtime_lowering_blocked=true`、
  `box_surface_blocked=true`、`owning_dyn_user_value_blocked=true`、`allocator_api_blocked=true` 和
  `dynamic_drop_dispatch_blocked=true`。
- 每个 checkpoint 必须保留 `borrowed_metadata_destructor_free=true`。
- Lowering design gate 必须要求 future IR owned object placeholder、erased drop identity、allocator identity、
  borrowed-vtable destructor verifier guard、missing-erased-receiver verifier guard 和 stdlib-before-runtime-lowering
  verifier guard。
- Lowering design gate 必须保留 `lowering_runtime_implemented=false`、
  `dynamic_drop_runtime_implemented=false` 和 `standard_library_implemented=false`。
- Provider output 必须恰好依赖同一 `ProjectKey` 的 `project_graph`，不能依赖 file/module/body query，也不能依赖
  其他项目的 project graph。
- Summary 和 stable fingerprint 必须与当前 facts 重新计算结果一致。

M18 baseline 的 summary 应为：

```text
checkpoint_count=6
query_cache_checkpoint_count=1
tooling_checkpoint_count=1
reuse_checkpoint_count=1
ir_verifier_checkpoint_count=1
borrowed_abi_guard_count=1
runtime_lowering_gate_count=1
m17_reference_count=6
standard_library_blocked_count=6
runtime_lowering_blocked_count=6
box_surface_blocked_count=6
owning_dyn_user_value_blocked_count=6
allocator_api_blocked_count=6
dynamic_drop_dispatch_blocked_count=6
borrowed_metadata_destructor_free_count=6
lowering_design_gate_count=1
lowering_runtime_implemented_count=0
```

## 测试覆盖

M18 新增 focused query/tooling tests 覆盖：

- enum name / invalid fallback。
- `m18_dyn_ownership_runtime_boundary_gate_baseline()` 的完整 baseline validation。
- record functions 对 summary 的增量更新。
- checkpoint 缺失名称、错误 kind/stage/policy、缺失 M17 引用、关闭 blocker、borrowed metadata destructor drift 的负例。
- lowering design gate 把 runtime、dynamic Drop 或 standard library 标记成已实现的负例。
- summary drift、fingerprint drift、M17 runtime facts drift、重复/缺失 checkpoint 的负例。
- summary、dump 和 fingerprint 稳定性。
- query record、stable key layout、query dependency kind rule、edge verifier、provider set、query context 和 query executor。
- IDE semantic fact、tooling reuse 非 body-local 行为和 workspace semantic index kind string。

M18 release gate 仍要求 query tests、frontend/tooling tests、full gtest、documentation tests、coverage gate 和
`git diff --check` 通过。

## 后续阶段

下一阶段建议进入 **M19 Dyn Ownership Runtime IR / Verifier Preparation**。M19 仍不应该直接实现标准库；它应先把
M18 lowering design gate 中记录的 future IR/verifier prerequisites 落成 verifier-visible 的 IR 形状和负例矩阵，
继续保持 `Box<dyn Trait>`、allocator API、owning dyn user values、dynamic Drop runtime 和标准库容器在后续独立阶段。

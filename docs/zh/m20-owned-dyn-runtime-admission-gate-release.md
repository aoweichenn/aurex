# Aurex M20a Owned Dyn Runtime Admission Design Gate Release Baseline

M20a 已完成 owned dyn runtime admission design gate。M20a 仍然不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不做 runtime ABI lowering、不生成 backend
runtime helper call，也不实现 dynamic Drop runtime。M20a 的目标是把 M17/M18/M19 已经建立的 dyn ownership
runtime facts、project boundary gate 和 IR/verifier facts 汇总成一个准入门禁，固定后续 owned dyn runtime
必须按 owned layout、erased drop identity、allocator identity、runtime lowering ABI 和标准库 surface 的顺序推进。

## 已落成的代码边界

- `OwnedDynRuntimeAdmissionCapability`，固定 6 类准入能力：
  `owned_object_layout`、`erased_drop_identity`、`allocator_identity`、`runtime_lowering_abi`、
  `box_dyn_surface` 和 `borrowed_dyn_abi_separation`。
- `OwnedDynRuntimeAdmissionStage`，把 M20a 的结果分为 admission design gate、IR shape prerequisite、
  runtime identity prerequisite、standard-library API prerequisite 和 blocked future implementation。
- `OwnedDynRuntimeAdmissionPolicy`，固定后续 policy 名称：
  `owned_handle_metadata_v1`、`erased_drop_identity_v1`、`allocator_identity_v1`、
  `runtime_lowering_abi_v1`、`box_dyn_surface_v1` 和
  `borrowed_dyn_remains_destructor_free_v1`。
- `OwnedDynRuntimeAdmissionFact`、`OwnedDynRuntimeAdmissionSummary` 和
  `OwnedDynRuntimeAdmissionGate`。
- `m20_owned_dyn_runtime_admission_gate_baseline()`。该 baseline 嵌入并校验：
  `m17_dyn_ownership_runtime_preparation_baseline()`、
  `m18_dyn_ownership_runtime_boundary_gate_baseline()` 和
  `m19_dyn_ownership_runtime_ir_verifier_baseline()` 的 fingerprint。
- `owned_dyn_runtime_admission_gate_fingerprint()`、summary、dump 和 validation。

## Admission 顺序

M20a 选择的后续推进顺序是：

1. M20b：owned dyn IR shape prerequisite。先定义 owned object layout 的 IR placeholder 和 verifier 不变量。
2. M20c：erased drop identity 与 allocator identity prerequisite。先把销毁和释放身份做成 stable facts。
3. M20d：runtime lowering ABI design closure。决定 backend helper ABI 何时从 blocked fact 变成 executable lowering。
4. M21+：标准库 surface。`Box<dyn Trait>` 和 allocator API 只能在前置 identity/runtime gate 完成后进入实现。

## Validation 不变量

M20a validation 会拒绝下列漂移：

- 删除对 M17 runtime facts、M18 boundary gate 或 M19 IR/verifier facts 的引用。
- 把 standard library API、`Box<dyn Trait>` surface、owning dyn user value、allocator API、runtime lowering、
  dynamic Drop runtime 或 backend helper 标记为已实现。
- 把 borrowed dyn ABI 改成携带 owning/drop metadata。
- 在 owned layout 前跳到 runtime lowering 或 `Box<dyn Trait>` surface。
- 删除 owned layout、erased drop identity、allocator identity 或 runtime ABI 的准入前置事实。
- summary 或 fingerprint 与当前 facts 漂移。

## 当前能做什么

当前语言层仍能使用 M8-M14 已完成的 borrowed dyn 能力：

- `&dyn Trait` / `&mut dyn Trait` borrowed erased view。
- borrowed dyn vtable dispatch。
- borrowed dyn supertrait upcast。
- `dyn (A + B)` borrowed principal-set composition。
- composition-to-principal projection。
- composition-to-supertrait explicit projection。
- 唯一路径下的 borrowed dyn view path expected-type projection 和 direct supertrait method dispatch。

M20a 新增的是 runtime/standard-library 进入前的 admission gate：query facts、dump、fingerprint 和 validation 都能看见
“可以开始设计 owned dyn runtime，但还不能实现标准库或 executable runtime”。

## 明确非目标

M20a 不实现：

- 标准库。
- `Box`。
- 不实现 `Box<dyn Trait>`。
- allocator trait、allocator API、allocator value 或 deallocation runtime call。
- owning dyn 用户值。
- owned trait-object coercion 或 owning trait-object method dispatch。
- owned trait-object runtime layout 的 IR 实体。
- trait-object destructor slot。
- dynamic Drop dispatch runtime。
- backend runtime helper call。
- runtime ABI lowering。

## 测试覆盖

M20a 新增 focused tests 覆盖：

- enum name / invalid fallback。
- baseline summary、dump、fingerprint 和 validation。
- M17/M18/M19 fingerprint dependency drift。
- standard-library blocker、`Box` blocker、allocator API blocker、runtime-lowering blocker、dynamic-drop blocker。
- capability / stage / policy mismatch 负例。
- admission fact 去重、summary drift 和 fingerprint drift。

## 代码量预估和偏差说明

M20a 预估新增/修改代码量是 900-1,600 行。实际代码量以提交时 `git diff --cached --shortstat` 为准。
如果实际高于预估，主要原因应是 M20a 同时新增 query DTO、validation、summary/dump/fingerprint、focused tests、
documentation tests 和 release 文档；如果低于预估，则说明 M17/M18/M19 facts-first 模式复用效果较好。

## 下一阶段

M20a 完成后，下一阶段应进入 **M20b Owned Dyn IR Shape Prototype Gate**。M20b 仍不应该实现标准库；它应先定义
owned dyn object placeholder 的 IR 形状、dump/fingerprint/verifier 不变量，并继续阻塞 `Box<dyn Trait>`、allocator
API、dynamic Drop runtime 和 backend runtime helper call。

# Aurex M19 Dyn Ownership Runtime IR / Verifier Preparation Release Baseline

M19 已完成 dyn ownership runtime 的 IR / verifier preparation。M19 仍然不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不做 runtime ABI lowering、不生成 backend
runtime call，也不实现 dynamic Drop runtime。M19 的目标是把 M18 lowering design gate 中的 future prerequisites
落成 IR/verifier 可见的边界事实和负例矩阵，让后续标准库/runtime 阶段不能悄悄复用 borrowed dyn vtable 或 cleanup
marker 来表达 owning dyn。

## 已落成的代码边界

- `DynOwnershipRuntimeIrVerifierFact`、`DynOwnershipRuntimeIrVerifierSummary` 和
  `FunctionDynOwnershipRuntimeIrVerifierFacts`。
- `m19_dyn_ownership_runtime_ir_verifier_baseline()`，固定 6 类 M19 boundary fact：
  `borrowed_vtable_destructor_free`、`static_cleanup_only`、`erased_drop_identity_required`、
  `allocator_identity_required`、`owned_dyn_object_placeholder_blocked` 和
  `runtime_lowering_blocked_without_stdlib`。
- `dyn_ownership_runtime_ir_verifier_facts_fingerprint()`、summary、dump 和 validation。
- IR collector：
  `function_dyn_ownership_runtime_ir_verifier_facts()`、
  `function_dyn_ownership_runtime_ir_verifier_facts(module)` 和
  `function_dyn_ownership_runtime_ir_verifier_facts_by_symbol()`。
- `TraitObjectVTableLayout::destructor_slot_blocked`。该字段默认 `true`，进入 clone/copy、dump、layout ABI
  fingerprint 和 verifier。它不是 destructor slot，也不是 owning dyn vtable；它只是让 borrowed vtable 的 destructor-free
  policy 成为 IR 可见不变量。
- `CleanupAbiPolicy::dynamic_erased_drop_blocked`。该 policy 是 verifier 负例哨兵，不代表可执行 dynamic Drop
  lowering；它用于明确拒绝 future dynamic erased drop runtime 在 M19 提前进入 IR。
- IR verifier 新增硬拒绝：
  borrowed dyn trait vtable 必须保持 destructor-free；`drop` / `drop_if` 携带
  `dynamic_erased_drop_blocked` 时直接失败。

## Validation 不变量

M19 validation 会拒绝下列漂移：

- borrowed vtable 不再 destructor-free。
- cleanup marker 被标成 dynamic erased drop runtime。
- erased drop identity、allocator identity、owned dyn object placeholder 或 runtime-lowering blocker 被移除。
- standard-library blocker、dynamic-drop blocker、runtime-lowering blocker 被标成已实现。
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

M19 新增的是这些能力的 runtime-ownership 边界保护：IR facts、dump、fingerprint 和 verifier 都能看见“当前只有 borrowed
view；owning dyn、dynamic Drop 和 allocator runtime 仍 blocked”。

## 明确非目标

M19 不实现：

- 标准库。
- 不实现 `Box<dyn Trait>`。
- allocator trait、allocator API 或 deallocation API。
- owning dyn 用户值。
- owned trait object layout。
- trait-object destructor slot。
- dynamic Drop dispatch runtime。
- erased drop glue runtime lowering。
- backend runtime helper call。
- runtime ABI lowering。

## 测试覆盖

M19 新增 focused tests 覆盖：

- query enum name / invalid fallback。
- M19 baseline summary、dump、fingerprint 和 validation。
- runtime blocker drift、standard-library blocker drift、dynamic-drop blocker drift。
- function-level IR collector 对 borrowed vtable 和 future blocker facts 的投影。
- verifier 对 borrowed vtable destructor-free drift 的拒绝。
- verifier 对 `dynamic_erased_drop_blocked` cleanup policy 的拒绝。
- cleanup policy dump 名称和 IR layout fingerprint 变化。

## 代码量预估和偏差说明

M19 预估新增/修改代码量是 900-1,700 行。实际代码量以提交时 `git diff --cached --shortstat` 为准。
如果实际高于预估，主要原因应是 M19 同时新增 query DTO、IR collector、verifier guard、CMake、focused tests 和 release
文档；如果低于预估，则说明复用 M17/M18 的 facts-first 模式和现有 dyn trait fixture 降低了实现成本。

## 下一阶段

M19 完成后，下一阶段可以进入 M20 标准库/owning dyn runtime design gate。M20 才适合开始设计 `Box<dyn Trait>`、
allocator identity 到 API 的映射、owned object layout、dynamic Drop metadata 和 runtime lowering 的实际执行边界。

# Aurex M20d Runtime Lowering ABI Design Closure Release Baseline

M20d 已完成 runtime lowering ABI design closure。M20d 仍然不实现标准库、不实现 `Box<dyn Trait>`、
不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop runtime，不做 backend
runtime helper call，也不做 executable runtime ABI lowering。M20d 的目标是把 M20c 已固定的
compiler-owned drop / allocator identity prerequisites 继续推进为可验证、可 dump、可 fingerprint、可 query 的
runtime lowering ABI 设计事实，让后续标准库或 runtime 阶段不能绕过 M20b/M20c 的身份与 blocker 不变量。

## 新增内容

M20d 新增或固定：

- `OwnedDynRuntimeLoweringAbiGate`。
- `OwnedDynRuntimeLoweringAbiFact`。
- `OwnedDynRuntimeLoweringAbiSummary`。
- `OwnedDynRuntimeLoweringAbiFactKind`、`OwnedDynRuntimeLoweringAbiStage` 和
  `OwnedDynRuntimeLoweringAbiPolicy`。
- `m20d_owned_dyn_runtime_lowering_abi_gate_baseline()`。
- `owned_dyn_runtime_lowering_abi_gate_fingerprint()`、summary、dump 和 validation。
- `ir::owned_dyn_runtime_lowering_abi_gate(const Module&)`。
- `runtime_abi_descriptor_key`：compiler-owned runtime ABI descriptor identity，覆盖 M20c 的 drop identity、
  allocator identity、prototype identity set 和 prototype count。
- `backend_helper_identity_key`：future backend helper prerequisite identity，只作为 query/verifier 事实存在，
  不能被调用。
- M20d validation 对 embedded M20c gate 的强引用：M20d facts 的 drop/allocator/prototype identity keys 必须和
  `OwnedDynDropAllocatorIdentityGate` 完全一致。

## ABI 设计事实

M20d 的 runtime lowering ABI descriptor 是编译器内部事实，不是 runtime slot，也不是标准库 API：

```text
owned_dyn_runtime_lowering_abi_gate {
  requires_m20c_owned_dyn_drop_allocator_identity_gate
  runtime_abi_descriptor_key=<stable query fingerprint>
  backend_helper_identity_key=<stable query fingerprint>
  runtime_lowering_blocked=yes
  backend_helper_callable=no
  executable_runtime_implemented=no
}
```

这条事实链明确记录了未来 runtime lowering 需要哪些身份材料，但不会在本阶段生成任何 executable lowering。
M20d 的 descriptor key 依赖 M20c 的 `prototype_identity_set_key`，因此 module 内多个 owned dyn prototype 的稳定排序、
drop identity、allocator identity 或 prototype count 发生漂移时，M20d fingerprint 也会变化。

## Facts

`OwnedDynRuntimeLoweringAbiGate` 将 M20d 拆成五类 facts：

- `owned_dyn_runtime_abi_descriptor_fact`：compiler-owned runtime ABI descriptor 已作为 query fact 可见。
- `owned_dyn_blocked_to_admitted_transition_guard_fact`：blocked-to-admitted transition 需要 verifier guard，
  当前仍保持 closed。
- `owned_dyn_backend_helper_prerequisite_fact`：future backend helper identity 已被记录，但
  `backend_helper_callable=false`。
- `owned_dyn_drop_allocator_runtime_bridge_fact`：drop identity、allocator identity 和 runtime ABI descriptor
  已绑定到同一个 M20c identity set。
- `owned_dyn_dynamic_drop_runtime_blocker_fact`：dynamic Drop runtime 继续 blocked。

Baseline validation 要求这五类 facts 恰好各出现一次，全部引用有效的 M20c drop / allocator identity gate，
并且全部保持 borrowed ABI unchanged、standard-library blocked、`Box<dyn Trait>` blocked、owning user value
blocked、allocator API blocked、runtime lowering blocked、dynamic Drop runtime blocked 和 backend helper blocked。
`backend_helper_callable_count` 和 `executable_runtime_implemented_count` 必须恒为 0。

## Validation 和负例矩阵

M20d validation / verifier-facing tests 会拒绝下列漂移：

- 删除或篡改 embedded M20c gate fingerprint。
- M20d facts 的 drop identity、allocator identity、prototype identity set 或 prototype count 与 M20c gate 不一致。
- `runtime_abi_descriptor_key` 为空或在 facts 间不一致。
- `backend_helper_identity_key` 为空、等于 descriptor key，或在 facts 间不一致。
- 把 backend helper 标成 callable。
- 把 executable runtime 标成 implemented。
- 把 standard library、`Box<dyn Trait>`、owning dyn user value、allocator API、runtime lowering、backend helper 或
  dynamic Drop runtime 标记成已实现。
- IR module 中 M20c prerequisite 漂移：空 drop/allocator identity、drop 和 allocator identity 相同、runtime slot
  不再 blocked、prototype symbol 无效、runtime/backend/dynamic-drop blocker 被关闭。

## 非目标

M20d 不实现：

- 标准库 `Box`、拥有型容器或 allocator API。
- 用户可写 owning dyn value。
- erased drop runtime、dynamic Drop dispatch 或 trait-object destructor ABI。
- executable runtime ABI lowering。
- LLVM/backend runtime helper call。
- borrowed dyn ABI 的 destructor slot。

M20d 只说明“compiler-owned owned dyn runtime ABI descriptor 已经能被稳定查询、dump、fingerprint 和验证”。它不把
descriptor 变成 runtime memory layout，也不允许用户构造 `Box<dyn Trait>`。

## 测试覆盖

M20d 新增 focused tests 覆盖：

- enum name / invalid fallback。
- M20d baseline 的 summary、dump、fingerprint 和 validation。
- `record_owned_dyn_runtime_lowering_abi_fact()` 的 incremental summary 更新。
- descriptor key、backend helper key、M20c gate fingerprint、summary/fingerprint drift 的负例。
- `backend_helper_callable=true` 和 `executable_runtime_implemented=true` 的硬拒绝。
- `ir::owned_dyn_runtime_lowering_abi_gate()` 对真实 `ir::Module` 的 projection。
- 多个 owned dyn prototype 的 stable sort / descriptor key / helper key 稳定性。
- 空 module invalid。
- prototype identity、runtime slot、runtime/backend/dynamic-drop blocker 漂移时 M20d gate invalid。

相关测试文件：

- `tests/gtest/infrastructure/query/owned_dyn_runtime_lowering_abi_gate_tests.cpp`
- `tests/gtest/midend/ir/owned_dyn_runtime_lowering_abi_tests.cpp`
- `tests/gtest/midend/ir/owned_dyn_drop_allocator_identity_tests.cpp`
- `tests/gtest/midend/ir/owned_dyn_ir_shape_prototype_tests.cpp`

## 代码量说明

M20d 原预估新增/修改代码量是 900-1,700 行。提交前 staged diffstat 是
`19 files changed, 2184 insertions(+), 32 deletions(-)`，高于预估上限。

偏差原因：

- M20d 同时新增 query DTO、baseline、summary、dump、fingerprint 和 validation。
- IR adapter 不是简单转发；它复用 M20c identity gate，并额外生成 runtime ABI descriptor key 和 backend helper
  identity key，同时验证 M20d facts 与 M20c drop/allocator/prototype identity set 一致。
- focused tests 覆盖 query baseline、drift negative matrix、真实 IR module projection、多个 prototype 稳定排序和
  backend helper/executable runtime 不能打开的负例。
- 文档侧同步 release 文档、入口文档、语言手册、特性清单、next-steps 和 documentation tests，避免把 M20d
  误读成标准库或 executable runtime lowering 已实现。

偏差不来自实现标准库、`Box<dyn Trait>`、allocator API、owning dyn user value、dynamic Drop runtime、
backend runtime helper call 或 executable runtime ABI lowering；这些仍全部保持 blocked。

## 下一阶段

M20d 完成后，M20 owned-dyn runtime 前置门禁链已经覆盖：

- M20a admission order。
- M20b compiler-owned two-field owned dyn handle prototype。
- M20c drop / allocator identity prerequisites。
- M20d runtime lowering ABI descriptor、backend helper prerequisite 和 blocked transition guard。

下一阶段可以进入标准库 / owning dyn runtime surface 的设计或实现入口，但标准库阶段必须显式引用 M20a-M20d facts，
并且不能绕过当前 blocker matrix。标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI
lowering、backend helper call 和 dynamic Drop runtime 仍不属于 M20d。

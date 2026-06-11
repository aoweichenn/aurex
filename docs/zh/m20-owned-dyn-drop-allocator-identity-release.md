# Aurex M20c Drop / Allocator Identity Prerequisite Gate Release Baseline

M20c 已完成 drop / allocator identity prerequisite gate。M20c 仍然不实现标准库、不实现
`Box<dyn Trait>`、不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop runtime，
不做 backend runtime helper call，也不做 runtime ABI lowering。M20c 的目标是把 M20b 已固定的
compiler-owned owned dyn handle prototype 继续推进到 drop identity 和 allocator identity 的 compiler-owned
稳定事实层，让后续 M20d 可以设计 runtime lowering ABI，而不是让标准库或 runtime 在本阶段提前进入。

## 本阶段新增内容

M20c 新增或固定：

- `OwnedDynDropAllocatorIdentityGate`、`OwnedDynDropAllocatorIdentityFact` 和
  `OwnedDynDropAllocatorIdentitySummary`。
- `OwnedDynDropAllocatorIdentityFactKind`、`OwnedDynDropAllocatorIdentityStage` 和
  `OwnedDynDropAllocatorIdentityPolicy`。
- `m20c_owned_dyn_drop_allocator_identity_gate_baseline()`，以及
  `owned_dyn_drop_allocator_identity_gate_fingerprint()`、summary、dump 和 validation。
- `ir::owned_dyn_drop_allocator_identity_gate(const Module&)`，把真实 IR module 中的 owned dyn prototype
  投影成 M20c identity facts。
- `OwnedDynObjectLayoutPrototype::erased_drop_identity_key` 和
  `OwnedDynObjectLayoutPrototype::allocator_identity_key`。
- `OwnedDynDropAllocatorIdentityFact::prototype_identity_set_key`，用于让 M20c query fingerprint 覆盖整个
  module 的 owned dyn prototype identity 集合；它是查询事实指纹，不是 runtime ABI 字段。
- IR dump 和 `layout_abi_fingerprint()` 混入 drop / allocator identity key。
- IR verifier 拒绝空 drop identity key、空 allocator identity key、drop/allocator key 相同、重复 drop key
  或重复 allocator key。
- M20b IR shape adapter 也把这两个 identity key 纳入 identity validity，避免后续 M20c 看到不可靠 shape gate。

## 身份模型

M20c 的 identity 是 compiler-owned prerequisite，不是 runtime slot，也不是标准库 API：

```text
owned_dyn_object_layout_prototype {
  data_field         = 0 : *mut u8
  vtable_field       = 1 : *const u8
  drop_slot          = blocked
  allocator_slot     = blocked
  drop_identity      = StableFingerprint128
  allocator_identity = StableFingerprint128
}
```

关键约束：

1. drop identity key 必须非零。
2. allocator identity key 必须非零。
3. 同一个 prototype 的 drop identity key 和 allocator identity key 不能相同。
4. 一个 IR module 中不能出现重复 drop identity key 或重复 allocator identity key。
5. erased drop runtime slot 和 allocator runtime slot 仍必须保持
   `IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED`。
6. M20c query gate 必须用稳定排序后的 `prototype_identity_set_key` 覆盖整个 prototype 集合，不能只依赖
   第一个 prototype 的 identity。
7. borrowed dyn ABI 仍保持 destructor-free；M20c 不给 borrowed vtable 增加 destructor slot。
8. standard-library、`Box<dyn Trait>`、owning dyn user value、allocator API、runtime lowering、dynamic Drop
   runtime 和 backend helper blocker 必须全部保持 true。

## Query Gate

`OwnedDynDropAllocatorIdentityGate` 将 M20c 拆成五类 facts：

- `owned_dyn_erased_drop_identity_prerequisite_fact`：compiler-owned erased drop identity 已可见。
- `owned_dyn_allocator_identity_prerequisite_fact`：compiler-owned allocator identity 已可见。
- `owned_dyn_cleanup_dropck_bridge_identity_fact`：cleanup/dropck 仍通过 static / marker facts 连接 identity，
  不进入 runtime lowering。
- `owned_dyn_handle_identity_binding_fact`：drop identity 和 allocator identity 绑定到 M20b owned handle prototype。
- `owned_dyn_drop_allocator_runtime_lowering_blocker_fact`：runtime lowering、dynamic Drop runtime 和 backend helper
  仍被阻断。

Baseline validation 要求这五类 facts 恰好各出现一次，全部引用有效的 M20b IR shape prototype gate fingerprint，
并且全部保持 compiler-owned、borrowed ABI unchanged、standard-library blocked、`Box<dyn Trait>` blocked、
owning user value blocked、allocator API blocked、runtime lowering blocked、dynamic Drop runtime blocked 和
backend helper blocked。

## Negative Matrix

M20c validation / verifier 会拒绝下列漂移：

- drop identity key 为空。
- allocator identity key 为空。
- drop identity key 与 allocator identity key 相同。
- prototype identity set key 为空或 facts 之间不一致。
- IR module 中重复 drop identity key 或重复 allocator identity key。
- erased drop runtime slot 或 allocator runtime slot 不再是 blocked sentinel。
- 任一 stdlib/runtime blocker 被改成 false。
- query fact 缺失 M20b 引用、缺失 verifier guard、打开 executable runtime，或 summary/fingerprint 与 facts 漂移。
- M20b IR shape gate fingerprint 漂移。

## 明确非目标

M20c 不实现：

- 标准库。
- 不实现 `Box<dyn Trait>` surface 或 runtime。
- allocator API。
- owning dyn 用户值、move/drop 用户语义或 owning trait-object coercion。
- erased drop glue runtime。
- dynamic Drop dispatch runtime。
- runtime ABI lowering。
- backend runtime helper call。

M20c 只说明“编译器内部已经有可验证、可 dump、可 fingerprint、可 query 的 drop / allocator identity
prerequisite”。它不把这些 identity 暴露为 Aurex 源码语法，也不让用户构造 owning dyn value。

## 测试覆盖

M20c 新增 focused tests 覆盖：

- Query enum name / invalid fallback。
- M20c baseline 的 summary、dump、fingerprint 和 validation。
- identity facts 的漂移拒绝矩阵。
- valid prototype 的 verifier、dump 和 layout ABI fingerprint。
- verifier 对空 identity key、相同 identity key、重复 identity key、blocked sentinel 漂移的负例。
- IR adapter 对 valid module、multiple prototype module、empty module 和 prototype drift 的 query gate 投影。
- M20b shape adapter 对新增 identity key 漂移的拒绝。

测试文件：

- `tests/gtest/infrastructure/query/owned_dyn_drop_allocator_identity_gate_tests.cpp`
- `tests/gtest/midend/ir/owned_dyn_drop_allocator_identity_tests.cpp`
- `tests/gtest/midend/ir/owned_dyn_ir_shape_prototype_tests.cpp`

## 代码量预估和实际偏差说明

M20c 原预估新增/修改代码量是 1,200-2,200 行。实际代码量以提交时
`git diff --cached --shortstat` 为准。

如果实际高于预估，主要原因应是：

- M20c 同时新增 query DTO、baseline、summary、dump、fingerprint、validation 和 IR module adapter。
- 为了让 identity 真正可验证，IR 层必须接入 dump、layout ABI fingerprint、verifier identity negative matrix，
  并回补 M20b adapter 对新增 identity key 的漂移拒绝。
- 测试拆成 focused query tests 和 focused IR tests；这增加行数，但能保持测试索引质量。
- 文档侧同步 release 文档、入口文档、语言手册、特性清单、next-steps 和 documentation tests，避免把 M20c
  误读成标准库或 runtime surface 已打开。

该偏差不来自实现标准库、`Box<dyn Trait>`、allocator API、runtime ABI lowering、backend runtime helper 或
dynamic Drop runtime；这些仍全部保持 blocked。

## 下一阶段

M20c 完成后，下一阶段应进入 **M20d Runtime Lowering ABI Design Closure**。M20d 仍不应该实现标准库；它应先把
runtime lowering ABI descriptor、backend helper prerequisite、blocked-to-admitted transition checks 和 verifier
negative matrix 固定下来，并继续阻塞 `Box<dyn Trait>`、allocator API、standard-library surface 和 dynamic Drop
runtime 的可执行实现。

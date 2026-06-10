# Aurex M20b Owned Dyn IR Shape Prototype Gate Release Baseline

M20b 已完成 owned dyn IR shape prototype gate。M20b 仍然不实现标准库、不实现 `Box<dyn Trait>`、
不实现 allocator API、不实现 owning dyn 用户值、不生成 dynamic Drop runtime，
不做 backend runtime helper call，也不做 runtime ABI lowering。M20b 的目标是把 M20a admission gate
要求的 owned object layout prerequisite 落成 compiler-owned IR 形状、query facts、dump/fingerprint 和 verifier
不变量。

## 本阶段新增内容

M20b 新增或固定：

- `OwnedDynObjectLayoutPrototype`，保存在 `ir::Module::owned_dyn_object_layout_prototypes`。
- `OwnedDynObjectLayoutPrototypePolicy::compiler_owned_handle_metadata_v1`。
- owned dyn handle 的 compiler-owned two-field shape：field 0 是 `*mut u8` erased payload pointer，field 1 是
  `*const u8` borrowed vtable pointer。
- `IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED`，用于把 erased drop identity slot 和 allocator identity slot 明确保持为
  blocked placeholder。
- IR dump 输出 `owned_dyn_object_layout_prototype`，并显示 object key、policy、data/vtable field、blocked drop /
  allocator slot，以及 stdlib/runtime blocker 状态。
- `layout_abi_fingerprint()` 混入 owned dyn object layout prototype，任何 handle shape、runtime slot 或 blocker 漂移
  都会改变 layout ABI fingerprint。
- IR verifier 对 owned dyn object layout prototype 的 identity、type、pointer field、two-field shape 和 blocker matrix
  做硬校验。
- `OwnedDynIrShapePrototypeGate`、`OwnedDynIrShapePrototypeFact`、`OwnedDynIrShapePrototypeSummary`、
  `OwnedDynIrShapePrototypeFactKind`、`OwnedDynIrShapePrototypeStage` 和
  `OwnedDynIrShapePrototypePolicy`。
- `m20b_owned_dyn_ir_shape_prototype_gate_baseline()`，以及
  `owned_dyn_ir_shape_prototype_gate_fingerprint()`、summary、dump 和 validation。
- `ir::owned_dyn_ir_shape_prototype_gate(const Module&)`，把真实 IR module 中的 compiler-owned prototype 投影成
  query-visible M20b facts。

## IR 形状不变量

M20b 的 owned dyn handle 只是一条编译器内部 IR 原型记录，不是用户值，也不是标准库容器：

```text
owned_dyn_object_layout_prototype {
  data_field   = 0 : *mut u8
  vtable_field = 1 : *const u8
  drop_slot    = blocked
  allocator_slot = blocked
}
```

关键约束：

1. `object_type` 必须是 single-trait dyn object type，且 `object_type_key` 必须与 type table 中的
   `trait_object_key` 一致。
2. data pointer 必须是 `*mut u8`。这只是 erased payload pointer identity，不代表 allocator 或 ownership API 已存在。
3. vtable pointer 必须是 `*const u8`，并继续复用 borrowed dyn ABI；borrowed vtable 仍保持 destructor-free。
4. handle field count 必须是 2，data/vtable field index 必须分别是 0/1。
5. erased drop runtime slot 和 allocator runtime slot 必须保持 `IR_OWNED_DYN_OBJECT_RUNTIME_SLOT_BLOCKED`。
6. `standard_library_blocked`、`box_surface_blocked`、`owning_dyn_user_value_blocked`、
   `allocator_api_blocked`、`runtime_lowering_blocked`、`dynamic_drop_runtime_blocked` 和
   `backend_helper_blocked` 必须全部为 true。

## Query Gate

`OwnedDynIrShapePrototypeGate` 将 M20b 拆成六类 facts：

- `owned_dyn_handle_metadata_ir_shape_fact`：owned handle two-field metadata 已在 IR 中可见。
- `owned_dyn_erased_payload_pointer_ir_shape_fact`：erased payload pointer 是 `*mut u8`。
- `owned_dyn_vtable_pointer_ir_shape_fact`：vtable pointer 继续是 borrowed ABI 的 `*const u8`。
- `owned_dyn_drop_identity_placeholder_ir_shape_fact`：drop identity 仍是 placeholder，没有 lowering。
- `owned_dyn_allocator_identity_placeholder_ir_shape_fact`：allocator identity 仍是 placeholder，没有 API。
- `owned_dyn_runtime_lowering_blocker_ir_shape_fact`：runtime lowering 和 backend helper 仍被阻断。

Baseline validation 要求这六类 facts 恰好各出现一次，全部引用有效的 M20a admission gate fingerprint，并且全部保持
compiler-owned、borrowed ABI unchanged、standard-library blocked、`Box<dyn Trait>` blocked、owning user value
blocked、allocator API blocked、runtime lowering blocked、dynamic Drop runtime blocked 和 backend helper blocked。

## Negative Matrix

M20b validation / verifier 会拒绝下列漂移：

- prototype symbol 缺失、重复 symbol 或重复 trait object key。
- object type 不是 single-trait dyn object，或者 object key 与 type table 中的 trait object key 不一致。
- data pointer 不是 `*mut u8`。
- vtable pointer 不是 `*const u8`。
- handle field count 不是 2，或 data/vtable field index 不是 0/1。
- erased drop runtime slot 或 allocator runtime slot 不再是 blocked sentinel。
- 任一 stdlib/runtime blocker 被改成 false。
- query fact 缺失 M20a 引用、缺失 verifier guard、打开 executable runtime，或 summary/fingerprint 与 facts 漂移。

## 明确非目标

M20b 不实现：

- 标准库。
- `Box<dyn Trait>` surface 或 runtime。
- allocator API。
- owning dyn 用户值、move/drop 用户语义或 owning trait-object coercion。
- erased drop glue runtime。
- dynamic Drop dispatch runtime。
- runtime ABI lowering。
- backend runtime helper call。

M20b 只说明“编译器内部已经有一个可验证、可 dump、可 fingerprint、可 query 的 owned dyn IR 形状原型”。它不把该原型暴露为
Aurex 源码语法，也不让用户构造 owning dyn value。

## 测试覆盖

M20b 新增 focused tests 覆盖：

- Query enum name / invalid fallback。
- M20b baseline 的 summary、dump、fingerprint 和 validation。
- shape facts 的漂移拒绝矩阵。
- IR policy name / invalid fallback。
- valid prototype 的 verifier、dump 和 layout ABI fingerprint。
- `Module` copy/move 对 prototype 的保留。
- verifier 对 object type、pointer type、two-field shape、blocked sentinel、blocker、duplicate identity 的负例。
- IR adapter 对 valid module、empty module 和 prototype drift 的 query gate 投影。

测试文件：

- `tests/gtest/infrastructure/query/owned_dyn_ir_shape_prototype_gate_tests.cpp`
- `tests/gtest/midend/ir/owned_dyn_ir_shape_prototype_tests.cpp`

## 代码量预估和实际偏差说明

M20b 原预估新增/修改代码量是 1,100-1,900 行。实际暂存统计为
`25 files changed, 2419 insertions(+), 32 deletions(-)`，高于预估。

偏差原因：

- 本阶段没有只加 IR record；同时补齐了 query DTO、baseline、summary、dump、fingerprint、validation 和
  IR module adapter。
- 为了让 prototype 真正可验证，IR 层还必须接入 Module copy/move/reserve、dump、layout ABI fingerprint 和 verifier
  identity/type/pointer/shape/blocker negative matrix。
- 测试没有塞回既有大文件，而是新增 focused query tests 和 focused IR tests；这增加了行数，但提升了索引和维护质量。
- 文档侧同步了 release 文档、入口文档、语言手册、特性清单、next-steps 和 documentation tests，避免把 M20b 误读成
  标准库或 runtime surface 已打开。

该偏差不来自实现标准库、`Box<dyn Trait>`、allocator API、runtime ABI lowering、backend runtime helper 或 dynamic Drop
runtime；这些仍全部保持 blocked。

## 下一阶段

M20b 完成后，下一阶段应进入 **M20c Drop / Allocator Identity Prerequisite Gate**。M20c 仍不应该实现标准库；它应先把
erased drop identity 和 allocator identity 做成 stable facts / verifier-visible prerequisites，并继续阻塞
`Box<dyn Trait>`、allocator API、runtime ABI lowering、backend helper 和 dynamic Drop runtime。

# Aurex M9 Dyn ABI / Tooling Release Baseline

日期：2026-06-08

状态：M9 release closure 已完成。M9a、M9b、M9c 已形成一个完整的 dyn ABI / tooling release baseline：现有
borrowed dyn runtime dispatch 被固定为可查询、可 fingerprint、可 dump、可投影的 ABI facts；advanced dyn 后续方向
被固定为 design gate，而不是在 M9 内继续扩张 runtime 或标准库。

## 0. Release 结论

M9 结束时，Aurex 的 dyn trait 状态是：

- 当前可用语言 surface 仍是 M8 已完成的 `&dyn Trait` / `&mut dyn Trait` borrowed dyn view，以及
  `dyn Trait[Assoc = Type]` associated equality dispatch。
- 当前唯一 ABI policy 是 `borrowed_view_v1`。
- 当前唯一 metadata policy 是 `borrowed_methods_only_v1`。
- `FunctionDynAbiFacts` 是 borrowed dyn ABI/tooling 的 release DTO。
- `DynAdvancedDesignGate` 是 advanced dyn 后续方向的 release gate DTO。
- M9 不新增标准库、不新增 owning dyn runtime、不新增 dynamic Drop dispatch runtime、不新增 supertrait upcasting
  runtime、不新增多 trait object composition runtime。

M9 的核心价值不是“多做一个 dyn feature”，而是把已经能运行的 borrowed dyn dispatch 从 backend/IR 私有细节提升为
稳定的工程事实层。后续 IDE、query cache、driver invalidation、debug dump、设计评审和 ABI migration 都应该消费
这些 facts，而不是扫描 IR dump 或推测 LLVM lowering 文本。

## 1. M9a 到 M9d 收口

| 阶段 | release 状态 | 产物 |
| --- | --- | --- |
| M9a design baseline | 已完成 | `m9-dyn-abi-tooling-design.md` 固定 facts-first dyn ABI DTO、metadata/fingerprint schema、tooling/query projection、cross-module invalidation 和 verifier/backend negative matrix。 |
| M9b implementation baseline | 已完成 | `FunctionDynAbiFacts`、borrowed view object/vtable/slot/coercion/dispatch descriptors、validation、stable fingerprint、summary、dump、checked/IR/tooling projection 和 invalidation tests。 |
| M9c advanced dyn design gate | 已完成 | `DynAdvancedDesignGate`、五个 advanced dyn candidate、policy separation validation、stable fingerprint、summary、dump 和 focused query tests。 |
| M9d release closure | 已完成 | 本 release baseline、progress/version/next-steps/README 状态更新、documentation tests 防漂移、release gate 验证。 |

## 2. 当前能做什么

M9 release 之后，仓库可以稳定表达和消费以下事实：

- `&T -> &dyn Trait` / `&mut T -> &mut dyn Trait` borrowed coercion 的 ABI descriptor。
- `VTableLayoutKey` 对应的 borrowed methods-only vtable layout descriptor。
- vtable method slot ordinal、requirement ordinal、method name、function symbol、function type、receiver type 和 return type。
- dyn coercion 的 source reference、target reference、source type、object type 和 borrow kind。
- dyn dispatch 的 layout、slot、method name、function symbol、function type 和 object type。
- `lower_function_ir` query result fingerprint 对 cleanup facts 与 dyn ABI facts 的变化敏感。
- IDE/tooling 可以展示 `abi=borrowed_view_v1`、`metadata=borrowed_methods_only_v1` 和
  `dispatch=vtable_slot slot=N`。
- release 文档可以说明 advanced dyn 为什么被 gate 阻塞，而不是误称它们已经可运行。

这些能力全部是 compiler/tooling/query 层的稳定事实消费能力。它们不要求标准库，也不创建 owning container。

## 3. 当前不会新增什么

M9 release 之后，仓库仍明确不会新增：

- 新用户语法。
- 新标准库 API。
- `Box<dyn Trait>`。
- owning dyn / owning existential container runtime。
- allocator / placement / heap object policy runtime。
- dynamic Drop dispatch runtime。
- dyn destructor ABI。
- consuming receiver 的 dyn dispatch。
- 不新增 supertrait upcasting runtime。
- 多 principal trait object composition runtime。
- auto trait / marker trait composition runtime。
- structural dyn conformance。
- 对 M8 `&dyn Trait` / `&mut dyn Trait` 语义的改变。

这些是 release contract，不是临时遗漏。任何后续阶段如果要改变其中一条，必须独立设计、独立估算、独立测试，并明确
ABI/metadata policy migration。

## 4. Release Fact Surface

### 4.1 `FunctionDynAbiFacts`

`FunctionDynAbiFacts` 是 M9b 的 release DTO。它的稳定职责是把 borrowed dyn ABI 投影为结构化 facts：

- `DynObjectAbiDescriptor`：trait object key、ABI policy、object type display name 和 principal trait display name。
- `DynVTableAbiDescriptor`：vtable layout key、ABI policy、metadata policy、symbol、concrete type、object type 和 slots。
- `DynVTableSlotAbiDescriptor`：slot ordinal、requirement ordinal、method name、function symbol、function type、receiver type 和 return type。
- `DynCoercionAbiDescriptor`：coercion key、layout key、borrow kind、source/target reference display names 和 object display name。
- `DynDispatchAbiDescriptor`：layout key、slot、method name、function symbol、function type 和 object type。

Release invariants：

- 只承认 `DynAbiPolicy::borrowed_view_v1`。
- 只承认 `DynMetadataPolicy::borrowed_methods_only_v1`。
- vtable slot 数量必须匹配 `VTableLayoutKey::method_slot_count`。
- slot ordinal 必须严格递增、在范围内且能映射到 checked method slot。
- coercion fact 的 borrow kind 必须匹配 `TraitObjectCoercionKey`。
- dump/summary 只消费 DTO，不反推 IR dump 文本。

### 4.2 `DynAdvancedDesignGate`

`DynAdvancedDesignGate` 是 M9c 的 release DTO。它的稳定职责是把 advanced dyn 后续方向变成可测试的设计准入事实：

- `supertrait_upcasting`：`design_gate`，需要新 metadata policy `supertrait_vptr_metadata_v1`。
- `owning_dyn`：`prototype_blocked`，需要后续标准库阶段和新 `owning_dyn_container_v1` /
  `owning_dyn_metadata_v1` policy。
- `dynamic_drop_dispatch`：`prototype_blocked`，需要后续 runtime 阶段和新 `dynamic_drop_metadata_v1` policy。
- `allocator_policy`：`prototype_blocked`，需要后续标准库阶段和新 `allocator_placement_policy_v1` /
  `allocator_metadata_v1` policy。
- `multi_trait_composition`：`design_gate`，需要新 metadata policy `multi_trait_metadata_v1`。

Release invariants：

- advanced candidate 不能把 `borrowed_view_v1` 当作 required ABI policy。
- advanced candidate 不能把 `borrowed_methods_only_v1` 当作 required metadata policy。
- `ready_for_future_stage` 在 M9 release 中不是可实现状态。
- 每个 candidate 必须有 blockers、required facts、non-goals 和 `standard_library_runtime_not_in_m9c`。
- standard-library-blocked candidate 必须留到独立标准库阶段。
- runtime-blocked candidate 必须留到独立 runtime/ABI 阶段。

## 5. Consistency Audit

M9d release closure 对文档和代码事实做了如下审计：

- M9 文档不再把 “进入 M9c” 当作下一步；M9c 已经完成。
- `next-steps` 的当前最高优先级不再是 M9d，而是 M10 planning / post-M9 advanced dyn policy selection。
- `progress` 和 `version` 的顶部状态改为 M9 release closure。
- `README` 文档基线改为 M9 Dyn ABI / Tooling Release Closure。
- 文档测试固定 M9 release baseline 文件存在，并固定 `FunctionDynAbiFacts`、`DynAdvancedDesignGate`、
  `borrowed_view_v1`、`borrowed_methods_only_v1`、`standard_library_runtime_not_in_m9c` 和 “不实现标准库/runtime” 边界。
- 没有新增 sema、parser、IR lowering、LLVM backend runtime 或标准库代码。

## 6. 后续路线

M9 结束后，下一步不应该继续把功能塞进 M9。建议进入 **M10 planning / Post-M9 Advanced Dyn Policy Selection**：

- 先选择一个 advanced dyn 方向作为独立阶段，而不是并行实现全部。
- 如果选择 supertrait upcasting，先设计 `supertrait_vptr_metadata_v1`、upcast layout projection facts、origin-preserving
  coercion facts 和 diagnostics。
- 如果选择 dynamic Drop dispatch，先设计 destructor metadata、drop glue runtime policy、cleanup ABI migration 和 verifier
  negative matrix。
- 如果选择 owning dyn 或 allocator policy，必须先开标准库/资源容器阶段，设计 owner container、allocation、move/destroy
  和 resource transfer policy。
- 如果选择 multi trait composition，先设计 principal set identity、method namespace merge 和 associated equality merge
  schema。

M10 之前不应实现标准库，也不应把 runtime feature 藏进 `borrowed_view_v1` / `borrowed_methods_only_v1`。

## 7. Release Gate

M9 release closure 的验证要求：

- `cmake --build build/coverage --target aurex_query_tests aurex_tests -j2`
- `./build/coverage/bin/aurex_tests --gtest_color=auto`
- `AUREX_COVERAGE_FOCUSED_SOURCES='src/infrastructure/query/dyn_advanced_design_gate.cpp:src/infrastructure/query/dyn_abi_facts.cpp' tools/check_coverage.sh -j2`
- `git diff --check`

覆盖率门槛保持 90% 以上。M9 release closure 是文档和一致性收口，预期不新增 production C++ runtime 代码。

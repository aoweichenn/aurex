# Aurex M12 Direct Composition Dispatch Release Baseline

日期：2026-06-09
状态：M12a/M12b 已完成

## 目标

M12 的目标是把 M11 已经落地的 borrowed principal-set dyn composition 从“必须先显式
projection”推进到可直接调用无歧义 principal method，并把这条路径收口到 release quality。

M12 仍是 compiler/runtime-core 阶段，不是标准库阶段，也不是 owning dyn 阶段。本阶段不实现：

- 标准库。
- owning dyn / owning existential container。
- `Box<dyn Trait>`。
- allocator API 或 allocator policy。
- trait-object Drop dispatch 或 dynamic Drop dispatch。
- bare `dyn A + B` parser syntax。
- composition-to-supertrait 隐式多步 direct dispatch。
- auto trait / marker trait composition。

## 已完成能力

M12a 打开 direct composition dispatch 的最小稳定子集：

```aurex
fn score(view: &dyn (Draw + Debug)) -> i32 {
    return view.draw() + view.debug();
}
```

`view.draw()` 的语义是：

1. 在 principal-set method namespace 中查找 `draw`。
2. 如果恰好一个 principal 提供该 method，则选择该 principal。
3. 记录 composition-to-principal projection fact。
4. 在 IR 中生成 `trait_object_composition_project`。
5. 复用普通 single-trait dyn `vtable_slot` dispatch。

这不是 composition-wide slot table，也不是把多个 principal 的 method slots flatten 到一个无名 namespace。

M12b 完成 hardening / release closure：

- Direct composition dispatch 的 checked binding 会保留表面 `receiver_type`，并用投影后的
  `dispatch_receiver_type` 计算 receiver access，避免 tooling/borrow facts 把 `&dyn (A+B)` 误当成实际
  vtable receiver。
- Associated equality direct dispatch 已覆盖：`dyn (Source[Item = i32] + Debug)` 上的 `view.item()` 会使用
  selected principal 的 associated equality substitution，return type 保持 `i32`。
- Direct dispatch 与显式 projection 混用时，principal projection fact 按 projection path 去重；lowered
  `FunctionDynAbiFacts::composition_projections` 也按 principal/object/source/target 去重，避免重复 ABI descriptor。
- Query fingerprint 已固定：projection borrow kind、target principal、target vtable layout 和 target reference
  type 改变时，`function_dyn_abi_facts_fingerprint()` 和 `lower_function_ir_result_fingerprint()` 都会变化。
- Negative matrix 已固定：ambiguous method、missing method、shared receiver 调 mutable method、以及
  composition-to-supertrait 隐式 direct call 均被拒绝。

## 运行时和 ABI

M12 继续复用 M11d/M11e 的 runtime representation：

- concrete `&T -> &dyn (A + B)` lower 为 `trait_object_composition_pack`。
- composition view 仍是 borrowed `{data*, principal_set_metadata*}`。
- `principal_set_metadata_v1` LLVM global shape 仍是 `{ [N x ptr] }`。
- 每个 metadata entry 指向 canonical principal order 上的 single-trait vtable witness。
- direct dispatch 先 lower 为 `trait_object_composition_project`，得到普通 `{data*, vtable*}` single dyn view。
- 后续 method call 继续走已有 `vtable_slot` indirect call。

因此 M12 没有新增 allocator、owner、drop、size、align 或 destructor metadata。

## Diagnostics

M12 固定的主要诊断边界：

- 多个 principal 同名 method：
  `dyn trait composition method `name` is ambiguous across multiple principal traits`
- principal set 中没有该 method：
  `has no visible impl for trait method `name``
- shared composition receiver 调用 `&mut Self` method：
  `mutable method receiver requires mutable pointer`
- inherited supertrait method 不做 composition-to-supertrait 隐式 direct dispatch；当前仍应先显式投影或等待后续
  borrowed-only 设计。

## 测试收口

M12 release baseline 覆盖：

- frontend/sema shared direct dispatch。
- frontend/sema mutable direct dispatch。
- associated equality direct dispatch。
- direct dispatch + explicit projection 混用去重。
- ambiguous/missing/mutable receiver/supertrait-direct negative cases。
- IR composition project + vtable slot lowering。
- FunctionDynAbiFacts composition projection descriptor 去重。
- dyn ABI fingerprint 和 lower-IR result fingerprint invalidation。
- IDE hover/query facts。
- LLVM principal vtable load。
- native execution 多 concrete direct dispatch。
- documentation tests。

## 后续

M12 已结束。后续如果继续推进 dyn，下一步应进入 **M13 Advanced Dyn Remaining Policy Design Baseline**，先做
剩余 dyn 能力的 policy selection 和 design gate，而不是直接实现标准库、owning dyn、allocator 或 dynamic
Drop dispatch。

优先候选应继续保持独立：

- borrowed composition-to-supertrait explicit/direct policy。
- dynamic Drop dispatch runtime design。
- owning dyn / `Box<dyn Trait>` 与标准库/allocator 阶段。
- auto trait / marker trait composition。


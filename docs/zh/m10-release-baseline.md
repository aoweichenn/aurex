# Aurex M10 Supertrait Upcasting Release Baseline

日期：2026-06-08

状态：M10a、M10b、M10c 和 M10d 已完成。M10 的 release baseline 是 borrowed dyn supertrait upcasting，不是标准库阶段，
也不是 owning dyn 阶段。

## 1. Release 结论

M10 在 M8 borrowed dyn runtime dispatch 和 M9 dyn ABI/tooling facts 的基础上，完成了 Aurex 自己的
**origin-bound erased view supertrait upcast**：

- `trait Child: Parent` 进入 parser/AST、checked supertrait graph、checked dump 和 stable fingerprint。
- `impl Child for T` 会要求可见的 direct parent impl evidence；泛型 parent args 会按 child/impl 实参替换。
- `&dyn Child -> &dyn Parent`、`&mut dyn Child -> &mut dyn Parent` 和 `&mut dyn Child -> &dyn Parent`
  是 contextual coercion，可以记录 checked `TraitObjectUpcastCoercionFact`。
- `&dyn Child -> &mut dyn Parent` 被拒绝；非 supertrait target 和泛型 parent args mismatch 也被拒绝。
- `dyn Child` receiver 上 inherited parent method call 会先投影 parent borrowed view，再按 parent vtable slot dispatch。
- IR 新增 `trait_object_upcast` value；`TraitObjectVTableLayout` 新增 `supertrait_edges`。
- LLVM backend 使用 `supertrait_vptr_metadata_v1` vtable metadata，vtable global shape 为
  `{ [methods x ptr], [supertraits x ptr] }`。
- Tooling/query 侧通过 `FunctionDynAbiFacts::upcasts`、`DynUpcastAbiDescriptor`、summary、dump、fingerprint、
  lower-IR query invalidation 和 IDE hover 暴露 upcast facts。

一句话：M10 把 borrowed dyn-to-dyn supertrait view 做成了可解析、可检查、可 lowering、可验证、可执行、可查询、
可缓存和可投影的完整 compiler/runtime 能力。

## 2. 当前能做什么

当前可以写：

```aurex
trait Parent {
    fn parent(self: &Self) -> i32;
}

trait Child: Parent {
    fn child(self: &Self) -> i32;
}

struct File {
    value: i32;
}

impl Parent for File {
    fn parent(self: &File) -> i32 {
        return self.value;
    }
}

impl Child for File {
    fn child(self: &File) -> i32 {
        return self.value + 1;
    }
}

fn read_parent(value: &dyn Parent) -> i32 {
    return value.parent();
}

fn score(value: &dyn Child) -> i32 {
    return read_parent(value) + value.parent() + value.child();
}
```

`read_parent(value)` 中的 `&dyn Child -> &dyn Parent` 是 coercion site。`value.parent()` 是 inherited parent
method dispatch：lowering 会生成 `trait_object_upcast`，从 child vtable 的 supertrait table 投影 parent vtable，
然后按 parent slot 加载函数指针。

## 3. 借用权和所有权边界

M10 的 upcast 不改变 Aurex 的 M7/M8 借用模型：

- upcast 不创建 ownership。
- upcast 不复制对象。
- upcast 不创建或延迟 cleanup obligation。
- upcast 不延长 origin，也不允许 target reference 比 source reference 活得更久。
- upcast 不放宽 active loan，不改变 local loan conflict matrix。
- shared source 不能升级成 mutable target。
- `&mut dyn Child -> &dyn Parent` 降级成 shared target view，不保留 mutable target 权限。

这些规则来自 Aurex 的 origin-bound erased view，而不是 Rust/Swift/Go/C++ 任一对象模型的直接复制。

## 4. 非目标

M10 明确不实现：

- 标准库。
- `Box<dyn Trait>`。
- owning dyn / owning existential container。
- allocator policy。
- dynamic Drop dispatch。
- trait-object destructor ABI。
- Drop/size/align/type metadata。
- multi trait composition。
- `dyn A + B`。
- auto trait / marker trait composition。
- structural conformance。
- consuming receiver dyn dispatch。

这些不是永久禁止，而是后续阶段需要独立设计、估算和测试的能力。M10 只收口 borrowed supertrait view。

## 5. Hardening 结果

M10d 的 hardening/release closure 已完成：

- Query/cache/tooling polish：`dyn_abi_facts_has_surface()` 会把 `upcasts` 计入 dyn ABI surface；IDE function hover
  展示 `metadata=supertrait_vptr_metadata_v1`、`upcasts=N`、首条 upcast source/target、borrow kind 和 metadata；
  `summarize_function_dyn_abi_facts()` 在 upcast-only 场景下展示 `first_upcast`。
- Fingerprint/invalidation：`FunctionDynAbiFacts` 已混入 upcast key、source/target object、edge fingerprint、
  borrow kind、metadata policy 和 display names；focused query tests 固定 edge/borrow 变化会改变 lower-IR result。
- Negative sample matrix：常规 negative sample suite 已覆盖非 supertrait target、shared-to-mut borrow upgrade
  和 missing parent evidence；IR/verifier focused tests 覆盖 layout/edge mismatch。
- Tooling sample coverage：IDE snapshot tests 覆盖 `widen(child: &dyn Child) -> &dyn Parent` 的 dyn ABI semantic fact
  和 hover projection。
- Documentation closure：README、progress、version、next-steps、requirements、architecture、usage、language manual、
  language feature inventory 和 documentation tests 已统一到 M10d release baseline。

Associated equality edge mapping / ambiguity solver 仍是后续项。当前稳定覆盖的是 generic parent args substitution；
文档不会把 associated equality edge ambiguity 写成已实现诊断。

## 6. M10c 代码量偏差

M10c 原预估为 1,600-2,800 行新增/修改。实际 diffstat 为：

```text
37 files changed, 1316 insertions(+), 255 deletions(-)
```

实际低于预估，原因是：

- M10b 已提前完成 `TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionFact`、`TraitObjectUpcastCoercionKey` 和
  `DynUpcastAbiDescriptor`，M10c 不需要重新铺 checked/query DTO。
- M8/M9 已有 borrowed dyn `{data*, vtable*}` ABI、vtable layout、`FunctionDynAbiFacts` 和 lower-IR query/cache
  投影，M10c 主要是新增 parent vtable projection，不是重做 dyn runtime。
- LLVM lowering 复用了现有 fat-view pack/extract 和 indirect call 路径，只新增 supertrait vptr metadata shape 和
  upcast projection。
- 标准库、owning dyn、allocator、dynamic Drop dispatch、Drop metadata 和 multi trait composition 都明确排除在
  M10c/M10d 外，没有被提前实现。

这个偏差是范围控制带来的正向偏差，不是功能漏做。

## 7. 下一步

M10 已结束。下一步建议进入 **M11 Advanced Dyn Design Baseline**，先做设计和小范围 query gate，而不是直接实现
标准库或 owning dyn。

M11 的候选主线应从 M9c gate 中剩余的 advanced dyn 能力选择：

- multi trait composition：需要新的 metadata/composition policy，不能复用 `borrowed_methods_only_v1`。
- dynamic Drop dispatch：需要 runtime/destructor ABI 设计，不应混入 M10 supertrait metadata。
- owning dyn / `Box<dyn Trait>` / allocator policy：需要标准库或 runtime ownership policy 的独立阶段。

M11 预计代码量：

| 阶段 | 内容 | 预计新增/修改代码量 |
| --- | --- | ---: |
| M11a design baseline | advanced dyn 后续选择、policy/schema 设计、非目标和测试计划 | 600-1,000 行文档/测试 |
| M11b query/prototype gate | 新候选 DTO、validation、fingerprint、summary/dump、documentation tests | 700-1,300 行 |
| M11c implementation package | 取决于最终选择；multi trait composition 或 Drop dispatch 都需要独立估算 | 待 M11a 后估算 |

# 介绍文档

Aurex 是一个系统语言编译器项目。当前实现基线是 **M19 Dyn Ownership Runtime IR / Verifier Preparation**。M2 已冻结标准库并把工程重心拉回语言核心；M3 已收口模块、泛型、query-backed sema、tooling、incremental syntax 和 backend reuse；M4 新增 nominal static trait、显式 trait impl、generic trait predicate、静态 trait method dispatch、associated type，以及 tooling/diagnostics 投影；M5 已在这条 static trait 模型上把 default method bodies 收口为 release baseline；M6 已完成资源、值生命周期、cleanup 和 drop-glue 基线；M7 加入 CFG-sensitive borrow facts、borrow summary、lifetime/dropck、place-state 和 RAII/drop lowering；M8 已完成 `&dyn Trait` / `&mut dyn Trait` borrowed erased view、checked vtable facts、IR/backend vtable dispatch 和 hardening 收口；M9 已完成 dyn ABI/tooling facts；M10 已完成 direct supertrait declaration、checked supertrait graph、borrowed dyn-to-dyn upcast facts、`trait_object_upcast` IR、`supertrait_vptr_metadata_v1` vtable metadata、LLVM parent vtable projection 和 inherited supertrait dispatch runtime；M11 已完成 principal-set borrowed dyn composition 的 design/query/frontend/sema、显式 composition-to-principal runtime projection 以及 facts/query/tooling/verifier release closure；M12 已完成唯一 principal method 的 direct composition dispatch 及 release hardening；M13a/M13b 已选择并落成 `dynproject[SourcePrincipal, TargetSupertrait](view)` frontend/query/sema；M13c 已把该显式 borrowed composition-to-supertrait projection lowering 为 `trait_object_composition_project` + `trait_object_upcast` runtime，复用 `principal_set_metadata_v1` 和 `supertrait_vptr_metadata_v1`；M13d 已新增 `FunctionDynAbiFacts::composition_supertrait_chains`、query/cache/tooling hover 和 verifier negative matrix，完成 M13 release closure；M14 已支持唯一 source-principal path 下的 `let parent: &dyn Parent = view;` 和 `view.parent()`，并记录 `BorrowedDynViewPathFact`；M15 已固定 advanced dyn ownership/runtime boundary 和 const generic boundary 的 query design gates；M16 已打开用户可写 typed scalar const generic check-only 子集，支持 `const N: usize`、mixed generic args、`GenericInstanceKey::const_args` 和 `[N]T`；M17 已固定 `DynOwnershipRuntimeFacts`，把 future owning dyn、erased drop glue、allocator 和 cleanup/dropck runtime boundary 变成 compiler/query/tooling facts；M18 已新增 `DynOwnershipRuntimeBoundaryGate` 和 `dyn_ownership_runtime_boundary_gate` project-level query，把 M17 facts 接入 query/cache/tooling/reuse/workspace index，并固定 future IR/verifier/runtime lowering prerequisites；M19 已新增 `DynOwnershipRuntimeIrVerifierFact`、function-level IR collector、borrowed vtable destructor-free verifier guard 和 dynamic erased drop blocked sentinel。

M1 阶段已经舍弃。它把标准库、host support、构建工具样例、自举实验和语言核心同时推进，导致基础语法与类型规则没有形成稳定基线。M2 不再沿着 M1 修补，而是先把语言地基重新做稳。

本阶段已经删除 `std/` 源树、std driver 查找/链接代码、std CLI 选项、std/M1/system 样例和 std 专项测试。需要运行时能力的语言测试应通过局部 `extern c` 声明表达，不再依赖 Aurex 标准库包装。

短期目标：

- 通过常规仓库测试、90% coverage gate、query/cache/profile gates、stress gates 和文档检查保持 M10d supertrait
  release baseline、M11/M12 advanced dyn release baseline、M13d composition-to-supertrait release baseline 和
  M14 borrowed view path release baseline 稳定。
- 保持 M7 borrow/lifetime/resource baseline 稳定；完整 Rust-style lifetime surface、raw pointer alias safe proof、
  indexed move-out 和 replace/take/swap 继续后移到独立阶段。
- 下一步进入 M20 标准库 / Owning Dyn Runtime Design Gate；M19 已把 dyn ownership runtime boundary
  固定为 IR/verifier 可见 facts 和负例矩阵。owning dyn、`Box<dyn Trait>`、dynamic Drop dispatch、标准库
  allocator、specialization、associated constants、default associated types 和 generic associated type 仍后续单独设计；
  M19 仍未实现任何标准库 API。

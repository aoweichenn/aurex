# 介绍文档

Aurex 是一个系统语言编译器项目。当前实现基线是 **M11e Principal-Set Composition Hardening / Release Closure**。M2 已冻结标准库并把工程重心拉回语言核心；M3 已收口模块、泛型、query-backed sema、tooling、incremental syntax 和 backend reuse；M4 新增 nominal static trait、显式 trait impl、generic trait predicate、静态 trait method dispatch、associated type，以及 tooling/diagnostics 投影；M5 已在这条 static trait 模型上把 default method bodies 收口为 release baseline；M6 已完成资源、值生命周期、cleanup 和 drop-glue 基线；M7 加入 CFG-sensitive borrow facts、borrow summary、lifetime/dropck、place-state 和 RAII/drop lowering；M8 已完成 `&dyn Trait` / `&mut dyn Trait` borrowed erased view、checked vtable facts、IR/backend vtable dispatch 和 hardening收口；M9 已完成 dyn ABI/tooling facts；M10 已完成 direct supertrait declaration、checked supertrait graph、borrowed dyn-to-dyn upcast facts、`trait_object_upcast` IR、`supertrait_vptr_metadata_v1` vtable metadata、LLVM parent vtable projection 和 inherited supertrait dispatch runtime；M11a 已选择 principal-set borrowed dyn composition 作为下一条 advanced dyn 主线并固定 `principal_set_metadata_v1` 设计/query gate；M11b 已落地 `PrincipalSetCompositionFacts` query prototype；M11c 已支持 `dyn (A + B)` borrowed composition annotation/coercion 的 frontend/sema 子集；M11d 已支持显式 composition-to-principal runtime projection；M11e 已完成 composition runtime facts/query/tooling/verifier hardening。

M1 阶段已经舍弃。它把标准库、host support、构建工具样例、自举实验和语言核心同时推进，导致基础语法与类型规则没有形成稳定基线。M2 不再沿着 M1 修补，而是先把语言地基重新做稳。

本阶段已经删除 `std/` 源树、std driver 查找/链接代码、std CLI 选项、std/M1/system 样例和 std 专项测试。需要运行时能力的语言测试应通过局部 `extern c` 声明表达，不再依赖 Aurex 标准库包装。

短期目标：

- 通过常规仓库测试、90% coverage gate、query/cache/profile gates、stress gates 和文档检查保持 M10d supertrait
  release baseline、M11 release baseline 和 M11e composition facts/tooling/verifier closure 稳定。
- 保持 M7 borrow/lifetime/resource baseline 稳定；完整 Rust-style lifetime surface、raw pointer alias safe proof、
  indexed move-out 和 replace/take/swap 继续后移到独立阶段。
- 下一步进入 M12 advanced dyn design baseline；owning dyn、`Box<dyn Trait>`、dynamic Drop dispatch、标准库
  allocator、specialization、associated constants、default associated types 和 generic associated type 后续单独设计。

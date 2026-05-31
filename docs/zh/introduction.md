# 介绍文档

Aurex 是一个系统语言编译器项目。当前实现基线是 **M6 资源、值生命周期与访问语义实现基线**。M2 已冻结标准库并把工程重心拉回语言核心；M3 已收口模块、泛型、query-backed sema、tooling、incremental syntax 和 backend reuse；M4 新增 nominal static trait、显式 trait impl、generic trait predicate、静态 trait method dispatch、associated type，以及 tooling/diagnostics 投影；M5 已在这条 static trait 模型上把 default method bodies 收口为 release baseline；M6-WP1 已完成资源和值生命周期专题的三轮设计审视，M6-WP2/WP3/WP4 已落地 resource classification、whole-local move analysis 和 cleanup lowering。

M1 阶段已经舍弃。它把标准库、host support、构建工具样例、自举实验和语言核心同时推进，导致基础语法与类型规则没有形成稳定基线。M2 不再沿着 M1 修补，而是先把语言地基重新做稳。

本阶段已经删除 `std/` 源树、std driver 查找/链接代码、std CLI 选项、std/M1/system 样例和 std 专项测试。需要运行时能力的语言测试应通过局部 `extern c` 声明表达，不再依赖 Aurex 标准库包装。

短期目标：

- 通过常规仓库测试、coverage、query/cache/profile gates 和文档检查保持 M4 static trait baseline 稳定。
- 通过同一套常规仓库测试、coverage、query/cache/profile gates、stress gates 和文档检查保持 M5 static
  default-method baseline 稳定。
- 按 M6 路线先推进 compiler-owned `Copy`、内部 `Discard` / `NeedsDrop`、whole-local move 和确定性 cleanup；
  完整 borrow checker、partial move 和 lifetime surface 后移到独立阶段。
- dynamic trait object、object safety、specialization、associated constants、default associated types 和 generic associated type 后续单独设计。

# 介绍文档

Aurex 是一个系统语言编译器项目。当前实现基线是 **M7a CFG-sensitive borrow facts、summary、query/tooling 与 diagnostics 收口基线**。M2 已冻结标准库并把工程重心拉回语言核心；M3 已收口模块、泛型、query-backed sema、tooling、incremental syntax 和 backend reuse；M4 新增 nominal static trait、显式 trait impl、generic trait predicate、静态 trait method dispatch、associated type，以及 tooling/diagnostics 投影；M5 已在这条 static trait 模型上把 default method bodies 收口为 release baseline；M6 已完成资源、值生命周期、cleanup 和 drop-glue 基线；M7a 在此基础上加入 `BodyFlowGraph`、local loan checker、`BorrowSummary`、projection-aware conflict matrix、borrow facts query fingerprint、IDE/LSP tooling projection 和多点 borrow diagnostics。

M1 阶段已经舍弃。它把标准库、host support、构建工具样例、自举实验和语言核心同时推进，导致基础语法与类型规则没有形成稳定基线。M2 不再沿着 M1 修补，而是先把语言地基重新做稳。

本阶段已经删除 `std/` 源树、std driver 查找/链接代码、std CLI 选项、std/M1/system 样例和 std 专项测试。需要运行时能力的语言测试应通过局部 `extern c` 声明表达，不再依赖 Aurex 标准库包装。

短期目标：

- 通过常规仓库测试、coverage、query/cache/profile gates 和文档检查保持 M4 static trait baseline 稳定。
- 通过同一套常规仓库测试、coverage、query/cache/profile gates、stress gates 和文档检查保持 M5 static
  default-method baseline 稳定。
- 保持 M7a 内部 borrow fact baseline 稳定；完整 Rust-style lifetime surface、raw pointer alias safe proof、
  partial move / replace / take / swap 和 user destructor syntax 继续后移到独立阶段。
- dynamic trait object、object safety、specialization、associated constants、default associated types 和 generic associated type 后续单独设计。

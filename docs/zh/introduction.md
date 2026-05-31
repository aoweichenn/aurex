# 介绍文档

Aurex 是一个系统语言编译器项目。当前文档基线是 **M4 trait/protocol release baseline**。M2 已冻结标准库并把工程重心拉回语言核心；M3 已收口模块、泛型、query-backed sema、tooling、incremental syntax 和 backend reuse；M4 新增 nominal static trait、显式 trait impl、generic trait predicate、静态 trait method dispatch、associated type，以及 tooling/diagnostics 投影。

M1 阶段已经舍弃。它把标准库、host support、构建工具样例、自举实验和语言核心同时推进，导致基础语法与类型规则没有形成稳定基线。M2 不再沿着 M1 修补，而是先把语言地基重新做稳。

本阶段已经删除 `std/` 源树、std driver 查找/链接代码、std CLI 选项、std/M1/system 样例和 std 专项测试。需要运行时能力的语言测试应通过局部 `extern c` 声明表达，不再依赖 Aurex 标准库包装。

短期目标：

- 通过常规仓库测试、coverage、query/cache/profile gates 和文档检查保持 M4 static trait baseline 稳定。
- M4 后工作必须作为独立设计流启动，不能通过零散修改重新打开 M4 trait contract。
- 资源语义不在 M4 中推进；ownership、borrow checker、move-out、partial move、drop order、destructor 和相关 capability 后续单独设计。
- dynamic trait object、object safety、default methods、specialization、associated constants 和 generic associated type 后续单独设计。

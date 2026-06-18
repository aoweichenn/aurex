# 当前进度

版本：0.1.9

当前开发线收敛到两块：

1. 泛型语法和实例化闭环。
2. 语法表面清理。

## 已完成

- 泛型调用和类型实参已经切到尖括号表面，`[]` 不再作为泛型语法。
- `sizeof<T>()` / `alignof<T>()`、`ptrat<T>`、`ptrcast<T>`、`bitcast<T>` 等 builtin type operand 已进入泛型实例化 side-table 闭环。
- slice / str 的基础成员投影使用 `.len` / `.ptr`，旧的 `slicelen`、`sliceptr`、`strblen`、`strptr` 不再作为当前语言表面。
- `[]T` / `[]mut T` 是当前 slice 类型表面，`[]const T` 属于旧写法。
- range loop 暂时保留 `for i in range(...)`，不引入更长的 counted-loop marker；真正 iterator/range protocol 后续专题处理。
- 函数、闭包和 C++ 风格捕获列表已有当前设计稿，下一步按语义优先顺序落实现。

## 当前保留文档集

仓库文档已经清理为中文版本，保留 23 个当前项目文件：

- 顶层入口：`docs/README.md`、`docs/zh/README.md`。
- 基础入口：介绍、需求、架构、运行流程、API、实现、使用、版本、进度、下一步。
- 语言入口：语言手册、特性清单、语法修正优化。
- 当前执行入口：泛型计划和语法修正目录。

英文镜像、旧评审报告、旧 spec、skill 草稿、历史设计、专题草案、release baseline 和微阶段流水账已经删除。

## 当前风险

- `progress.md` 和 `version.md` 不再承担历史流水账职责，只记录当前版本边界。
- 被删除的历史设计如果后续仍有实现价值，应重新整理进当前执行文档，而不是恢复旧目录。
- iterator/range protocol 尚未设计，`for i in range(...)` 只是当前 counted loop 表面。

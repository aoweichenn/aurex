# 下一步计划

## 最高优先级

先把泛型相关工作完整收口，再推进其他语法面。

泛型后续处理：

- 确认 parser、AST dump、sema、IR lowering、sample 和负例都只承认尖括号泛型。
- 清理旧的 `[]` 泛型痕迹和文档引用。
- 补齐 generic builtin 的正负样例，尤其是 retained generic body 的 type operand side table。
- 检查泛型实例化错误诊断，避免把新语言阶段的旧兼容逻辑继续保留下来。

## 语法修正

当前语法修正按 `docs/zh/syntax-revision-optimization/` 推进：

- `01-angle-bracket-generics.md`：已落地，继续查漏。
- `02-builtin-intrinsic-surface.md`：继续把 builtin 表面收敛到短且一致的写法。
- `03-range-loop-surface.md`：保留 `for i in range(...)`，等 iterator/range protocol 专题统一处理。
- `04-mut-const-access-surface.md`：保留 `[]T` / `[]mut T`，旧 `[]const T` 不再作为当前设计。
- `05-function-closure-surface.md` 和 `06-function-closure-cpp-capture-list.md`：下一步进入函数/闭包语义和捕获实现。
- `07-builtin-member-projection.md`：继续验证 `.len` / `.ptr` 在泛型、IR 和样例里的覆盖。

## 文档维护

- 只维护中文文档。
- 不恢复英文镜像。
- 不新增 milestone 流水账文档。
- 新设计必须更新 `docs/README.md`、`docs/zh/README.md` 和对应专题索引。

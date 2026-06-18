# 下一步计划

## 最高优先级

先把已落地语法面的边界继续收紧，再推进 iterator 和低层 builtin 设计。

泛型后续处理：

- 确认 parser、AST dump、sema、IR lowering、sample 和负例都只承认尖括号泛型。
- 清理旧的 `[]` 泛型痕迹和文档引用。
- 补齐 generic builtin 的正负样例，尤其是 retained generic body 的 type operand side table。
- 检查泛型实例化错误诊断，避免把新语言阶段的旧兼容逻辑继续保留下来。

iterator / for-in 后续处理：

- 当前只保留 counted `range(...)` 和 array/slice value for-in。
- 用户自定义 iterator protocol、range value、mutable/reference item iteration、str iteration 和 generic iterable capability 需要单独设计。
- 本轮不引入新的 builtin/intrinsic 名字。

closure 后续处理：

- 当前 capture-list 已支持 `[]`、`[x]`、`[&x]`、`[&mut x]`、`[=]`、`[&]`、显式覆盖、init-capture 和 move capture。
- 后续再设计 closure trait、borrowed-view capture 和 escaping closure lifetime。

## 语法修正

当前语法修正按 `docs/zh/syntax-revision-optimization/` 推进：

- `01-angle-bracket-generics.md`：已落地，继续查漏。
- `02-builtin-surface.md`：已切到 `as`、`sizeof<T>()`、`alignof<T>()`；低层 builtin 之后单独设计。
- `03-range-loop-surface.md`：保留 `for i in range(...)`，已补 array/slice value for-in；iterator/range protocol 后续统一处理。
- `04-mut-const-access-surface.md`：保留 `[]T` / `[]mut T`，旧 `[]const T` 不再作为当前设计。
- `05-function-closure-surface.md` 和 `06-function-closure-cpp-capture-list.md`：闭包 capture-list、init-capture 和 move capture 已进入当前语言表面；后续只推进 closure trait 与 escape/lifetime 专题。
- `07-builtin-member-projection.md`：`.len` / `.ptr` 已覆盖 str、shared slice、mutable slice、泛型样例和 IR lowering。

## 文档维护

- 只维护中文文档。
- 不恢复英文镜像。
- 不新增阶段流水账文档。
- 新设计必须更新 `docs/README.md`、`docs/zh/README.md` 和对应专题索引。

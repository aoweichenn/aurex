# 下一步计划

## 最高优先级

泛型语法修正、protocol iterator for-in 和一等 `range(...)` value 已经收口；下一步优先推进 for-in 剩余边界和低层 builtin 设计。

泛型保留边界：

- 当前语言表面只承认尖括号泛型。
- parser、AST dump、sema、IR lowering、sample 和负例已经覆盖尖括号泛型、generic builtin side table 和泛型实例化诊断。
- 旧 `[]` 泛型只保留为 parser 诊断入口，不作为兼容语义继续推进。

iterator / for-in 后续处理：

- 当前已支持 counted `range(...)`、普通表达式位置的 `range<T>` value、range value for-in、array/slice value for-in、直接 protocol iterator、`iter()` protocol iterator、inherent method dispatch、静态 trait dispatch 和 generic `where` 下的 trait dispatch。
- 仍需单独设计 mutable/reference item iteration、str iteration、range literal 和标准库 iterable adapter。
- 本轮不引入新的 builtin/intrinsic 名字。

closure 后续处理：

- 当前 capture-list 已支持 `[]`、`[x]`、`[&x]`、`[&mut x]`、`[=]`、`[&]`、显式覆盖、init-capture 和 move capture。
- 后续再设计 closure trait、borrowed-view capture 和 escaping closure lifetime。

## 语法修正

当前语法修正按 `docs/zh/syntax-revision-optimization/` 推进：

- `01-angle-bracket-generics.md`：已落地，继续查漏。
- `02-builtin-surface.md`：已切到 `as`、`sizeof<T>()`、`alignof<T>()`；低层 builtin 之后单独设计。
- `03-range-loop-surface.md`：保留 `for i in range(...)`，已补一等 range value、array/slice value for-in 和用户 protocol iterator for-in；str iteration、reference item iteration、range literal 和标准库 adapter 后续处理。
- `04-mut-const-access-surface.md`：保留 `[]T` / `[]mut T`，旧 `[]const T` 不再作为当前设计。
- `05-function-closure-surface.md` 和 `06-function-closure-cpp-capture-list.md`：闭包 capture-list、init-capture 和 move capture 已进入当前语言表面；后续只推进 closure trait 与 escape/lifetime 专题。
- `07-builtin-member-projection.md`：`.len` / `.ptr` 已覆盖 str、shared slice、mutable slice、泛型样例和 IR lowering。

## 文档维护

- 只维护中文文档。
- 不恢复英文镜像。
- 不新增阶段流水账文档。
- 新设计必须更新 `docs/README.md`、`docs/zh/README.md` 和对应专题索引。

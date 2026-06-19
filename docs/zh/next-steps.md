# 下一步计划

## 最高优先级

当前第一优先级切换为架构与耦合重构。泛型语法修正、protocol iterator for-in、一等 `range(...)` value 和 str byte iteration 已经收口；下一步不继续优先堆新语言表面，而是先把当前高耦合实现整理成更清晰、更安全、更容易学习和扩展的结构。

先把已落地语法面的边界继续收紧，再推进新的语言表面；所有后续功能都必须建立在更清晰的目录、测试和阶段边界之上。

本轮重构目标：

- 降低高耦合大文件里的隐式依赖，尤其是 sema、IR lowering、checked module dump/clone、borrow/place/drop cleanup 路径。
- 把目录和文件组织作为重构的一等目标：生产代码、测试代码和 CMake/脚本入口都要按模块职责分区，不能继续用单个超大文件承载无关逻辑。
- 把混在同一函数里的“分类、计划构造、诊断、side-table 记录、lowering 细节”拆成更小的策略、builder、value object 或局部协作者。
- 用设计模式只解决真实问题：Strategy 用于可替换 lowering/analysis policy，Builder 用于复杂 plan 构造，Facade/Adapter 用于跨阶段 side-table 和 checked fact 访问；不引入空泛继承层级、service locator 或全局 registry。
- 优先消除会阻碍后续功能的耦合，而不是做无行为价值的格式化 churn。
- 保持性能不退化：不新增重复 AST/IR 扫描，不在热路径复制大对象，不把 O(1) fact 查询改成线性搜索。
- 保持行为不变：每一批重构必须有构建、聚焦测试、完整回归或明确说明为什么不需要完整测试。

目录和测试组织要求：

- `internal/` 只能作为实现根目录，下面必须按职责继续拆分；不再向平铺 `internal/` 目录继续追加新文件。
- 生产代码新增文件必须落在已有职责目录，或先创建清晰的职责目录，例如 `borrow/`、`place/`、`lowering/`、`diagnostics/`、`passes/`、`tooling/adapter/`。
- 测试目录要镜像生产模块和功能领域：frontend、midend、backend、driver、tooling、integration 下面继续按语义子域拆分，避免 `*_tests.cpp` 成为万能入口。
- CMake/脚本只负责目标、依赖、过滤、标签和执行策略；测试源清单、样例矩阵、诊断期望表要从执行脚本里拆出去。
- 大文件拆分按职责进行，不按机械行数切割；新增功能不得继续塞进已知超大文件，除非同时抽出 touched scope 的 cohesive helper。
- 测试 helper 要保持薄、稳定、可复用；不能把领域断言、样例矩阵和通用命令执行混成新的巨型 support。

第一批重构候选按顺序处理：

1. **目录/文件组织与测试脚本治理**：先拆 `tests/gtest/integration/sample_suite/` 和 `cmake/AurexTestSources.cmake`，再按同样方式处理 `early_item_expansion_tests.cpp`、`query_key_tests.cpp`、`cli_driver_tests.cpp`、`parser_tests.cpp` 等超大测试文件。
2. **for-in plan / lowering 解耦**：把 `counted_range`、`range_value`、array/slice/str iterable、protocol iterator 的 sema plan 构造和 IR lowering 分支拆成清晰策略，减少 `sema_statement_analyzer.cpp` 和 `lower_ast_stmt.cpp` 继续膨胀。
3. **CheckedModule dumping / clone 分层**：`checked_module.cpp` 同时承担 clone、layout、dump、debug name 等职责，优先拆出稳定 helper，降低新增 checked fact 时的连锁修改风险。
4. **borrow/place/drop cleanup 协作边界**：梳理 move analysis、place state、borrow flow graph、dropck facts 的事实流向，减少同一语义在多个遍历中重复编码。
5. **IR lowering helper API 收敛**：把 string/slice/range/protocol 等 source 抽象成统一 value-source / address-source helper，避免后续 reference item iteration 或 range literal 再复制 loop 结构。
6. **大型 analyzer 的函数级拆分**：对 `sema_generic_analyzer.cpp`、`sema_statement_analyzer.cpp`、`sema_trait_analyzer.cpp` 中新增功能最常触碰的函数做局部抽取，形成可读、可测、可定位的责任块。

重构准入标准：

- 先读完整相关代码路径和调用点，再改代码。
- 每次只改一个明确边界，避免跨模块大范围重写。
- 新 helper / class 必须有清晰职责名和数据所有权，不引入隐藏全局状态。
- 新目录和文件名必须表达模块职责，不能使用 `misc`、`common`、`manager` 这类扩大边界的名字。
- 新增或修改的 C++ 成员访问遵守 `this->` 规则。
- 魔法数字、魔法字符串、重复 switch 名称要收敛为命名常量或局部转换函数。
- 修改生产代码必须跑相关 build/test；触碰跨阶段契约时必须跑完整 `ctest`。

本轮非目标：

- 不借重构之名改变语言语义。
- 不先做 mutable/reference item iteration、range literal 或 closure trait。
- 不为了“套设计模式”引入不必要的抽象层。
- 不恢复旧文档、英文镜像或阶段流水账。

## 后续功能线

泛型保留边界：

- 当前语言表面只承认尖括号泛型。
- parser、AST dump、sema、IR lowering、sample 和负例已经覆盖尖括号泛型、generic builtin side table 和泛型实例化诊断。
- 旧 `[]` 泛型只保留为 parser 诊断入口，不作为兼容语义继续推进。

iterator / for-in 后续处理：

- 当前已支持 counted `range(...)`、普通表达式位置的 `range<T>` value、range value for-in、array/slice value for-in、str byte iteration、直接 protocol iterator、`iter()` protocol iterator、inherent method dispatch、静态 trait dispatch 和 generic `where` 下的 trait dispatch。
- 仍需单独设计 mutable/reference item iteration、range literal、标准库 iterable adapter 和 Unicode scalar / char iteration adapter。
- 本轮不引入新的 builtin/intrinsic 名字。

closure 后续处理：

- 当前 capture-list 已支持 `[]`、`[x]`、`[&x]`、`[&mut x]`、`[=]`、`[&]`、显式覆盖、init-capture 和 move capture。
- 后续再设计 closure trait、borrowed-view capture 和 escaping closure lifetime。

## 语法修正

当前语法修正按 `docs/zh/syntax-revision-optimization/` 推进：

- `01-angle-bracket-generics.md`：已落地，继续查漏。
- `02-builtin-surface.md`：已切到 `as`、`sizeof<T>()`、`alignof<T>()`；低层 builtin 之后单独设计。
- `03-range-loop-surface.md`：保留 `for i in range(...)`，已补一等 range value、array/slice value for-in、str byte iteration 和用户 protocol iterator for-in；reference item iteration、range literal、标准库 adapter 和字符级 str adapter 后续处理。
- `04-mut-const-access-surface.md`：保留 `[]T` / `[]mut T`，旧 `[]const T` 不再作为当前设计。
- `05-function-closure-surface.md` 和 `06-function-closure-cpp-capture-list.md`：闭包 capture-list、init-capture 和 move capture 已进入当前语言表面；后续只推进 closure trait 与 escape/lifetime 专题。
- `07-builtin-member-projection.md`：`.len` / `.ptr` 已覆盖 str、shared slice、mutable slice、泛型样例和 IR lowering。

## 文档维护

- 只维护中文文档。
- 不恢复英文镜像。
- 不新增阶段流水账文档。
- 新设计必须更新 `docs/README.md`、`docs/zh/README.md` 和对应专题索引。

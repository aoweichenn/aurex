# 当前进度

版本：0.1.9

当前开发线先切到架构与耦合重构。泛型语法和实例化闭环、for-in、closure、borrow/drop 和 IR lowering 的已落地路径要整理成更低耦合、更容易验证、更适合后续扩展的结构。目录/文件组织、测试代码组织和 CMake/测试脚本拆分也属于第一优先级，不再把超大文件和万能测试入口当作可接受状态。

当前开发线收敛到四块：

1. 架构与耦合重构：优先处理目录/文件组织、测试脚本和测试代码拆分，以及 sema、checked fact、borrow/place/drop cleanup、IR lowering 中的高耦合大函数和重复 fact 流。
2. for-in 剩余语义：mutable/reference item iteration、range literal、标准库 iterable adapter 和 Unicode scalar / char iteration adapter。
3. 低层 builtin 表面和安全边界设计。
4. closure trait、borrowed-view capture 和 escape/lifetime 专题。

## 已完成

- 泛型调用和类型实参已经切到尖括号表面，`[]` 不再作为泛型语法。
- `sizeof<T>()` / `alignof<T>()`、`ptrat<T>`、`ptrcast<T>`、`bitcast<T>` 等 builtin type operand 已进入泛型实例化 side-table 闭环。
- 泛型实例化诊断已经覆盖 type/const 实参 kind mismatch，`T`/`N` 等参数名会出现在错误消息中；当前 const 参数名转发仍作为泛型上下文内的合法路径保留。
- slice / str 的基础成员投影使用 `.len` / `.ptr`，历史 helper 名字不再作为当前语言表面。
- `[]T` / `[]mut T` 是当前 slice 类型表面，`[]const T` 属于旧写法。
- range loop 保留 `for i in range(...)`，不引入更长的 counted-loop marker。
- 普通表达式位置的 `range(...)` 已生成一等 `range<T>` value，并支持 `for item in values` 形式复用。
- `for item in expr` 当前已支持 array/slice 按值迭代、str byte iteration、直接 protocol iterator、`iter()` protocol iterator、inherent method dispatch、静态 trait dispatch 和 generic `where` 下的 trait dispatch。array/slice 元素必须满足 `Copy`；str item 是 `u8`；protocol item 不要求 `Copy`。
- 函数闭包已切到 C++ 风格 capture-list，支持 `[]`、显式值/共享引用/可变引用捕获、`[=]` / `[&]` 默认捕获、显式覆盖、init-capture 和 move capture。
- 样例套件集成测试已从单个 `sample_suite_tests.cpp` 拆到 `tests/gtest/integration/sample_suite/`，正例扫描、native runtime smoke、负例矩阵和共享 helper 分开维护。
- 测试 CMake 源清单已拆到 `cmake/AurexTestSources.cmake`，`cmake/AurexTests.cmake` 保留目标、依赖、CTest 过滤、标签和执行策略。
- `early_item_expansion_tests.cpp` 已拆到 `tests/gtest/frontend/macro/early_item_expansion/`，按枚举/identity、noop 基线、契约漂移、builtin derive M22-M26 和 Aurex macro surface 分文件维护；共享查询 helper 拆成薄支撑头，避免继续使用单个宏扩展万能测试入口。

## 当前保留文档集

仓库文档已经清理为中文版本，保留 22 个当前项目文件：

- 顶层入口：`docs/README.md`、`docs/zh/README.md`。
- 基础入口：介绍、需求、架构、运行流程、API、实现、使用、版本、进度、下一步。
- 语言入口：语言手册、特性清单、语法修正优化。
- 当前执行入口：语法修正优化目录。

英文镜像、旧评审报告、旧 spec、skill 草稿、历史设计、专题草案和阶段流水账已经删除。

## 当前风险

- `progress.md` 和 `version.md` 不再承担历史流水账职责，只记录当前版本边界。
- 被删除的历史设计如果后续仍有实现价值，应重新整理进当前执行文档，而不是恢复旧目录。
- 泛型语法修正专题已关闭，旧 `[]` 泛型兼容不得重新进入语义路径。
- 当前 sema、checked module dump/clone、borrow/place/drop cleanup、IR lowering、测试代码和测试脚本都存在大文件高耦合风险；继续叠加新语言表面前，需要先做行为保持型重构，避免后续功能反复复制分支、side-table 访问逻辑和测试样板。
- `query_key_tests.cpp`、`cli_driver_tests.cpp`、`parser_tests.cpp`、`functions_tests.cpp` 等测试文件仍偏大，需要按模块/语义领域继续拆分，不能继续作为新增用例的默认落点。
- mutable/reference item iteration、range literal、标准库 iterable adapter 和 Unicode scalar / char iteration adapter 尚未设计；当前 range value、str byte iteration 和 protocol iterator 已支持直接 lowering，protocol iterator 支持直接 iterator、`iter()`、inherent/static trait dispatch 和 generic where dispatch。
- closure trait、borrowed-view capture 和更完整 escape/lifetime 仍是后续专题。

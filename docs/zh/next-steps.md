# 下一步计划

## 当前分支原则

旧标准库已冻结并从 M2 当前树删除。下一阶段不要继续扩张 std，也不要用 std 样例证明语言能力。所有新能力先用自包含 `.ax` 样例验证；库层必须等基础语法、类型系统和模块边界稳定后重新设计，不复用旧 std 路线。

## M2 优化目标（最高优先级）

本节根据 `docs/review/AUREX_M2_完整报告.md` 制定，优先级高于下面的“优先路线”。在完成本节定义的 M2.1 Semantic + Performance + Security Closure 之前，不继续扩张 std、trait/interface/protocol、borrow checker、macro、async、package manager、通用 iterator protocol、selfhost 或 M1 build-tool 路线。后续新增能力必须先证明不会扩大当前 review 已经指出的语义漏洞、性能爆点和攻击面。

### 总目标

1. 语义闭包优先于新特性：safe surface 不允许隐式穿过 raw pointer，contextual typing 不能被旧缓存污染，泛型和 capability 必须用结构化身份表达，`[]` 的泛型/索引/slice 语义必须在 AST 或 checked 层变成可验证的显式节点。
2. 性能和抗攻击能力进入质量门：泛型实例化、大表达式、错误输入、二进制输入、深层括号、长操作符链和海量诊断都必须有可测上限，不能依赖“正常源码不会这么写”的假设。
3. 诊断从“能报错”升级到“可定位、可解释、可修复”：保留当前 `^` 精确定位优势，补齐 expected/actual 类型、previous declaration note、`did you mean`、配对 token 位置和诊断数量上限。
4. 所有修复必须落到测试矩阵：每个 P0/P1 语义修复至少有 negative sample 或 gtest；每个 P0 性能/安全修复至少有 stress/perf 用例和 baseline 记录；跨编译器性能基线稳定后再建立阈值；修复完成后再更新语法、unsupported 和 progress 文档。

### 硬性验收指标

| 维度 | 当前 review 暴露的问题 | M2.1 目标 |
|:-----|:----------------------|:----------|
| raw pointer safety | `p.x` / `p[i]` 可在 safe context 访问 raw pointer pointee | 所有 raw pointer dereference、field projection、index projection、write projection 都必须要求 unsafe context；safe reference projection 保持 safe |
| `&expr` 语义 | `&x` 根据 expected type 可隐式变成 raw pointer | `&x` / `&mut x` 只产生 safe reference；raw pointer 地址必须走显式 builtin 或显式 unsafe 边界 |
| contextual typing | expression type cache 忽略 `expected_type` | literal、`null`、reference、generic call、struct/array/tuple literal 不再被跨上下文缓存污染 |
| capability | `Eq` / `Ord` / `Hash` 是字符串 predicate，和实际 operator 不一致 | 改为 `CapabilityKind` 与结构化规则；能力判断和 operator/type checker 使用同一套 predicate |
| `[]` 后缀 | generic/index/slice 混在同一语法节点后由 sema 猜 | checked 层必须能区分 generic apply、index、slice、type apply；错误信息指出用户写的是哪一类 |
| 泛型实例化 | 2000 实例约 1.15GB RSS | side table 局部化或稀疏化后，2000 实例目标 RSS 降到约 150MB 量级，增长曲线接近线性 |
| match exhaustiveness | 结构化 pattern 笛卡尔积可指数爆炸 | M2.1 先设置可解释上限；超限时保守失败并要求 wildcard 或更窄 pattern，不能把未知当作 complete |
| parser 深度 | 3000 token `+` 链和 2000 层括号可栈溢出 | 左结合操作链循环化；嵌套分组有深度预算和诊断，不崩溃 |
| lexer 错误输入 | 10KB 全 null / 0xFF / 0x80 约 12s | 无效字节聚合诊断和 error budget；10KB 二进制拒绝目标 < 50ms |
| diagnostics | 诊断质量约 80/100，缺 note/help | 目标约 92/100；补齐低成本高收益诊断信息 |
| 综合性能 | 部分场景落后 Clang/Zig 5-17 倍 | 完成核心 6 项后，综合差距目标收敛到 2 倍以内或给出不可达原因 |

### Review 覆盖映射

| Review 章节 | 必须进入 M2.1 的处理结果 |
|:------------|:--------------------------|
| 总体评价 | 把“语义靠上下文和后期 sema 猜”的路径改成显式 typed identity、显式 unsafe boundary、显式 checked node |
| P0 语义缺陷 | `expected_type` cache、`&expr` 双语义、raw pointer projection、`[]` 多义、capability 字符串 predicate 全部作为 Phase 1 阻塞项 |
| P1 语义缺陷 | generic lookup、generic param identity、mangling key、`?` 形状识别、M2 unsupported 分类全部作为 Phase 2 阻塞项 |
| P2 工程质量 | reference slice index、syntax cache 禁读、Parser→Sema 契约、`analyze_expr` 拆分、record/enum/member index、frontend-only CMake、lookup reserve、error budget 全部纳入 Phase 2-5 |
| P0-Perf | generic side table、match 笛卡尔积、identifier 临时 string、AST 胖节点/复制全部作为 Phase 3 阻塞项 |
| P1-Perf | Lowerer scope、module export 缓存、generic key、diagnostic line table、member lookup、unordered_map reserve 全部纳入 Phase 3 |
| 性能实测和极限压力 | 保留 200/500/1000/2000 泛型实例、大表达式、冷启动、千函数/万函数等基线，作为后续跨编译器对比和阈值候选 |
| 工业级攻击测试 | parser 栈溢出、binary lexer 挂起、错误恢复退化全部必须有 budget、stress case 和“不崩溃”验收 |
| 诊断质量测试 | 数组常量越界是真缺口；`did you mean`、expected/actual、previous declaration note、paired-token note、warning/note/help、颜色/多行 span 分批补齐 |
| M2.1 收口清单 | 21 个建议测试必须进入测试矩阵或被等价测试覆盖 |
| 最终总结和性能目标 | Semantic + Performance + Security Closure 完成前，不推进更高层语言/库路线；综合性能目标按“5-17× 收敛到 2× 内”管理 |

### 当前 P0 收口状态（2026-05-15）

当前 `m2` 分支已经优先关闭 review 中会破坏语义安全、造成指数/超线性资源消耗或形成 DoS 入口的 P0 项。下面状态是实现事实，不是长期架构终点；凡是标为“阶段性关闭”的项，表示 M2.1 先移除了当前爆点，并保留更大规模架构优化为后续任务。

| 项目 | 当前状态 | 已落地边界 | 后续保留 |
|:-----|:---------|:-----------|:---------|
| raw pointer projection unsafe | 已关闭 | `*p`、`p.field`、`p.field = v`、`p[i]`、`p[i] = v` 统一要求 unsafe；reference projection 保持 safe | 后续 borrow/lifetime 设计不属于 M2.1 |
| `&expr` 双语义 | 已关闭 | `&place` / `&mut place` 只产生 safe reference；raw pointer 地址必须显式走 `ptraddr` / `ptrat` 等边界 | 后续可设计更完整的 address-of/raw-pointer conversion API |
| contextual expression cache | 已关闭为短期正确性方案 | context-sensitive 节点禁止复用不带 `expected_type` 的旧 final cache；generic 实例读取 sparse side table 中实际记录的类型 | 长期仍应拆出 intrinsic type、contextual final type 和 coercion/adjustment 模型 |
| `[]` 多义 | 已关闭在 checked/materialized 层 | parser 保留 postfix bracket 语法承载；sema materialize 后形成 generic apply、index、slice 等显式 checked 节点，后端不直接靠 bracket 猜语义 | 长期可进一步让 parser AST 直接区分更多节点，提升诊断精度 |
| capability 字符串 predicate | 已关闭 | capability 使用结构化 `CapabilityKind`，where clause、generic instantiation 和 operator predicate 共享规则；无操作锚点的 `Hash` 约束被拒绝 | 用户自定义 trait/protocol、associated type 和资源 capability 继续后置 |
| 数组常量索引越界 | 已关闭 | 固定数组的整数常量 index 在 sema 报错，覆盖正数越界和负数字面量越界 | 变量 index 和完整运行时 bounds check 仍按当前 M2 边界处理 |
| 泛型 side table 全模块分配 | 已关闭当前爆点 | generic instance side table 改为 sparse map，只为实际分析到的 expr/type/pattern/stmt 记录元数据；IR lower 支持 sparse 读取 | 后续可继续做函数 body 节点区间、实例生命周期释放和 perf RSS 复测 |
| match exhaustiveness 笛卡尔积 | 已关闭当前爆点 | 结构化组合上限为 4096，超限时保守报错，要求 wildcard 或拆分 pattern，不把未知当 complete | 长期替换为 pattern matrix / witness search，减少字符串组合 |
| parser 栈溢出攻击面 | 已关闭主要崩溃点 | 左结合二元链使用迭代 operator/operand 栈；深 grouped expression 增加 M2 深度预算并给诊断；新增 3000 项二元链和深括号 stress 测试 | 类型、pattern、generic 参数等更广递归预算仍可在 Phase 5 stress lane 扩展 |
| lexer 二进制输入 DoS | 已关闭 | 连续 invalid byte 聚合为单个 range 诊断；lexer error diagnostics capped at 128 + summary | 全局 diagnostic 输出上限和 line table 优化仍在 Phase 4/5 |
| identifier 临时 string | 已关闭当前主热路径 | `SymbolTable` scope lookup 使用 transparent hash，`find(std::string_view)` 不再每层构造临时 `std::string`；syntax 层 `IdentifierInterner` 使用 global bump allocator 保存稳定文本；AST name-bearing 字段携带原生 `IdentId`；sema typed lookup key 复用 `AstModule` identifier arena，不再维护第二套私有 interner | 跨模块 stable identifier hash、并行全局 ID 和 delayed display string 仍后置 |
| AST 多次整树复制 / 胖节点 | 已关闭当前主爆点 | module loader append 使用 compact payload move 并重新 intern 到目标 `AstModule`；driver 持有 parser/module AST，并把 mutable 引用交给 sema 和 IR lowering；`SemanticAnalyzer(const AstModule&)` 已删除，避免隐式整树复制；`CheckedModule::normalized_ast` 默认不再保留 AST snapshot；sema 构造期不再对 `exprs/types` 做 `size+4096` reserve；postfix materialization 不再按值复制胖 `ExprNode` / `TypeNode`；`TypeNode` / `ExprNode` / `PatternNode` / `StmtNode` / `ItemNode` 主存储已改成 32B compact header + per-kind payload arena，legacy fat node 只作为边界 materialized value；sema、IR lowering 和 AST dump 的 `ExprNode` 热路径使用 compact view / payload 直读，literal 分析不再重建胖节点；sema item owner 查询改为显式 `ItemId`，不再靠 `items.data()` 反推；module/import/path 元数据 finalize 后同样进入 AST identifier arena | 2M 节点跨机器 RSS 阈值和 CI perf lane 仍是后续性能任务 |

### 当前 Phase 2-5 收口状态（2026-05-16）

Phase 2-5 已按 M2.1 的“先关闭当前语义漏洞、性能爆点和构建阻塞，再保留大架构重写”的边界推进。下面项目已落地到代码、样例或单测：

| 项目 | 当前状态 | 已落地边界 | 后续保留 |
|:-----|:---------|:-----------|:---------|
| generic lookup / identity / key | 已关闭当前语义问题，热路径继续收紧 | generic lookup 复用普通模块可见性/import resolver；generic param identity 使用 template key + param index + source range，并在模板信息中缓存；实例 key 和 ABI suffix 基于 canonical type id，用预分配拼接和 `to_chars` 生成，不再靠 display string；generic function instance、generic struct/enum `TypeInfo` 和 checked enum case display 都保存 base name + semantic key/type args，checked dump / IR lowering / 诊断边界才延迟生成 `id[i32]`、`Box[i32]`、`Maybe[i32]_some` 这类展示名；当前 typed lookup key 已复用 AST 原生 `IdentId` | 跨阶段 stable key / stable def hash 仍后置 |
| `?` try shape | 已关闭 name-based magic | Result/Option 通过 enum identity、完整 case 集和 payload 形状识别；额外同名 case 或 malformed payload 不被误识别；return enum 的错误/none 路径也做结构化检查 | 未来标准库可提供正式 Result/Option 定义，但不在 M2 绑定 std 名字 |
| M2 unsupported policy | 已关闭主要边界 | generic C ABI/prototype、method-local generics、resource capability、foreign/pointer impl target 等均以 M2 unsupported 或明确 semantic diagnostic 拒绝，并有回归测试覆盖 | 更细的 diagnostic category enum 可在诊断分级阶段补 |
| reference slice index | 已关闭 | `&[]const T` / `&[]mut T` safe deref 后按 slice index 规则检查和 lowering；新增 positive sample 覆盖 IR/LLVM IR | 运行时 bounds check 仍按当前 M2 边界处理 |
| Parser→Sema 契约 | 已关闭 | parser-only AST 自动规范化 root module；带 modules 但 `item_modules` 缺失/非法时 sema 入口诊断失败 | 后续可把契约转成更强 typed AST builder API |
| syntax cache 禁读 | 已关闭 | generic/contextual 分析关闭 syntax type cache 时禁止读旧 cache，也禁止写入污染 | 长期仍应拆 intrinsic/contextual/coercion side table |
| record / enum case lookup 索引 | 已关闭当前退化路径 | struct/enum lookup 命中后做 type identity 校验；enum case 索引 miss 不再静默全表扫描通过 | 成员 typed key 仍可继续从 string key 迁移到 IdentId |
| lookup / registry reserve | 已关闭当前 rehash 热点 | sema 全局表、generic 实例表、module visibility/export cache、函数/enum/const/ABI 临时表和局部 scope 按已知规模 reserve；struct field 注册避免逐字段重复查表；identifier reserve 进入 `AstModule::identifiers`，避免 sema 再开一套 arena | 跨阶段 stable hash 和增量失效 key 仍后置 |
| Identifier interner / typed lookup key | 当前主线已落地 | syntax 层 `IdentifierInterner` 使用 global bump allocator；AST 原生 `IdentId` 覆盖 type/expr/pattern/stmt/item/module/import 名字字段；parser push、module loader append、postfix set 和 sema 入口都会 finalize/re-intern identifier 元数据；type/function/value/generic template/enum case lookup 使用 `{ModuleId, IdentId}` 或 `{ModuleId, TypeHandle, IdentId}` typed 索引；生产索引完整时 visible/export module lookup miss 不再拼接 `module:name` / method string；诊断/dump 仍保留原始 display string | 跨模块 stable hash、并行 global ID 和增量编译 fingerprint 仍后置 |
| lowerer scope / module export cache | 已关闭 | lowerer 使用 scope stack + shadow log，不再每个 scope 复制整个 locals map；module export modules 做缓存 | 后续可加更细粒度失效模型给增量编译 |
| diagnostics line table / 输出上限 | 已关闭 | `SourceFile` 建 line starts 表，诊断位置查询不再线性扫描；driver 输出最多 128 条诊断并打印 summary | did-you-mean、previous declaration note、warning/help 分级仍是增强项 |
| frontend-only CMake / tests | 已关闭 | 增加 `AUREX_FRONTEND_ONLY` 配置和 `aurex_frontend_tests` 目标，可在无 LLVM/IR/driver/CLI 情况下构建运行前端测试 | CI 分层和 release perf lane 后续接入 |
| perf baseline benchmark harness | 基础线已接入，跨前端对比通道和泛型/AST RSS 压测入口已接入 | 增加基于 Google Benchmark 的 `aurex_frontend_bench`，覆盖 lexer mixed、lookup-heavy lex→parse→sema、generic-instantiation-heavy lex→parse→sema、AST bulk lex→parse→sema；`tools/perf_report.py` / `make perf` 解析 `--benchmark_format=json` 并输出轻量 baseline；新增 `aurex_frontend_compare_bench` 和 `tools/frontend_compare.py`，用 Google Benchmark real_time 对比 Aurex `--check`、Clang/G++ `-fsyntax-only`、rustc stable metadata emission 的 lookup/generic frontend/check 基线；新增 `tools/generic_stress.py` 和 `tools/ast_stress.py`，`make perf-stress` 固化 200/500/1000/2000 泛型实例和 10000/50000/100000 AST bulk statements 的 elapsed time + peak RSS 基线；compact AST + AST 原生 `IdentId` 后本机 100000 AST bulk statements 约 180 MiB RSS / 112 ms；Google Benchmark `sema_ast_bulk/1024` 当前约 174 ns/expr；`tools/frontend_compare.py` 本机 baseline 中 Aurex lookup/96 约 8.4 ms、generics/96 约 9.1 ms，Clang++ 分别约 20.6 ms / 22.4 ms，G++ 分别约 25.8 ms / 24.4 ms；2000 generic instance stress 约 112.5 MiB RSS / 386 ms；`--check` / checked dump 路径不再保留后端 lowering 专用的泛型实例 side table；暂不强制阈值 | 2M AST 节点跨机器稳定阈值和 CI perf lane 仍后置 |

当前仍不应声称已经完成的内容：跨阶段 stable identifier hash / parallel global ID、完整 pattern matrix / witness search、完整 `analyze_expr` 分层重写、did-you-mean/previous declaration note/warning-help 诊断增强、以及 2M AST 节点的跨机器 RSS/耗时阈值和 CI perf lane。这些已从当前爆点中解耦，并已有 generic/AST stress baseline 入口，但仍属于 M2.1 后续架构和 perf lane 工作。

### Phase 0：基线冻结和报告建立

1. 复现 review 中的关键数据，并把命令、机器环境和 baseline 写入性能记录：
   - `--check` 冷启动、小文件、千函数、万函数、1000/2000 泛型实例。
   - 大表达式二元树、长左结合操作链、深层括号、深层 block。
   - 10KB 二进制输入、全 null、全 0xFF、全 0x80、分号风暴、海量错误诊断。
   - `tools/frontend_compare.py` 记录可用现代前端版本和 lookup/generic frontend/check 对比；rustc 不可用时记录 missing，不把缺失工具当成 Aurex 性能失败。
2. 把 stress case 固化为可重复工具或测试，不要求每次普通单测都跑最重版本，但必须能在 release 前手动或 CI perf lane 复测。
3. 建立 M2.1 禁止线：
   - 不允许新增会绕过 unsafe 的 raw pointer 入口。
   - 不允许新增依赖 display string 的语义身份。
   - 不允许新增整模块按实例复制的 side table。
   - 不允许新增无上限递归、无上限错误恢复或无上限诊断输出。

### Phase 1：P0 语义闭包

1. raw pointer projection 统一 unsafe 检查。
   - 引入统一的 place/projection 表示，例如 `PlaceInfo { is_place, is_writable, crosses_raw_pointer }`。
   - `*p`、`p.field`、`p.field = v`、`p[i]`、`p[i] = v` 只要 projection 链上穿过 raw pointer，就必须调用同一套 unsafe context 检查。
   - `&T` / `&mut T` reference 的 field/index projection 保持 safe，但 mutability、writable place 和 const pointee 规则必须继续生效。
   - 新增 negative：`raw_pointer_field_requires_unsafe`、`raw_pointer_field_write_requires_unsafe`、`raw_pointer_index_requires_unsafe`、`raw_pointer_index_write_requires_unsafe`。
   - 新增 positive：同样操作包在 `unsafe { ... }` 内可通过，并验证 write lowering 不生成无效 IR。
2. 拆除 `&expr` 的 raw pointer 兼容语义。
   - `&place` 永远产生 `&T`，`&mut place` 永远产生 `&mut T`。
   - 需要 raw pointer 时使用显式 `ptraddr(...)` / `ptrat[T](...)` 这类 builtin，并按 unsafe/ABI 边界诊断。
   - 所有函数调用、变量初始化、return、struct literal 字段填充都不能因为 expected type 是 `*const T` / `*mut T` 而把 `&x` 改判为 raw pointer。
   - 新增 negative：`ref_ptr_diverge`、`implicit_address_to_raw_pointer_rejected`。
3. 修复 expression type cache 的 contextual typing 污染。
   - 把表达式分析拆成 intrinsic type、contextual final type 和 coercion/adjustment 三层；或者把缓存键显式包含 expected type，并禁止 context-sensitive 节点读旧 final cache。
   - integer/float literal、`null`、reference literal、generic call、array literal、struct literal、tuple literal、block/match/if expression 的 final type 不能跨 expected type 复用。
   - generic body checking 期间，`cache_syntax_types` 关闭时必须既禁写也禁读，避免实例化读到模板上下文残留类型。
   - 新增 negative：`contextual_type_cache_attack`、`null_context_attack`、`generic_context_cache_no_read`。
4. 拆清 `[]` 的语义节点。
   - type context 中的 `Name[Args]` 进入 type apply；expression context 中含 `:` 的后缀进入 slice；确定是 generic call/value index 后 checked 层必须记录为不同节点。
   - 诊断必须区分：非泛型类型被 type apply、非泛型函数被 explicit generic call、非 indexable 值被 index、slice bound 非整数、slice 对象不是 array/slice/str。
   - 后端和 IR lower 不再通过“看起来像 bracket postfix”猜语义。
5. Capability 改成结构化能力。
   - 用 `enum class CapabilityKind { Sized, Eq, Ord, Hash }` 或等价结构替代字符串 predicate。
   - capability parser、where clause、generic instantiation、operator checking 和诊断共用一套 capability 表。
   - `Eq` / `Ord` 的规则必须和 `==` / `<` 等 operator 支持集一致；如果 float、reference 或其他类型不能提供可靠能力，就必须在 capability 和 operator 两边同步拒绝或同步说明例外。
   - `Hash` 不能只是名字存在，M2.1 至少要定义“可接受但无运行时 hash operator”的明确边界，或暂时拒绝没有操作锚点的类型。
   - 新增 negative：`reference_does_not_satisfy_eq`、`reference_hash_rejected`、`enum_hash_rejected`、`where_unknown_capability_structured`；float equality 继续跟随当前 `==` 支持集作为 `Eq` 正例。
6. 补齐数组常量索引边界检查。
   - 对固定数组和可静态求值的整数 index，在 sema 阶段检查 `0 <= index < length`。
   - 变量 index 仍按当前 M2 运行时/后端边界处理，但不能把常量越界留到 LLVM 或 native crash。
   - 新增 negative：`array_constant_index_out_of_bounds`。
7. 收紧 `return null` 和复合表达式 null 推导。
   - `return null`、if/block/match tail `null`、`?` 错误路径中的 `null` 必须按函数返回类型或 expected type 进行 contextual typing。
   - 无指针 expected type 时拒绝 `null`，有指针 expected type 时保留正确 pointee/mutability，不把 `null` 降成 invalid type 后污染外层表达式。
   - 新增 positive/negative：`return_null_infer`、`null_if_expr_expected_pointer`、`null_match_expr_expected_pointer`、`null_without_pointer_context_rejected`。

### Phase 2：P1 语义一致性和可扩展身份

1. 统一 generic lookup 与普通 lookup。
   - generic struct/enum/type alias/function/impl lookup 必须走和普通类型/值同源的模块可见性与 import resolver。
   - 修复 visible modules、qualified import、ambiguous import 和 private generic item 的不一致。
   - 新增 mixed：`imported_generic_lookup`、`private_generic_import_rejected`。
2. generic parameter identity 改为结构化身份。
   - `T` 不能只按名字成为全局等价类型；身份至少包含 template key、parameter index、decl source range。
   - 嵌套 generic、generic alias、generic impl、同名类型参数 shadowing 必须可区分并可诊断。
3. mangling 和实例化 key 不能依赖 display string。
   - 使用 module id、symbol id、generic args type ids、ABI namespace 组成结构化 key。
   - display name 只用于诊断和 dump，不能决定链接名唯一性。
   - 新增 negative：`generic_mangle_collision`、`module_mangle_display_collision`。
4. `?` / try-like 语义摆脱纯 name-based magic。
   - 已按本项要求把 `Result` / `Option` shape 规则收紧为结构化识别：enum identity、case identity、payload 形状、错误类型转换规则。
   - 用户定义同名 `ok` / `err` / `some` / `none` 不能被误识别。
   - 新增 mixed：`option_result_magic_name_collision`。
5. 区分 M2 unsupported 与 semantic error。
   - array by-value 参数/返回、array-containing assignment、generic C ABI/prototype、foreign impl、method-local generics 等应给出 `M2Unsupported` 或等价诊断类别。
   - 不再把后端限制伪装成语言语义错误；文档和测试应能说明这是阶段限制还是永久规则。
6. reference slice index 一致性。
   - `&[]const T` / `&[]mut T` 在 safe deref 后应按 slice 规则 index，或者明确诊断当前 M2 不支持 reference-to-slice index。
   - 新增 positive：`reference_slice_index`。
7. Parser 到 Sema 的隐藏契约显式化。
   - `item_modules`、source ranges、module ownership 等必须在 sema 入口做 assert/diagnostic，而不是默认由某条 driver 路径填好。
   - 新增 unit：`parser_ast_requires_item_modules`。
8. record/member 查询做二次验证。
   - `find_record` / member map 命中后验证 type identity，防止哈希或 display key 退化造成错绑。
   - `find_enum_case` fallback 全表扫描只能作为 debug/assert 辅助，不应掩盖索引失效。
9. 拆分 `analyze_expr` 的职责。
   - 把 name resolution、place analysis、contextual typing、operator checking、aggregate checking、generic instantiation、coercion/adjustment、diagnostic emission 分成明确 helper 或小型阶段。
   - 每个 helper 声明输入不变量、是否可读写 expr type cache、是否依赖 expected type、是否允许产生 coercion。
   - 拆分后的行为必须先由现有 tests 保护，再逐步替换大函数内部逻辑，避免一次性重写造成诊断回退。
10. foreign impl 和 unsupported policy 定死。
   - `impl` target、pointer impl、generic extern/prototype、method-local generic 等边界要么实现，要么统一用 M2 unsupported 诊断拒绝。
   - 新增 negative：`foreign_impl_policy`、`generic_extern_unsupported_category`。

### Phase 3：P0 性能和安全收口

1. 泛型 side table 局部化或稀疏化。
   - 当前按全模块 `exprs.size()` / `patterns.size()` 为每个实例分配 side table，是 2000 实例内存爆炸的直接原因。
   - M2.1 先改为函数 body 节点区间、实例相关节点切片或 sparse map；只有实际分析到的 expr/pattern 才占内存。
   - 实例结束后释放临时上下文；跨实例共享只保留 canonical type、checked signature 和必要 memo。
   - 验收：200、500、1000、2000 实例 RSS 接近线性；2000 实例不再超过 200MB，目标约 150MB。
2. match exhaustiveness 防指数爆炸。
   - M2.1 立即加组合上限，例如 4096 个 witness/combination。
   - 超限时采用保守策略：不能证明 complete 就报“覆盖检查超出 M2.1 上限，需要 wildcard 或拆分 pattern”，不能默认通过。
   - 长期替换为 pattern matrix / witness search，避免字符串拼接笛卡尔积。
   - 验收：bool/enum 小结构保持精确；大结构不超时、不爆内存、不误判完整。
3. parser 左结合操作链循环化，深层分组加预算。
   - `a+b+c+...`、长比较/逻辑链、postfix 链采用 loop/Pratt climb，不用线性递归堆栈。
   - 括号、泛型参数、pattern 嵌套、block 嵌套设置明确 recursion/depth budget，超限给诊断。
   - 验收：`token_chain_3000` 不崩溃；深括号输入给出单个清晰诊断。
4. lexer error budget 和无效字节聚合。
   - 连续 invalid byte 作为 range 聚合诊断，不逐字节做昂贵恢复。
   - 单文件诊断数量、无效字符扫描、错误恢复 token 消耗都应有 budget。
   - 验收：10KB 全 null / 0xFF / 0x80 在 < 50ms 内拒绝；诊断输出不超过上限。
5. diagnostic line table 和输出上限。
   - SourceFile 建立 line starts，line/column 查询从线性扫描改为 O(log lines) 或 O(1) amortized。
   - 大量错误默认截断并输出“too many errors” summary；保留开关给调试模式输出更多。
   - 验收：5000 errors 场景目标约 0.2s 级别，输出大小受控。
6. 延迟 LLVM 初始化。
   - `--check`、parser/sema-only、dump tokens/ast 等路径不得初始化 LLVM backend 或 clang/toolchain。
   - 验收：小文件 `--check` 保持约 6ms；`--emit=ir` / native 路径只在需要时付 LLVM 成本。
7. Identifier interner 和 typed lookup key。
   - identifier、module name、qualified name、field name、case name 改为 `IdentId` / `QualifiedNameKey` 或等价结构。
   - hot path 不再反复构造 `std::string`；lookup map 建表前 reserve。
   - display string 延迟生成，只用于诊断/dump。
   - 已完成当前主线：syntax 层 `IdentifierInterner` 使用 global bump allocator；AST 名字字段携带原生 `IdentId`；type/function/value/generic template/enum case lookup 使用 AST module interner + typed lookup key；`CheckedModule` 字符串 key 保留给 ABI、dump 和跨阶段边界，跨模块 stable hash / parallel global ID 继续后置。
8. Sema 不再复制整棵 AST。
   - Sema 读取 parser/module AST 引用；normalized AST 使用 overlay 或 per-node adjustment，不复制胖节点。
   - 大表达式和大模块场景优先减少 3-4 次整树复制。
   - 已完成当前主线：driver 持有 AST 并向 sema/IR 传引用，`CheckedModule::normalized_ast` 默认不复制 AST，`SemanticAnalyzer(const AstModule&)` 被删除以禁止隐式整树复制，sema 构造期不再触发 `exprs/types` 整体 reserve，postfix materialization 移除 `ExprNode`/`TypeNode` 按值复制。
   - 验收：2M AST 节点 RSS 从约 3GB 降到约 1.5GB 或更低；如果未达成，必须记录剩余大头。
9. Lowerer scope stack 和 module export 缓存。
   - Lowerer 不在每个 scope 复制整个 `locals_` map，改为 scope stack + shadow log。
   - `module_export_modules()` / qualified lookup 结果缓存，失效点明确。
10. 成员索引、enum case 索引和 map reserve。
   - struct field、method、enum case、module export、function/type/value registry 建表前统一 reserve。
   - 成员查找优先使用 `(type_id, ident_id)` typed key；线性扫描只作为小集合 fallback 或 debug 验证。
   - enum case 索引 miss 时不再静默全表扫描通过；debug 模式可以 assert 索引一致性，release 模式给内部错误或保守诊断。
11. Generic key 结构化并延迟 display。
   - 泛型实例化 key 已使用 template key + canonical type handle args，不使用 `display_name()`；generic param identity 已缓存，实例 key / ABI suffix 使用预分配拼接和 `to_chars`，减少热路径临时字符串。
   - generic function instance 的 checked signature 已改为 base name + semantic key + type args；`function_display_name()` 只在 checked dump / IR lowering 需要输出名时生成展示字符串，`--check` 热路径不再为函数实例生成 `id[i32]` 展示名。
   - generic struct / enum 的 `TypeInfo.name` 和 `StructInfo` / `EnumCaseInfo` 用户展示名已改为 base name + generic args 延迟格式化；实例化热路径不再为 `Box[i32]`、`Maybe[i32]`、`Maybe[i32]_some` 生成展示字符串，checked dump、IR record/constant name 和诊断需要时再格式化。
   - `SemanticOptions.retain_generic_side_tables` 将 check/checked dump 和 IR/codegen 模式分开；`--check` 路径在每个泛型函数实例分析完成后释放 sparse side table，只保留签名、语义 key 和必要类型信息，IR/native 输出路径继续保留 lowering 所需 side table。
12. Compact AST 和 bump allocator 作为 M2.1 后半段优化。
   - 当前已经消除 sema/checked 主路径整树复制和 side table 爆炸；`TypeNode`、`ExprNode`、`PatternNode`、`StmtNode` 和 `ItemNode` 已落为 32B header + per-kind payload arena；sema、IR lowering 和 AST dump 的 `ExprNode` 热路径已迁到 compact view / payload 直读；global bump allocator + AST 原生 `IdentId` 已覆盖当前 parser/module/sema 主路径。
   - 本机 `tools/ast_stress.py --skip-build --counts 10000,50000,100000` baseline 中，100000 AST bulk statements 约为 180 MiB RSS / 112 ms，较 compact 前约 575 MiB RSS / 135 ms 明显收敛；Google Benchmark `sema_ast_bulk/1024` 当前约 174 ns/expr。
   - 后续目标：跨模块 stable hash / parallel global ID、2M AST 节点跨机器 RSS/耗时阈值。

### Phase 4：诊断信息升级

1. `did you mean` 建议。
   - 对变量、函数、类型、module alias、field、enum case 使用同 scope 候选和编辑距离。
   - 候选必须受可见性和命名空间限制，不能建议 private 或错误 namespace 的名字。
2. expected/actual 类型显示。
   - call argument、return、assignment、field init、array element、match arm、if branch 都显示 expected 与 actual。
   - 类型显示使用 canonical display，避免 mangled/internal name 泄漏给用户。
3. previous declaration note。
   - duplicate function/type/const/field/enum case、ABI collision、shadowing restriction 都标记上一处声明。
4. parser paired-token note。
   - 缺 `)` / `]` / `}` 时，如果有 opening token，输出 opening token 位置。
5. warning 和 note/help 级别。
   - Diagnostic 支持 error/warning/note/help；M2.1 默认仍以 error 为主，但基础设施先支持分级。
6. 多行 span 和颜色输出后置为低风险增强。
   - 不影响语义修复；在前四项完成后再做。

### Phase 5：测试矩阵和完成定义

1. 补 frontend-only 构建和测试模式。
   - CMake 增加不依赖 LLVM backend 的 frontend-only 配置或 target，至少能跑 lexer/parser/sema/diagnostic/golden 前端测试。
   - 普通 frontend 变更不应被 LLVM 安装、链接或 backend 初始化问题阻塞。
   - CI 或本地质量门区分 frontend-only、IR、LLVM backend、native execution 四层，便于快速定位回归来源。
2. 建立 stress/perf 测试分层。
   - 常规单测只跑轻量 baseline case；release/perf lane 跑 2000 泛型实例、AST bulk / 2M AST 节点、二进制输入、5000 errors、深括号和长操作链。
   - 每个 perf case 记录基线、目标、允许波动和机器说明。

M2.1 至少新增或更新以下测试类别：

| 类别 | 必须覆盖 |
|:-----|:---------|
| unsafe/raw pointer | raw pointer field/index/read/write 需要 unsafe；reference field/index 不误报；unsafe block 内通过 |
| contextual typing | literal/null/reference/generic call/aggregate literal 在不同 expected type 下不读旧 final cache |
| capability/generic | reference capability 拒绝；Hash 无操作锚点类型拒绝；float Eq 跟随 `==` 支持集；imported generic lookup；generic param identity；mangling collision |
| `[]` 语义 | generic apply、type apply、index、slice 的正反例和诊断区分 |
| array bounds | 常量 index 越界拒绝；变量 index 保持当前策略 |
| parser/type stress | 3000 token 操作链、深括号、深泛型、深 pattern、deep type nesting 不崩溃 |
| lexer stress | 二进制、全 null、全 0xFF、控制字符风暴快速拒绝 |
| match stress | 小结构精确 exhaustiveness；大结构超限保守诊断 |
| diagnostics | expected/actual、did-you-mean、previous declaration note、too many errors |
| perf | 200/500/1000/2000 泛型实例 RSS/耗时；10000/50000/100000 AST bulk RSS/耗时；2M AST 节点 RSS；5000 errors 耗时 |

完成定义：

1. `tools/run_tests.sh` 通过，新增 negative/positive samples 全部纳入 suite。
2. P0 stress/perf case 不能崩溃、不能超出预算、不能产生无上限诊断。
3. `docs/spec/m2_grammar.md`、`docs/spec/m2_syntax_matrix.md`、`docs/spec/m2_unsupported.md` 和 `docs/zh/progress.md` 与实际实现同步。
4. review 中 P0 语义项和 P0-Perf 项全部关闭；未关闭的 P1/P2 必须有 issue、测试或文档说明。
5. 在完成上述闭包前，下面的语言特性路线只允许做文档澄清、测试补强和 bug fix，不作为新能力扩张入口。

## 优先路线

1. 现代基础语法第一优先级

   当前阶段先完善基础语法，而不是扩张高级特性或重建库层。对照范围不只限 Rust、Go、Zig、Kotlin、C++，还要参考 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V 等现代语言，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness 和 unsafe boundary 相关研究。结论是：Aurex 的大语法骨架已经具备，第一优先级应收口现代语言共同证明过的基础表达能力，而不是先做 trait、borrow checker、macro、async、std 或通用 iterator protocol。

   第一批 P0 基础语法按这个顺序推进：

   - 内建操作拼写已经先规范为 `sizeof[T]`、`alignof[T]`、`cast[T](x)`、`ptrcast[T](p)`、`bitcast[T](x)`、`ptraddr(p)`、`ptrat[T](addr)`、`strptr(s)`、`strblen(s)`、`strvalid(bytes)`、`strfromutf8(bytes)`、`strraw(data, len)`；旧的函数式拼写不再作为源码语法。
   - 最小 `unsafe` block / `unsafe fn` 已完成：raw pointer dereference、`ptrcast`、`bitcast`、`ptrat`、`strraw` 这类破坏不变量的操作已经不能留在普通安全表达式表面。
   - ADT-first enum 已完成非泛型 M2 形态：普通 `enum OptionI32 { some(i32), none }` / `enum Token { span(usize, usize), eof }` 成为主力写法；保留 `enum Status: u8 { ok = 0, err = 1 }` 作为显式 C-like/repr enum。
   - array literal / repeat literal 已完成：`[1, 2, 3]` 和 `[0; 128]` 现在能构造固定长度数组值。

   已完成的第一优先级基础项：default private、ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer、最小 unsafe 和 M2 pattern ergonomics。顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public，`impl` / `extern` block 不能显式 `priv`。slice 当前是 `ptr + len` 的 borrowed fat value，支持 `[]const T` / `[]mut T` 和 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`，不包含容器迭代或运行时 bounds check。tuple 当前支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、局部 `let (a, _) = value;` 解构和 `match pair { (a, b) => ... }`；匿名 tuple 不支持 `.0` / `.1` 或 `.first` / `.second` 字段访问，需要字段访问时使用 named struct；空 tuple 不属于 M2。函数类型当前是非捕获函数指针，支持 `fn(...) -> T`、`unsafe fn(...) -> T`、`extern c fn(...) -> T`、`unsafe extern c fn(...) -> T`、函数名作为值以及局部/参数/字段函数指针间接调用；完整 closure 捕获仍暂缓。

   第二批 P1 基础语法中 raw/multiline raw string、byte string、Unicode scalar `char`、数值后缀、tuple/destructuring、struct pattern、slice pattern、nested enum payload pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已补齐。容器迭代、完整 closure 捕获、trait/interface/protocol、package manager、macro、async 继续暂缓。完整库存和优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

2. unsafe 与 `str` 安全边界

   最小 unsafe 已落地：`unsafe { ... }` 建立 unsafe context，带 tail expression 时作为表达式求值，没有 tail expression 时类型为 `void`；`unsafe fn` 和 unsafe 函数指针调用必须发生在 unsafe context。raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 已经是 unsafe-only。M2 这里不包含 borrow checker、lifetime、unsafe trait、unsafe impl、unsafe extern block 或资源/所有权模型。`str` 的无 std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool` 和 `strfromutf8(bytes) -> str`；失败时返回空 `str`，不会把无效输入包装成 UTF-8 文本。

3. enum ADT 与 pattern 地基

   enum ADT-first 已落地到非泛型 M2 基线：base type / discriminant 可省略，tag 自动分配，多字段 payload 可构造，并且 pattern 侧支持 `.case(a, b)`、`.case((a, b))` 等按字段/嵌套解构。局部 tuple/struct/slice/enum destructuring、match tuple/slice/struct pattern、binding or-pattern alternatives、`let ... else`、`if value is pattern`、`while value is pattern` 和 if 表达式 pattern condition 已落地。

4. 数组、slice、字符串与函数类型基础语法

   Aurex 已有数组类型和值语法、borrowed slice、tuple、`str`、C string、raw/multiline raw string、byte string、byte literal、Unicode scalar `char`、函数声明、函数指针类型、C FFI、最小 safe reference、最小非资源类 `where` capability 和结构化 match 覆盖检查。字面量体系、tuple 基础、pattern ergonomics、`str` checked UTF-8 构造/切片、`&T` / `&mut T`、generic enum / generic type alias / owner generic impl 已经补齐到 M2 基线；下一步更适合继续收窄剩余诊断边界，而不是重建库层。

5. 值语义边界

   M2 当前已经删除 M1 的 `move(...)` / `noncopy struct`，避免把失败的 move-only MVP 继续当作语言地基。当前阶段不继续推进资源模型；只写清普通值传递、struct/enum payload、match payload、`?` 和数组/含数组类型限制的统一规则。

6. 资源语义暂缓

   `Copy` / `Drop` / destructor / borrow / move-out 不作为当前 M2 近期任务。等 `unsafe`、ADT、array/slice/string、function type 和 pattern 地基稳定后，再重新开资源语义专题。

7. safe reference

   最小 `&T` / `&mut T` 已进入 M2 核心：reference 和 raw pointer 是不同类型，`&mut` 要求 writable place，reference 解引用是 safe，raw pointer 解引用仍是 unsafe-only。borrow checker、lifetime、borrowed return、alias model 和资源语义继续暂缓。

8. Capability / trait / where

   最小语言机制已落地：`where T: Eq + Hash` 支持内建非资源能力 `Sized`、`Eq`、`Ord`、`Hash`，并在泛型实例化、泛型函数体检查和相关运算符上给出诊断。资源相关能力 `Copy` / `Drop`、用户自定义 trait、associated type、const generic、trait object 和完整 protocol 仍等后续专题再定。

9. 字符串基础类型

   保留 `str` 作为语言级 borrowed UTF-8 slice 的设计方向，但不要复活旧 std 的 `String`/`Bytes` 实现。`str` 的类型、ABI、字面量、unchecked `strraw`、checked `strvalid` / `strfromutf8` 构造边界，以及 `text[l:r]` checked byte-offset slicing 已落地；未来库层 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 继续后置。

10. 测试性能继续收口

   保持测试 harness 直接调用 C++ driver，避免每个用例启动脚本。新增用例时区分 check/IR/native 三类，只有必须验证运行时行为的样例才生成并执行二进制。

## 明确暂缓

- std 容器、文件、目录、进程、console。
- M1 frontend / axbuild 样例；M1 阶段已经舍弃，不能作为当前路线继续推进。
- host support C shim。
- 安装后 std 查找。

重新设计或重新评估这些内容的前置条件是：基础语法、模块边界、`unsafe`、ADT、slice/string 和最小泛型约束已有稳定语言级设计和测试矩阵；拥有型资源库还需要后续资源语义专题完成。

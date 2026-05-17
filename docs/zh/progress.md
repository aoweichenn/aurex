# 当前进度文档

版本：0.1.2
阶段：M2.5 frontend-foundation

## 总体状态

当前仓库已经从 M2 language-core-no-std 基线进入 M2.5 frontend-foundation 阶段。M2 的目标不是继续修补 M1，而是重新收口语言核心：冻结并删除标准库和 M1 系统样例，把注意力放回基础语法、类型系统、模式匹配、`unsafe` 边界、IR 和 LLVM 后端。M2.5 建立在这条已收口主线之上，开始处理 query 化、lossless syntax 和 IDE-native 前端所需的结构化地基。

M1 阶段已经舍弃。主要原因不是单个功能失败，而是整体设计方向不稳：

- 标准库、host support、构建工具样例和语言核心同时扩张，导致测试结果很难判断是语言问题、库问题还是工具链问题。
- 部分能力仍缺少语言级规则，例如资源释放、容器 API 约束，以及未来标准库 `Result` / `Option` 定义和当前结构化 `?` shape 之间的正式绑定。
- 语法地基还没有冻结时就推进 selfhost/build-tool 路线，造成前端、库 API、资源模型互相牵制。
- M1 的很多样例验证的是“能跑通当前 demo”，不是验证语言核心是否稳定、可解释、可长期扩展。

因此 M2 只承认当前 C++ Stage0 编译器、Aurex IR、LLVM backend 和自包含语言样例作为有效基线。当前仓库没有 `std/` 目录，也没有 `selfhost/` 目录；相关旧路线只作为历史输入，不再代表当前进度。

## 已完成能力

当前生产编译器位于 `src/` 和 `include/`，使用 C++20 实现。

- CLI 支持 `--check`、`--dump-*`、`--emit=*`、`--opt-level`、`-I`、`-o`、`--clang` 和 `--clang-arg`。
- driver 能完成文件 IO、模块加载、编译流水线调度、LLVM IR 临时文件生成和 clang 调用。
- module loader 支持根模块和 import 模块合并，能检测模块名不匹配、缺失 import、循环 import、重复模块名和重复加载。
- lexer/parser 是手写实现，输出 ID-backed AST；token、AST、module、checked、IR、LLVM IR 都有 dump 路径。
- 语义分析已具备类型表、符号表、函数签名、ABI 名称、struct/enum 元数据、泛型实例化、表达式类型表和 source range 诊断；诊断分级支持 error/warning/note/help，lookup miss 已有 `did you mean` help，常见 type mismatch 输出 expected/actual note，duplicate 主路径输出 previous declaration note，parser 成对 delimiter 缺失输出 opening delimiter note。M2.5 的首项工作已完成：sema 诊断在创建时携带显式 kind/category/code，不再从 message 文本反推机器元数据。
- 当前语言样例覆盖函数、函数指针类型、`extern c`、`export c fn`、普通 `fn main`、import、可见性、const、type alias、struct、enum、opaque struct、泛型、method、指针、数组、slice、字段访问、索引、cast、内建 size/align、`if` 表达式、block 表达式、`match`、`while`、C-style `for`、基础 `for i in range(...)`、`defer` 和 `?`。
- 程序入口支持根模块普通 `fn main`，签名包括无参数或 `argc/argv`，返回 `i32` 或 `void`。
- Aurex IR 是 typed CFG/SSA-like 中间层，后端只消费 IR，不回读 AST。
- IR verifier 会检查函数、block、value、terminator、类型和引用一致性。
- pass pipeline 支持 `O0` 到 `O3` 选项；当前实际优化以保守的局部 mem2reg 和 CFG cleanup 为主。
- LLVM backend 使用 LLVM C++ API 从 Aurex IR 生成 LLVM IR，并通过 clang 生成 asm、object 和 executable。

## 已删除能力

M2 明确删除并暂缓这些 M1 内容：

- `std/` 源树。
- host C support 源文件和自动链接。
- driver 的标准库查找、import path 自动注入和安装后 std 查找。
- CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 安装规则中的 `share/aurex/std`。
- std/M1/system/build-tool 样例。
- 依赖标准库名字的资源语义特判。
- selfhost / Stage1 / AIR snapshot 路线在当前仓库中的实现。

这些内容不是永远不能回来，而是必须等基础语法、module/package、`unsafe`、slice/string 和泛型约束稳定后再重新设计或重新评估；拥有型资源库还要等后续资源语义专题完成。

## 质量门

当前主要质量门：

```sh
tools/run_tests.sh
tools/bench.py
make perf
make perf-stress
make perf-stress-threshold
make perf-release-threshold
make perf-ast-stress
```

测试覆盖：

- lexer/parser/AST dump。
- driver/CLI/native toolchain。
- positive/negative `.ax` sample suite。
- 模块、可见性、泛型、函数、方法、pattern matching、error handling 和类型系统诊断。
- Aurex IR lowering、IR verifier、pass pipeline、LLVM backend。
- native execution 和安装后 compiler 执行。

`tools/bench.py` 使用 Release `build-perf` 构建目录，并用 Google Benchmark
测量 frontend 热路径。`make perf` 输出基于 JSON 的 Aurex frontend baseline，
覆盖 lexer、lookup-heavy sema、generic-instantiation-heavy sema 和 AST bulk sema 路径，并运行
Google Benchmark 的进程级现代前端对比通道，对可用的 `clang++`、`g++`、`rustc`
做 frontend/check 模式基线；`make perf-compare` 只运行跨前端对比通道。
`make perf-stress` 运行 `tools/generic_stress.py`、`tools/ast_stress.py` 和
`tools/diagnostic_stress.py`，生成 mixed-feature 泛型、AST bulk 和 diagnostic
源码，并记录 `aurexc --check` elapsed time + peak RSS baseline；`make perf-ast-stress`
只运行 AST bulk RSS/time lane。三条 stress lane 默认使用 `--shape=mixed`：generic lane 覆盖
generic struct/enum/type alias、where constraint、impl method、pointer alias、tuple/pair、slice
和 pattern matching；AST lane 覆盖 extern、type alias、struct/enum、impl/method、generic
constraint、tuple、array/slice、match/or-pattern、`let else`、`if is`、`try`、`defer`、
for/while/range、compound assignment、unsafe pointer/string builtin，并在 2M bulk 中抽样保留复杂
feature block；diagnostic lane 循环覆盖 unknown name、type mismatch、call arity/type、field/index、
struct literal、enum payload、builtin、generic apply、array/void、operator 和 match-arm 类型错误。
`tools/generic_stress.py` / `tools/ast_stress.py` / `tools/diagnostic_stress.py` 现在支持
`--max-elapsed-ms`、`--max-rss-mib`、`--threshold-profile` 和 `--threshold-scale`；三条 lane 共用
`tools/perf_thresholds.py`，把 raw thresholds、profile、scale、machine info、effective thresholds、
进程级 wall/user/sys/RSS/page fault 指标、以及 `aurexc --profile-output` 产出的
`aurex-profile-v1` 阶段 profile 写入 JSON。`make perf-stress-threshold` 默认跑 100/200 generic +
1000/5000 AST bulk + 100/500 errors 的轻量阈值门，GitHub Actions `stress-thresholds` job 固定
`AUREX_PERF_THRESHOLD_PROFILE=github-ubuntu-24.04-fedora`；`make perf-release-threshold` 现在默认用
独立 `build-perf-lto` + `AUREX_STRESS_ENABLE_LTO=ON` 跑 5000 generic、2M AST 和 5000 errors 的
Release+LTO 发布阈值门。`make perf-release-lto-threshold` 和 `make perf-release-all-threshold`
保留为同一发布阈值门的兼容别名，不再重复跑普通 Release 与 Release+LTO 两套逻辑；发布 AST RSS 阈值为
8192 MiB，用来保留高复杂 mixed 源码而不是降级成过渡态 toy case。
generic function instance 签名、
generic struct/enum `TypeInfo` 和 checked enum case display 已经把内部 semantic key / TypeHandle
args 和展示名分离，`--check` 热路径不再为了 checked signature 或泛型类型实例生成
`id[i32]`、`Box[i32]`、`Maybe[i32]_some` 这类展示字符串，dump、IR lowering 和诊断需要时再延迟格式化。
`--check` 模式不再保留泛型实例 side table；`--emit=typed` 会保留 typed generic body 但不做 lowering，
用于把 retained-side-table 内存和 IR/codegen 成本分开压测。`tools/generic_stress.py --shape=templates` 覆盖 2000/5000+ 不同泛型模板场景；IR/native 输出模式继续保留 lowering 所需表，保证 codegen 行为不退化。
AST 主路径也已按 P0-Perf-4 收口：driver 持有 parser/module AST 并把 mutable 引用传给 sema 和 IR lowering，
`SemanticAnalyzer(const AstModule&)` 被删除以避免隐式整树复制，`CheckedModule::normalized_ast` 已改成轻量
normalization overlay，彻底不拥有 `AstModule` snapshot，sema 构造期不再对 `exprs/types` 做 `size+4096`
reserve，postfix suffix 创建不再按值复制胖 `ExprNode` / `TypeNode`。节点级 C symbol side table 也已从
`std::vector<std::string>` 改为 `expr_c_name_ids` / `pattern_c_name_ids` / `item_c_name_ids` 的 `IdentId`
表，实际文本只进入 checked module 的 C-name interner 去重，不再每个节点分配一个 `std::string`。
2026-05-15 的 compact AST 主线已继续落地：`TypeNode`、`ExprNode`、
`PatternNode`、`StmtNode` 和 `ItemNode` 的主存储从胖节点 vector 改成 32B compact header +
per-kind payload arena，module loader 合并模块时按 compact payload remap/append，不再依赖
fat-vector 地址或 `.data()` 指针反推 ID；sema 的 item owner 查询也改为显式 `ItemId`，不再靠
`items.data()` 地址反推。2026-05-16 继续把 sema、IR lowering 和 AST dump 的 `ExprNode` 热路径迁到 compact
view / payload 直读，移除 `ExprNode` 兼容 wrapper 和 literal 分析里的胖节点重建；同日继续落地
global bump allocator + syntax 层 `IdentifierInterner`，AST 的 type/expr/pattern/stmt/item/module/import
名字字段携带原生 `IdentId`，parser、module loader、postfix suffix 创建和 sema 入口都会把节点/metadata
重新收口到当前 `AstModule` 的 identifier arena，sema typed lookup key 不再维护第二套私有 interner；函数、类型、值、
generic template、enum case、struct field、method/member 和局部 scope lookup 都使用 `IdentId` typed 索引，
checked-module map 不再保留并行 string-key lookup 路径；生成的 ABI/display/dump 文本进入 bump-backed
`IdentifierInterner`，`FunctionSignature`、`Symbol`、`StructInfo`、`EnumCaseInfo`、`TypeAliasInfo` 和 `TypeInfo`
只保存 `InternedText` / typed id，不再持有堆分配 `std::string` payload。2026-05-16
性能线随后删除旧胖 `ExprNode` 生产类型，parser 创建、`AstModule` 存储、module loader append 和 postfix
suffix 都直接写 compact expression header + per-kind payload。随后又把 parser 表达式创建 API
收紧到字段级 append/set：name、unary/binary、call、if/block/match、array、field/index/slice、
struct literal、generic apply、try 和 cast-like 节点都不再先构造 payload 大临时对象；postfix suffix
的 call/field/index/slice/generic apply/struct literal/try 路径也直接写 compact payload。2026-05-16 后续 bump pass 又把
`TypeNodeList` / `ExprNodeList` / `PatternNodeList` / `StmtNodeList` / `ItemNodeList` 的 header vector 和
per-kind payload vector 接到 `BumpAllocatorAdapter`，`IdentifierInterner` 的 text vector、hash bucket/node
也进入同一个 bump arena；parser 会根据 token 形态估算 statement/item/type/pattern/identifier 规模，并在 AST
模块创建初期按 token 形态对 expression header 和每类 payload 计算容量上界，直接从 bump arena 取好 vector backing storage 并对表达式 arena page pre-touch；parser 创建表达式节点时只在这些已分配区间内顺序 emplace，不再依赖 vector 热路径自动扩容，也避免解析过程中逐节点首次触达新页。后续 lexer/sema bump pass 已把 lexer token 输出改为 `TokenBuffer`，token vector backing storage 由 bump arena 持有并一次性 reserve；token 容量不再被 262144 上限截断，也不再预触永远不会写入的估算余量页，避免 bump vector 扩容留下旧 backing storage。sema 的 `CheckedModule`、`GenericSideTables`、`PatternCaseNameTable`、`TypeTable`、`SymbolTable`、analyzer lookup/cache 主表、`FunctionSignature` 参数/generic args、`StructInfo` 字段、`EnumCaseInfo` payload 列表、`TypeInfo` tuple/function/generic args、generic template 参数列表和 generic constraint bucket 也改为 bump-backed storage；sema 持久 name / c_name / value_text / generic key 字段改为 `InternedText`，复制 `CheckedModule` / `TypeTable` / `SymbolTable` 时会重新 intern 到目标 arena，不再复制字符串 buffer。generic function instance 列表使用 bump-backed deque 保持元素地址稳定；retained generic instance 使用函数体局部 NodeSpan side table，只有非连续节点 ID 映射才共享 module-level sparse layout，不再按实例复制 sparse ID mapping vector；generic method / enum-case / visible-module cache bucket 不再由 `operator[]` 创建普通 heap vector；IR lowering 的源码 local lookup 和 IR verifier 的 symbol 去重也改为 interned typed id，不再保留持久 string-key map。
2026-05-16 后续补齐了 source-name 去重：`FunctionSignature.name`、`Symbol.name`、`StructInfo.name`、
`StructFieldInfo.name`、`TypeAliasInfo.name`、`EnumCaseInfo.name/case_name/enum_name`
等源代码已有 identifier 的字段在普通 driver 路径直接借用 `AstModule::identifiers`，不再把函数名、字段名、
局部名再复制进 checked C-name interner；`CheckedModule` move 只重绑原本属于自身 `c_names` 的
`InternedText`，不会把 borrowed AST name 错绑到 checked interner，显式 copy 才重新 intern 到目标模块。
ABI 校验也从临时 `IdentifierInterner` 改为 `std::string_view` key，避免校验阶段把所有 C symbol 再复制一遍。
`tools/ast_stress.py --skip-build --counts 100000 --shape mixed` 本机 baseline 中，100000 mixed AST
bulk statements 约 96.3 MiB RSS / 77.9 ms；当前默认 Release+LTO 发布阈值门中，5000 generic 约
450.5 MiB / 13073.0 ms，2M 高复杂 mixed AST 源码约 106820 KiB、4325.9 MiB / 2841.3 ms，
5000 diagnostic errors 约 32.9 MiB / 66.7 ms。2M AST 阶段 profile 显示 module.read 约
27.2 ms / 阶段后 227.1 MiB，module.lex 约 247.7 ms / 1291.3 MiB，module.parse 约
1130.0 ms / 3468.1 MiB，sema.analyze 约 1141.8 ms / 4325.9 MiB。Google Benchmark
`sema_ast_bulk/1024` 约 128 ns/expr；`tools/frontend_compare.py` 本机 baseline 中 Aurex `--check` lookup/96 约 10.1 ms、
generics/96 约 9.6 ms，Clang++ 分别约 21.2 ms / 24.3 ms，G++ 分别约 25.1 ms / 24.3 ms。
generic side table 生命周期已在主路径收口：sema-only expected-type 和 pattern-case cache 进入可释放 arena 并在分析后丢弃；retained instance 只保留 lowering 需要的表；非连续 NodeSpan sparse ID mapping 按模板共享；小型 per-instance side table arena 从默认 64 KiB floor 降到 1 KiB block，覆盖 2000/5000+ 不同泛型模板的固定开销。跨模块 stable hash、parallel global ID、轻量 generic/AST/diagnostic 阈值门、默认 Release+LTO 的 5000 generic / 2M 高复杂 AST / 5000 errors 发布阈值门、8 GiB AST RSS 阈值、阶段级 profile，以及 profile/scale 形式的跨机器阈值校准机制已接入；后续只保留新增机器 profile 数据和 query 级增量复用，不再缺身份或 release gate 主路径。
2026-05-17 M2.5 起步后，诊断协议先做了结构化收口：语义诊断现在由显式 semantic kind 映射到稳定 category/code，message 只保留展示职责；CLI 文本、JSON 输出和后续 diagnostics query / LSP 适配将共享同一事件语义。下一批 M2.5 任务转向 query key、依赖跟踪、lossless syntax 和 IDE-native 入口，而不是继续扩张语言面。
2026-05-17 正则性能/测试线继续把 `RegexSet` exact-literal prefix trie 推进为持久标量 Aho-Corasick fast path：纯字面量 set 构建共享 trie 后补 failure/output link，`matches_set`、`find_set`、first-match scan、all-span/overlap scan 和 vectored flatten 后入口都用同一份自动机线性扫描；database 升级为 v3，序列化 node/terminal/max-literal 元数据，roundtrip 后不退回 VM active-list。测试侧补上 Unicode byte span、suffix failure output、重复 literal、database fast path workspace 和 deterministic RegexSet property corpus；`tools/regex_differential.py` 现在同时生成固定 + property Python `re` 差分、RegexSet exact-literal property cases、Unicode 17.0 full case-fold 与 UAX #29 `\X` conformance 程序，作为 CTest slow conformance 入口。
2026-05-16 后续表达式 P0 语义线把 expression type cache 从 final-only 记录拆为三层：
`expr_intrinsic_types` 保存表达式自身类型，`expr_types` 继续保存当前语义使用的 contextual final type，
`expr_expected_types` 作为 final cache key，`CoercionRecord` 记录 contextual integer/float literal、`null`
到 pointer、slice mutability 等调整。主模块和 generic side table 都有对应 intrinsic/final 存储，local dense
和 sparse fallback 行为一致；integer/float/null、unary/binary、slice、array/tuple literal、if/block/match
在 expected type 下会保留 intrinsic type，不再把 expected type 污染成表达式自身类型。IR lowering 仍读取
`expr_types` final table，coercion/intrinsic 只作为 checked 语义 overlay。
同日后续工程质量线把 `analyze_expr` 从单一大 dispatch 收口为一条明确主路径：
`analyze_expr(expr, expected)` 只负责 final cache lookup / expected key 记录，`analyze_expr(expr, view, expected)`
只做表达式类别调度；literal、value/name/call、control、aggregate、projection、operator、builtin 分别进入小型
helper。二元表达式内部也拆成 operand contextual typing、类型不匹配诊断、整数字面量 hazard 检查和 operator
result 记录，没有保留并行的新旧 analyzer 路径。
2026-05-16 后续 match 性能/正确性线又把结构化穷尽检查从“枚举 bool / enum 叶子笛卡尔积 + 4096 组合上限”替换为
pattern matrix / usefulness witness search：bool、enum payload、tuple、struct、fixed array、open integer literal
和 dynamic slice 通过 constructor specialization、default matrix 和 slice 代表长度特化判定覆盖和 unreachable arm；
无 guard 和字面量 true guard 计入覆盖，字面量 false 和动态 guard 不计入覆盖。动态 slice 不再只能靠 `[..]`
兜底，`[]` + `[_, ..]`、bool head partitions 等有限代表长度覆盖可被证明；开放整数域 literal 也进入
usefulness constructor，重复 literal arm 会被判为 unreachable，缺少剩余整数域时输出 open-domain wildcard 诊断。

当前 `build` 目录可能不是完整测试配置；可信验证应以 `tools/run_tests.sh` 重新 configure/build/ctest 为准。

## M2 当前短板

M2 的核心短板集中在语言地基，不在标准库规模：

- block statement 和 block expression 主体规则已统一；expression block 可完整承载普通 statement，并额外要求 final expression。
- const initializer 已补齐纯标量运算；当前仍没有函数调用、控制流表达式或完整 comptime。
- compound assignment 已补齐；`++` / `--` 自增自减语法已从 M2 基础语法移除，统一使用 `+= 1` / `-= 1`。
- 基础 range-for 已补齐为 `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)`；当前仍没有容器迭代、slice 迭代或 iterator protocol。
- trailing separator 策略已冻结：圆括号/方括号列表允许 trailing comma，comma 分隔花括号列表允许但不强制最后一个 comma。
- 现代基础语法继续收口：ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer 和基础字面量体系已完成。
- default private 已完成：顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public。
- `pub fn` 返回类型已收紧为必须显式；private helper 仍可推导。
- lexer 已支持嵌套 `/* ... */` 块注释。
- enum 已支持 ADT-first 形态：普通 enum 可省略 base type 和 discriminant，tag 自动分配；多字段 payload 可用 `.case(a, b)` pattern 按字段解构；显式 `enum Status: u8 { ok = 0, err = 1 }` 仍作为 C-like/repr enum 形态保留。generic enum 已进入 M2 基线，`Option[T]` / `Result[T, E]` 这类类型参数 ADT 可被实例化和匹配。
- 数组、slice、tuple、函数指针和字面量基础语法已闭合：固定数组支持 `[1, 2, 3]` 和 `[0; 128]`，borrowed slice 支持 `[]const T` / `[]mut T` 以及 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`；tuple 支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、局部 `let (a, _) = value;` 解构和 tuple pattern；匿名 tuple 不支持直接字段访问，需要字段访问时使用 named struct；函数类型支持 `fn(i32) -> i32`、`extern c fn(*const u8, ...) -> i32`、函数名作为值、局部/参数/字段函数指针间接调用；字面量支持普通字符串、C 字符串、raw/multiline raw string、byte string、byte literal、Unicode scalar `char` 和整数/浮点类型后缀。
- 最小 `unsafe` 已落地：`unsafe { ... }` 可作为 statement 或 expression，`unsafe fn` 和 unsafe 函数指针类型会把调用限制在 unsafe context 内；raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 都必须在 unsafe context 中使用。
- 泛型已支持最小非资源类 `where` capability predicate：`Sized`、`Eq`、`Ord`、`Hash`。`Copy` / `Drop` 等资源类约束、用户自定义 trait、associated type、const generic 和 trait object 仍暂缓。
- 泛型边界已扩展到 generic struct / function / enum / type alias，以及 `impl[T] Box[T]` 这类 owner generic impl；method-local generic parameter 仍不属于 M2。
- M1 的语言级 `noncopy` / `move` MVP 已从 M2 基线删除。当前先保留普通值语义和必要的数组/含数组类型限制；copy/drop/borrow/ownership 暂缓为后续资源语义专题。
- 最小 safe reference 已落地：`&T` / `&mut T`、`&place` / `&mut place`、reference 安全解引用、`&mut` 可写 place 检查、`&mut T` 到 `&T` 的只读化赋值，以及按 pointer-sized ABI lowering。raw pointer 解引用仍必须在 `unsafe` 中；borrow checker、lifetime、borrowed return、alias/resource 语义继续暂缓。
- `str` 已有语言级雏形，普通数组/slice 地基已落地，`strraw` 已纳入 unsafe；M2 no-std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool`、`strfromutf8(bytes) -> str` 和 `text[l:r]` checked slicing。`strfromutf8` 失败或 `str` 切片越界/落在 UTF-8 continuation byte 上时返回空 `str`，不会把无效输入包装成 `str`。更完整的 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 仍后置到库层重建。

当前完整语法库存、已支持高级能力、未完成特性和基础语法优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

## 当前结论

M2 的正确目标是先把基础语法和核心语义做窄做稳，再谈标准库、自举和构建工具。当前编译器已经能支撑语言核心实验和 native 输出，但不应把 M1 的 std/selfhost 经验继续当作有效路线推进。

下一步最重要的是继续冻结 M2 语法基线。ADT-first enum、generic enum、enum multi-payload destructuring、数组值语法、slice type/expression、tuple/destructuring、function pointer / function type、字面量体系、最小 unsafe 边界、最小 safe reference、M2 pattern ergonomics、最小非资源类 `where` capability 和 `str` checked UTF-8 边界已经落地；pattern 当前支持 tuple match pattern、slice pattern、struct pattern、nested enum payload destructuring、局部 struct/slice/enum destructuring、binding or-pattern alternatives、`let ... else`、`if value is pattern` / `while value is pattern`，以及 if 表达式 pattern condition。结构化 match 穷尽检查已使用 pattern matrix / usefulness witness search 覆盖 bool、enum payload、tuple、struct、fixed array、open integer literal 和 dynamic slice，不再枚举笛卡尔积；guard 已区分无 guard、字面量 true/false 和动态表达式，动态 slice 长度和开放域 witness 已进入当前 M2.1 主线。

# M7 Hardening Performance Closure

本文件记录 M7c/M7d 进入下一阶段前的硬化闭环，覆盖本轮要求的五件事：
`u32/i32` 审计、重复扫描/O(n²) 热点优化、现代性能工具实测、incremental cache/LSP/tooling 专项回归、
以及 M7 稳定性结论。

## 1. 整数宽度审计

本轮继续遵循一个边界：**计数/容量/权威性 fingerprint 计数能扩到 `u64` 就扩到 `u64`；ID、index、稳定 key
schema 和外部格式字段不做无计划宽化**。否则会把局部性能硬化变成 query schema / AST identity / dump 格式迁移。

已扩到 `u64` 或保持 `u64` 的字段：

- `TypeCheckBodyAuthority` 中的 side-table、coercion、borrow summary、borrow contract、lifetime、body loan 等 count。
- `GenericInstanceBodyAuthority`、`GenericInstanceSignatureAuthority`、`GenericTemplateSignatureAuthority` 和
  `ItemSignatureAuthority` 的 count 字段。
- `GenericTemplateSignatureInfo::param_count` / `constraint_count`。
- 本轮新增扩展 `NormalizedAstOverlay::{original,final}_{expr,type}_count`，从 `usize` 改为 `u64`，避免
  checked-module 语义计数随宿主位宽变化。

审计后刻意保留 `u32` 的类别：

- AST / sema / IR handle：`TypeId`、`ExprId`、`PatternId`、`StmtId`、`ItemId`、`ModuleId`、`TypeHandle`、
  `SymbolId`、`SourceId`、IR `ValueId` / `BlockId` / `FunctionId` 和 query `QueryNodeId`。
- 稳定 query key schema：`StableModuleId::part_count`、`ModulePartKey::stable_index`、
  `DefKey` / `GenericParamKey` index、`GenericInstanceKey::predicate_count`、
  `CanonicalTypeKey::function_param_count`、`StableKeyWriter::write_u32` 格式字段。
- lifetime / body-flow / body-loan 的 point、region、action、loan、origin、predicate index。它们当前被
  checked fact schema、dump、fingerprint 和 vector index 共同约束。
- `receiver_arg_count`、`part_index`、`param_index`、`predicate_index` 等小域 index。`receiver_arg_count` 当前只表达
  receiver 是否占用第一个参数槽。
- IDE/tooling 中用于稳定 fingerprint 的 schema 字段，例如 item generic 参数个数混入 `mix_u32` 的位置。

审计结论：当前没有发现仍应直接宽化的 `i32`/`u32` 大规模计数字段；剩余 `u32` 主要是身份、index、稳定格式或有界小域。
这些字段如需 64 位化，应作为单独的 schema migration，配套 stable key 版本、dump/golden 更新、incremental cache
兼容策略和 LSP/tooling DTO 迁移。

## 2. 重复扫描和算法优化

本轮落地两处热路径优化。

第一处是 statement control-flow query cache。`block_guarantees_return`、`stmt_guarantees_return`、
`block_may_fallthrough` 和 `stmt_may_fallthrough` 原本每次查询都会重新迭代扫描对应 statement/block 子树。
现在 `SemanticAnalyzerCore::ControlFlowQueryCache` 按 `StmtId` 保存四类结果，并且迭代求值过程中会把子语句/子块的
结果也写入缓存。对同一个 analyzer 内的重叠控制流查询，复杂度从 `O(Q * subtree)` 收敛为每个 query kind 近似
`O(unique statements + Q)`。实现仍然是显式栈迭代算法，没有引入递归。

第二处是 body loan precheck 合并。`may_need_local_loan_check` 过去会对每条语句先扫一遍“是否可能绑定 reference
loan”，再扫一遍“是否存在 two-phase receiver”。现在 `expr_may_need_local_loan_check` 在一次表达式/嵌套语句遍历中同时检查
reference loan shallow binding 和 two-phase receiver call binding。纯表达式/赋值压力下减少一次重复子树遍历，同时把
`StmtNode` 热路径读取改为 `const&`，避免 AST payload 的拷贝。

这两处优化都保持原有 conservative 语义：不会因为缓存而跳过 malformed/invalid id 的默认结果，也不会把 unknown/raw
borrow proof 误判为 safe proof。

## 3. 性能工具和实测数据

本轮加入可复用脚本 `tools/m7_hardening_perf.py`。默认输出目录为 `build/m7_hardening_perf/`，包含：

- `frontend_bench.json`：Google Benchmark JSON。
- `query_unit_hyperfine.json`：hyperfine JSON。
- `summary.md`：工具版本、benchmark 文本、hyperfine 文本和 `/usr/bin/time -v` 文本。

已验证工具：

- Google Benchmark：`build/perf/bin/aurex_frontend_bench --benchmark_min_time=0.2s`。
- hyperfine：`hyperfine 1.20.0`。
- `/usr/bin/time -v`：可用，用于 RSS、page fault、context switch。
- `perf`：已安装，版本 `7.0.10-201.fc44.aarch64`，但当前容器内 `perf stat true` 不可用；本轮不把 perf counter
  作为通过条件。

同机、同命令口径对照使用基线提交 `eef0c25b0b50a6851fd79b3049d942ee68563da1` 的临时 worktree 和当前工作树，
均为 Release benchmark、`--benchmark_min_time=0.2s`。CPU time 单位为 ns。

| case | 基线 CPU | 当前 CPU | 变化 |
| --- | ---: | ---: | ---: |
| `BM_LexMixed/64` | 326730 | 328216 | +0.45% |
| `BM_LexMixed/128` | 761019 | 761727 | +0.09% |
| `BM_SemaLookup/96` | 5846244 | 5869678 | +0.40% |
| `BM_SemaLookup/192` | 14101242 | 14293369 | +1.36% |
| `BM_SemaGenerics/64` | 9958871 | 9978566 | +0.20% |
| `BM_SemaGenerics/128` | 22800884 | 23023081 | +0.98% |
| `BM_SemaAstBulk/1024` | 4664270 | 4679396 | +0.42% |
| `BM_SemaAstBulk/4096` | 20339915 | 20352088 | +0.06% |

结论：本轮优化没有引入可见性能回退；所有 broad frontend case 都在当前容器噪声范围内。当前 `SemaAstBulk` 的
4x statement 规模 CPU 比例约 `4.35x`，保持近线性；`lookup/generics` 的 2x 规模比例仍主要受模块/泛型查找和
单态化开销影响，后续若继续优化应单独做 lookup candidate index 与 generic instantiation cache 的专题。

本轮脚本实测：

- `hyperfine --warmup 1 --runs 3 QueryUnit.*`：`732.6 ms +/- 169.2 ms`。容器负载较高，hyperfine 报告 outlier。
- `/usr/bin/time -v` focused sema：45 个控制流/body-loan/lifetime 白盒测试，gtest wall `30 ms`，
  `/usr/bin/time` wall `0.06 s`，max RSS `46184 KB`，exit status `0`。

## 4. 回归专项

本轮代码层新增/加强回归：

- `SemanticWhiteBoxStatementControlFlowQueries` 继续验证 invalid id、return/break/continue、block、if/else-if
  的 guarantee/fallthrough 语义，并新增控制流 cache bucket 填充断言和重复查询断言。
- `SemanticWhiteBoxParserOnlyModuleContractIsNormalized` 增加 `NormalizedAstOverlay` count 字段的 `u64`
  静态断言。
- body-loan/lifetime 专项覆盖 `BodyLoan`、`Borrow`、`LifetimeFacts` 相关白盒和集成测试。

已执行的专项命令和结果：

```sh
cmake --build build/full-llvm-fedora --target aurex_tests aurex_frontend_tests aurex_query_tests -j$(nproc)
build/full-llvm-fedora/bin/aurex_tests --gtest_brief=1 --gtest_filter='CoreUnit.SemanticWhiteBoxStatementControlFlowQueries:CoreUnit.SemanticWhiteBoxParserOnlyModuleContractIsNormalized:CoreUnit.*BodyLoan*:CoreUnit.*Borrow*:CoreUnit.*LifetimeFacts*:AurexIntegrationTest.*Borrow*:AurexIntegrationTest.*Lifetime*'
build/full-llvm-fedora/bin/aurex_tests --gtest_brief=1 --gtest_filter='QueryUnit.*'
build/full-llvm-fedora/bin/aurex_tests --gtest_brief=1 --gtest_filter='AurexIntegrationTest.IncrementalCache*:CoreUnit.ToolingSession*:CoreUnit.ToolingWorkspaceIndex*:CoreUnit.Lsp*:CoreUnit.IdeTooling*'
```

实测结果：

- `aurex_tests`、`aurex_frontend_tests`、`aurex_query_tests` 构建通过。
- 文档专项：2 个测试通过。
- M7 control-flow/body-loan/borrow/lifetime 专项：75 个测试通过。
- `QueryUnit.*`：78 个测试通过。
- incremental cache / tooling session / workspace index / LSP / IDE tooling 专项：78 个测试通过。
- `aurex_frontend_tests --gtest_brief=1`：356 个测试通过。
- `aurex_query_tests --gtest_brief=1`：78 个测试通过。
- `aurex_tests --gtest_brief=1`：695 个测试通过，`elapsed=106.557 user=57.436 sys=16.238`。
- coverage target 的 12 个 CTest 分组全部通过；覆盖率门槛中 line `95.05%` 和 function `98.72%` 通过，
  global region `94.43%` 低于 `95.00%` 门槛，因此 coverage target 以 region gate 失败收尾。当前缺口主要来自
  `sema_statement_analyzer.cpp`、IDE tooling 和若干既有 sema 大文件的未覆盖分支；这不是运行失败或本轮新增逻辑
  未测，而是全局 region 覆盖率仍需单独补齐。

## 5. 稳定性结论

M7c-C storage escape 的旧二次热点已经在上一轮收口，本轮没有重新引入旧扫描。新增缓存和合并预检只影响
analyzer-local transient state，不进入 public checked facts，不改变 query key schema，也不改变 LSP/tooling DTO。

当前可以继续推进 M7d-A dropck facts，但后续仍建议保留三条 gate：

- frontend Google Benchmark broad matrix 不允许出现无解释的大幅回退。
- `QueryUnit.*`、incremental cache、LSP/tooling snapshot 必须随 M7d facts 投影持续跑。
- 若后续要把 `u32` handle/index 宽化为 `u64`，必须作为单独 migration，不应混在普通性能优化提交中。

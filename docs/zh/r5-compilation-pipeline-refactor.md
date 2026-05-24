# R5 Compilation Pipeline / Driver Action 重构

## 当前优先级

R5 是当前编译器工程化重构主线的最高优先级。它优先于 M3 模块系统、M3 泛型闭环、LSP protocol adapter、完整 subtree reparse、RAII、trait、closure、iterator、async、macro、package manager 和标准库重建。

这不是新的语言功能阶段，而是把 Aurex driver 从“顺序编译函数”推进到现代编译器的 session + pipeline 架构。M3 后续能力必须消费这条主路径，不能绕过 driver、query、diagnostics 或 cache 边界另开旁路。

## 参考模型

R5 借鉴 Clang / LLVM 的工程边界，但不照搬它们的复杂模板和插件体系：

- Clang 的 `CompilerInvocation` 思路：命令行解析后的配置对象必须保持纯数据属性。
- Clang 的 compiler instance / frontend action 思路：一次编译的 source、diagnostics、profile、cache、backend emitter 等运行时服务应由 session 持有，而不是散落在 `Compiler::run` 的局部变量和长参数链中。
- LLVM pass pipeline 思路：阶段之间应显式声明输入、输出、invariant、verifier gate 和后续 analysis invalidation 边界。
- rustc query 思路：前端阶段产物应通过 stable key、dependency edge 和 cache/reuse 进入统一 query 主路径。

R5 的取舍是：保留 Aurex 现有轻量 C++20 风格，不引入热路径虚调用、不引入泛型化过度的 pass 框架、不改变语言语义和输出协议。

## 目标架构

R5 的目标结构如下：

```text
CLI
  -> CompilerInvocation
  -> Compiler facade
  -> CompilationSession
       - SourceManager
       - DiagnosticSink
       - CompilationProfiler
       - LlvmIrEmitter
       - cache/profile policy
  -> CompilationPipeline
       -> FrontendPipeline
            - source/tokens/lossless stage
            - module/frontend stage
            - semantic stage
            - incremental cache write stage
       -> LoweringPipeline
            - checked dump stage
            - AST + checked module -> Aurex IR lowering stage
            - IR verifier / optimization pass manager stage
            - IR dump stage
       -> BackendPipeline
            - LLVM IR emission stage
            - LLVM IR dump stage
            - temporary LLVM IR file stage
            - clang native artifact stage
       -> PipelineStage records
            - stage id / input / output / profile name
            - diagnostic ownership
            - cache/query impact notes
```

### 边界要求

- `cli.cpp` 只负责参数解析、帮助文本、版本输出和 CLI exit code 策略。
- `CompilerInvocation` 继续是纯配置，不持有一次编译的 mutable 状态。
- `Compiler` 退回 public facade，只负责注入 backend emitter 并启动 pipeline。
- `CompilationSession` 持有一次编译的运行时服务和 finish/profile 逻辑。
- `FrontendPipeline` 持有 source/token/lossless/module graph/AST dump/sema/cache write 等前端阶段入口。
- `LoweringPipeline` 持有 checked dump、IR lowering、IR pass pipeline 和 IR dump 的阶段入口。
- `BackendPipeline` 持有 LLVM IR emission、LLVM IR dump、temporary LLVM file 和 clang native invocation。
- `PipelineStage` 固定 driver 主阶段的 profile name、输入/输出、diagnostic owner 和 cache/query 影响面。
- `ModulePassManager` 固定 IR pass 顺序、`PassResult`、`PreservedAnalyses` 和 verifier gate，不让具体 pass
  在一个总控函数中隐式修改 analysis 契约。
- `CompilationPipeline` 只负责编排阶段和 emit mode 分发，不实现 parser、sema、module loader、lowering 或 backend 细节。
- diagnostics 文本 / JSON 渲染继续只走 `diagnostic_renderer`，不允许在 pipeline 内复制协议拼接逻辑。
- incremental cache、query、module loader、sema、IR lowering 和 native backend 继续保留各自 owner，不把实现细节反向塞回 driver。

## 第一轮实现范围

第一轮只做行为保持型结构拆分：

1. 新增 driver 内部 `CompilationPipeline`。
2. 新增或内聚一次编译的 `CompilationSession`，统一持有 `SourceManager`、`DiagnosticSink`、`CompilationProfiler` 和 `LlvmIrEmitter`。
3. 把 `Compiler::run` 从完整编排函数收缩为 facade。
4. 保持所有 emit mode 行为不变：`tokens`、`lossless`、`ast`、`modules`、`check`、`typed`、`checked`、`ir`、`llvm_ir`、`assembly`、`object`、`executable`。
5. 保持 profile phase 名称不变，避免 perf/stress/profile 基线漂移。
6. 保持 diagnostics JSON/text 协议不变。
7. 保持 incremental cache reuse/write 行为不变。

这一轮不引入新的 pass manager，不重写 IR lowering，不重写 module loader，也不改变 M3 模块/泛型设计。

## 当前落地状态

R5.1 已完成 `Compiler` facade 和内部 `CompilationPipeline` 拆分：public driver API 保持窄入口，完整编译编排从
`compiler.cpp` 移入 driver 内部 pipeline。

R5.2 已完成第一轮前端拆分：

- `CompilationSession` 从 pipeline 局部实现中独立出来，统一持有一次编译的 source、diagnostics、profile 和
  backend emitter。
- `FrontendPipeline` 接管 cache lookup、source read、tokens/lossless dump、module loading、AST/modules dump、
  semantic analysis 和 checked incremental cache write。
- `CompilationPipeline` 继续保留全局阶段顺序、checked dump、IR lowering、LLVM IR dump 和 native artifact 分发。
- profile phase 名称、emit mode 停止点、diagnostics 渲染路径和 incremental cache 行为保持不变。

R5.3 已完成 lowering/backend/阶段契约拆分：

- `LoweringPipeline` 接管 checked dump、AST + checked module 到 Aurex IR 的 lowering、IR pass pipeline
  入口和 IR dump。
- `BackendPipeline` 接管 LLVM IR emission、LLVM IR dump、临时 LLVM IR 文件写入、clang native invocation
  和 native 输出路径校验。
- `PipelineStage` 记录集中维护 driver 主阶段的 profile name、输入/输出、diagnostic ownership 和
  cache/query 影响面，避免 phase 名称继续散落在 pipeline、frontend 和 module loader 中。
- `CompilationPipeline` 现在只负责 cache/frontend/sema/lowering/backend 的顺序编排、emit mode 停止点和
  error/profile finish，不再直接实现 lowering 或 backend 操作。
- 原 profile phase 名称保持不变：`checked.dump`、`ir.lower`、`ir.pass_pipeline`、`ir.dump`、
  `llvm.emit_ir`、`llvm_ir.dump`、`llvm.write_temp` 和 `native.clang` 等仍是兼容契约。

R5.4 已完成 IR verifier / pass manager 第一层：

- 新增 `include/aurex/ir/pass_manager.hpp` 和 `src/ir/pass_manager.cpp`，提供轻量
  `ModulePassManager`、`ModulePass`、`PassResult`、`PreservedAnalyses`、`VerifierGate` 和
  `PassPipelineRunSummary`。
- `run_pass_pipeline` 保持兼容旧调用方；新增 `run_pass_pipeline_with_summary` 返回 scheduled/executed pass
  数量、是否改变 IR、以及最终保留的 analysis 集合。
- `PassPipelineOptions` 增加 opt-in 的 `verify_after_each_pass`，默认关闭以保持现有性能和行为；input/output
  verifier gate 保持旧行为。
- 现有 local mem2reg 和 CFG cleanup 继续保持原优化语义，但现在通过 pass manager 声明 analysis
  preservation：mem2reg 保留 CFG/type/symbol/record layout，CFG cleanup 保留 type/symbol/record layout。
- 这一步不引入 LLVM 式虚接口层或模板化 analysis manager，也不改变 IR、diagnostics 或 backend 输出协议。

R5.5 已完成 IR analysis cache / invalidation 第一层：

- 新增 `include/aurex/ir/analysis_manager.hpp` 和 `src/ir/analysis_manager.cpp`，提供轻量
  `ModuleAnalysisManager`。
- `ModuleAnalysisManager` 惰性构建并缓存 CFG、dominance 和 value-use analysis；缓存绑定当前 `Module`
  地址，切换 module 时自动清空。
- `ModulePassRun` 现在接收 `ModuleAnalysisManager&`，pass 可以直接查询 analysis；`ModulePassManager`
  仍保留旧式 `run(module, verifier)` 入口，会为普通调用方创建内部 analysis manager。
- pass 报告 `changed = true` 后，`ModulePassManager` 会按 `PreservedAnalyses` 自动失效未保留 analysis；
  `changed = false` 的 pass 不触发缓存失效。
- dominance 依赖 CFG：如果 CFG 未被保留，dominance 会一起失效；如果只保留 CFG，不保留 dominance，
  dominance 单独失效。

R5.6 已完成 IR verifier diagnostics 上下文第一层：

- `VerifierGate` 现在为 input、after-pass 和 output verifier failure 统一加上稳定上下文：
  `stage=...`、`profile=...`、`verifier=input|after_pass|output`，after-pass 额外包含 `pass=...`。
- 现有 verifier 原始错误 body 和 `ErrorCode` 不改变；上下文只作为前缀包装，原始 body 保留在 `: ` 之后，
  例如 `return value value id is invalid` 仍保持原文本。
- after-pass failure 继续保留 `IR verifier failed after pass <pass>` 前缀，降低已有日志和测试的迁移成本。
- `PassPipelineOptions` 和 `VerifierGateOptions` 追加 `stage_name` / `stage_profile_name`，旧 aggregate
  初始化仍兼容；direct IR API 默认使用 `ir_pass_pipeline` / `ir.pass_pipeline`。
- `LoweringPipeline` 不再手写 IR pass pipeline stage 名称，而是从 `PipelineStageRecord` 读取
  `PipelineStageId::ir_pass_pipeline` 的 `name` 和 `profile_name` 并传入 IR verifier gate。`PipelineStage`
  因此继续作为 driver profile、cache/query 和后续 IDE/LSP 可视化的唯一阶段目录。

R5.7 已完成 profile JSON 阶段元数据第一层：

- `PipelineStage` 增加按 `profile_name` 反查 `PipelineStageRecord` 的接口，profile writer 不再只能输出
  裸 phase name。
- `aurex-profile-v1` 的每个 driver 主阶段 phase 现在附带可选 `stage` 对象，包含 stage id、input、output、
  diagnostic ownership 和 cache/query impact；原有 `name`、`detail`、elapsed/RSS 字段和 phase name
  全部保持不变。
- incremental cache 的内部 query diff / plan / pruning / provider-eval 仍作为 profile 子事件保留，不伪装成
  driver 主阶段；IDE/LSP 或 profile viewer 可以用 `stage` 对象识别主阶段，用 `name/detail` 继续识别子事件。
- `PIPELINE_STAGE_RECORD_COUNT` 改为由 `PipelineStageId::count` 推导，新增阶段时不会再手动同步 magic number。

R5.8 已完成 cache/query profile 子事件父阶段映射第一层：

- `PipelineStage` 增加 `PipelineProfileSubeventRecord` 目录，记录 internal profile subevent name 和
  parent driver stage，而不是让 profile viewer 或 cache/query 工具各自维护映射表。
- `incremental_cache.source_stage_reuse` 映射到 `incremental_cache.lookup`；
  `incremental_cache.query_diff`、`incremental_cache.query_plan`、`incremental_cache.query_pruning` 和
  `incremental_cache.query_provider_eval` 映射到 `incremental_cache.write`。
- profile JSON 对这些子事件输出可选 `parent_stage` 对象，字段仍来自父阶段的 `PipelineStageRecord`；
  子事件继续不携带 `stage` 对象，因此不会被误认为 driver 主阶段。
- 原 phase `name`、`detail` 字段和 query pruning/provider-eval detail schema 保持不变，现有 stress/perf
  parser 只会看到新增 JSON metadata，不需要重新解析 phase 名称。

## 后续拆分顺序

R5.8 完成后，后续按下面顺序继续：

1. 继续在 diagnostics 边界消费 `PipelineStageRecord`，而不是在各子 pipeline 中新增散落字符串。
2. 后续 IDE/LSP 可视化层消费 profile JSON 的 `stage`/`parent_stage` 对象和 query records，
   而不是重新维护阶段表。
3. 后续 M3 模块、泛型闭环和 LSP adapter 必须复用 `CompilationSession` + `CompilationPipeline`
   + `FrontendPipeline` + `LoweringPipeline` + `BackendPipeline` 的主路径。

## 验收标准

- public API 不扩大；`include/aurex/driver/compiler.hpp` 继续保持窄 facade。
- `Compiler::run` 不再直接持有完整编译流水线细节。
- 原有 driver、CLI、integration、IR、sema、query 测试全部通过。
- coverage gate 不降低；新文件应由现有 driver/integration 测试自然覆盖。
- `tools/format_check.py --base HEAD` 和 `git diff --check` 必须通过。
- 如果执行 perf gate，`make perf-stress-threshold` 不允许因为本次结构拆分出现明显退化。

## 非目标

- 不实现 M3 module part。
- 不实现 M3 query-backed 泛型闭环。
- 不实现 LSP protocol adapter。
- 不实现完整局部 subtree reparse。
- 不实现 RAII/drop/borrow checker。
- 不实现用户 trait、closure、iterator、async、macro 或 package manager。
- 不重建标准库。

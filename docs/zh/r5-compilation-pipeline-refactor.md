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
            - IR verifier / optimization pass pipeline stage
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

## 后续拆分顺序

R5.3 完成后，后续按下面顺序继续：

1. IR verifier / pass manager 主线：建立轻量 `Pass`、`Analysis`、`PreservedAnalyses` 和 verifier gate，
   不照搬 LLVM 全套模板复杂度。
2. 把 `PipelineStage` 记录作为 profile、cache/query 和后续 IDE/LSP 阶段可视化的唯一阶段目录继续维护。
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

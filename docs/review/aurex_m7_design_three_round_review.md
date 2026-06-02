# Aurex M7 Origin/Loan/Lifetime 设计三轮评审

日期：2026-06-02

评审对象：

- `docs/zh/m7-origin-loan-lifetime-design.md`
- `docs/zh/m7-roadmap.md`
- 全局 `compiler-engineering` skill 的 M7 指南

## 0. 总评

M7 设计基线方向正确：它没有把 Rust borrow checker 原样搬进 Aurex，而是选择把 M6 已有 resource facts、
`OwnedUseMode`、whole-local move analysis、cleanup/drop lowering、`BorrowEscapeAnalyzer` 和 borrow alias
hardening 提升成 `Place` / `Origin` / `Loan` / `Point` / `BorrowAction` / `BorrowSummary` 事实模型。

三轮评审后结论：

- 设计主方向可以进入 M7-WP2。
- 需要修正 Mojo 参考术语，避免把 Swift 的 `borrowing` / `consuming` / `inout` 误投射到 Mojo。
- `BorrowSummary` 必须支持 origin set/subset dependency，不能只支持单一 return origin。
- M7 实现路线必须增加 shadow fact/dump 阶段，先生成事实并和现有 M6 行为交叉校验，再启用拒绝性诊断。
- inferred return 的函数体分析顺序是实装风险点，M7 summary 生成不能假设 signature return type 已经最终可用。

## 1. 第一轮：源码贴合度评审

### 1.1 结论

设计报告对 Aurex 当前源码基线判断基本准确。

证据：

- `OwnedUseMode` 已经区分 `owned_consume`、`shared_borrow`、`mutable_borrow` 和 `place_only`，说明 M7 可以沿用
  M6 expression owned-use side table，而不是重造一套 use-mode 概念。
- `BodyMoveAnalysis` 已经有 `MoveAction`、`MoveBlock`、`MoveState`、`MoveEnvironment` 和 `BorrowEnvironment`，
  且数据流是 worklist solver；这支持“抽共享 BodyFlowGraph”的路线。
- 当前函数体分析顺序是 typed body check 后运行 `BorrowEscapeAnalyzer`，再运行 `BodyMoveAnalysis`。M7 替换
  `BorrowEscapeAnalyzer` 时，确实应该接在 typed body 已完成、move/resource facts 可生成的区域。
- IR lowering 已经消费 `expr_owned_use_modes` 并维护 cleanup/drop_if。M7 把隐式 cleanup/drop 纳入 borrow action
  timeline 是必要的。

### 1.2 发现的问题

**问题 A：`BorrowSummary` 不能是单一 origin dependency。**

当前设计报告里的摘要形状写成 `ReturnSlot -> OriginParam/static/unknown`，这对简单 `return &x` 足够，但对以下情况不够：

```aurex
fn pick(cond: bool, a: &i32, b: &i32) -> &i32 {
    if cond {
        return a;
    }
    return b;
}
```

正确摘要应该能表达“返回值依赖 `{a, b}` 这组可能 origin”，并且未来若引入显式 lifetime surface，需要能把这组依赖约束到同一个 output origin 或拒绝歧义。M7a 可以先保守接受/拒绝，但内部 summary 结构不能一开始写死成单值。

处理建议：

- `FunctionBorrowSummary` 使用 `ReturnOriginDependencySet`。
- 支持 `OriginDependencyKind::parameter_set`、`static_global`、`unknown`。
- diagnostics 能指出多来源 borrowed return 的所有候选来源。

**问题 B：inferred return 与 summary 生成顺序存在落地风险。**

当前函数体分析中，`analyze_borrow_escapes(function)` 和 `analyze_body_moves(function, signature)` 发生在
`finalize_inferred_return(...)` 之前。M7 summary 如果依赖最终 signature return type，会在 inferred return 函数上读到过早状态。

处理建议：

- M7-WP4 明确 summary 生成以 return expression/carrier type 为主，不依赖最终 signature return type。
- 或者拆成两阶段：先收集 return carrier facts，`finalize_inferred_return` 后再固化 `FunctionBorrowSummary`。
- 对 inferred return 函数补白盒测试。

**问题 C：删除 `BorrowEscapeAnalyzer` 的时机要有门槛。**

路线图说不依赖新增特判是对的，但不能在 M7-WP3 早期直接删除现有防线。应该先让新 checker 在 shadow mode 下生成 facts，
证明覆盖 M6 现有 negative matrix 后，再替换 `BorrowEscapeAnalyzer` 的拒绝性诊断。

## 2. 第二轮：语言设计与参考资料评审

### 2.1 结论

Rust NLL/Polonius、Swift ownership、C++ lifetime profile、Mojo ownership、Zig/Go 简洁性约束这些参考方向是合理的。
不过报告原文对 Mojo argument convention 的术语过度简化，需要修正。

### 2.2 发现的问题

**问题 D：Mojo 术语需要修正。**

当前 Mojo ownership 文档的 argument conventions 是 `read`、`mut`、`var`、`ref`、`out`、`deinit`，并且 `var` 常配合
caller-side `^` transfer sigil。报告原先写 `borrowed / inout / owned` 容易和 Swift SE-0377 混淆。

处理建议：

- Swift 章节继续保留 `borrowing` / `consuming` / `inout`。
- Mojo 章节改成 `read` / `mut` / `var` / `ref` / `out` / `deinit`。
- Aurex 未来 surface 可以借鉴这些“用户可读 convention”思想，但不要直接复制术语。

**问题 E：Rust NLL 的“value scope”和“reference lifetime”要显式拆开。**

M7 设计报告已经强调 CFG point/liveness，但还需要警惕把 lexical cleanup scope 与 reference live range 混成一个概念。
M6 cleanup/drop scope 决定 value 何时 cleanup；M7 loan liveness 决定 borrow 到哪里还会被使用。二者交叉，但不是同一个维度。

处理建议：

- 在设计文档中明确：drop/cleanup action 是 borrow checker 的 invalidating action，但 loan live range 不等于 value lexical scope。
- `Point` 模型必须支持“value 仍在 scope 内，但 loan 已因 last-use 结束”的 NLL 行为。

## 3. 第三轮：工程落地性评审

### 3.1 结论

路线图可落地，但需要增加实现安全阀：shadow facts、debug dump、parity test、feature gating。

### 3.2 发现的问题

**问题 F：M7-WP2/WP3 缺少 shadow fact/dump 阶段。**

直接把 borrow checker 接成拒绝性诊断，风险太高。M7-WP2 应先生成 body action/point/place facts 和 dump；M7-WP3 初期只在
test/debug 配置下比较 facts，不影响用户样例。

处理建议：

- 新增 borrow fact dump 或 checked dump section。
- M7-WP3 分成 `collect-only`、`diagnostic-shadow`、`enforced` 三步。
- 用现有 M6 negative samples 做 parity matrix。

**问题 G：query/cache/tooling key 必须从第一天设计。**

如果 `BorrowSummary` 晚于实现才接入 query，M7 后期会重做很多工作。summary 的 stable fingerprint、function body identity、
generic instance identity、trait requirement summary 都应该在 WP4 前被设计清楚。

处理建议：

- WP2 就预留 stable IDs。
- WP4 验收必须包含 summary fingerprint/query key。
- WP6 只是 projection 和 tooling 消费，不应该才开始思考 key 形状。

**问题 H：并行 hardening 任务与 M7 主线要更严格隔离。**

TargetInfo、typed query route table、CompileBudget、ModuleIdentity、LSP hardening 都值得做，但如果和 borrow checker 混在同一个
大改里，会导致回归难定位。

处理建议：

- 每个 hardening 任务单独 commit 和测试门槛。
- 影响 checked facts/query identity 的任务才允许和 M7 summary work package 同步设计。
- 不要在 M7-WP3 active-loan checker 同一提交里改 TargetInfo 或 LSP parser。

## 4. 已执行修正

本评审同时要求修正文档：

- 把 `BorrowSummary` 改成支持 origin dependency set/subset。
- 在路线图中加入 shadow fact/dump 和 enforcement staging。
- 修正 Mojo ownership 术语。
- 在 skill 中补充 summary set、inferred return timing 和 shadow mode 要求。

## 5. 最终建议

M7 下一步不要直接写 borrow checker 主逻辑。先做 M7-WP2 的共享 BodyFlowGraph/action builder，并产出可测试的只读 facts/dump。
这一步完成后再进入 M7-WP3 本地 loan checker。这样能最大程度复用 M6 已收口的 move/resource/cleanup 行为，同时避免把
`BorrowEscapeAnalyzer` 的临时安全网过早拆掉。

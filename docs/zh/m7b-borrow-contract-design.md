# Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线

日期：2026-06-02

状态：M7b 设计基线。本文用于固定 M7a 之后的下一实现包，不代表代码已经实现。

## 0. 结论摘要

M7a 已经把 Aurex 从“树遍历式 borrowed-view 逃逸特判”推进到可查询的 CFG-sensitive borrow facts：
`BodyFlowGraph`、`BodyLoanCheckResult`、`FunctionBorrowSummary`、query/cache/tooling projection 和 enforced
local loan diagnostics 都已收口。M7b 不应该重新打开 M7a，也不应该一次性吞下 raw pointer alias model、partial move、
`dyn Trait`、async/generator borrow 或 full Polonius engine。

M7b 的正确目标是把 M7a 的内部事实提升为稳定的函数边界 contract，并补齐 reborrow、method receiver 和
`BorrowEscapeAnalyzer` parity：

1. 引入显式但窄的 borrow contract surface，优先用 `@borrow(...)` 函数属性表达返回值来源，不引入 Rust-style
   lifetime generic surface。
2. 扩展 `BorrowSummary` 为可比较的 `BorrowContract`，支持 parameter/self/static/unknown return origin set、
   subset/outlives 约束和 trait/extern/prototype contract。
3. 实现 reborrow 模型：从 shared 或 mutable loan 派生 child loan，child 活跃期间限制 parent 使用；child last-use
   后 parent 可恢复。
4. 对 method receiver 引入显式 access contract，并只在 receiver auto-borrow 调用场景引入 two-phase borrow 的
   reservation/activation 状态机。
5. 用新 checker 覆盖旧 `BorrowEscapeAnalyzer` 的 borrowed-view escape matrix，先 shadow 对比，再 enforced
   替换，最后删除或降级旧 analyzer。

M7b 不做：

- full Rust lifetime 参数、HRTB、variance 完整 surface。
- full Polonius Datalog engine。
- Stacked Borrows / RustBelt 级 unsafe alias semantics。
- raw pointer alias safe proof。
- partial move / replace / take / swap 完整 place-level resource semantics。
- `dyn Trait`、trait object layout、async drop 或 generator borrow。

## 1. 当前工程基线

M7a 已落地的关键事实：

- `CheckedModule::body_flow_graphs` 保存函数体 point、edge、place 和 action timeline。
- `CheckedModule::body_loan_checks` 保存本地 loan/origin/conflict facts。
- `CheckedModule::borrow_summaries` 保存函数级 return origin dependency set、unknown/local escape 和 stable
  fingerprint。
- `TypeCheckBodyAuthority` 混入 summary / loan-check fingerprint 和状态位。
- IDE semantic facts 暴露 `borrow_summary` / `body_loan_check`。
- 普通 `--check` 不再长期保留完整 `BodyFlowGraph`，W7a 性能闭环已完成。

仍然存在的 M7b 接缝：

- `BorrowEscapeAnalyzer` 仍是 `src/sema/internal/sema_statement_analyzer.cpp` 内的独立树遍历 scope map；它仍负责旧
  borrowed-local escape 诊断。
- `FunctionBorrowSummary` 能记录 dependency set，但还不是用户可写、跨模块稳定的 contract。
- 当前 summary 对 extern/prototype/trait requirement 缺少公开 contract surface，只能 conservative unknown。
- 当前 local loan checker 还没有 reborrow parent/child 模型，也没有 mutable reborrow suspend/resume。
- method receiver 的 `self: &T` / `self: &mut T` 已有签名形式，但没有独立 access contract 和 two-phase receiver
  reservation。
- generic/trait borrowed-return 的 impl contract 还不能做“impl 不得比 requirement 更宽”的 subset 检查。

## 2. 参考依据与取舍

### 2.1 Rust NLL

Rust NLL 的核心启发是 lifetime 不应按词法块粗暴截断，而应跟随 CFG use/liveness 缩短。Aurex 已在 M7a 采纳
point/liveness 的方向。M7b 继续采纳这个思想，但不复制完整 Rust lifetime annotation surface。

采用：

- loan 生命周期按 point 和 last-use 计算。
- diagnostics 绑定 creation point、invalidating point 和 later use。

拒绝：

- M7b 不引入 `'a` 风格的完整 lifetime generic、HRTB 和 variance surface。

### 2.2 Rust Polonius

Polonius 把 borrow checking 拆成 origin、loan、subset、invalidates 等 facts。Aurex M7a 已经选择类似词汇和本地
worklist。M7b 应继续扩展 fact vocabulary，但仍先用 Aurex-local deterministic solver，而不是引入 Datalog runtime。

采用：

- `origin_subset(parent, child)` / `origin_live_at(origin, point)` / `loan_invalidated_at(loan, point)` 这类事实形状。
- query/cache/tooling 只消费 checked facts，不重新跑语义。

拒绝：

- M7b 不把 full Polonius engine 作为实现依赖。

### 2.3 Rust two-phase borrow

Rust two-phase borrow 解决的是 method receiver auto-borrow 中“先保留 mutable borrow，再计算其他参数，最后激活调用”的问题。
这正好对应 Aurex 后续 method receiver ergonomics，但如果泛化到任意 `&mut`，会放大诊断和实现复杂度。

采用：

- 仅对 compiler-inserted mutable receiver auto-borrow 启用 reservation/activation。
- reservation 期间只禁止会破坏 receiver place 的操作；activation 时执行完整 mutable loan conflict check。

拒绝：

- 不允许用户显式 `&mut` 默认变成 two-phase。
- 不先支持复杂 nested two-phase 和 closure/generator 捕获。

### 2.4 RustBelt 与 Stacked Borrows

RustBelt 和 Stacked Borrows 说明 safe/unsafe alias contract 必须有可解释语义。Aurex M7b 应吸收这个警告，但不把
unsafe alias model 混进 safe borrow contract 第一版。

采用：

- safe reference 和 raw pointer 继续分层。
- raw pointer 派生返回仍是 `unknown`，不能作为 safe proof。

拒绝：

- M7b 不证明 raw pointer alias safety，不定义完整 optimizer alias contract。

### 2.5 Swift ownership modifiers

Swift 的 `borrowing` / `consuming` 和 noncopyable work 表明工业语言会把 ownership 逐步变成 API contract，而不是永远藏在实现里。
这支持 Aurex 在 M7b 引入函数边界 borrow contract。

采用：

- 函数、method、trait requirement、extern/prototype 都应能承载 ownership/borrow contract。
- `consuming` 语义继续复用 M6 `OwnedUseMode::owned_consume`，不要建第二套资源模型。

拒绝：

- M7b 不新造 Swift 风格的一整套参数关键字；先复用现有 `self: &T` / `&mut T` 和 `@borrow(...)` contract。

### 2.6 Hylo / mutable value semantics

Hylo/Val 系列提醒我们，mutable value semantics 的核心是把值更新、别名和共享状态边界拆清楚。Aurex M7b 应继续把
place-level resource semantics 和 safe borrow 分层：reborrow 可以做，partial move / replace / take / swap 不能顺手塞进来。

采用：

- reborrow parent/child loan 明确建模。
- value mutation 和 borrow alias conflict 仍由 place/action matrix 检查。

拒绝：

- M7b 不把完整 value-semantic container/iterator API 或 partial move 当成本阶段目标。

### 2.7 C++ Lifetime Safety

C++ Core Guidelines lifetime safety profile 是重要反例：生命周期安全如果只靠后置 lint，很难形成语言核心保证。Aurex 已经有机会在
Sema 主路径里建模，应继续把 borrow contract 放进 checked facts 和 query identity。

采用：

- lifetime/borrow 是 compiler-sema contract，不是 IDE lint。
- public API contract 进入 stable fingerprint。

拒绝：

- 不把 safe reference lifetime 降级为 warning-only。

## 3. M7b 语义模型

### 3.1 BorrowContract

M7b 新增函数边界 contract 概念：

```text
BorrowContract {
  function: FunctionLookupKey
  declared: bool
  return_origins: OriginSelectorSet
  receiver_access: none | shared | mutable | consuming
  param_access: [read | shared_borrow | mutable_borrow | consuming]
  subset_constraints: [(OriginSelector, OriginSelector)]
  unknown_return_allowed: bool
  fingerprint: StableFingerprint128
}
```

`FunctionBorrowSummary` 继续由函数体推导；`BorrowContract` 是 signature/API 边界。对有 body 的私有函数，可以从
summary 自动生成 contract。对 `pub`、`extern`、prototype、trait requirement，M7b 应允许或要求显式 contract，以稳定跨模块和
trait impl 语义。

### 3.2 OriginSelector

M7a summary 已有 parameter/local/temporary/unknown。M7b 在函数边界上只允许这些稳定 selector：

- `param(name)`：返回值可借用某个参数。
- `self`：method 返回值可借用 receiver。
- `static`：返回值可借用 static/global storage。
- `unknown`：明确无法提供 safe proof，只能作为 unsafe/extern 边界保守事实。

禁止出现在 public safe contract 里的 selector：

- local。
- temporary。
- raw pointer derived origin。

原因很直接：local/temporary 不能越过函数返回，raw pointer 不提供 safe borrow proof。

### 3.3 `@borrow(...)` surface

M7b 选择复用现有 `@name("...")` 属性位置，引入一个窄属性：

```aurex
fn id_ref[T](value: &T) -> &T @borrow(return = [value]) {
    return value;
}

fn choose_ref[T](left: &T, right: &T, take_left: bool) -> &T @borrow(return = [left, right]) {
    if take_left {
        return left;
    }
    return right;
}

impl TextView {
    fn as_str(self: &TextView) -> str @borrow(return = [self]) {
        return self.text;
    }
}

extern c {
    fn view_bytes(bytes: []const u8) -> str @borrow(return = [bytes]);
}
```

设计理由：

- 不新增 lifetime token，不污染泛型参数空间。
- 属性位置已经存在，适合 ABI 和 contract metadata。
- contract 可挂在 `fn`、trait requirement、extern/prototype 和 impl method 上。
- 对 body 可推导的 private 函数不强制写属性，保持常见代码简洁。

限制：

- `@borrow(return = [local])` 无效，因为 local 不是函数边界 selector。
- 如果返回类型不能含 borrow，`@borrow` 报冗余 contract。
- 如果返回类型可含 borrow 且函数无 body，缺 `@borrow` 时 summary 为 unknown；safe caller 不能用它证明 borrowed return。
- `unsafe extern` 可以显式写 `@borrow(return = [unknown])`，但这只暴露事实，不提升为 safe proof。

### 3.4 Contract 与 summary 的一致性

对有 body 的函数：

- 推导出的 summary return origin set 必须是 declared contract 的子集。
- 如果 summary 出现 local/temporary escape，safe 函数报错。
- 如果 summary 为 unknown，而 declared contract 只列出参数/static，报 contract mismatch。
- private 函数没有 declared contract 时，直接使用 inferred summary。

对 trait requirement：

- requirement 可以声明 `@borrow(return = [self])` 或参数来源。
- impl method 的 inferred/declared contract 必须是 requirement contract 的子集，不能扩大返回来源。
- inherited default method 使用 trait-owned default body 的 inferred summary 与 requirement contract 比较。

对 generic 函数：

- 当 generic `T` 可能含 borrow 时，`T` 参数到 `T` 返回的 identity/wrapper summary 必须记录 dependency。
- `where` trait predicate 不隐式承诺 borrowed-return；trait method contract 必须来自 requirement 或 impl。

### 3.5 Reborrow

M7b 需要把 reborrow 从“像普通新 borrow”提升为 parent/child loan：

```text
Reborrow {
  parent_loan: LoanId
  child_loan: LoanId
  kind: shared_from_shared | shared_from_mutable | mutable_from_mutable
  suspend_parent_until: child_last_use_point
}
```

规则：

- `&*shared_ref` 生成 shared child loan，parent shared 仍可读。
- `&*mutable_ref` 生成 shared child loan，child 活跃期间 parent mutable 不能写。
- `&mut *mutable_ref` 生成 mutable child loan，child 活跃期间 parent mutable 不能读写或再借用。
- child last-use 后 parent 恢复可用。
- 从 raw pointer reborrow 不进入 safe proof，只能在 unsafe/raw alias model 里处理。

实现策略：

- 不引入递归 loan stack；使用 `parent_loan` index 和 iterative liveness。
- 对每个 child loan 计算 live range，再把 parent suspended range 投影成 conflict facts。
- M7b 先覆盖 local carrier 和 receiver reborrow；跨闭包/generator 捕获后置。

### 3.6 Method receiver access contract

现有 Aurex method receiver 已能写成：

```aurex
fn get(self: &Box[T]) -> T
fn set(self: &mut Box[T], value: T) -> void
```

M7b 不新增 receiver 关键字，先从类型推导 access contract：

- `self: &T` -> shared receiver access。
- `self: &mut T` -> mutable receiver access。
- `self: T` -> consuming receiver access。

这个 contract 进入 method call binding、borrow summary 和 query authority。未来如果要引入 `borrowing self` /
`consuming self` 之类语法，应在 M7b 之后单独设计。

### 3.7 Two-phase borrow

M7b 只允许 method receiver auto-borrow 进入 two-phase：

```aurex
values.push(values.len());
```

概念上拆成：

1. reserve mutable receiver loan。
2. 计算其他参数。
3. activate mutable receiver loan at call point。
4. 调用结束后按 returned borrow contract 决定 loan 是否继续关联返回 carrier。

规则：

- reservation 期间，shared read 可以继续，只要不写/移动/reinit/drop receiver place。
- activation 点执行完整 mutable conflict check。
- 显式 `&mut values` 不自动 two-phase。
- 如果 activation 失败，diagnostic 指向 method call，并给出 reservation 和冲突参数 evaluation note。

### 3.8 BorrowEscapeAnalyzer parity

M7b 的替换标准不是“新 checker 看起来更高级”，而是覆盖旧矩阵：

- 返回 local reference。
- 返回 temporary reference。
- 返回 local slice / `strfromutf8(local_bytes)`。
- 返回 `strraw(...)` / unchecked raw-derived view。
- 把 borrowed local view 写入外层 storage 或 aggregate。
- pattern destructuring 中的 borrowed carrier escape。
- branch/match/block return 中混合 parameter/local/unknown 来源。
- assignment 覆盖后旧 borrowed carrier 仍被返回或存储。

推进顺序：

1. shadow mode：旧 analyzer 和新 contract checker 同时运行，测试记录差异。
2. enforced mode：新 checker 发主诊断，旧 analyzer 只在 shadow 记录。
3. removal mode：删除或降级 `BorrowEscapeAnalyzer`。

## 4. Pipeline 影响

### 4.1 Lexer / Parser / AST

- 不新增关键字。
- 扩展函数 ABI attribute 解析，支持 `@borrow(return = [name, self, static, unknown])`。
- AST 新增 `BorrowContractDecl` 或在现有 function attribute payload 中保存结构化 contract。
- AST dump / lossless dump 必须稳定输出 contract。

### 4.2 Sema

- `FunctionSignature` 或相邻 checked side table 保存 declared contract。
- `FunctionBorrowSummary` 保持 body inferred facts。
- 新增 `FunctionBorrowContract` 作为 signature/API 边界事实。
- summary-vs-contract checker 在 body analysis 后运行。
- trait requirement / impl matching 增加 borrow contract subset 检查。
- method call binding 保存 receiver access 和 two-phase eligibility。

### 4.3 Query / Incremental Cache

- contract fingerprint 混入 item signature 和 type-check-body authority。
- summary fingerprint 继续 range-insensitive。
- contract parse/AST changes 应使 affected signature/body query 重算。
- IDE/tooling 消费同一份 checked facts。

### 4.4 Diagnostics

新增诊断族：

- missing borrow contract：extern/prototype/trait requirement 返回 borrowed type 但没有 safe contract。
- contract mismatch：函数体返回来源超出 declared contract。
- local/temporary escape：新 checker 替代旧 analyzer 的主诊断。
- reborrow conflict：child loan 活跃期间使用 parent mutable loan。
- two-phase activation conflict：reservation 成功但 activation 点冲突。
- trait impl broadens borrow contract：impl 返回来源比 requirement 更宽。

### 4.5 IR / Backend

M7b 不改变 IR ABI。borrow contract 是 Sema/query/tooling 层事实，不直接进入 LLVM lowering。后续 unsafe alias model 或 optimizer
alias metadata 才可能影响 backend。

## 5. 候选方案比较

| 方案 | 优点 | 问题 | 结论 |
| --- | --- | --- | --- |
| Rust-style `'a` lifetime generics | 表达力强，生态验证充分 | 语法和类型系统成本高，HRTB/variance/trait surface 会膨胀 | M7b 不采用 |
| 完全无显式语法，只靠推导 | 用户表面最简单 | extern/prototype/trait/public API contract 不稳定，跨模块 unsafe 边界模糊 | 不够 |
| `where return: borrow(x)` | 可读，接近约束系统 | 会扩张现有 trait `where` grammar，容易和 trait predicate 混杂 | 暂缓 |
| `@borrow(return = [x])` | 不新增 keyword，适合 ABI/API metadata，易渐进 | attribute 参数 parser 要扩展，语义需严格校验 | M7b 采用 |
| full Polonius Datalog | 理论清晰，未来可扩展 | 依赖和性能/调试成本过高 | 不采用 |
| Aurex-local facts + worklist | 贴合现有 M7a，性能可控 | 需要持续保持事实语义清晰 | M7b 采用 |

## 6. M7b 验收边界

M7b 完成时应满足：

- `BorrowEscapeAnalyzer` 已被新 checker parity 覆盖并删除或降级。
- `@borrow(...)` 可用于函数、method、trait requirement、extern/prototype。
- public/prototype/extern borrowed return 有稳定 contract。
- trait impl contract 不得比 requirement 更宽。
- reborrow parent/child 和 mutable parent suspend/resume 有白盒覆盖。
- receiver access contract 和 method auto-borrow two-phase 有正负例。
- query/cache/tooling 能显示 contract 和 reborrow facts。
- release CTest、coverage、query sanitizer、perf/stress gate 全部通过。

## 7. 参考文献

- Rust RFC 2094 Non-Lexical Lifetimes: <https://rust-lang.github.io/rfcs/2094-nll.html>
- Rust RFC 2025 Nested method calls / two-phase borrows: <https://rust-lang.github.io/rfcs/2025-nested-method-calls.html>
- rustc-dev-guide Borrow check: <https://rustc-dev-guide.rust-lang.org/borrow_check.html>
- rustc-dev-guide Two-phase borrows: <https://rustc-dev-guide.rust-lang.org/borrow_check/two_phase_borrows.html>
- Polonius current status: <https://rust-lang.github.io/polonius/current_status.html>
- Rust Reference lifetime elision: <https://doc.rust-lang.org/reference/lifetime-elision.html>
- Rust Reference trait and lifetime bounds: <https://doc.rust-lang.org/reference/trait-bounds.html>
- Stacked Borrows: <https://plv.mpi-sws.org/rustbelt/stacked-borrows/>
- RustBelt: <https://plv.mpi-sws.org/rustbelt/>
- Swift Evolution SE-0377 Borrowing and consuming parameter ownership modifiers:
  <https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md>
- Swift Ownership Manifesto:
  <https://github.com/swiftlang/swift/blob/main/docs/OwnershipManifesto.md>
- C++ Core Guidelines lifetime profile: <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-lifetime>
- Hylo language: <https://www.hylo-lang.org/>
- Oxide: The Essence of Rust: <https://arxiv.org/abs/1903.00982>

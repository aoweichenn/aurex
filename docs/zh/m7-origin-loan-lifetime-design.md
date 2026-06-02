# Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究

日期：2026-06-02

状态：M7 设计研究基线。本文不代表实现已完成；它用于固定 M7 的语义目标、工程路线和后续实现工作包。

## 0. 结论摘要

M7 不应该把 Rust borrow checker 整套语法和实现原样搬进 Aurex。Aurex 现在已经有 M6 的资源分类、whole-local move
analysis、cleanup/drop lowering、`OwnedUseMode` side table、保守 borrowed-view 逃逸检查和 borrow alias
use-after-move 防线。正确路线是把这些临时机制提升成一个可查询、CFG-sensitive、projection-aware 的事实模型：

- `Place`：可访问位置，包含 root local/param/temp/global 以及 field/index/deref/slice 等 projection。
- `Origin`：抽象生命周期来源，先覆盖 local、parameter、temporary、static/global 和 unknown。
- `Loan`：在某个 CFG point 对某个 place 发出的 shared 或 mutable borrow。
- `Point`：函数体 CFG 中表达式/语句动作前后的稳定点位。
- `BorrowAction`：borrow、read、write、move、drop、return、call、end-scope、reinit 等检查动作。
- `BorrowSummary`：函数边界的 borrowed-return 和 call effect 摘要，用于跨函数检查和 incremental query。

M7 的第一版应该做“内部事实模型 + 保守用户表面”：默认不强迫用户写显式 lifetime 参数，只允许从参数或 static/global
派生的 borrowed return，并在 checked facts 中记录输出 origin 依赖。等 M7a 的事实、诊断、测试和 tooling 站稳后，再在
M7b/M8 讨论显式 origin/lifetime 语法。

M7 的核心取舍：

1. 借 Rust NLL 和 Polonius 的事实词汇，不先引入完整 Datalog 引擎。
2. 借 Swift/Mojo 的 API ownership 表达方向，但不在 M7a 贸然暴露复杂 lifetime surface。
3. 借 C++ Lifetime Profile 的经验，说明生命周期安全必须在语言核心里建模，不能后期靠 lint 补救。
4. 借 Zig/Go 的简单性约束，保持第一版规则可解释、诊断稳定、编译成本可控。
5. 借 TypeScript/Kotlin/C# 的 control-flow diagnostics 经验，重视错误定位和 flow-sensitive 可读性。

## 1. 当前 Aurex 基线

M6 已完成：

- compiler-owned `Copy`，内部 `Discard` / `NeedsDrop` / ownership resource summary。
- `expr_owned_use_modes` side table，模式包括 `owned_copy`、`owned_consume`、`shared_borrow`、`mutable_borrow`
  和 `place_only`。
- `BodyMoveAnalysis` 中的迭代式 CFG/worklist initialized/moved/maybe-moved dataflow。
- move 后重新初始化、consume-origin diagnostics、whole-local move 拒绝。
- lexical cleanup-action stack、drop flag、IR `drop` / `drop_if` cleanup node。
- `BorrowEscapeAnalyzer` 对 `&T`、`&mut T`、slice、`strfromutf8(...)`、`strraw(...)` 等 borrowed carrier
  做保守逃逸阻断。
- borrow alias use-after-move 追踪：alias 可以绑定回 move-only origin，origin 被 consume 后，alias 使用复用
  `use of moved value` 诊断。
- resource field/index/deref overwrite 当前在 Sema 临时拒绝，避免没有 place-level cleanup 时泄漏旧资源。

这说明 M7 不是从零开始。现有代码里的问题也很清楚：

- `BorrowEscapeAnalyzer` 是 typed AST 树遍历加 scope map，不是 CFG-sensitive loan checker。
- `BorrowEnvironment` 只能把 alias 粗略绑定到 whole-local resource local，不能表达 field/index projection。
- call 只靠保守扫描实参和 receiver，缺少函数摘要，所以 generic、trait method 和跨模块调用都会过度保守或需要特判。
- borrowed value 的“活到哪里”没有按 CFG last-use/NLL 计算，容易把本可接受的短 borrow 拒绝，也容易在复杂分支里靠保守策略遮住设计漏洞。
- raw pointer、slice、`str`、method receiver、pattern alias、block/if/match return 的 lifetime 传播没有统一事实表。

M7 应该把这些分散逻辑收束到一个 sema-owned borrow fact model。IR/lowering 继续消费 checked facts 和 cleanup/drop
facts，不应该让 borrow checker 依赖后端。

## 2. 流行语言与后 C++ 系统语言的经验

流行度会随年度和统计口径变化。Stack Overflow Developer Survey、TIOBE、PYPL 等指数适合帮助我们选取工程样本，但不应该作为语义设计的权威依据。对 M7 真正重要的是：这些语言在安全、性能、可解释性和迁移成本之间怎样取舍。

### 2.1 C++ 及其后继设计

C++ 的 RAII、move semantics 和 Core Guidelines 已经证明 deterministic cleanup 对系统编程有效，但 C++ 的引用、
指针、iterator 和 lifetime 规则是历史叠加出来的。Lifetime Profile 和 `owner<T*>` / `not_null<T*>` 等指南属于补救型静态分析，无法从语言核心上保证 safe reference 不悬垂。

对 Aurex 的启发：

- RAII/cleanup 必须继续保留，M6 的 cleanup/drop facts 是 M7 的输入。
- lifetime 检查必须在 Sema 核心建模，不能等到 lint、IDE 或 IR verifier。
- 不能把 raw pointer 和 safe reference 混成一套规则。raw pointer 属于 future unsafe model，safe borrow 必须有独立事实。
- C++ 的最大教训不是“语法要复杂”，而是“安全模型不能后补”。

Carbon、Cpp2/cppfront 这类后 C++ 实验说明了另一个现实：C++ 生态迁移和互操作会极大限制语言安全模型。Aurex 目前没有背负 C++ 源码兼容目标，所以更应该在早期把 ownership/lifetime 规则设计干净，而不是复制 C++ 的宽松历史包袱。

### 2.2 Rust

Rust 的优势不是 borrow checker 这个名字，而是它把所有权、move、borrow、lifetime、unsafe 边界和 trait/generic
约束绑定进同一套类型系统。NLL 把 lifetime 从词法块推进到 CFG point/liveness；Polonius 把 origin、loan、subset、
invalidates 等关系拆成事实；RustBelt 和 Stacked Borrows 给 unsafe 和 aliasing 模型提供语义研究基础。

可借鉴：

- 用 CFG point 和 liveness 决定 loan 是否仍然活跃，而不是简单按词法 scope。
- 用 `Origin` / `Loan` / `Point` / `Place` 事实词汇降低实现耦合。
- 对 projection conflict 做规则化，不把 field/index/deref 都退化成 whole-local。
- 把 diagnostics 做成“冲突点 + loan 发出点 + 后续使用点/invalidating action”的多点信息。

不应照搬：

- M7a 不需要 Rust 式完整 lifetime 参数语法。
- 不需要立刻实现 two-phase borrow、higher-ranked lifetimes、full reborrow/subtyping、unsafe alias model。
- 不需要 Datalog 引擎作为第一版实现依赖。Aurex 可以先生成同构 facts，用确定性 worklist/bitset solver 实现。

### 2.3 Swift

Swift 的 `borrowing`、`consuming`、`inout` 和 noncopyable work 说明：工业语言会把 ownership 作为 API 合同逐步显式化，但必须保护大多数用户代码的默认可读性。

对 Aurex 的启发：

- 参数所有权修饰是有价值的，但应该建立在内部 summary 已稳定之后。
- `consuming` 与 move-only resource 的调用约束应该复用 M6 `OwnedUseMode::owned_consume`，不要另建平行语义。
- `inout` 风格的独占访问能给 `&mut`、method receiver 和未来 API surface 提供可解释模板。

### 2.4 Mojo

Mojo 关注 Python 生态、性能和系统级控制，公开设计中强调 argument convention、ownership、transfer、origin/lifetime
概念。当前 Mojo surface 使用 `read`、`mut`、`var`、`ref`、`out`、`deinit` 等约定，而不是 Swift 风格的
`borrowing` / `consuming` / `inout`。它给 Aurex 的价值不在语法复制，而在“先让常见调用写法自然，再让高级用户能表达所有权边界”。

对 Aurex 的启发：

- `read` / `mut` / `var` / `ref` 这类面向调用约定的词汇说明，ownership surface 可以做得比显式 lifetime 参数更可读。
- M7a 内部 summary 应该已经能表达这些概念，哪怕暂不暴露语法。
- 默认规则要能让小函数、slice view、string view、method receiver 写起来不繁琐。

### 2.5 Zig 与 Go

Zig 倾向显式 allocator、`defer` / `errdefer` 和直接控制；Go 倾向 GC、escape analysis、简单语法和工具链统一。二者都不是 borrow-checker 语言，但它们提醒我们：工程语言的规则必须可解释，错误信息必须短路径可修复，编译器复杂性不能压垮普通工作流。

对 Aurex 的启发：

- M6 的 `defer` 与 cleanup stack 是正确地基，M7 不应破坏它。
- 对用户可见 surface 要保守，避免 M7a 直接暴露复杂 lifetime calculus。
- 函数摘要和默认推断应该服务于“常见代码自然通过，危险代码清晰拒绝”。

### 2.6 TypeScript、Kotlin、C#、Java

这些语言不是 M7 lifetime checker 的直接模板，但它们在 flow-sensitive typing、nullability、control-flow narrowing、
tooling diagnostics 和渐进迁移上有成熟经验。

对 Aurex 的启发：

- IDE/tooling 看到的事实应该来自 compiler checked facts，而不是独立重算。
- flow-sensitive diagnostics 要告诉用户“为什么这里已经不安全”，不能只说类型不匹配。
- surface 应该支持逐步增强：先让无 lifetime 标注的代码得到保守安全，再引入显式 annotation。

### 2.7 Hylo、Austral、Move、Cyclone、Linear Haskell、Vale

这些语言和研究不一定流行，但对 M7 有强参考价值：

- Hylo/Val 系列强调 mutable value semantics，提醒我们不要把共享状态和 borrowed view 混在一起。
- Austral 和 Linear Haskell 表明 linear/affine capability 与 borrow checking 有交集，但不是同一层；Aurex 的 future
  `MustConsume` 不应和 M7 loan checker 混成一个系统。
- Move 的 resource model 适合资产不可复制/不可丢弃，但不是 safe reference lifetime 的完整答案。
- Cyclone 的 region-based memory 说明 region/origin 命名能提升安全性，但过早暴露 region syntax 会增加用户负担。
- Vale 的 region/generation 思路适合未来 unsafe/runtime memory model 研究，不适合作为 M7a 主线。

## 3. M7 目标与非目标

### 3.1 目标

M7a 目标：

1. 引入稳定的 Place/Origin/Loan/Point/BorrowAction/BorrowSummary 内部模型。
2. 替代或吸收 `BorrowEscapeAnalyzer` 的保守逃逸逻辑。
3. 支持 CFG-sensitive loan liveness：短 borrow 在最后一次使用后结束。
4. 支持 projection-aware access conflict：field、index、deref、slice 不再全部退化为 whole-local。
5. 支持 safe reference、slice、`str` borrowed carrier 的 return contract。
6. 支持函数摘要：参数 origin 到返回值 origin 的依赖，call effects，receiver effects。
7. 与 M6 move/cleanup/drop facts 集成：move/drop/write/reinit 都能 invalidates 相关 loan。
8. 产生高质量 diagnostics，并给 IDE/LSP 暴露稳定 facts。
9. 保持实现为迭代式 dataflow/worklist，不引入递归遍历。

M7b 目标候选：

1. 显式 origin/lifetime surface。
2. 更细 reborrow/subtyping。
3. method receiver ownership modifiers。
4. trait/generic borrowed-return contract 的公开语法。
5. two-phase borrow 或类似 reservation/activation 机制。

### 3.2 非目标

M7a 不做：

- 用户 destructor syntax 和 custom destructor body lowering。
- partial field move、replace/take/swap 的完整 place-level resource semantics。
- 完整 Rust 式 lifetime generics、HRTB、variance、unsafe alias model。
- trait object、`dyn Trait`、object safety。
- async drop、generator/coroutine borrow。
- full Datalog Polonius engine。
- raw pointer alias safety。raw pointer 只能作为 unsafe/future model 的边界，不能被当成 safe reference。

## 4. 语义模型

### 4.1 Place

`Place` 描述可访问位置：

```text
Place {
  root: local | parameter | temporary | global | return_slot | unknown
  projections: [field(name/id), index(expr/class), deref, slice_range, tuple_index, enum_payload]
  type: TypeHandle
  mutability: immutable | mutable | unknown
  range: SourceRange
}
```

M7a conflict 规则：

- 同一个 root 且 projection 完全相同：冲突。
- 一个 place 是另一个 place 的 prefix：冲突，例如 `x` 与 `x.f`。
- 不同 struct field：可以判定不冲突，例如 `p.x` 与 `p.y`。
- index/slice 默认保守冲突。只有常量数组 index 且可证明不同，才可在后续版本放宽。
- deref safe reference 追踪到 borrowed origin；raw pointer deref 不进入 safe place 证明。

### 4.2 Origin

`Origin` 是 borrowed value 的来源：

```text
Origin {
  kind: local | parameter | temporary | static_global | function_return | unknown
  owner: local/param/function/global id
  parent: optional origin
  range: SourceRange
}
```

M7a 规则：

- 从 local/temporary 派生的 borrow 不能逃出函数。
- 从 parameter 派生的 borrow 可以返回，但必须记录到函数摘要。
- 从 static/global 派生的 borrow 可以返回，但要和 mutability/unsafe 边界一致。
- unknown origin 在 safe surface 中保守处理：如果返回值或长期 carrier 依赖 unknown，就拒绝或要求 summary。
- loan live range 与 value lexical scope 必须分开：value 可能仍在 scope 内，但借用可以在最后一次 carrier use 后结束；
  cleanup/drop scope 只作为 invalidating action 进入 checker。

### 4.3 Loan

`Loan` 描述一次 borrow：

```text
Loan {
  id: LoanId
  place: PlaceId
  origin: OriginId
  kind: shared | mutable
  issued_at: PointId
  carrier: ExprId/local/temp/result
  range: SourceRange
}
```

基本冲突：

- active mutable loan 排斥任何读写以外的其他 access，除非是受控 reborrow。
- active shared loan 允许 shared read，禁止 write、move、drop、mutable borrow。
- move/drop/write/reinit 是 invalidating action，会检查是否有 active loan 依赖冲突 place。
- scope end/drop cleanup 也必须作为 invalidating action 建模，不能只看显式语句。

### 4.4 Point 与 BorrowAction

函数体 CFG 每个 expression/statement 的关键边界生成 point：

```text
point before expression
point after expression
point before statement
point after statement
point scope cleanup
point branch join
point function exit
```

BorrowAction 包含：

```text
Borrow(place, shared|mutable, carrier)
Access(place, read|write|move|drop|reinit|borrow_shared|borrow_mutable)
Return(carrier/place)
Call(callee, arguments, receiver, summary)
EndScope(local/temp)
CleanupDrop(place)
```

M7a 可以先把现有 `BodyMoveAnalysis` CFG 抽出为共享的 `SemaBodyFlowGraph`，然后让 move analysis 和 borrow
analysis 共享 action building 的 place/requested-use 结果，避免维护两套近似但不一致的遍历器。

## 5. 算法方案

### 5.1 备选方案

方案 A：继续扩展 `BorrowEscapeAnalyzer`。

- 优点：改动小。
- 缺点：树遍历模型无法可靠表达 CFG liveness、分支 merge、loop、cleanup、projection conflict 和 call summary。
- 结论：拒绝。它适合 M6 hardening，不适合 M7。

方案 B：直接移植 Rust borrow checker。

- 优点：语义成熟。
- 缺点：Aurex AST/type/trait/resource 模型不同，Rust surface 复杂度和历史包袱很重，移植成本与诊断适配成本过高。
- 结论：拒绝原样移植，但借鉴事实词汇和 NLL 设计。

方案 C：Polonius-style facts + Aurex deterministic worklist solver。

- 优点：事实模型清晰，容易 query/tooling，先不用 Datalog runtime；适合逐步引入 origin/loan/subset。
- 缺点：需要设计 place/action/fact side tables，需要替换现有临时分析。
- 结论：选择。

### 5.2 M7a 求解流程

1. 在函数 body type-check 完成后生成 `SemaBodyFlowGraph`。
2. 从 typed expressions/statements 生成 `BorrowAction` 和 `Place`。
3. 生成局部 origin：
   - 参数 origin。
   - local/temporary origin。
   - static/global origin。
   - call return origin，来自 callee `BorrowSummary`。
4. 生成 loan：
   - `&place`、`&mut place`、slice、safe string view、method receiver borrow。
   - carrier local/temp/result 绑定 loan。
5. 做 carrier liveness：
   - 反向 dataflow 计算 borrowed carrier 在哪些 point 后仍可能使用。
   - 如果 carrier 被 move/drop/end-scope，loan 结束。
6. 做 active-loan checking：
   - 正向 worklist 传播 active loans。
   - 在每个 `Access` 检查冲突。
   - 在 join 处 union active loans。只要某条路径上 loan 可能活跃，后续 invalidation 必须保守拒绝。
7. 做 return contract：
   - return carrier 的 origin 如果是 local/temporary，拒绝。
   - 如果是 parameter/static/global，写入 function `BorrowSummary`。
   - 如果是 unknown 且 safe surface 无显式 annotation，拒绝。
8. 产出 diagnostics 和 checked facts。

复杂度目标：

- places/actions/loans 线性接近函数体大小。
- active loans 用 dense bitset 或 sorted small vector，根据函数大小切换。
- projection conflict 先用 prefix/tree path 比较，field disjoint 为 O(path length)。
- 所有图遍历使用迭代 worklist，遵守当前 C++ 工程标准。

### 5.3 函数摘要

`BorrowSummary` 最小形状：

```text
FunctionBorrowSummary {
  function_key
  parameter_origins: [OriginParam]
  return_origin_dependencies: [ReturnSlot -> OriginDependencySet]
  argument_requirements: [arg -> shared|mutable|consume|none]
  receiver_requirement: shared|mutable|consume|none
  may_return_borrowed_carrier: bool
  may_store_borrowed_arg: bool
  conservative_unknown: bool
}

OriginDependencySet =
  parameter_set([OriginParam]) | static_global | unknown
```

M7a 规则：

- 当前模块内函数由分析结果生成 summary。
- branch、match、block result 或 wrapper call 可以让同一个 return slot 依赖多个 parameter origin；summary 必须记录 origin
  set，不能把多来源 borrowed return 压成单一 origin。
- extern/native/prototype 若返回类型可含 borrow 且没有 summary，按 unknown/conservative 处理。
- generic/trait method 使用 monomorphic checked body 或 trait requirement summary；没有 summary 时拒绝 risky borrowed return。
- summary 是 query/incremental cache 的稳定输入，不能只存在临时 analyzer 对象里。
- 对 inferred return 函数，summary 生成不能假设 signature return type 已经最终可用；实现应先收集 return carrier facts，
  并在 return type inference finalized 后固化 `FunctionBorrowSummary`，或直接以 return expression/carrier type 为依据。

## 6. 用户表面建议

M7a：

- 不要求用户写 lifetime 参数。
- 支持已有 `&T` / `&mut T` / slice / `str`，通过内部 summary 判断 borrowed return。
- 参数 derived return 可以通过推断和 checked summary 通过。
- local/temporary derived return 继续拒绝，但诊断应该更精确。

M7b/M8 候选 surface：

```text
fn first['a](xs: 'a []const T) -> 'a &T;
fn view['a](bytes: 'a []const u8) -> 'a str;
fn consume(handle: consuming File);
fn inspect(value: borrowing T);
fn update(value: inout T);
```

注意：上面只是 surface 候选，不是 M7a 语法承诺。M7a 要先固定内部 `OriginParam` 和 summary，否则过早设计语法会把实现锁死。

## 7. 诊断策略

诊断必须包含：

1. primary：发生非法访问/返回/冲突的位置。
2. note：loan 发出的位置。
3. note：borrowed carrier 后续使用位置，解释为什么 loan 仍活跃。
4. note：move/drop/write/reinit 的 invalidating action。
5. 对跨函数 call，note callee summary 或 prototype 缺失。

示例：

```aurex
fn bad() -> &i32 {
    let x = 1;
    return &x;
}
```

应报：

- `cannot return reference derived from local 'x'`
- note: `local storage is declared here`
- note: `borrow is created here`

```aurex
fn nll_ok() {
    var x = 1;
    let r = &x;
    use(r);
    x = 2;
}
```

应通过，因为 `r` 的 loan 在 `use(r)` 后不再 live。

```aurex
fn conflict() {
    var x = 1;
    let r = &x;
    x = 2;
    use(r);
}
```

应拒绝，因为 shared loan 在 write 时仍活跃。

## 8. 与 M6 资源/drop 的关系

M7 不替代 M6，M7 消费 M6：

- `OwnedUseMode::owned_consume` 继续是 move/consume 事实来源。
- `ResourceSemanticsClassifier` 继续决定 Copy/NeedsDrop/resource cleanup。
- cleanup stack/drop flags 生成的隐式 drop 也必须进入 borrow action。
- 对被 active loan 借用的 resource，move/drop/overwrite 都要按 conflict rules 检查。
- place-level cleanup、partial move、replace/take/swap 属于 M7/M8 后续资源工作包。M7a 可以先在 borrow checker 中保守拒绝不能证明安全的 place overwrite。

## 9. Work Package 建议

M7-WP1：设计基线与文档

- 固定本文。
- 写 `m7-roadmap.md`。
- 在 global compiler skill 中记录 M7 设计原则。

M7-WP2：共享 BodyFlowGraph 与 action builder

- 从 `BodyMoveAnalysis` 抽出 CFG/action skeleton。
- 保持现有 move tests 全绿。
- 不先改 diagnostics 文案。

M7-WP3：Place/Origin/Loan facts 与本地 loan checker

- 支持 `&`、`&mut`、slice、safe `str` view。
- 支持短 borrow last-use。
- 支持 write/move/drop/reinit 冲突。

M7-WP4：BorrowSummary 与 borrowed-return contract

- 生成函数 summary。
- 替换 `BorrowEscapeAnalyzer` 的 return escape 逻辑。
- 支持 call/method receiver borrowed-return。

M7-WP5：Projection-aware conflict

- field disjoint。
- tuple index。
- array/slice/index 保守 conflict。
- deref safe reference origin。

M7-WP6：tooling/query/diagnostics

- 已完成：borrow summary / body loan check facts 进入 `TypeCheckBodyAuthority` result fingerprint。
- 已完成：IDE semantic facts 暴露 `borrow_summary` / `body_loan_check`，函数 hover 展示 summary dependency。
- 已完成：borrow diagnostics 补齐 loan creation、invalidating action 和可定位时的 later carrier use notes。
- 已完成：CLI incremental-cache 与 IDE snapshot query collection 共用同一份 checked facts。

M7-WP7：release closure

- 已完成：文档写清 M7a 已支持能力和暂缓项。
- 收口验证目标：full build/ctest/coverage/query/perf gates。
- 已明确 M7 后续：explicit lifetime surface、partial move、user destructor、unsafe alias model。

并行 hardening 任务：

- TargetInfo/DataLayout 贯通到 LLVM backend matrix。
- typed query route table。
- CompileBudget。
- ModuleIdentity。
- LSP protocol parser hardening。

这些可以作为 M7 foundation work，但不能干扰 M7 borrow semantic 主线。

## 10. 测试矩阵

正例：

- shared borrow last-use 后写入。
- mutable borrow 独占使用后结束。
- 参数派生 `&T` 返回。
- 参数派生 slice/str 返回。
- field disjoint mutable borrow。
- block/if/match 返回参数派生 borrow。
- method receiver 返回参数/receiver 派生 borrow。
- cleanup/drop 与无活跃 loan 的普通 scope exit。

负例：

- 返回 local/temporary 派生 `&T`、`&mut T`、slice、str。
- shared borrow 活跃时 write/move/drop。
- mutable borrow 活跃时 read/shared borrow/second mutable borrow。
- borrowed alias 存在时 consume resource。
- index/slice 与同 root write/move/drop。
- raw pointer 派生 safe view 绕过。
- call summary unknown 却返回 borrowed local。
- branch/loop 中某条路径 loan 仍活跃时 invalidation。
- cleanup/drop 隐式 action 与后续 carrier use 冲突。

白盒：

- place prefix/disjoint conflict。
- origin interning/stable IDs。
- loan liveness bitset join。
- summary fingerprint。
- sparse/dense side table fallback。
- stale source/range diagnostics 不崩溃。

## 11. 设计风险

1. 过早暴露 lifetime syntax：会把用户表面锁死。先做内部 facts。
2. 双 CFG 漂移：move analysis 和 borrow analysis 如果各自构建 CFG，会出现语义分叉。应抽共享 skeleton。
3. projection 过度乐观：index/slice 很容易错判 disjoint。M7a 应保守。
4. call summary 过度宽松：extern/generic/trait 没 summary 时必须保守。
5. diagnostics 噪声：borrow checker 失败容易级联。要在 primary conflict 后抑制派生噪声。
6. 与 cleanup/drop 顺序不一致：隐式 drop 必须进入同一 action timeline。
7. raw pointer 混入 safe proof：M7a 必须把 raw pointer 视为 unsafe 边界。

## 12. 参考文献与取舍

- Stack Overflow Developer Survey 2025，Programming languages：
  https://survey.stackoverflow.co/2025/technology#most-popular-technologies-language
  用途：流行语言样本筛选，不作为语义正确性依据。
- TIOBE Index：https://www.tiobe.com/tiobe-index/
  用途：流行度参考，排名随月份变化。
- PYPL：https://pypl.github.io/PYPL.html
  用途：流行度参考，排名随数据源变化。
- C++ Core Guidelines，Resource Management：
  https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-resource
  用途：RAII/resource 经验，说明 cleanup 与 ownership 的工程价值。
- C++ Core Guidelines，Lifetime Safety Profile：
  https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#SS-lifetime
  用途：说明后补 lifetime analysis 的局限，Aurex 应在语言核心建模。
- cppfront / Cpp2：https://github.com/hsutter/cppfront
  用途：后 C++ 安全 surface 参考，主要作为迁移约束反例。
- Carbon Language：https://github.com/carbon-language/carbon-lang
  用途：C++ successor 的互操作和迁移约束参考。
- Rust RFC 2094 Non-Lexical Lifetimes：
  https://rust-lang.github.io/rfcs/2094-nll.html
  用途：M7 CFG point/liveness 的核心参考。
- Rust RFC 2025 Nested method calls / two-phase borrows：
  https://rust-lang.github.io/rfcs/2025-nested-method-calls.html
  用途：two-phase borrow 作为未来 M7b/M8 候选，不进入 M7a。
- rustc dev guide，MIR borrow check：
  https://rustc-dev-guide.rust-lang.org/borrow_check.html
  用途：借鉴 MIR-level borrow checking 阶段划分和 diagnostics 思路。
- Polonius：https://github.com/rust-lang/polonius
  用途：Origin/Loan/Point/facts 词汇和关系拆分。
- RustBelt：https://plv.mpi-sws.org/rustbelt/
  用途：unsafe/ownership 语义基础，作为长期安全模型参考。
- Stacked Borrows：https://plv.mpi-sws.org/rustbelt/stacked-borrows/
  用途：未来 unsafe/raw pointer alias model 参考，不进入 M7a。
- Swift SE-0377 Parameter Ownership Modifiers：
  https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md
  用途：`borrowing` / `consuming` / `inout` 参数 ownership surface 参考。
- Swift Ownership Manifesto：
  https://github.com/apple/swift/blob/main/docs/OwnershipManifesto.md
  用途：渐进式 ownership API 设计参考。
- Swift SE-0390 Noncopyable structs and enums：
  https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md
  用途：noncopyable 类型与用户可读 surface 参考。
- Mojo ownership：
  https://mojolang.org/docs/manual/values/ownership/
  用途：owned/borrowed/inout 和 origin/lifetime 用户模型参考。
- Zig Language Reference，defer / errdefer：
  https://ziglang.org/documentation/master/#defer
  用途：显式 cleanup 和简单工程规则参考。
- Go Memory Model：https://go.dev/ref/mem
  用途：简单性和并发内存语义边界参考，不作为 borrow model。
- TypeScript Design Goals：
  https://github.com/microsoft/TypeScript/wiki/TypeScript-Design-Goals
  用途：渐进式类型系统和 tooling-first 设计参考。
- TypeScript Narrowing：
  https://www.typescriptlang.org/docs/handbook/2/narrowing.html
  用途：control-flow-sensitive diagnostics 参考。
- Kotlin Null Safety：
  https://kotlinlang.org/docs/null-safety.html
  用途：safe surface 和 flow-sensitive smart cast 参考。
- C# Nullable reference types：
  https://learn.microsoft.com/en-us/dotnet/csharp/nullable-references
  用途：compiler flow analysis 与 warning 迁移参考。
- Cyclone project：https://cyclone.thelanguage.org/
  用途：region/origin 思路参考，说明显式 region 语法的成本。
- Linear Haskell, Practical Linear Types：
  https://arxiv.org/abs/1710.09756
  用途：linear capability 与 borrow checker 分层参考。
- Austral：https://austral-lang.org/
  用途：linear types 和 borrow surface 的小语言参考。
- Move language：https://move-language.github.io/move/
  用途：resource type 参考，主要用于区分 resource linearity 与 safe borrow lifetime。
- Hylo：https://github.com/hylo-lang/hylo
  用途：mutable value semantics 和 ownership surface 参考。

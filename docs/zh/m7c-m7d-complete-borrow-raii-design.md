# Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线

日期：2026-06-03

状态：设计基线。本文用于固定 M7b 之后的 M7c/M7d 语义目标、工程分期和验收口径，不代表实现已经完成。

## 0. 结论摘要

M7c/M7d 不做 Rust 的逐字复刻。Aurex 应该吸收 Rust 已经证明有效的安全类别和算法形状，但用户表面、函数边界合同、
query/tooling 投影、RAII/dropck 和资源能力要按 Aurex 当前语言与编译器工程来设计。

最终目标是：

1. Safe reference 只能来自可证明的 origin/loan，不能从 raw pointer 或 unknown provenance 洗成安全引用。
2. 函数边界继续以 `FunctionBorrowContract` 和装饰器式 `@borrow(...)` 作为默认 public API contract，而不是强迫所有用户写
   Rust-style apostrophe lifetime 参数。
3. 对需要在类型中保存 borrowed view 的 struct/enum/generic API，引入轻量 origin 参数和 `&[origin] T` 形式的
   origin-qualified reference type。
4. 引入 region/lifetime solver：把 M7a/M7b 已有 `Place`、`Origin`、`Loan`、`Point`、`BorrowAction`、
   `BorrowSummary`、`FunctionBorrowContract` 升级为可表达 universal region、inference region、outlives constraint
   和 type-outlives constraint 的模型。
5. M7d 把 RAII 从“IR cleanup/drop lowering 已存在”提升到语言级 drop check：drop 是 borrow checker action，
   destructor body 要通过 dropck，泛型 drop glue 要产生 type-outlives 约束，partial move/reinit/drop flag 要在 place 级建模。
6. 查询、增量缓存、IDE/LSP、diagnostics 继续消费同一份 checked facts，不让工具链重新发明 borrow checker。

一句话：Aurex 的路线是“Rust 级安全底线 + Mojo/Hylo 式更低噪声的 origin surface + C++/Swift 式 RAII 工程性 + 编译器
facts first 的工具链可解释性”。这比单纯照抄 Rust 更适合当前 Aurex。

## 1. 当前工程基线

### 1.1 M6 已有资源与 cleanup 地基

M6 已经完成：

- compiler-owned `Copy`、内部 `Discard` / `NeedsDrop` / ownership resource summary。
- `OwnedUseMode` side table，覆盖 copy、consume、shared borrow、mutable borrow 和 place-only。
- whole-local move analysis、move 后 reinit、consume-origin diagnostics。
- lexical cleanup-action stack、drop flag、IR `drop` / `drop_if` cleanup node。
- target-independent drop-glue planner 和 stable drop-glue key。

这意味着 M7d 不是从零设计 RAII。现有问题是这些事实还没有完整进入 safe borrow/dropck 的统一类型规则：drop lowering
能生成 cleanup，但 sema 还不能证明 destructor 运行时不会观察悬垂借用，也不能表达 place-level partial initialization /
partial move / field drop state。

### 1.2 M7a/M7b 已有借用事实

M7a/M7b 已经落地：

- `CheckedModule::body_flow_graphs`：函数体 point、edge、place、action timeline。
- `CheckedModule::body_loan_checks`：本地 origin/loan/conflict facts，projection-aware conflict，reborrow 和
  receiver two-phase facts。
- `CheckedModule::borrow_summaries`：函数级 returned-origin dependency set、unknown/local escape 标志和 stable
  fingerprint。
- `CheckedModule::borrow_contracts`：`FunctionBorrowContract`，装饰器式 `@borrow(return = [...])`，summary-vs-contract
  enforcement，trait/generic borrowed-return subset matching。
- `FunctionCallBinding` / `TraitMethodCallBinding`：receiver access、auto-borrow、two-phase eligibility。
- query/cache/tooling projection：`TypeCheckBodyAuthority` 混入 borrow summary、borrow contract、body loan fingerprint
  和 reborrow/two-phase counts。

仍然存在的缺口：

- `BorrowEscapeAnalyzer` 仍在 `src/sema/internal/sema_statement_analyzer.cpp` 中保留，说明旧树遍历 borrowed-local escape
  诊断还没有完全退场。
- `FunctionBorrowContract` 只能表达返回值来源 selector，还不是完整 lifetime type system。
- 没有显式 lifetime/origin 参数，struct/enum/generic type 不能稳定携带 borrowed field 的 outlives 约束。
- 没有 universal/inference region solver、type-outlives predicate、dropck facts。
- 没有 place-level resource state：partial move、partial initialization、field overwrite、replace/take/swap 仍缺正式模型。
- raw pointer / unsafe alias model 仍然应保持在 safe borrow proof 之外。

## 2. 参考语言与研究结论

本节不是罗列“哪个语言酷”，而是判断哪些设计能解决 Aurex 当前真实缺口。

### 2.1 Rust：采纳安全类别和事实形状，不照抄表面

Rust 的关键价值：

- NLL 把 lifetime 从词法块推进到 CFG point/liveness。Aurex 已经有 `BodyFlowGraph` 和 body loan facts，应该继续沿这个方向。
- MIR borrow check 的阶段非常适合作为工程参照：先生成较低复杂度的中间表示，再做 move dataflow、region constraints、
  region inference、borrows-in-scope、最终 action check。
- two-phase borrow 对 method receiver auto-borrow 有明确价值，但它必须是受控例外，不应泛化到所有显式 `&mut`。
- dropck 证明 destructor 运行时引用仍然有效，是 RAII 必须补的安全层。
- RustBelt、Stacked Borrows、Oxide 的共同启示是：safe/unsafe 边界、provenance、aliasing 和 optimizer contract 需要语义模型，
  不能靠“用户别这么写”。

不照抄：

- 不把 Rust-style apostrophe lifetime 参数作为默认用户表面。Aurex 现有 `@borrow` contract 更适合先覆盖函数边界。
- 不在 M7c 引入 HRTB、完整 variance、trait object lifetime bound、async/generator borrow。
- 不把 full Polonius Datalog engine 作为第一版依赖。M7c 先用同构 fact vocabulary 和确定性 worklist/bitset solver。
- 不允许 raw pointer 派生 safe proof。unsafe alias model 是后续 M8/M9 的独立层。

### 2.2 C++ 与后 C++ 设计：RAII 值得保留，lifetime lint 不够

C++ 的 RAII 和 move semantics 证明了 deterministic cleanup 对系统语言很重要，但 C++ 的引用、iterator、view、pointer
lifetime 主要靠规范、习惯和工具补救。C++ Core Guidelines 的 lifetime profile 是有用的工程补丁，但它不是核心类型系统。
Safe C++ 这类后 C++ 方案也走向 MIR/dataflow/borrow checking，说明只靠注解和 lint 不足以处理 iterator invalidation、
string_view 悬垂和跨控制流的 use-after-free。

对 Aurex 的结论：

- RAII 继续保留，而且要比 C++ 更强：drop timing 是 compiler fact，dropck 是 sema hard error，不是 warning。
- safe view、iterator、slice、str 都必须携带 origin contract，否则不能进入 safe API。
- unsafe context 要明确归责；raw pointer、external origin、FFI unknown 不能被默认当作 safe borrow。

### 2.3 Swift：ownership modifier 是 API/ABI contract，不只是实现细节

Swift 的 `borrowing` / `consuming` 参数、noncopyable type 和 `deinit` 设计说明，工业语言会把 ownership 变成 API contract，
同时尽量减少 caller-side 噪声。Swift 也提醒我们：noncopyable、deinit、consuming method 与 partial destruction 的组合很容易
复杂化，必须先有清楚的 place state 和 dropck。

对 Aurex 的结论：

- `consuming` 等价能力应复用 M6 `OwnedUseMode::owned_consume`，不要另建第二套资源语义。
- M7d 的 destructor/dropck 要先限制在 compiler-checkable 范围：不能从 destructor 中重新引入悬垂 borrow 或隐式递归 drop
  漏洞。
- 将来 public API 的 ownership convention 可以从 checked facts 投影出来，但 M7c 不需要先改调用语法。

### 2.4 Mojo：origin、origin union、parametric mutability 值得借鉴

Mojo 的 origin/lifetime 文档非常接近 Aurex 当前需要：origin 是编译器用来追踪变量 lifetime 与 reference validity 的符号值；
`ref[origin]` 和 origin union 能表达返回值从一个或多个输入派生；`read`、`mut`、`var`、`ref`、`out`、`deinit` 把函数参数约定
做成用户能理解的 API。

对 Aurex 的结论：

- Aurex 可以采用 origin union 的思想，继续让 `@borrow(return = [left, right])` 成为函数边界来源集合，而不是把两个参数强行
  合并成一个显式 lifetime 名。
- 显式 origin surface 应该服务于 type-carrying borrowed fields 和高级 API，不应该污染普通函数。
- Parametric mutability 值得作为 M7c 之后的增强：一个 getter 能根据 receiver mutability 返回 mutable 或 immutable view，
  减少重复 API。

### 2.5 Hylo / Mutable Value Semantics：默认值语义能降低引用噪声

Hylo 和 mutable value semantics 的核心思路是禁止普通值之间共享可变状态，把引用降级为函数边界和 projection 机制中的二等能力。
这不是 Rust 的路线，但它解释了为什么 Aurex 不应该到处暴露 lifetime 标注：如果默认值语义、move/resource state 和函数 contract
足够强，很多借用可以停留在可推断 facts 中。

对 Aurex 的结论：

- 默认保持 value-first：owned value、move、drop、borrow 是不同权利，不把“所有高性能 API 都变成显式引用 API”。
- Projection 要安全且可解释：field projection、view projection、subscript view 都通过 place/origin facts，而不是裸 reference
  对象任意存储。
- 未来 collection/iterator API 可以更接近 Hylo/Mojo 的 safe projection，而不是 C++ iterator/pointer 模式。

### 2.6 Move、Austral、Linear Haskell、Cyclone、Verona、Pony、Vale

这些系统说明线性/仿射能力、region、reference capability 和 unsafe alias model 都有价值，但不是 M7c/M7d 主线：

- Move/Austral/Linear Haskell 更适合解释 future `MustConsume` 和不可丢弃资源，不等同于 safe borrow lifetime。
- Cyclone/Vale 的 region/provenance 适合 unsafe/runtime memory model 研究，但 M7c 应先做 safe references。
- Pony/Verona 的 capability/region 并发模型值得后续设计 data-race safety 时研究，不能混进 M7c 同步借用检查。

## 3. Aurex 设计原则

1. **Safe by proof**：safe reference 必须有 origin/loan proof；缺 proof 就是 compile error 或 unsafe boundary。
2. **Contract before annotation**：函数边界优先用 `FunctionBorrowContract` 和 `@borrow(...)`，只有类型需要携带 borrowed storage
   时才暴露 origin 参数。
3. **Facts first**：所有 borrow/lifetime/dropck 结果进入 `CheckedModule`，query/cache/tooling 消费同一份事实。
4. **RAII is a checked action**：drop、cleanup、overwrite、early exit 都是 borrow checker action，不能只留给 IR lowering。
5. **Place-level precision**：same/prefix conflict 保守，known disjoint field 放宽，index/slice 默认保守，后续再证明常量 index
   disjoint。
6. **No warning-only safety**：lifetime、dropck、safe borrowed return 不做 warning-only；安全边界失败就是语义错误。
7. **Raw pointer stays unsafe**：raw pointer、external origin、wildcard origin 不参与 safe proof。
8. **Big chunks, staged enforcement**：实现按大块推进，但每块仍有 collect-only、shadow/parity、enforced 三段验证，避免一次切换破坏
   诊断和增量缓存。
9. **Low coupling by construction**：collector、solver、enforcer、diagnostic builder、tooling projection 必须拆开；公共边界只暴露
   stable facts，不让某个 analyzer 类同时掌握 AST 遍历、约束求解、诊断和 IDE 文案。
10. **Patterns where they pay**：设计模式只在降低耦合、隔离策略或稳定扩展点时使用；不为了形式感套 visitor/factory/strategy。

## 4. M7c 语义设计：完整 safe borrow 与 lifetime core

### 4.1 Lifetime/origin identity

M7c 增加以下内部概念：

```text
UniversalRegion {
  id
  owner: function | type | trait | impl | static
  name: optional IdentId
  kind: parameter | self | static | external | wildcard
  mutability: immutable | mutable | parametric
}

InferenceRegion {
  id
  owner_function
  created_at: PointId
  lower_bounds: set<PointId or UniversalRegion>
  upper_bounds: set<UniversalRegion>
}

OutlivesConstraint {
  longer: RegionId
  shorter: RegionId
  reason: assignment | return | call | field | dropck | trait | reborrow
  range
}

TypeOutlivesConstraint {
  type: TypeHandle
  region: RegionId
  reason: borrowed_field | generic_param | dropck | returned_view
  range
}
```

M7a/M7b 的 `Origin` 继续作为 loan/source fact；M7c 的 `RegionId` 负责 type-level lifetime 与 solver 约束。两者关系：

- `Origin` 回答“这个 borrow 从哪里来”。
- `Region` 回答“这个 reference/type 在哪些 point 或 API 边界上必须有效”。
- `Loan` 是从 `Place + Origin + Point` 发出的访问权。
- `OutlivesConstraint` 把 origin/loan 对返回值、字段、泛型类型和 dropck 的要求提升到 type system。

### 4.2 用户表面：保留低噪声，不复制 Rust lifetime 写法

默认规则：

- 普通参数继续写 `x: &T`、`x: &mut T`。
- 普通函数返回 borrowed value 时继续优先用 `@borrow(return = [...])` 表达来源。
- private function 可以由 body summary 推断；public/prototype/extern/trait requirement 需要 declared contract 或 conservative unknown。

新增显式 origin surface 只用于 type-carrying borrowed storage 和 ambiguous public APIs。推荐语法是 contextual `origin`
参数加 `&[origin] T` reference prefix，不引入 Rust apostrophe token，也不引入新的 `ref` 关键字：

```aurex
struct View[T, origin data] {
    item: &[data] T;
}

@borrow(return = [source])
fn first[T](source: &Slice[T]) -> &T;

fn view_field[T, origin data](source: &[data] T) -> &[data] T;

fn choose[T, origin left, origin right](
    take_left: bool,
    lhs: &[left] T,
    rhs: &[right] T,
) -> &[left | right] T;

fn update[T, origin data](value: &mut[data] T) -> void;
```

说明：

- `&[data] T` 是显式 origin-qualified shared reference type；`&T` 是 `&[_] T` 的低噪声 shorthand。
- `&mut[data] T` 是显式 origin-qualified mutable reference type；`&mut T` 是 `&mut[_] T` 的 shorthand。
- `&[left | right] T` 表示 origin union；返回值只要仍被使用，`left` 和 `right` 对应 owner 都不能被 drop/write/move
  invalidated。
- `origin` 是 contextual generic parameter，不污染普通 type parameter namespace。`fn f[origin](x: origin)` 仍可表示名为
  `origin` 的普通类型参数；只有 `origin name` 这个二元形态声明 origin parameter。
- `@borrow(return = [...])` 与 `&[origin] T` 是同一 checked fact 的两种表面：前者适合函数 contract，后者适合类型声明。

### 4.3 Origin syntax 审计

M7c 的显式 origin 写法必须同时满足四个目标：美观、无歧义、用户友好、编译器友好。候选方案如下：

| 方案 | 示例 | 优点 | 问题 | 结论 |
| --- | --- | --- | --- | --- |
| Rust-style apostrophe | `&'data T` | Rust 用户熟悉 | 视觉噪声高；和 Aurex 当前语法风格不一致；把用户带进 Rust 心智模型 | 拒绝 |
| prefix keyword | `ref[data] T` | 清楚、容易解析 | 需要新增 `ref` contextual keyword；和已有 `&T` 形成两套引用写法 | 拒绝作为 source surface，可作为内部 dump 词汇 |
| bare origin | `&data T` | 短 | `data` 与普通类型名位置冲突，parser 和用户都难判断 | 拒绝 |
| type suffix | `&T from data` | 英文化 | suffix 影响类型组合、formatter 和 recovery，和现有 type grammar 不一致 | 拒绝 |
| decorator-only | `@borrow(field = data) item: &T` | 不改 type grammar | origin 离类型太远，struct/generic wrapper 难读，tooling 定位差 | 只保留函数级 `@borrow` |
| reference origin bracket | `&[data] T` / `&mut[data] T` | 沿用 `&T`；origin 与引用绑定；无需新 keyword；parser context 明确 | 需要扩展 reference type constructor | 采用 |

推荐 grammar：

```text
ReferenceType
  = "&" OriginQualifier? Type
  | "&" "mut" OriginQualifier? Type

OriginQualifier
  = "[" OriginExpr "]"

OriginExpr
  = "_"
  | OriginName ("|" OriginName)*

OriginName
  = identifier | "self" | "static"
```

无歧义规则：

- `&[data] T` 只有在 `[` 内是 identifier / `self` / `static` / `_` / union 时才解析为 origin qualifier。
- `&[16]u8` 仍是 reference to array，因为 `16` 不是合法 origin selector。
- `&[]const u8` 仍是 reference to slice，因为空 bracket 不是合法 origin selector。
- `Box[T]` 的泛型参数只出现在 named type 之后；`&[data] T` 的 bracket 只出现在 reference constructor 之后，parser context 不重叠。
- `|` 只在 `OriginQualifier` 内作为 origin union；它不进入普通 type grammar，也不影响 pattern 的 `|`。
- `origin` 是 generic parameter list 中的 contextual modifier，不新增全局关键字。用户已有变量、类型、字段名叫 `origin` 不受影响。

拒绝方案：

- Rust-style lifetime token：成熟但视觉噪声高，并且会把 Aurex 现有 `@borrow` contract 弱化成注解重复。
- 只保留 `@borrow` 不加 type-level origin：无法表达 struct 中保存 `&T`、generic wrapper、trait associated type 中的 borrowed
  storage。
- 完全采用 Mojo `read/mut/var/ref` 调用约定：方向正确，但会在 M7c 过早改动函数参数语法和 ABI 讨论。

### 4.4 与 lambda、closure 和函数式路线的兼容性

当前 Aurex 已有非捕获函数指针类型：`fn(...) -> T`、`unsafe fn(...) -> T`、`extern c fn(...) -> T`。完整
lambda/closure 捕获、`Fn` / `FnMut` / `FnOnce` 风格能力、iterator adapter 和函数式 pipeline 仍是后续独立设计项，
但 M7c/M7d 不能把语法和事实模型设计死。

结论：

- `&[origin] T` / `&mut[origin] T` 只在 type context 中解析；`|` 只在 `&[left | right] T` 的 origin qualifier 内表示
  origin union。因此它不占用未来表达式级 `|x| expr`、`fn(x) { ... }`、`{ x => ... }` 或 capture-list 语法空间。
- `[` 在 `&[origin] T` 中只有紧跟 reference type constructor 时才是 origin qualifier；表达式里的 `[capture]`、数组字面量、
  generic apply、index/slice 仍由现有 context 区分，不靠 sema 猜。
- capturing closure 不能降级成现有 `fn(...) -> T` 函数指针。非捕获函数指针继续是薄值；捕获 closure 应是 compiler-generated
  environment value，带 capture facts、dropck facts 和 callable contract。
- closure environment 本质上是匿名 struct：按捕获方式保存 owned value、`&[origin] T` shared borrow 或
  `&mut[origin] T` mutable borrow。它的 dropck 和 place-state 规则应复用 M7d，而不是闭包系统单独再发明一次资源模型。
- closure call ability 可以以后再命名，但语义层至少要能区分 shared-call、mutable-call、consuming-call，分别对应
  Rust `Fn` / `FnMut` / `FnOnce` 或 Swift/functional callback 常见的只读、可变、一次性捕获模型。
- 返回或存储 closure 时，closure env 中的 borrowed captures 必须参与 M7c region solver：捕获 local borrow 的 closure
  不能逃出 owner region；捕获 mutable reference 的 closure 在活跃期间要限制 parent place。
- iterator/view/functional adapter 的 item borrow 必须表达为 origin contract，例如从 collection 派生 item view 时，
  yielded reference 的 origin 依赖 collection borrow，而不是凭函数式 API 名字特殊放行。

这说明用户担心的点是实际风险：如果 M7c 把 origin 写法做成表达式级或全局 keyword，就会抢未来 lambda/closure 空间。
`&[origin] T` 的优势是把 origin 限定在 reference type prefix 里，既美观，也把 parser recovery 范围收窄。M7c/M7d
只预留 facts，不实现 closure；closure/lambda 进入时必须复用以下 facts：

```text
ClosureCaptureFact {
  closure_expr
  capture_place
  capture_mode: by_value | shared_borrow | mutable_borrow
  origin
  loan
  range
}

ClosureEnvironmentFact {
  closure_type
  captures
  call_access: shared | mutable | consuming
  dropck_requirements
  returned_origin_dependencies
  fingerprint
}
```

### 4.5 Elision 与 contract inference

M7c elision 规则：

1. 一个 borrowed input，elided borrowed output 默认来自该 input。
2. method receiver 为 `&Self` / `&mut Self` 时，elided borrowed output 默认来自 `self`。
3. 多个 borrowed input 且没有 `@borrow` 或 `&[a | b] T`，public API 报错；private 函数可先用 inferred summary，但导出时必须固化。
4. 没有 borrowed input 且 output 含 borrow，只能来自 `static` 或 explicit unsafe/external contract；否则报错。
5. local/temporary origin 不能进入返回类型或 escaping field。
6. raw pointer derived origin 只能进入 `unknown` / external origin，不能作为 safe proof。

这比 Rust 更贴合 Aurex：常见单输入 view 不写 lifetime，多输入 view 用来源集合讲清楚，不要求用户手工造一个最短公共 lifetime 名。

### 4.6 Region solver

M7c solver 采用确定性 local worklist/bitset：

输入：

- `BodyFlowGraph` point/edge/action。
- `BodyLoanCheckResult` loan/reborrow/two-phase facts。
- `FunctionBorrowSummary` / `FunctionBorrowContract`。
- type-level origin params、reference type、struct field type、trait requirement、generic predicate。
- M6 resource summary 和 cleanup/drop facts。

输出：

- `FunctionLifetimeFacts`：universal/inference region、outlives constraints、type-outlives constraints、solved point sets。
- `LifetimeDiagnostic`：escape、ambiguous elision、outlives violation、dropck violation、unknown proof。
- stable fingerprint：参与 type-check-body authority 和 public API fingerprint。

算法：

1. 为函数签名创建 universal regions：参数、self、static、declared origin params。
2. 为 local borrow/result carrier 创建 inference regions。
3. 从 assignment、return、call binding、reborrow、field initialization、pattern binding 生成 outlives constraints。
4. 从 type containing borrow 生成 `T: region` constraints。
5. 在 CFG 上用 bitset 传播 region live-at point 集合。
6. 对每个 invalidating action 检查 active loan 与 region live-at。
7. 对函数边界检查 solved return regions 是否是 declared contract 的 subset。
8. 对 struct/enum/generic API 检查 stored borrowed field 的 owner region 是否 outlive container value。

复杂度目标：

- 单函数点位 `P`、loan 数 `L`、region 数 `R`，第一版控制在 `O((P + E) * word_count(R + L))`。
- 普通非借用函数 fast path 不生成完整 region solver 输入。
- release `--check` 不长期保留 full `BodyFlowGraph`，只保留 stable facts 与 fingerprint。

### 4.7 Trait / generic / associated type

规则：

- trait requirement 可以带 `@borrow(...)`，也可以在类型中声明 `origin` 参数。
- impl method contract 必须是 requirement contract 的 subset，不能扩大返回来源。
- associated type 如果可能含 borrow，必须携带 type-outlives predicate。
- generic `T` 默认可能含 borrow；只有 `T: Copy` 或 future `T: NoBorrow` 这类明确能力才能放宽。
- 泛型函数不看调用点 body 来证明 callee contract；必须从 signature/contract/param-env 取证。

新增 checked facts：

```text
TraitLifetimeRequirement {
  trait_key
  method_or_assoc
  origin_params
  type_outlives
  borrow_contract
  fingerprint
}

GenericLifetimePredicate {
  subject_type
  outlives_region
  source: explicit | inferred | dropck | associated_projection
}
```

### 4.8 BorrowEscapeAnalyzer 退场条件

M7c 必须把 `BorrowEscapeAnalyzer` 从主诊断路径移除或降级为 unreachable parity guard。不能继续在旧 analyzer 上叠特判。

退场矩阵至少覆盖：

- return local reference。
- return temporary reference。
- return slice/str view from local buffer。
- return raw pointer derived view。
- assignment into escaping struct field。
- pattern alias escape。
- branch/match 多 origin escape。
- generic wrapper `T = &U`。
- trait requirement / impl mismatch。
- method receiver returned view。
- reborrow child escaping parent。
- closure capture escape parity placeholder：M7c 不实现 closure，但新 facts 不得让未来 closure 再走旧 tree-scanning 特判。
- cleanup/drop while returned carrier still live。

验收方式：

- M7c-A shadow：新 facts 与旧 analyzer 对所有现有负例一致。
- M7c-B enforced：新 checker 发主诊断，旧 analyzer 不再发重复诊断。
- M7c-C cleanup：旧 analyzer 删除或保留为 debug-only invariant guard，并在 docs 中记录状态。

## 5. 模块边界、解耦与设计模式

M7c/M7d 的最大工程风险不是“规则不够多”，而是把 lifetime、borrow、dropck、resource、diagnostics、tooling 全塞进
`SemanticAnalyzerCore` 或单个巨大 analyzer。Aurex 后续必须按事实生产和消费关系拆模块。

### 5.1 推荐模块拆分

硬规则：`src/frontend/sema/internal/` 只允许作为 private implementation root，下面不能直接放 `.cpp` / `.hpp` 文件。
新实现必须按职责建子目录；同一职责层级内的 private header 和实现源文件继续拆到 `private/`、`sources/`，不要在同层混放
`.hpp` 与 `.cpp`。现有 direct files 是历史债，M7c/M7d 不继续扩大。

```text
src/sema/internal/lifetime/facts.*
  只定义 FunctionLifetimeFacts、RegionId、OutlivesConstraint、TypeOutlivesConstraint、stable dump/fingerprint。

src/sema/internal/lifetime/collect.*
  从 checked signature、AST/body-flow、borrow summary、contracts、types 生成约束。不做最终诊断，不修改 checker state。

src/sema/internal/lifetime/solve.*
  Deterministic worklist/bitset solver。输入 immutable facts，输出 solved region/live-at facts 和 violation records。

src/sema/internal/lifetime/enforce.*
  把 violation records 转成 sema diagnostics，并执行 cascade suppression。不能重新遍历 AST 推断语义。

src/sema/internal/dropck/facts.*
  DropCheckFact、DropActionFact、destructor requirements、generic dropck predicates。

src/sema/internal/dropck/collect.*
  从 resource summary、lifetime facts、destructor body identity 和 place actions 生成 dropck 约束。

src/sema/internal/dropck/solve.*
  求解 destructor/type-outlives/drop action safety，不读 AST，不发 diagnostics。

src/sema/internal/dropck/enforce.*
  将 dropck violations 转成 sema diagnostics，并负责 cascade suppression。

src/sema/internal/place/state.*
  place-level initialized/moved/drop flag dataflow。供 borrow checker、dropck 和 lowering facts 共用。

src/sema/internal/raii/surface.*
  Drop/deinit 用户表面检查、destructor body identity、Drop capability 约束。

src/sema/internal/borrow/flow.*
src/sema/internal/borrow/loan.*
src/sema/internal/borrow/contract.*
src/sema/internal/borrow/summary.*
  后续整理现有 M7a/M7b direct files 时的目标目录；M7c/M7d 新代码不能继续放在 flat internal root。

src/sema/internal/diagnostics/lifetime.*
src/sema/internal/diagnostics/dropck.*
  诊断文案、notes、suppression 和 fix suggestion；不能承担语义推断。

src/sema/internal/pipeline/*.*
  只编排 pass，不持有 solver scratch 或复杂业务逻辑。

src/tooling/*
  只投影 CheckedModule facts，不直接调用 lifetime/dropck solver。
```

`include/aurex/frontend/sema/checked_module.hpp` 只能暴露稳定、可 fingerprint、可 query 的数据形状。复杂算法 helper、worklist state、
bitset scratch、diagnostic assembly 不进 public header，避免把实现细节变成 ABI/API 耦合。

这条目录规则不只适用于 sema。后续 parser、IR、driver、tooling、backend 只要出现 `internal/` 或类似私有实现目录，都必须把文件按
grammar、facts、solver、lowering、pass、diagnostics、adapter、backend-emission 等职责继续分层；禁止把 unrelated files
堆在同一个私有根目录下。

### 5.2 数据流边界

推荐 pipeline：

```text
typed body + resource facts + BodyFlowGraph + BorrowContract
  -> LifetimeFactCollector
  -> LifetimeSolver
  -> LifetimeEnforcer
  -> CheckedModule lifetime facts + diagnostics

resource summaries + place actions + lifetime facts
  -> DropCheckFactCollector
  -> DropCheckSolver
  -> DropCheckEnforcer
  -> CheckedModule dropck facts + lowering obligations

place actions + resource summary
  -> PlaceStateAnalyzer
  -> place state facts
  -> borrow/dropck/lowering
```

硬约束：

- Collector 不报错，只记录 facts 和 source ranges。
- Solver 不读 AST，不读 diagnostics sink，不访问 tooling。
- Enforcer 不修改 solver 输入，不做新推断，只消费 violation records。
- Lowering 不重新推导 lifetime/dropck/place state，只消费 sema checked facts。
- Tooling 不重新跑 sema，只显示 facts。
- Query/cache fingerprint 不混入不稳定 source order 或绝对路径。

### 5.3 适合使用的设计模式

- **Strategy**：projection conflict policy、origin elision policy、dropck generic policy。这样可以在 M7c/M7d/M8 逐步替换策略，
  不把所有 if/else 写进 solver。
- **Builder**：`LifetimeFactBuilder`、`DropCheckFactBuilder`、`PlaceStateFactBuilder`。适合把 AST/body-flow/call-binding
  输入整理成不可变 facts。
- **Facade**：`LifetimeAnalysisPass` 对 sema pipeline 暴露一个薄入口，内部编排 collect/solve/enforce；pipeline 不知道内部细节。
- **Adapter**：IDE/LSP/query projection 从 checked facts 转成外部 DTO，避免 LSP 类型进入 sema。
- **Value object**：`RegionId`、`LoanId`、`PlaceId`、`DropActionId`、`ConstraintReason` 使用小型强类型 ID，减少 index 混用。

不建议使用：

- 大型 inheritance hierarchy：borrow/lifetime/dropck 是数据流问题，不需要虚函数森林。
- Visitor 泛滥：现有 parser/AST 已有自身访问模式；新增 visitor 只有在多个独立 pass 共享 traversal 时才值得引入。
- Service locator：会隐藏依赖，让 solver 偷偷访问 diagnostics/tooling/cache。
- Singleton/global mutable state：会破坏 query/incremental 和并发分析空间。
- 字符串 DSL：contract、origin、dropck 不能靠字符串解析；必须是 AST/checked facts。

### 5.4 类与文件大小约束

实现时应主动防止以下坏味道：

- 一个 analyzer 同时超过“收集 facts、求解、诊断、tooling 投影”两个职责。
- public header 暴露 transient scratch vectors、worklist queues、diagnostic cache。
- `SemanticAnalyzerCore` 继续增加大量私有 helper，使所有模块都 friend 它。
- `src/<stage>/internal/` 继续直接新增文件，而不是按职责放入子目录。
- `CheckedModule` 增加无法 stable dump/fingerprint 的临时状态。
- body-flow、borrow、lifetime、dropck 各自重复遍历 AST，造成性能和诊断不一致。

推荐做法：

- 每个 pass 的 `.hpp` 只放薄 class interface 和 public fact helper；复杂实现放 `.cpp`。
- 共享 facts 用 plain structs + stable IDs；共享算法用小型 free functions 或局部 helper class。
- 大函数按“collect input -> normalize -> solve -> emit result”分解，保持可测试。
- 新增测试按模块镜像 production file，避免所有 whitebox 都堆到一个巨型 test。

## 6. M7d 语义设计：RAII、dropck 与 place-level resource state

### 6.1 RAII 的安全目标

M7d 要保证：

- 所有 `NeedsDrop` value 在所有 normal/early-exit path 上准确 drop 一次。
- moved-out / partially initialized place 不被重复 drop。
- drop 前不存在活跃 shared/mutable loan 会被 destructor、field drop 或 cleanup invalidated。
- destructor body 不能读取已经 dangling 的 borrowed field。
- generic drop glue 生成并检查 `T: region` 或等价 type-outlives predicate。
- `defer`、`return`、`break`、`continue`、`?` 和 panic/unwind 策略与 cleanup order 一致。

### 6.2 Destructor surface

M7d 第一版建议：

```aurex
impl Drop for File {
    fn drop(self: deinit File) -> void {
        close(self.fd);
    }
}
```

约束：

- `Drop` 仍是 compiler-owned capability，用户不能伪造 arbitrary trait bound。
- `drop` receiver 使用 `deinit`/sink-like convention：函数开始时 `self` initialized，返回时 `self` uninitialized。
- destructor 不返回 borrowed reference，不允许把 `self` 的 field borrow 逃逸。
- destructor 中不能显式调用导致同一 object 递归 drop 的 API，第一版用保守诊断限制。
- destructor 不能 fail unless language 后续有明确 unwind/abort cleanup 模型；M7d 第一版按 non-throwing/drop-no-fail 设计。

当前 M7d-C 已采用该窄 surface：`deinit` 是参数冒号后的 contextual 修饰符，只在 Drop self 参数位置有语义；
`Drop` 是 compiler-owned reserved destructor surface，不作为普通用户 trait 或 generic bound 暴露。backend custom
destructor call lowering 仍未完成，因此该阶段先收口 semantic/checking/tooling facts。

### 6.3 Dropck facts

新增：

```text
DropCheckFact {
  type
  destructor_function
  field_regions
  required_outlives: [(TypeHandle or RegionId) outlives RegionId]
  may_observe_fields: PlaceSet
  may_move_fields: PlaceSet
  fingerprint
}

DropActionFact {
  place
  type
  point
  cleanup_kind: lexical | overwrite | early_exit | explicit_drop | defer
  destructor_key
  range
}
```

检查：

- `drop(place)` 对所有 still-initialized borrowed fields 生成 type-outlives/dropck 约束。
- 如果 destructor 可能访问 `field: &[a] T`，则 `a` 必须 outlive destructor execution point。
- 如果 generic `struct BoxView[T] { value: T }` 有 destructor 且 `T` 可能含 borrow，则生成 `T: drop_region` 约束。
- 对 `#[may_dangle]` 类能力不在 M7d 第一版开放；后续 unsafe library verification 再讨论。

### 6.4 Place-level resource state

M7d 引入 place state dataflow：

```text
PlaceState {
  place
  initialized: definitely | maybe | no
  moved: no | maybe | yes
  needs_drop: bool
  drop_flag: optional
  last_write_point
  last_move_point
}
```

覆盖：

- struct field partial move。
- tuple element partial move。
- enum payload conservative move。
- array/index move-out 第一版继续拒绝，除非常量 index 和 drop flag 能证明。
- reinit after partial move。
- overwrite old value 先 drop old initialized field，再 write new value。
- `replace` / `take` / `swap` 作为 compiler-known primitives，生成精确 move/reinit/drop actions。

与 borrow checker 的关系：

- move/drop/write/reinit/cleanup 都是 invalidating actions。
- partial move 只 invalidates 对被移动 place 及 prefix/same projection 的 loans。
- known-disjoint fields 可独立 move/drop/reinit。
- container-level drop 需要按 field initialized state 发 drop actions。

### 6.5 Cleanup lowering 与 IR

IR 已有 `drop` / `drop_if`，M7d 需要前端提供更精确的 drop plan：

- sema 生成 `DropActionFact` 和 place state。
- lowering 消费 facts，生成 drop flag 和 `drop_if`，不重新推断资源状态。
- IR verifier 增加 drop action consistency：drop target type、drop flag type、double-drop impossibility、drop after move 禁止。
- pass pipeline 的 cfg cleanup 不得删除仍有 semantic cleanup obligation 的路径。

## 7. 比 Rust 更适合 Aurex 的增强点

M7c/M7d 不承诺“一次超过 Rust 所有功能”，但可以在几个核心体验上比 Rust 更适合 Aurex：

1. **Origin contract 优先**：`@borrow(return = [a, b])` 直接表达来源集合，IDE 能展示“返回值依赖哪些输入”，比裸 lifetime name
   更容易读。
2. **Origin union 一等事实**：多输入 borrowed return 不强迫用户手工命名一个公共 lifetime，compiler facts 直接保存 union。
3. **Parametric mutability 预留**：未来 `&[origin, mut = M] T` 或等价 surface 允许 getter 根据 receiver mutability 返回 view，
   减少重复 API。
4. **RAII/dropck 早整合**：M7d 就把 drop 当 borrow action，避免先有 destructor 语法、后补 dropck 的 C++ 式历史债。
5. **工具链可解释**：borrow/lifetime/drop facts 稳定进入 `CheckedModule`、query、cache、IDE/LSP，而不是隐藏在单个 checker
   内部。
6. **safe/unsafe 分层更硬**：raw pointer 不参与 safe proof，external/unknown origin 不能混入 safe ref。
7. **值语义优先**：借 Hylo/MVS 的经验，不把所有性能能力都暴露成可存储 reference；普通代码继续 value-first。
8. **资源能力更细**：M6 已拆 `Copy`、`Discard`、`NeedsDrop`，后续可加 `MustConsume`，这比 Rust `Drop`/`Copy`
   二元表面更适合显式资源协议。

## 8. 实现分期

### M7c-A：Lifetime surface 与 checked facts

交付：

- Parser/AST 支持 contextual `origin` 参数和 `&[origin] T` / `&mut[origin] T` type surface。
- Type system 增加 origin-qualified reference type、origin union qualifier、anonymous inferred origin。
- `CheckedModule` 增加 `FunctionLifetimeFacts`、`TypeLifetimeInfo`、`GenericLifetimePredicate`。
- `@borrow(...)` 与显式 origin surface 映射到同一 contract/facts。
- 不改变 diagnostics 行为，先提供 dump、fingerprint、whitebox tests。

验收：

- AST dump / checked dump 稳定。
- public signature fingerprint 因 origin contract 改变而改变。
- 不含 borrow 的普通函数走 fast path。
- 文档和 language manual 更新。

### M7c-B：Region solver 与 enforcement

交付：

- Deterministic worklist/bitset region solver。
- Elision/ambiguity diagnostics。
- type-outlives 和 return-origin subset enforcement。
- trait/generic/associated type lifetime predicates。
- `BorrowEscapeAnalyzer` shadow parity。

验收：

- local/temporary/raw escape 由新 checker 诊断。
- branch/match 多 origin return 正确。
- generic wrapper `T = &U` 不漏。
- trait impl contract subset 正确。
- query/cache/tooling facts 稳定。

### M7c-C：BorrowEscapeAnalyzer 退场与 public API closure

交付：

- 旧 analyzer 删除或降级。
- function/prototype/extern/trait 的 lifetime contract release policy。
- IDE hover/diagnostic 展示 origin set、outlives reason、fix suggestion。
- release perf/coverage/query gates 收口。

验收：

- 所有旧 borrowed-view escape negative tests 由新 checker 覆盖。
- diagnostics 无重复、无 cascade。
- full CTest、coverage >= 95%、query sanitizer、perf stress 通过。

### M7d-A：Dropck facts 与 destructor safety

交付：

- `DropCheckFact`、`DropActionFact`、dropck solver。
- Destructor body identity 接入 lifetime facts。
- drop as invalidating action 覆盖 lexical cleanup、explicit drop scaffold、overwrite、early exit。
- 泛型 drop glue 生成 type-outlives constraints。

验收：

- drop while borrowed 拒绝。
- borrowed field destructor dangling 拒绝。
- generic `T` containing borrow dropck 正确。
- `defer` / `return` / `?` cleanup order 与 borrow facts 对齐。

### M7d-B：Place-level resource state 与 partial move/reinit

交付：

- 已完成：place state dataflow。
- 已完成：本地 owned struct field partial move、field reinit 和字段级 cleanup/drop flags。
- 已完成：generic template body-flow 读取 generic side table 类型，struct field cleanup 不再丢失 `Box[T]` 字段类型。
- 已完成：lowering 为 struct droppable fields 建立字段级 drop flag，并在 field move/reinit/cleanup 时更新。
- 未完成：tuple partial move；当前 `.0` / `.1` source surface 仍由 parser 拒绝。
- 未完成：indexed move-out 和 array/slice/index 精确 disjoint proof。
- 未完成：`replace` / `take` / `swap` compiler-known primitives。
- 未完成：通过 borrowed/reference base 的 resource field overwrite。

验收：

- 已通过：partial field move 后只 drop initialized fields。
- 已通过：moved field 使用报错，field reinit 后通过。
- 已通过：known-disjoint struct fields 的 cleanup/drop flag 不互相污染。
- 未通过/未实现：borrowed/reference field overwrite 的 old resource drop proof。
- 未通过/未实现：tuple/index partial move 和 `replace` / `take` / `swap`。

### M7d-C：RAII user surface 与 release closure

交付：

- 已完成：`Drop` / `deinit self` 窄 destructor surface，语法为
  `impl Drop for T { fn drop(self: deinit T) -> void { ... } }`。
- 已完成：`Drop` reserved surface 诊断，拒绝 `trait Drop`、qualified/type-arg Drop surface、generic Drop impl、
  associated type、额外方法、borrow contract、unsafe/extern/export/variadic Drop method 和非法 self/return type。
- 已完成：`CheckedModule::destructors`、`FunctionSignature::is_destructor`、checked dump、clone/copy 和 stable
  fingerprint。
- 已完成：resource classifier、drop-glue `custom_destructor` step、dropck destructor facts、query authority 和
  IDE hover `destructor=custom`。
- 已完成：IR verifier 对 immutable drop target 的拒绝。
- 已完成：docs/language manual/version/progress/next-steps 更新。
- 未完成：backend custom destructor call lowering；当前 `drop` / `drop_if` 仍是 backend cleanup marker。
- 未完成：用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、async/unwind-aware drop 和标准库拥有型资源封装。

验收：

- release full CTest。
- coverage >= 95%。
- query sanitizer/query graph fuzz。
- perf-release threshold。
- docs/version/progress/next-steps 更新。

## 9. 明确延期项

M7c/M7d 完成后仍不等于“完整 Rust 语言”。以下应作为 M7e/M8+ 独立设计：

- HRTB / higher-ranked lifetime。
- full variance。
- `dyn Trait` object lifetime bound 和 object safety。
- async/generator/coroutine borrow。
- full Polonius Datalog runtime。
- Stacked-Borrows-level unsafe alias operational semantics。
- interior mutability / GhostCell-style library proof。
- concurrency/data-race capability model。
- `MustConsume` 资源协议。
- array/slice/index 精确 disjoint proof。
- self-referential struct / pinning / address-stability。

## 10. 测试策略

新增测试按层组织：

- Parser/AST：origin params、`&[origin] T` / `&mut[origin] T`、origin union、error recovery。
- Sema unit：region id allocation、constraint generation、solver fixed point、fingerprint。
- Source positive：single input elision、receiver elision、多输入 explicit contract、stored view、trait impl subset、generic wrapper。
- Source negative：local/temporary/raw escape、ambiguous output、drop while borrowed、destructor dangling field、partial move after use。
- Checked dump golden：lifetime facts、dropck facts、place state。
- Tooling：hover/definition/diagnostic facts 不重新跑 sema。
- Query/cache：signature contract 改动触发正确 invalidation；body-only borrow fact 改动不污染 unrelated module surface。
- IR/lowering：drop/drop_if、drop flag、partial move/reinit、early exit cleanup。
- Perf：非借用函数 fast path，大型 AST、泛型实例、diagnostic stress。

## 11. 参考资料

- Rust RFC 2094: Non-Lexical Lifetimes, <https://rust-lang.github.io/rfcs/2094-nll.html>
- Rust Compiler Development Guide: MIR borrow check, <https://rustc-dev-guide.rust-lang.org/borrow-check.html>
- Rust Compiler Development Guide: Two-phase borrows, <https://rustc-dev-guide.rust-lang.org/borrow-check/two-phase-borrows.html>
- Rust Reference: Lifetime elision, <https://doc.rust-lang.org/reference/lifetime-elision.html>
- Rust Reference: Trait and lifetime bounds, <https://doc.rust-lang.org/reference/trait-bounds.html>
- Rustonomicon: Drop Check, <https://doc.rust-lang.org/nomicon/dropck.html>
- Polonius project, <https://github.com/rust-lang/polonius>
- Stacked Borrows: An Aliasing Model for Rust, <https://plv.mpi-sws.org/rustbelt/stacked-borrows/>
- RustBelt: Securing the Foundations of the Rust Programming Language, <https://doi.org/10.1145/3158154>
- Oxide: The Essence of Rust, <https://arxiv.org/abs/1903.00982>
- C++ Core Guidelines, Lifetime profile and resource safety, <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-lifetime>
- Safe C++ proposal P3390R0, <https://safecpp.org/P3390R0.html>
- Swift Evolution SE-0377: Parameter ownership modifiers, <https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md>
- Swift Evolution SE-0390: Noncopyable structs and enums, <https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md>
- Mojo Manual: Ownership, <https://docs.modular.com/mojo/manual/values/ownership/>
- Mojo Manual: Lifetimes, origins, and references, <https://docs.modular.com/mojo/manual/values/lifetimes/>
- Hylo language site and research index, <https://hylo-lang.org/>
- Mutable Value Semantics, Journal of Object Technology 2022, <https://research.google/pubs/mutable-value-semantics/>
- Native Implementation of Mutable Value Semantics, <https://arxiv.org/abs/2106.12678>

# Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线

## 1. 文档状态

本文是 M6-WP1 设计基线。它完成 M5 之后资源语义专题的三轮设计审视：

1. 第一轮从现代语言、工业编译器和前沿研究中建立证据矩阵。
2. 第二轮在证据矩阵上固定 Aurex 自己的语义组合、编译器边界和实现顺序。
3. 第三轮从使用者角度使用真实案例、失败路径和未来扩展压力测试设计。

本文不是“照抄 Rust borrow checker”的方案，也不是立刻添加一个 `Drop` trait 的语法草案。M6 的目标是先把
资源和值生命周期做成稳定语言地基，再让后续借用检查、拥有型标准库、动态 trait object 和并发能力建立在同一套
事实模型上。

当前设计结论：

- M6 正式名称是 **Resource And Access Semantics**。
- M6 第一条实现主线是值分类、whole-local move、use-after-move、确定性 cleanup 和 generic drop glue。
- 完整 borrow checker 不属于 M6 第一批实现。它在 M6 稳定后作为 M7 独立设计流推进。
- `Copy`、允许丢弃、需要清理和必须消费是不同维度，不能继续混成一个模糊的 `Drop` capability。
- `defer` 保留为语言能力，并与 compiler-inserted cleanup 进入同一套词法 cleanup action 模型。
- 第一版不做 partial move、indexed move-out、异常展开、async drop、全局资源析构、完整 lifetime 语法、
  `dyn Trait` 或 array/aggregate ABI 全量解禁。

## 2. 为什么 M6 现在启动

M5 已收口 static trait default methods。当前 Aurex 已经具备启动资源专题所需的地基：

| 地基 | 当前状态 | 代码入口 |
| --- | --- | --- |
| 引用和值类别基础 | `TypeKind` 已区分 `pointer`、`reference`、`slice`、`str`、aggregate 和 generic param | `include/aurex/sema/type.hpp` |
| safe/raw 边界 | `&T` / `&mut T` 与 raw pointer 分离；`&mut` 要求 writable place；raw pointer 解引用需要 `unsafe` | `src/sema` |
| 显式词法清理 | `defer` 已覆盖正常离开、`return`、`break`、`continue` 和 `?` failure 路径 | `src/ir/lower_ast_stmt.cpp` |
| 泛型 capability | `Sized`、`Eq`、`Ord`、`Hash` 已落地；`Copy` / `Drop` 会被识别后明确拒绝 | `src/sema/internal/sema_generic_analyzer.cpp` |
| 静态 trait | nominal trait、显式 impl、associated type、default method、静态 direct call 已完成 | M4 / M5 |
| 稳定 query 身份 | `BodySlotKind::destructor_drop`、`GenericParamKind::resource` 和 `lifetime` 已预留 | `include/aurex/query/query_key.hpp` |
| 真实资源压力 | regex 库大量依赖 `defer destroy(&mut value)` 和手工 `free_typed` | `examples/libs/regex` |

但当前 `defer` 不是 RAII。它不能判断值是否已被移动，不能表达 drop obligation、drop flag、字段级 glue、
覆盖赋值清理或泛型资源行为。当前 IR 也只有 `alloca`、`load`、`store`、`call`、`aggregate` 等普通值节点，
没有正式 cleanup 节点。覆盖赋值仍然直接 `store`。

因此 M6 不是库层包装工作，而是编译器语义主线。

## 3. 第一轮审视：跨语言和研究证据矩阵

### 3.1 先拆开四类问题

现代语言没有一套适用于所有场景的唯一资源系统。设计前必须区分四类问题：

| 问题 | 典型错误 | 解决层 |
| --- | --- | --- |
| 值是否可复制 | double free、昂贵隐式深拷贝、意外复制锁句柄 | value capability |
| 资源何时释放 | 泄漏、double close、覆盖赋值漏清理、提前返回漏清理 | deterministic cleanup |
| 访问是否冲突 | 同时存在可变和共享访问、迭代期间修改容器 | access / alias analysis |
| 引用是否悬空 | 返回局部引用、容器重分配后继续持有 view | lifetime / origin analysis |

RAII 主要解决第二类问题。borrow checker 主要覆盖第三和第四类问题。两者相关，但不能在设计上混为一谈。

### 3.2 C++：吸收 RAII，拒绝历史复杂度

C++ 的关键价值是确定性析构：

- 资源绑定到对象生命周期。
- 局部对象按构造完成顺序的逆序析构。
- 覆盖旧值时必须处理旧资源。
- scope-bound resource handle 能统一文件、锁、内存和句柄。

Aurex 采纳：

- 词法作用域确定性 cleanup。
- named local 在成功初始化后激活 cleanup。
- 覆盖赋值必须先保留旧值直到 RHS 求值结束，再清理旧值并写入新值。
- 自定义 cleanup 与字段递归 cleanup 分层。

Aurex 拒绝：

- 隐式 copy constructor、move constructor、copy assignment、move assignment 和 destructor overload 组合。
- rule of three / five / zero 的历史兼容复杂度。
- exception unwind ABI 和跨模块隐式特殊成员合成。
- 让普通方法重载决定资源安全。

### 3.3 Rust 和 rustc：吸收分层 elaboration，不复制完整表面

Rust 将 move、初始化状态、drop obligation 和 borrow checking 分层。rustc MIR drop elaboration 会根据控制流把
drop 分类为静态 drop、dead drop、conditional drop 和 open drop，并在必要时使用 drop flag。

Aurex 采纳：

- whole-local move 后源值不可再作为 owned value 使用。
- CFG-sensitive 数据流，而不是纯词法块检查。
- cleanup obligation 先形成 checked fact，再经过 elaboration 降低。
- 条件初始化和分支 join 需要 drop flag。
- raw pointer 继续留在显式 `unsafe` 边界。

Aurex 后移：

- partial field move、indexed move-out、任意 place-tree move path。
- 完整 lifetime 参数和 higher-ranked region。
- two-phase borrow。
- Polonius 级关系分析。

原因不是这些能力不重要，而是它们必须建立在稳定值语义和 cleanup 模型之上。

### 3.4 Swift：吸收 ownership convention 和 noncopyable 泛型教训

Swift 的 `borrowing` / `consuming` 参数和 noncopyable 类型说明：资源语义不只是给 struct 增加一个标志。
它会影响参数契约、泛型、析构、ABI、pattern、调用者可读性和库演进。

Aurex 采纳：

- 在语义层明确区分 owned consume、shared borrow 和 mutable borrow。
- by-value 参数是 ownership boundary，不是模糊的“复制一些字节”。
- noncopyable aggregate 的能力按字段传播。
- 用户可见 diagnostics 必须解释某次使用是 copy、borrow 还是 consume。

Aurex 第一版不直接复制 Swift ARC。共享所有权应由后续库类型表达，不应成为每个 Aurex 值的默认运行时成本。

### 3.5 Mojo：吸收 transfer 和 last-use 讨论，拒绝默认 ASAP 析构

Mojo 明确区分借用、可变借用、owned 参数和 ownership transfer，并允许编译器围绕最后一次使用优化生命周期。

Aurex 采纳：

- move 是 ownership transfer，不应被描述为必然执行物理内存复制。
- optimizer 可以在不改变可观察行为时消除 relocation 或 copy。
- API 文档和 tooling 应能显示参数消费模式。

Aurex 拒绝把 named resource 的默认析构时机缩短到最后一次使用。文件、锁、事务和外部句柄的释放时机可能可观察。
M6 named local 使用稳定词法生命周期；临时值使用明确的 full-expression cleanup boundary。未来只能在证明析构无可观察
副作用时做更激进优化。

### 3.6 Move：关键启发是把能力拆开

Move 将 `copy`、`drop`、`store`、`key` 作为不同 abilities。这里最重要的不是链上存储，而是：

> “允许复制”“允许静默丢弃”“离开生命周期时需要执行清理”不是同一件事。

Aurex 采纳能力拆分，但调整术语：

- `Copy`：普通值上下文是否允许复制。
- `Discard`：owned value 是否允许在没有显式业务消费动作时结束生命周期。
- `NeedsDrop`：编译器是否必须执行 cleanup glue。
- `MustConsume`：未来 opt-in 线性资源方向；第一版只预留，不开放语言表面。

Move 中名为 `drop` 的 ability 更接近 Aurex 的 `Discard`，不能与析构协议混用同一个名字。

### 3.7 Zig 和 Go：`defer` 的经验不能丢

Zig 的词法 `defer` 和 `errdefer` 说明：即使语言有清晰资源 API，显式退出动作仍然有价值。它适合：

- FFI raw handle。
- 分步初始化失败回滚。
- 临时恢复状态。
- 不值得包装成完整 owned type 的窄资源边界。

Go 的 `defer` 是函数级模型，参数在登记时求值。Aurex 当前实现更接近 Zig 的词法作用域模型：deferred call
在作用域退出时执行，并按逆序展开。

Aurex 选择：

- 保留词法 scope `defer`。
- `defer` 与 compiler cleanup 共用 cleanup action stack。
- deferred call 的表达式和参数在 cleanup action 执行时求值。需要 snapshot 的用户应先绑定局部值。
- 该规则必须进入语言文档和测试，不能继续依赖实现偶然行为。

### 3.8 Hylo：place、projection 和 access 是后续借用检查的正确抽象

Hylo 的 mutable value semantics 强调：很多 API 不需要长期引用身份，而需要受控的临时 access。对 Aurex 的启发是：

- M7 不应只围绕“引用变量”建模。
- place、projection、read、mutate 和 consume 应成为统一分析词汇。
- 字段、索引和解构将来都应进入 place-tree，而不是各自补丁。

M6 第一版只支持 whole-local move，正是为了给后续 place-tree 留出清楚升级路径。

### 3.9 Pony 和 Verona：为 isolation、region 和并发保留入口

Pony reference capabilities 和 Verona region/capability 研究说明，所有权系统还可以服务于：

- 数据竞争隔离。
- 对象图区域回收。
- actor / task 之间的安全转移。
- arena 生命周期。

Aurex M6 不实现 capability lattice、region inference 或并发 `Send` / `Sync`。但资源分类和 query identity 不得把
未来扩展堵死。`OwnershipDomain`、resource fingerprint 和 place origin 必须允许以后增加 isolation 或 region metadata。

### 3.10 Cyclone：region 是高级层，不是第一块地基

Cyclone 的 region-based memory management 证明，region 可以在 C-like 系统语言中提供强生命周期保证，并改善 arena
和对象图场景。但 region polymorphism 会扩展类型、调用契约、错误信息和推导复杂度。

Aurex 结论：

- region 是 M8 以后值得设计的 opt-in 高级能力。
- M6 cleanup glue 和 M7 origin analysis 应避免把所有 owned 值写死成单对象 heap ownership。
- 第一版仍以词法 owned local 和显式 borrow 为中心。

### 3.11 Lean、Koka、Roc 和 Perceus：RC 与 reuse 是优化方向，不是唯一语义

Lean 和 Perceus 路线说明：精确 reference counting、borrow inference 和 reuse analysis 可以让函数式值在唯一持有时原地
更新，并减少 allocation。

Aurex 采纳：

- 未来 `Rc[T]`、字符串、持久化集合和 COW 容器可以使用 RC / uniqueness / reuse 优化。
- IR 不应假定每次 move 都执行物理复制。
- optimizer 可以在语义稳定后增加 retain/release elimination 和 reuse pass。

Aurex 不采纳：

- 用默认 RC 替代所有资源管理。
- 假设 RC 自动解决 cycle、文件关闭、锁释放或事务提交。

### 3.12 Linear Haskell、Idris 2、Granule 和 Austral：必须消费是独立能力

线性和 quantitative type 研究说明，资源系统不只关心“会不会 double free”，还可能关心“某个值必须使用一次”。
例如事务 token、权限票据、协议状态机和一次性 capability。

Aurex 结论：

- 预留 `MustConsume`，但 M6 第一版不开放。
- 普通文件、容器和锁 guard 仍优先使用 RAII，不要求用户手工证明 exact-once consume。
- 将来引入 must-consume 时，必须与 `Discard` 和 `NeedsDrop` 分开。

### 3.13 Carbon 和 Clang：渐进安全与诊断质量同样重要

Clang Lifetime Safety Analysis 使用 owner、pointer、borrow、origin 和 control-flow 思路在 C++ 上发现生命周期问题。
它不是一个完整 sound type system，但说明：

- origin chain 对诊断非常重要。
- “值从哪里来、在哪次操作后失效”应成为一等事实。
- tooling 投影不能等到最后补。

Aurex M6 从 WP3 起就记录 move origin、cleanup origin 和类型分类原因。M7 再扩展 loan origin。

### 3.14 Oxide、RustBelt、Aeneas、Polonius 和新 region 研究

形式化和验证研究提供边界约束：

- Oxide 说明 ownership 类型规则需要清楚的 place、environment 和 lifetime 关系。
- RustBelt 说明 unsafe 抽象边界不能只依靠表面语法，库封装契约同样关键。
- Aeneas 说明把借用语言降低到较小的、可验证的中间形式有长期价值。
- Polonius 说明更关系化的 borrow 分析可以表达复杂事实，但工程成本高。
- 新 region 研究值得跟踪，但不能替代已经成熟的分阶段工程路线。

Aurex 的直接结论是：M6 必须先产出可查询、可验证的资源 facts，M7 才有资格增加借用关系。

## 4. 第一轮结论：Aurex 不选择单一语言复制路线

Aurex 选择以下组合：

```text
C++ 的确定性 RAII
+ Move 的能力拆分
+ Swift / Mojo 的 consume / borrow 调用契约
+ Rust / Clang 的 CFG 数据流和 origin 诊断
+ Zig 的显式 defer 逃生舱
+ Hylo 的 place / access 升级方向
+ Cyclone / Verona 的未来 region 扩展入口
+ Lean / Koka 的未来 RC 和 reuse 优化入口
```

这是一套 Aurex 自己的分层模型，不是 Rust 子集。

## 5. 第二轮审视：Aurex 语义决策

### 5.1 资源属性必须是正交向量

每个可存储类型都需要得到结构化 `ResourceSemanticsSummary`。概念形状如下：

```text
copy:        Copy | MoveOnly
discard:     Discard | MustConsume
cleanup:     Trivial | NeedsDrop
ownership:   OwnedValue | BorrowedView | RawPointer | SharedManaged
```

这里的 `SharedManaged` 只为未来库类型和优化预留，M6 不把它变成默认 ABI。

四个维度不能压缩成一个枚举。例如：

| 类型 | copy | discard | cleanup | ownership |
| --- | --- | --- | --- | --- |
| `i32` | `Copy` | `Discard` | `Trivial` | `OwnedValue` |
| `&T` | `Copy` | `Discard` | `Trivial` | `BorrowedView` |
| `str` | `Copy` | `Discard` | `Trivial` | `BorrowedView` |
| raw `*mut T` | `Copy` | `Discard` | `Trivial` | `RawPointer` |
| future `Vec[T]` | `MoveOnly` | `Discard` | `NeedsDrop` | `OwnedValue` |
| future transaction token | `MoveOnly` | `MustConsume` | type-specific | `OwnedValue` |

### 5.2 `Copy` 是 compiler-owned capability

`Copy` 表示普通 owned use 是否允许保留源值。

规则：

- 数字、布尔、字符、函数指针、raw pointer、reference、slice 和 `str` 默认 `Copy`。
- tuple、array、struct 和 enum 只有在所有 owned components 都是 `Copy` 时才结构化满足 `Copy`。
- generic `T` 只有在 param environment 中存在 compiler-owned `T: Copy` evidence 时才可复制。
- `Clone` 如果未来加入，是普通显式操作，不会自动满足 `Copy`，也不会被赋值或传参偷偷调用。
- 用户不能通过普通 trait impl 把不安全类型伪装成 `Copy`。

第一版可以让 nominal 类型通过受限声明请求 `Copy`，但 compiler 必须结构化验证。不能提供 C++ 式用户自定义隐式 copy
constructor。

### 5.3 `Discard`、`NeedsDrop` 和 `MustConsume` 分离

规则：

- `Discard` 表示 owned lifetime 可以在没有业务层显式 consumer 的情况下结束。
- `NeedsDrop` 表示结束 initialized lifetime 时 compiler 必须执行 cleanup glue。
- 一个 `NeedsDrop` 资源通常仍然是 `Discard`：因为自动 cleanup 正是合法的结束方式。
- `MustConsume` 表示不能仅靠作用域离开静默结束，未来需要显式消费证明。
- M6 第一版实现 `Discard` / `NeedsDrop` 内部 facts，但不开放用户可写 `MustConsume` surface。

因此不引入模糊的 `where T: Drop`。对任意可存储 owned `T`，编译器都可以构造 drop glue：可能是 no-op，也可能递归调用
清理。`NeedsDrop` 是内部优化和 lowering fact，不是用户随意 impl 的普通 trait。

### 5.4 Owned use、borrow 和 mutate

M6 先固定 use mode：

| 使用上下文 | `Copy` 类型 | `MoveOnly` 类型 |
| --- | --- | --- |
| `let b = a` | copy，`a` 仍 initialized | consume，`a` 进入 moved |
| by-value 参数 `fn f(x: T)` | copy | consume |
| `return a` | copy 或可消除 copy | consume 到 caller |
| aggregate field / enum payload 构造 | copy | consume 到新 owner |
| `&a` | shared borrow，不 consume | shared borrow，不 consume |
| `&mut a` | mutable borrow，不 consume | mutable borrow，不 consume |

M6 不复活 M1 的 ad hoc `move(...)` builtin。语义由 owned-use context 决定。未来如果用户案例证明显式意图标记有价值，
可以单独设计 `consume expr`，但它不能成为编译器正确性的前提。

### 5.5 whole-local move 是第一版硬边界

第一版允许：

```aurex
let first: Resource = open();
let second: Resource = first; // consume first
use(second);
```

第一版拒绝：

```aurex
let field = pair.left;  // move-only field move-out: later place-tree work
let value = values[i];  // indexed move-out: later container/place work
let .some(item) = opt;  // non-Copy payload extraction: staged later
```

原因：

- whole-local analysis 只需要局部变量 bitset。
- partial move 需要 move path tree、父子 place 状态、字段 drop flag、enum active variant 和 pattern 绑定策略。
- indexed move-out 还需要容器 hole policy。

不能为了语法演示在第一版做不完整 partial move。

### 5.6 CFG-sensitive move analysis

每个函数体使用迭代式 worklist，不写递归数据流。对每个局部变量维护：

```text
definitely_initialized
maybe_initialized
last_consume_origin
```

transfer：

- successful initialization / reinitialization：加入 definitely 和 maybe。
- non-`Copy` owned consume：从 definitely 和 maybe 移除。
- join：definitely 取交集，maybe 取并集。
- read / borrow / consume 前：
  - 不在 maybe：报 definite use-after-move 或 use-before-init。
  - 在 maybe 但不在 definitely：报 maybe-moved。
  - 在 definitely：允许。

cleanup 分类：

- 在 definitely：static drop。
- 不在 maybe：dead drop。
- 在 maybe 但不在 definitely：conditional drop，使用 drop flag。

复杂度目标：

```text
O(CFG edges * local-bitset words * fixed-point iterations)
```

状态存到 block 边界，不为每个表达式复制完整 local map。move origin 另用稀疏 side table 保存，满足 diagnostics 和 tooling。

### 5.7 赋值顺序

对 whole-local overwrite：

```text
evaluate target place
-> evaluate RHS completely
-> if old target remains initialized, run old-value cleanup
-> store new owned value
-> mark target initialized
```

这样 `x = transform(&x)` 在 RHS 求值期间仍可观察旧值。`x = x` 对 move-only 值也不会 double-drop：RHS consume
先让旧 slot 进入 moved，overwrite cleanup 识别 dead drop，再写回新值。

第一版不允许 managed resource 的字段覆盖赋值和 indexed 覆盖赋值。普通 trivial-copy 字段赋值保持现有行为。

### 5.8 cleanup action stack

M6 选择统一 cleanup action stack。

action 激活规则：

- named local 在 initializer 成功完成后登记 local-drop action。
- `defer call(...)` 执行到该 statement 时登记 defer action。
- aggregate 分步构造期间，已完成字段先登记临时 cleanup；aggregate 完整构造后再统一 transfer 给最终 owner。
- full-expression temporary 在表达式边界清理，除非已 transfer。

scope exit 按登记逆序执行 action。适用于：

- 正常离开。
- `return`。
- `break`。
- `continue`。
- `?` failure early return。

示例：

```aurex
var a: Resource = open_a();
defer log("after a");
var b: Resource = open_b();
defer log("after b");
```

退出顺序：

```text
log("after b")
drop(b)
log("after a")
drop(a)
```

这条规则比“所有 defer 先执行”或“所有局部先析构”更可组合，也与资源实际激活顺序一致。

### 5.9 `defer` 的精确语义

M6 固定：

- `defer` 只接受 call expression，保持现有窄 surface。
- deferred call 在 scope exit 时执行。
- deferred call 参数在执行 action 时求值，不在登记 statement 时 snapshot。
- 同一 scope 内按 action 激活顺序逆序执行。
- deferred expression 引用的局部值必须在所有可达 exit path 上仍可使用。

这条规则有意不同于 Go 的函数级 `defer`。Go 在执行 `defer` statement 时保存函数值和参数；Aurex 选择
词法 action 在退出时重新求值，是为了与现有 scope `defer`、`&mut` access、RAII cleanup action stack 和未来
place analysis 统一。代价是：如果 deferred expression 引用的局部变量后续被重新赋值，退出时看到的是重新赋值后的
slot，而不是登记时的旧值。需要 snapshot 的用户必须显式绑定独立值；move-only owned resource 更推荐升级为
compiler-managed destructor，而不是长期依赖手写 `defer close(&mut value)`。

例如：

```aurex
var handle: Handle = open();
defer close(&mut handle);
consume(handle);
```

必须诊断：deferred cleanup 在退出时仍会访问已经 moved 的 `handle`。如果 `Handle` 已经升级为 compiler-managed
resource，用户通常不应再写手工 `defer close(&mut handle)`。

### 5.10 自定义 destructor protocol

M6 固定语义，不急着冻结 parser spelling：

- nominal 类型最多声明一个 custom destructor body。
- destructor 是 compiler-owned lifecycle protocol，不是普通可调用方法。
- 用户不能显式调用 destructor，不能重载它，也不能通过普通 trait candidate search 选择它。
- destructor body 接收当前值的 mutable view。
- destructor body 执行后，compiler 再按规则递归 drop fields。
- destructor 不返回业务错误；需要报告错误的资源必须提供显式 `close` API，destructor 只做 no-fail fallback。
- 第一版 destructor body 不允许 move-out `self` 字段，不允许 resurrect `self`，不做异常展开。

surface spelling 在 M6-WP5 前再做一次 parser 设计审视。候选包括 sealed lifecycle impl 或专用 `deinit` declaration。
无论选择哪种拼写，都不能把 destructor 降级成普通 trait method。

### 5.11 aggregate cleanup

结构化 drop 顺序：

| 类型 | cleanup 顺序 |
| --- | --- |
| struct | custom destructor body，然后字段按声明逆序 |
| tuple | 元素按位置逆序 |
| enum | custom destructor body，然后当前 active payload |
| array | 元素按索引逆序 |
| generic aggregate | 单态化后按 concrete component 分类生成 glue |

这是 Aurex 的显式语言选择，不是照搬 Rust。Rust 的 field drop order 与 Aurex 这里的结构化 cleanup 顺序不同；
Aurex 选择逆序，是为了让字段、tuple、array、aggregate 构造 rollback 和 cleanup action stack 共享“后激活先清理”
的直觉。实现时必须用 golden / IR / native tests 固定这个顺序，不能让 backend 或字段遍历容器的偶然顺序决定语言语义。

array local cleanup 与 array by-value ABI 是两个问题。M6 可以为本地数组生成 glue，但不承诺解除现有 array-containing
参数、返回、赋值和 enum payload 限制。

### 5.12 参数、返回值和 trait receiver

M6 规则：

- ordinary by-value `T` 参数是 owned 参数。调用者传入 non-`Copy` 值时发生 consume。
- `&T` 是 shared borrow，`&mut T` 是 mutable borrow。M6 先保留现有引用边界，M7 再证明别名和 lifetime。
- non-`Copy` 返回值把 ownership transfer 给 caller。
- callee 对未 transfer 的 owned 参数负责 cleanup。
- trait method 的 `self: Self` 将来表示 consuming receiver；`self: &Self` 和 `self: &mut Self` 表示 borrow receiver。
- default trait method 在 generic `Self` 下只能使用 param environment 允许的 resource 操作。

### 5.13 FFI 和 `unsafe`

M6 第一版收紧 FFI：

- `extern c` by-value 参数和返回值只接受 ABI 支持且 `Trivial` cleanup 的类型。
- C 资源句柄通过 raw pointer、整数 handle 或显式 opaque wrapper 穿过边界。
- compiler 不假设 C 会运行 Aurex destructor。
- raw pointer 不自动成为 owner。
- 将来如需 `forget`、`ManuallyDrop` 或 ownership adoption，必须是窄 `unsafe` intrinsic 或受审计库 API。
- `unsafe` 可以承担外部契约，但不能静默关闭 Aurex 对普通 safe owned locals 的 move/drop 追踪。

### 5.14 globals、constants、exceptions、async 和并发

M6 第一版明确后移：

- managed global initialization 和进程退出析构顺序。
- thread-local destructor。
- exception / panic unwind cleanup。
- async state-machine drop。
- generator suspension。
- region / arena surface。
- `Send`、`Sync`、isolation 或 actor transfer。

当前 Aurex 没有异常和 async surface。先把正常 CFG exits 做正确，比提前冻结 unwind ABI 更重要。

### 5.15 generic drop glue

对任意可存储具体类型，compiler 可以查询或生成 `DropGlueKey(type)`：

- `Trivial` 类型 glue 是 no-op。
- nominal resource glue 调用 custom destructor body，再递归 component glue。
- struct / tuple / array glue 逆序处理 component。
- enum glue 根据 active tag 选择 payload。
- generic glue 在 monomorphization 后使用 concrete args。

身份规则：

- glue query identity 使用 stable type key、nominal owner `DefKey`、destructor `BodyKey`、generic args 和资源分类 fingerprint。
- 不能使用 display string。
- 不能把 session-local `TypeHandle.value` 当持久化身份。

### 5.16 IR 和 verifier 分层

M6 不应直接在 AST lowering 的每个分支手写 free call。建议分层：

```text
checked AST
-> CFG + move/resource facts
-> cleanup obligations
-> cleanup elaboration
-> target-independent Aurex IR cleanup nodes / glue calls
-> IR verifier
-> LLVM lowering
```

IR 设计入口：

- 正式表达 unconditional drop。
- 正式表达 conditional drop 或等价 drop-flag CFG。
- overwrite 先表达 drop-and-replace obligation，再 elaboration。
- move 可以主要保留为 sema / elaboration fact；最终物理表示允许优化成 load/store 或直接 relocation。
- verifier 检查 glue target、place type、flag 类型、cleanup block target 和 double-elaboration invariant。

具体 IR enum 拼写在 M6-WP4 实现前审视，本文只冻结语义需求。

### 5.17 Query、incremental cache 和 tooling

必须新增或扩展的事实：

| Fact | 用途 |
| --- | --- |
| type resource summary | type checking、generic predicate、hover |
| move origin | use-after-move diagnostics、IDE note |
| cleanup origin | 解释某个自动 destructor 为什么存在 |
| destructor body identity | query invalidation、tooling definition |
| drop glue identity | monomorphization、cache reuse、backend |
| body resource-check fingerprint | body-local incremental invalidation |

tooling 至少应能解释：

- 类型是 `Copy` 还是 `MoveOnly`。
- 类型是否 `NeedsDrop`。
- 某次 owned use 是 copy 还是 consume。
- moved value 最初在哪个 source range 被消费。
- 自动 cleanup 来自 lexical exit、overwrite、early return 还是 aggregate rollback。

## 6. 第二轮 rejected alternatives

| 方案 | 不选择原因 |
| --- | --- |
| 直接复制 Rust 全量 borrow checker | move/drop 地基尚未稳定；partial move、region、two-phase borrow 会把阶段扩大数倍 |
| 只加一个普通用户 trait `Drop` | 无法表达 compiler obligation、字段 glue、覆盖赋值、drop flag 和 dyn ABI；用户误 impl 会破坏安全 |
| C++ special member function 模型 | overload、隐式合成、ABI 和异常历史负担太重 |
| 所有值默认 ARC | 每个值引入运行时成本；仍不能解决文件、锁、cycle 和 FFI cleanup |
| 仅保留手工 `defer destroy` | regex 已证明样板和漏清理风险；无法支持拥有型标准库 |
| named local 默认 last-use 析构 | 释放时机可观察，锁、文件和事务行为容易惊讶 |
| 第一版开放 partial move | 需要 place tree 和字段级 drop flag，容易制造 double-drop |
| 资源语义同时解禁 array ABI | 两条独立 backend / ABI 主线会互相放大风险 |
| 先做 `dyn Trait` | 会在 drop glue 和 erased ownership 未稳定时过早冻结 vtable ABI |
| 第一版做 region / isolation | 研究价值高，但会阻塞最基本的 deterministic cleanup |

## 7. 第三轮审视：用户案例和反例压力测试

### 7.1 普通 `Copy` 值

```aurex
let first: i32 = 1;
let second: i32 = first;
return first + second;
```

结果：正常 copy，不产生 cleanup，不改变当前直觉。

### 7.2 regex 手工资源迁移

当前：

```aurex
var compiled: regex.Regex = regex.compile(pattern);
defer regex.destroy(&mut compiled);
return regex.is_match(&compiled, input);
```

目标：

```aurex
var compiled: regex.Regex = regex.compile(pattern);
return regex.is_match(&compiled, input);
```

结果：`compiled` 初始化后激活 lexical cleanup；`return` 表达式先求值，然后 cleanup。手工 `destroy(&mut compiled)`
不能与 compiler-managed destructor 同时保留，否则可能 double close。regex 迁移必须作为后续库案例单独验证。

### 7.3 move 后使用

```aurex
let first: File = open(path);
let second: File = first;
read(&first);
```

结果：`read(&first)` 报 use-after-move，并指向 `let second = first` 的 consume origin。

### 7.4 分支 maybe-moved

```aurex
var file: File = open(path);
if condition {
    consume(file);
}
read(&file);
```

结果：join 后 `file` 是 maybe-moved，必须报错。cleanup 使用 drop flag，只在仍 initialized 的路径执行。

### 7.5 move 后重新初始化

```aurex
var file: File = open(first_path);
consume(file);
file = open(second_path);
read(&file);
```

结果：允许。overwrite 检测旧值已 moved，不执行旧 cleanup；新初始化重新激活 cleanup。

### 7.6 覆盖赋值的 RHS 顺序

```aurex
var file: File = open(path);
file = reopen_from(&file);
```

结果：先求值 `reopen_from(&file)`，再清理旧 `file`，最后写入新 owner。未来 M7 仍需检查返回值是否错误借用了旧
storage。

### 7.7 `defer` 和 RAII 交错

```aurex
var outer: File = open(a);
defer log("outer");
var inner: File = open(b);
defer log("inner");
```

结果：

```text
log("inner")
drop(inner)
log("outer")
drop(outer)
```

该顺序稳定、可解释、适合回滚。

### 7.8 deferred use-after-move

```aurex
var file: File = open(path);
defer inspect(&file);
consume(file);
```

结果：报错。`inspect(&file)` 在 exit 执行时会访问 moved value。诊断应同时指向 defer statement 和 consume origin。

### 7.9 generic identity

```aurex
fn identity[T](value: T) -> T {
    return value;
}
```

结果：适用于 `Copy` 和 move-only `T`。函数把 owned 参数 transfer 给 caller，不需要 `T: Copy`。

### 7.10 generic duplicate use

```aurex
fn duplicate[T](value: T) -> (T, T) {
    return (value, value);
}
```

结果：要求 `T: Copy`。不能依赖隐式 clone。

### 7.11 resource payload、pattern 和 `?`

```aurex
let result: Result[File, Error] = open(path);
let file: File = result?;
```

完整目标需要 success payload move、failure payload return transfer、active enum payload cleanup 和中间临时 cleanup。
M6 设计必须覆盖这个方向，但第一批 whole-local 实现可以拒绝 non-`Copy` payload 的 `?` 和 consuming pattern，直到
aggregate/payload transfer WP 完成。

### 7.12 partial field move

```aurex
let socket = connection.socket;
```

如果 `socket` 是 move-only，父对象进入 partially moved 状态。第一版拒绝。后续 place-tree 阶段再设计字段级 drop flag
和父对象可用性。

### 7.13 indexed move-out

```aurex
let item = values[index];
```

如果 item move-only，容器中会留下 hole。第一版拒绝。未来必须通过 `take`、`remove`、swap-remove 或容器专用 API
表达，不允许普通索引偷偷制造 hole。

### 7.14 aggregate 部分初始化失败

```aurex
let pair = Pair {
    left: open(left_path)?,
    right: open(right_path)?,
};
```

如果 `right` 失败，已经成功构造的 `left` 必须 cleanup。M6-WP5 aggregate construction lowering 必须使用临时
cleanup action，完整构造后再 transfer 到 `pair`。

### 7.15 文件显式 `close`

文件关闭可能失败。只靠 destructor 无法返回错误：

```aurex
var file: File = open(path);
let result = file.close();
```

目标设计应让显式 `close` 消费资源，destructor 作为未显式 close 时的 no-fail fallback。第一版具体 consuming-method
surface 在库层重建前单独审视。

### 7.16 锁 guard

```aurex
{
    let guard = mutex.lock();
    update();
}
```

词法 drop 正确表达解锁时机。不能默认 last-use drop，否则锁可能比源代码作用域更早释放。

### 7.17 FFI raw handle

```aurex
extern c {
    fn c_open() -> *mut void;
    fn c_close(handle: *mut void) -> void;
}

fn use_handle() -> void {
    let handle = c_open();
    defer c_close(handle);
}
```

raw pointer 仍是 `Copy + Discard + Trivial`。compiler 不猜 ownership。用户可以继续用窄 `defer`，或在未来封装成 owned
wrapper。

### 7.18 self-reference

```aurex
struct SelfRef {
    data: Buffer,
    ptr: *const u8,
}
```

raw pointer 可能指向 `data`。普通 move 会让内部地址契约失效。M6 第一版不提供 pinning 或自引用安全保证；该模式属于
`unsafe` 封装审计。未来如有真实需求再设计 `Pin`-like 库或 immovable 类型。

### 7.19 shared ownership cycle

未来 `Rc[Node]` 可能形成 cycle。RAII 和 RC 都不能自动解决所有 cycle。库层必须显式提供 weak reference 或 region /
arena 策略，不能宣称资源系统自动解决对象图回收。

### 7.20 future `dyn Trait`

trait object 未来必须有 erased drop glue 策略。M6 先稳定 concrete drop glue、ownership transfer 和 borrowed-view
规则，之后再设计 vtable slot、object safety 和 erased storage。顺序不能倒置。

## 8. 第三轮结论：第一版边界

M6 第一版必须实现：

- compiler-owned `Copy`。
- 内部 `Discard` / `NeedsDrop` 分类。
- whole-local move 和 CFG-sensitive use-after-move。
- reinitialization。
- lexical cleanup action stack。
- 正常 scope exit、overwrite、`return`、`break`、`continue` 和 `?` exit cleanup。
- conditional drop flag。
- nominal / aggregate / generic drop glue。
- IR verifier、LLVM lowering、query/cache/tooling 和 diagnostics。

M6 第一版明确不实现：

- partial field move。
- indexed move-out。
- arbitrary place-tree move。
- 完整 borrow checker 和 lifetime 参数。
- lexical-only borrow checker 临时版本。
- Polonius。
- two-phase borrow。
- unwind / exception cleanup。
- async drop。
- global managed resource destruction。
- region、arena、isolation、`Send` / `Sync`。
- default ARC。
- `dyn Trait`、object safety 和 vtable ABI。
- specialization。
- C++ special member function 模型。
- 全量 array / aggregate ABI 解禁。
- 标准库重建。

## 9. M7 借用检查入口

M7 必须建立在 M6 facts 上，目标是 CFG-sensitive origin / loan analysis：

```text
place
-> projection
-> access kind: read | mutate | consume
-> loan origin
-> activation and use ranges
-> CFG-sensitive conflict check
-> borrowed-return and lifetime contract
```

M7 第一版不应写成长期固化的纯 lexical checker。它可以从较窄的 origin dataflow 开始，但分析模型必须允许 NLL-style
结束借用。two-phase borrow、region surface 和更高级关系求解按真实案例后移。

## 10. 编译器 pipeline 影响图

| 层 | M6 影响 |
| --- | --- |
| Lexer / parser | WP5 前只保留 destructor surface 设计入口；不提前冻结 token |
| AST | destructor declaration、owned-use source range、cleanup origin |
| Sema type system | resource summary、结构化传播、generic evidence |
| Body checking | use mode、whole-local move、reinit、maybe-moved diagnostics |
| CFG | 迭代 worklist、block bitset、cleanup exit edge |
| Checked module | destructor facts、resource summaries、move origins、drop glue identities |
| Query | destructor body、body resource check、drop glue stable keys |
| IR | cleanup obligation、drop/drop-if 或等价 CFG、overwrite elaboration |
| Verifier | cleanup 类型、flag、glue、exit edge 和 double-elaboration invariant |
| LLVM backend | glue function、conditional branch、direct cleanup call |
| Tooling | hover classification、move origin、cleanup reason、definition |
| Incremental cache | resource fingerprint、destructor-body invalidation、generic glue reuse |
| Tests | unit、positive、negative、IR、LLVM、native、tooling、cache、stress、coverage |

## 11. 风险矩阵

| 风险 | 严重度 | 控制措施 |
| --- | --- | --- |
| 把 `Drop` 当普通 trait 导致安全绕过 | Critical | `NeedsDrop` 和 destructor protocol 由 compiler 持有 |
| move 后 defer 仍访问旧值 | Critical | defer action 纳入 body move analysis，exit path 检查 |
| `defer` 参数不是 snapshot 导致用户误解 | High | 语言文档明确退出时求值；snapshot 需要显式独立绑定；测试覆盖重新赋值案例 |
| overwrite double-drop | Critical | RHS 先求值，旧状态检查后 cleanup，drop flag verifier |
| 分支 join 漏清理 | Critical | definitely/maybe bitset + conditional drop |
| aggregate 部分初始化失败泄漏 | High | 字段临时 cleanup action，完成后 transfer |
| aggregate cleanup 顺序与其他语言直觉不一致 | Medium | 明确这是 Aurex 逆激活顺序；golden / IR / native tests 固定顺序 |
| generic glue cache 污染 | High | stable type key / `DefKey` / `BodyKey`，禁止 display string 和 session handle |
| partial move 偷偷进入第一版 | High | whole-local hard boundary，negative tests |
| array ABI 工作拖垮 M6 | High | local glue 与 by-value ABI 解禁拆开 |
| FFI 假 ownership | High | raw pointer 默认非 owner，C ABI resource by-value 拒绝 |
| named local 提前析构改变行为 | High | lexical drop，禁止默认 ASAP destruction |
| destructor 失败无处报告 | Medium | 显式 consuming `close` API + no-fail fallback destructor |
| 性能因每表达式复制状态退化 | High | block-level bitset、稀疏 origin table、stress gate |
| 后续 dyn trait ABI 被锁死 | High | dyn trait 后移，M6 先只稳定 concrete glue |
| borrow checker 做成误报严重 lexical 版本 | High | M7 从一开始使用 CFG-sensitive model |

## 12. 验收门槛

每个实现 WP 都必须保持：

```sh
tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') \
  $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')
git diff --check
cmake --build cmake-build-release -j4
ctest --test-dir cmake-build-release --output-on-failure -j4
tools/check_coverage.sh -j4
make perf-stress-threshold
make query-sanitizer
```

大型收口再运行：

```sh
make perf-release-threshold
```

新增代码覆盖率目标：

- 新增代码 lines / functions / regions 至少 `95%`。
- 受影响模块尽量达到 `95%`。
- 新增样例必须进入常规仓库目录，不使用临时 fixture。

## 13. 三轮审视后的冻结项和开放项

已冻结：

- 四维 resource summary。
- whole-local move 第一版边界。
- CFG-sensitive move analysis。
- lexical named-local cleanup。
- full-expression temporary cleanup。
- cleanup action stack 统一交错 `defer` 和 compiler drop。
- overwrite 顺序。
- destructor 是 sealed lifecycle protocol，不是普通方法。
- generic glue 和 stable identity 方向。
- FFI 和 `unsafe` 边界。
- M7 borrow checker 后移。

仍开放：

- destructor 的最终 surface spelling。
- future `consume expr` 是否需要用户可见语法。
- 显式 `close` consuming method 的库层习惯用法。
- M6 后期 non-`Copy` enum payload pattern / `?` 的具体开放批次。
- place-tree partial move 的后续阶段编号。
- region、RC、isolation 和 dyn trait 的后续排序。

这些开放项都有明确进入点，不阻塞 M6-WP2 的资源分类 scaffold。

## 14. 参考资料

### 工业语言和编译器

- C++ Core Guidelines，RAII：
  https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-raii
- Rust Reference，destructors：
  https://doc.rust-lang.org/reference/destructors.html
- rustc dev guide，drop elaboration：
  https://rustc-dev-guide.rust-lang.org/mir/drop-elaboration.html
- Rust RFC 2094，non-lexical lifetimes：
  https://rust-lang.github.io/rfcs/2094-nll.html
- Rust RFC 2025，two-phase borrows：
  https://rust-lang.github.io/rfcs/2025-nested-method-calls.html
- Polonius current status：
  https://rust-lang.github.io/polonius/current_status.html
- Swift SE-0377，borrowing and consuming parameter ownership modifiers：
  https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md
- Swift SE-0390，noncopyable structs and enums：
  https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md
- Mojo manual，ownership：
  https://docs.modular.com/mojo/manual/values/ownership/
- Move Book，abilities：
  https://move-language.github.io/move/abilities.html
- Zig language reference，`defer`：
  https://ziglang.org/documentation/master/#defer
- Go specification，defer statements：
  https://go.dev/ref/spec#Defer_statements
- Pony tutorial，reference capabilities：
  https://tutorial.ponylang.io/reference-capabilities/reference-capabilities.html
- Hylo language tour，bindings 和 mutable value semantics：
  https://docs.hylo-lang.org/language-tour/bindings
- Austral specification，linear types、borrowing 和 capability：
  https://austral-lang.org/spec/spec.html
- Vale guide，generational references 和 regions：
  https://vale.dev/guide/regions
- Roc `Str` documentation，reference counting 和 opportunistic mutation：
  https://roc-lang.org/builtins/main/Str/
- Clang，Lifetime Safety Analysis：
  https://clang.llvm.org/docs/LifetimeSafety.html
- Carbon，safety strategy：
  https://docs.carbon-lang.dev/docs/design/safety/
- Project Verona，safe scalable memory management 和 compartmentalisation：
  https://www.microsoft.com/en-us/research/project/project-verona/
- Lean reference manual，reference counting：
  https://lean-lang.org/doc/reference/latest/Run-Time-Code/Reference-Counting/

### 论文和研究

- Cyclone regions：
  https://www.cs.cornell.edu/projects/cyclone/papers/cyclone-regions.pdf
- Oxide: The Essence of Rust：
  https://arxiv.org/abs/1903.00982
- RustBelt：
  https://plv.mpi-sws.org/rustbelt/popl18/paper.pdf
- Aeneas：
  https://arxiv.org/abs/2206.07185
- Perceus: Garbage Free Reference Counting with Reuse：
  https://www.microsoft.com/en-us/research/publication/perceus-garbage-free-reference-counting-with-reuse/
- Linear Haskell：
  https://www.microsoft.com/en-us/research/publication/linear-haskell-practical-linearity-higher-order-polymorphic-language/
- Idris 2 quantitative type theory：
  https://arxiv.org/abs/2104.00480
- Verona reference capabilities：
  https://www.microsoft.com/en-us/research/publication/reference-capabilities-for-flexible-memory-management/

# 类、trait 与组合：背景说明

## 1. 背景

用户在讨论语言表达力时，最常见的一个直觉是“`class.method` 很顺手，所以应该直接做类”。这个直觉本身没有问题，但需要先分清楚两件事：

1. **类的便利性**，也就是数据和方法绑定在一起、调用写法自然、封装直观。
2. **继承式 OO 类模型**，也就是 `extends`、`override`、虚派发、父类共享状态、深层对象层次。

现代语言通常保留第 1 点，但明显削弱第 2 点。换句话说，很多语言并不是“不做 class”，而是**不再把继承树作为默认复用方式**。

对 Aurex 来说，这个区别尤其重要。当前路线已经把 `trait object`、动态分派、完整 closure capture ABI、generator、macro 等都放在后置区；M2.5 也明确不把 trait object 和动态分派纳入主线。[M2.5 路线图](m2.5-roadmap.md) 里的 P0/P1 只把 static trait、resource 语义、module/package 边界和最小容器形状作为表达力边界。  
因此，如果要讨论“类”，更合理的方式是先把它拆成**语法糖层**和**对象模型层**，而不是直接把继承式 class 整套搬进来。

## 2. 先定义几个词

### 2.1 Class-like object

这里指的是“像 class 一样好用”的能力集合：

- 数据和方法绑在一起。
- 有构造器。
- 有封装边界。
- 有析构或资源清理。
- `obj.method(...)` 的调用方式自然。

这部分本质上可以由 `struct + impl + trait + Drop` 提供，未必需要独立的 class 对象模型。

### 2.2 Trait

trait 不是“对象是什么”，而是“对象能做什么”。

它更像能力契约：

- `Reader` 说明类型能读。
- `Closable` 说明类型能关。
- `Eq` / `Ord` 说明类型满足比较语义。

trait 的重点是**行为抽象**，不是布局，也不是继承关系。

### 2.3 Composition

组合就是把一个类型的行为拆成多个明确组件：

- `Service` has a `Repo`
- `Service` has a `Logger`
- `Service` has a `Clock`

这和“让一个父类塞进一堆共享状态，再让子类继承”是不同的思路。组合把依赖关系写明，复用方式更显式。

### 2.4 Static trait 和 dynamic trait

需要特别区分：

- **static trait**：编译期知道具体类型，trait 只作为约束，调用通常在单态化后直接落到具体实现。
- **dynamic trait / trait object**：值在运行时只保留“数据指针 + vtable 指针”之类的对象形态，方法调用通过 vtable 间接分派。

也就是说，trait 本身并不等于动态分派。动态的是对象化之后的调用目标和部分生命周期行为，不是 trait 定义本身。

## 3. 为什么现代语言更偏组合

### 3.1 关系更显式

组合把依赖写在字段里，读代码时很容易看出一个类型依赖了哪些部件。  
继承则把行为藏进父类层次里，理解一个子类经常要先翻父类，再翻祖父类，成本高得多。

### 3.2 避免脆弱基类

继承式 class 的经典问题是“父类一改，子类全跟着变”。  
这会让修改变成一种高风险操作，因为行为可能通过 override、共享状态、虚派发链条被间接影响。

组合的依赖更局部。替换一个组件，不会天然污染整棵继承树。

### 3.3 更容易组合多种能力

真实系统里，一个类型往往同时需要多种能力：

- 可比较
- 可打印
- 可关闭
- 可序列化
- 可迭代

trait / interface 的方式比“塞进一个巨型基类”更自然，也更容易扩展。

### 3.4 更利于静态优化

组合 + static trait + 泛型单态化，通常更容易：

- 编译期分析
- 内联
- 去掉虚调用
- 保持调用路径清楚
- 做增量编译和 query 失效分析

继承层次越深，override 越多，运行时解析空间越大，优化和诊断都更难做稳。

### 3.5 更适合测试和替换

组合天然适合依赖注入。测试时把某个组件换成 fake 或 stub 即可。  
如果很多行为都塞进基类内部，替换和隔离就会更麻烦。

## 4. class 和 trait 的区别

### 4.1 class 更像“这个类型是什么”

class 适合表达：

- 有状态实体
- 构造和析构
- 封装不变量
- 方法和字段强绑定
- 对象身份比较重要

### 4.2 trait 更像“这个类型能做什么”

trait 适合表达：

- 统一能力接口
- 复用算法
- 泛型约束
- 多个能力叠加
- 与具体布局解耦

### 4.3 class 和 trait 不是互斥关系

一个类型完全可以同时拥有：

- `struct` 数据布局
- `impl` 方法
- `trait` 能力
- `Drop` 清理逻辑

如果再加上 class-like sugar，很多时候只是让写法更像 class，而不是引入真正的继承模型。

## 5. 在 Aurex 里，推荐怎么理解

### 5.1 先把 class 的便利性拆出来

如果 Aurex 以后要提供 `class.method` 的顺手体验，优先考虑把它当成**语法糖**，而不是独立对象模型。

一个合理的 lowering 方向是：

```aurex
class File implements Reader, Closable {
    handle: i32;
    path: str;

    pub fn open(path: str) -> Result[File, Error] { ... }
    pub fn read(self: &mut Self, buf: []mut u8) -> usize { ... }
    pub fn close(self: &mut Self) { ... }
}
```

降低到：

```aurex
struct File {
    handle: i32;
    path: str;
}

impl File {
    pub fn open(path: str) -> Result[File, Error] { ... }
    pub fn read(self: &mut File, buf: []mut u8) -> usize { ... }
    pub fn close(self: &mut File) { ... }
}

trait Reader {
    fn read(self: &mut Self, buf: []mut u8) -> usize;
}

trait Closable {
    fn close(self: &mut Self);
}

impl Reader for File {
    fn read(self: &mut File, buf: []mut u8) -> usize { ... }
}

impl Closable for File {
    fn close(self: &mut File) { ... }
}
```

如果未来还需要资源清理，则进一步接到：

```aurex
impl Drop for File {
    fn drop(self: &mut File) {
        self.close();
    }
}
```

这套方案能保住 `class.method` 的便利感，但不会提前引入继承和虚派发。

### 5.2 static trait 放在 M3，dynamic trait 放在更后面

基于当前路线，更稳妥的阶段划分是：

- **M3**：泛型闭环、static trait、资源语义 / RAII、最小 owned containers、class-like sugar、API ergonomics
- **更后面**：dynamic trait / trait object / vtable ABI

原因很直接：动态 trait 会锁死对象布局、vtable 形状、drop glue、object safety 和部分 ABI 规则，太早做会把后续设计空间压缩掉。

### 5.3 组合是默认复用方式

在 Aurex 里，默认推荐的组织方式应当是：

- `struct` 负责数据
- `impl` 负责方法
- `trait` 负责能力抽象
- `composition` 负责复用
- `Drop` 负责资源清理

这比“把一切放进继承树”更符合当前语言核心、query 架构和后续扩展路线。

## 6. 三种常见形态的对照

```text
1) 继承式 class
   Base -> DerivedA / DerivedB
   重点：层次、override、共享状态、virtual dispatch

2) class-like + composition
   Service has Repo / Logger / Clock
   重点：对象手感保留，但复用靠字段和委派

3) struct + trait + composition
   struct File
   trait Reader / Closable
   impl Reader for File
   重点：静态抽象、组合复用、易优化

4) dyn Trait
   data_ptr + vtable_ptr
   重点：运行时多态、异构集合、插件边界
```

## 7. 设计建议

### 7.1 如果目标是“写起来像 class”

优先做：

- `struct + impl`
- 构造器语法糖
- 方法调用糖
- 默认 private
- `Drop` / RAII

这能拿到大部分 class 的便利性，但语义仍然清楚。

### 7.2 如果目标是“表达能力”

优先做：

- static trait
- associated type / associated const
- `where` 约束
- 组合式 API

这比继承更适合系统语言和现代语言的编译期优化模型。

### 7.3 如果目标是“运行时多态”

再考虑：

- `dyn Trait`
- vtable ABI
- object safety
- trait object layout

这一步不应该和 class-like sugar 混在一起做。

## 8. 结论

对 Aurex 来说，最合理的路线不是“要不要 class”，而是：

1. **先把 class 的便利性拆成语法糖和对象模型两层。**
2. **把语法糖层做出来，用 `struct + impl + trait + Drop` 承载语义。**
3. **默认以组合和 static trait 作为语言的组织方式。**
4. **把 dynamic trait / trait object 留到更后面的阶段。**

这样既能保留 `class.method` 的顺手感，又不会过早引入继承和虚派发的长期复杂度。

## 9. Rust 是怎么拆这些矛盾的

Rust 的经验很适合拿来做 Aurex 的设计参照，因为它不是用一个“大而全的类模型”去同时解决封装、复用、借用、动态分派和资源语义，而是把这些张力拆成多个显式层：

- 语法便利和语义模型分离。
- 编译期约束和运行时检查分离。
- 静态分派和动态分派分离。
- 形式化语义和工程实现分离，但保持可对齐。

### 9.1 总图

```text
source
  |
  +--> class-like syntax
  |       |
  |       v
  |   struct + impl + trait + Drop
  |       |
  |       +--> static trait / monomorphization / inlining
  |       |
  |       +--> RAII / deterministic cleanup
  |       |
  |       +--> explicit escape hatches: UnsafeCell / RefCell / Mutex / Arc
  |
  +--> dyn Trait boundary
          |
          v
   data ptr + vtable ptr
          |
          v
   late binding / runtime dispatch
```

这张图里，真正“动态”的地方只有 `dyn Trait` 之后的对象化边界：具体类型被隐藏掉，调用目标通过 vtable 在运行时解析，trait object 还要携带生命周期边界和 dyn-compatible 约束。[Rust Reference: trait objects](https://doc.rust-lang.org/reference/types/trait-object.html)、[Rust Reference: traits / dyn compatibility](https://doc.rust-lang.org/stable/reference/items/traits.html)。

### 9.2 借用系统的矛盾：既要安全，又要好写

Rust 早期最大的张力是：借用规则如果只按词法作用域算，会把很多本来正确的局部代码误判掉。Rust 的解法不是放松整个安全模型，而是把 lifetimes 提升到控制流图上，用 NLL 缩短借用范围；再用 two-phase borrows 处理 `vec.push(vec.len())` 这类方法调用的先取后用场景。[RFC 2094 NLL](https://rust-lang.github.io/rfcs/2094-nll.html)、[RFC 2025 / two-phase borrows](https://rust-lang.github.io/rfcs/2025-nested-method-calls.html)、[rustc-dev-guide: two-phase borrows](https://rustc-dev-guide.rust-lang.org/borrow-check/two-phase-borrows.html)。

Polonius 进一步尝试把借用检查拆成更明确的数据流/关系分析。Rust 官方当前的状态是：它已经预览性并入 rustc，但仍未达到广泛生产默认的成熟度，后续还要继续做完整分析和优化。[Polonius current status](https://rust-lang.github.io/polonius/current_status.html)。

对 Aurex 的启发是：借用/资源分析不应该被做成“只认词法块”的简单规则，但也不该一开始就把最复杂的 Polonius 全量方案锁进语言主路径。更稳的路线是先让语义模型是控制流敏感的，再决定实现是传统 borrow checker 还是更精细的关系分析。

### 9.3 共享可变的矛盾：既要能改，又要能推理

Rust 的原则是 `&T` 默认不可变，`&mut T` 默认独占。问题是，真实系统里很多数据结构都需要内部共享和局部可变。Rust 的解法不是把“共享可变”变成默认语义，而是把它收进显式逃生口：`UnsafeCell` 是唯一能解除 `&T` 不可变保证的底层原语，`RefCell` 用运行时借用检查提供安全 API，`Mutex` / 原子类型处理并发共享，GhostCell 则尝试用权限和数据分离来降低传统 interior mutability 的额外开销。[UnsafeCell docs](https://doc.rust-lang.org/nightly/core/cell/struct.UnsafeCell.html)、[Rust Reference: interior mutability](https://doc.rust-lang.org/stable/reference/interior-mutability.html)、[GhostCell paper](https://plv.mpi-sws.org/rustbelt/ghostcell/paper.pdf)。

GhostCell 的核心思路是把“谁有权修改”从“数据本体在哪里”里拆出来，用单独的权限对象管理一整组数据。这对图、双向链表、arena 这类结构尤其有价值，因为它比把每个节点都塞进细粒度运行时借用计数更轻。[GhostCell citation](https://pure.mpg.de/pubman/item/item_3331795_7)、[ICFP 2021 page](https://icfp21.sigplan.org/details/icfp-2021-papers/31/GhostCell-Separating-Permissions-from-Data-in-Rust)。

对 Aurex 的启发是：不要把“允许共享可变”做成核心语义；把它做成少量、显式、可审计的库级或受限语言级机制，优先保住普通值语义、RAII 和静态推理的稳定边界。

### 9.4 语义正确性和优化：别让编译器猜

Rust 不是只追求“能编译”，还必须让 unsafe 代码和优化器之间有一个可解释的契约。RustBelt 从语义层面证明了 Rust 语言和库中相当一部分安全性质；Stacked Borrows 则给出了一个可执行的别名模型，帮助解释哪些指针/引用操作在优化之后仍然合法。[RustBelt paper](https://people.mpi-sws.org/~dreyer/papers/rustbelt/paper.pdf)、[Stacked Borrows paper](https://plv.mpi-sws.org/rustbelt/stacked-borrows/).

这类工作的重要意义在于：它们不是在教编译器“更聪明”，而是在教编译器“知道自己什么时候可以相信别名信息，什么时候不能”。这对 Aurex 很关键，因为如果后端想做更激进的内联、去虚调用、重排内存访问或 drop glue 优化，就必须先定义清楚 alias / provenance / reference validity 的边界。

对 Aurex 的启发是：如果我们未来要引入更复杂的资源语义、引用模型或 unsafe 优化，最好从一开始就保留一个可以被验证、可以被解释的 IR / 语义层，不要只靠“实现里现在刚好这样”。

### 9.5 静态抽象和动态分派：Rust 把选择权显式交给用户

Rust 用 `impl Trait` 和 `dyn Trait` 把抽象的两条路明确分开：`impl Trait` 表示“返回一个具体但隐藏的类型”，适合迭代器、闭包、组合式 API；`dyn Trait` 则表示“我就是要运行时多态”，这会变成动态大小类型，通常放在指针后面，靠 vtable 做 late binding。[Rust Reference: impl Trait](https://doc.rust-lang.org/stable/reference/types/impl-trait.html)、[Rust Reference: trait objects](https://doc.rust-lang.org/reference/types/trait-object.html)、[Rust Book: trait objects](https://doc.rust-lang.org/stable/book/ch18-02-trait-objects.html)。

同时，trait object 还有 dyn compatibility 约束：并不是任意 trait 都能直接对象化，`Self: Sized`、关联项等条件都会影响可用性。[Rust Reference: traits / dyn compatibility](https://doc.rust-lang.org/stable/reference/items/traits.html)。

这套设计的好处是，静态路径保持可内联、可单态化、可优化；动态路径则明确付出间接调用和布局约束的代价。Rust 没有把这两种模式混成一个隐式对象系统。

对 Aurex 的启发是：`class.method` 的手感可以做，但底层仍然应该分成静态分派和动态分派两套语义。默认路径走 `struct + impl + trait + Drop`，需要异构集合、插件边界或晚绑定时，再显式进入 `dyn Trait`。

### 9.6 泛型、编译时间和代码体积：工业界不会假装这不存在

Rust 选择了 monomorphization，也就是把每组具体泛型实参都展开成一份具体代码。这样性能通常更好，但代价是编译时间和二进制体积可能上升。[rustc-dev-guide: monomorphization](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html)。

工业上对这个问题的常见缓解方式不是回退到动态分派，而是继续把问题拆层：

- `impl Trait` 把“复杂 concrete type”隐藏掉，减少 API 暴露面和类型爆炸。[Rust Reference: impl Trait](https://doc.rust-lang.org/stable/reference/types/impl-trait.html)
- incremental compilation 用 query DAG 和 red-green 机制只重算受影响部分。[rustc-dev-guide: incremental compilation](https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation.html)、[in detail](https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation-in-detail.html)
- codegen units 让后端能并行、让增量失效范围更可控。[rustc-dev-guide: monomorphization](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html)、[parallel compilation](https://rustc-dev-guide.rust-lang.org/parallel-rustc.html)
- ThinLTO 在链接阶段再把一部分跨模块优化拿回来。[rustc-dev-guide: optimized build / ThinLTO](https://rustc-dev-guide.rust-lang.org/building/optimized-build.html)

对 Aurex 的启发是：如果我们要做泛型、约束、static trait、class-like sugar，就应该从一开始把 query key、实例化缓存、单态化和后端分区设计成一条线，而不是等代码已经长大之后再补性能补丁。

### 9.7 形式化研究给我们的直接结论

Oxide 说明，borrow checking 可以被提炼成更接近源语言的形式化体系，而且 NLL 这样的现代特性也能被纳入一个相对简洁的理论框架里。[Oxide: The Essence of Rust](https://arxiv.org/abs/1903.00982)。

RustBelt、Stacked Borrows、GhostCell 这三条线则合在一起说明了一件事：Rust 不是靠“某一个万能规则”赢的，而是靠一组彼此配合的显式机制：

- 静态借用规则负责默认安全；
- interior mutability 负责局部例外；
- aliasing model 负责 unsafe 和优化边界；
- trait object / dyn compatibility 负责动态边界；
- monomorphization / incremental compilation / ThinLTO 负责工程可用性。

这正好对应 Aurex 现在的方向。我们的语言不应该先做一个“通吃一切”的 class 系统，而应该先把默认路径做成清晰、可验证、可优化的组合式模型，再把例外机制一层一层往外加。

## 10. 参考资料

下面这些是这份背景文档真正依赖的核心出处，优先级按“对 Aurex 设计最直接”排序：

1. [Rust Reference: trait objects](https://doc.rust-lang.org/reference/types/trait-object.html)
2. [Rust Reference: traits / dyn compatibility](https://doc.rust-lang.org/stable/reference/items/traits.html)
3. [Rust Reference: impl Trait](https://doc.rust-lang.org/stable/reference/types/impl-trait.html)
4. [Rust Reference: interior mutability](https://doc.rust-lang.org/stable/reference/interior-mutability.html)
5. [UnsafeCell docs](https://doc.rust-lang.org/nightly/core/cell/struct.UnsafeCell.html)
6. [RFC 2094: NLL](https://rust-lang.github.io/rfcs/2094-nll.html)
7. [RFC 2025: two-phase borrows](https://rust-lang.github.io/rfcs/2025-nested-method-calls.html)
8. [Polonius current status](https://rust-lang.github.io/polonius/current_status.html)
9. [RustBelt: Securing the Foundations of the Rust Programming Language](https://people.mpi-sws.org/~dreyer/papers/rustbelt/paper.pdf) - PACMPL / POPL 2018
10. [Stacked Borrows: An Aliasing Model for Rust](https://plv.mpi-sws.org/rustbelt/stacked-borrows/) - PACMPL / POPL 2020
11. [GhostCell: Separating Permissions from Data in Rust](https://icfp21.sigplan.org/details/icfp-2021-papers/31/GhostCell-Separating-Permissions-from-Data-in-Rust) - PACMPL / ICFP 2021
12. [Oxide: The Essence of Rust](https://arxiv.org/abs/1903.00982) - arXiv 2019
13. [rustc-dev-guide: incremental compilation](https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation.html)
14. [rustc-dev-guide: monomorphization](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html)
15. [rustc-dev-guide: optimized build / ThinLTO](https://rustc-dev-guide.rust-lang.org/building/optimized-build.html)

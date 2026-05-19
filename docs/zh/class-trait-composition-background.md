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


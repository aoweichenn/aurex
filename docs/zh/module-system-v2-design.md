# Aurex 模块系统 V2 设计草案

## 1. 背景

当前 Aurex 已经有最小模块系统：

- 每个文件用 `module path;` 声明模块路径。
- `import path as alias;` 按导入者目录和 `-I` import path 找文件。
- `pub import` 可以把导入模块继续暴露给外部。
- `pub` / `priv` 控制顶层 item、字段、method 和 import 可见性。
- `ModuleLoader` 会递归加载 import，检测循环 import、重复模块名、模块名和 import 路径不匹配，然后把所有文件合并成一个 `AstModule`。

这个模型适合 M2 语言核心，但已经不够支撑后续两个目标：

1. **把大型系统库拆小**。例如 regex / compiler 前端里，一个逻辑模块可能自然需要拆成 `lexer`、`parser`、`arena`、`diagnostic`、`lowering` 等多个文件，但这些文件应该共享内部 helper，不应该把 helper 全部 `pub` 出去。
2. **承接 M2.5 query / lossless / IDE 架构，进入 M3 模块设计**。M2.5 已经完成 query key 和 IDE-facing 基础设施；M3 需要把 module graph、exports、item signature、body typecheck 等边界落到稳定模块身份上，不能继续把“物理文件路径”当成“逻辑模块身份”。

所以模块系统 V2 的重点不是“加更多 import 写法”，而是把当前的文件级拼接模型升级为：

```text
PackageKey
  |
  +-- ModuleKey: 逻辑模块身份
        |
        +-- ModulePartKey: 一个模块的一个源文件片段
        |
        +-- ModuleGraph: import / part / dependency edges
        |
        +-- ModuleExports: 对外导出表
```

## 2. 当前实现事实

当前关键结构大致是：

```text
source file
  -> lexer
  -> parser
  -> AstModule
  -> ModuleLoader append/remap
  -> combined AstModule
  -> sema lookup / visibility / exports cache
```

`AstModule` 已经有：

```text
module_path
imports
modules
item_modules
identifiers
```

`ModuleLoader` 已经有：

```text
loading_files_         // 循环 import 检测
loaded_file_modules_   // 同一文件避免重复加载
loaded_modules_        // 同名模块重复检测
modules_               // 模块记录
```

sema 已经有：

```text
visible_modules_cache_
module_export_modules_cache_
ModuleLookupKey
DefNamespace
StableModuleId / StableDefId / IncrementalKey
```

这说明 V2 不需要推倒重来。更合理的路线是：**保留当前 M2 能跑通的文件加载和合并路径，逐步把模块身份、导出表、可见性和查询边界抽出来。**

## 3. 设计目标

### 3.1 必须解决的问题

1. **逻辑模块和物理文件解耦**

一个逻辑模块可以由多个源文件组成。文件变了不应该天然等于模块身份变了。

2. **支持包内实现拆分**

库内部多个模块之间可以共享内部 API，但这些 API 不进入 public ABI。

3. **支持稳定 query key**

module graph、module exports、item list、item signature 和 body typecheck 都能以稳定 key 表达。

4. **支持 IDE / LSP**

必须能回答：

- 一个名字来自哪个模块？
- 一个 re-export 最终指向哪个定义？
- 修改一个文件后哪些模块、导出、函数体需要重算？
- private / package-private / public 的诊断应该落在哪个 source range？

5. **降低大型库样板代码**

大型库不应该为了拆文件而写大量薄 wrapper，也不应该为了跨文件调用把内部 helper 标成 `pub`。

### 3.2 非目标

V2 不应该一次性做这些事：

- 不做 package manager。
- 不做版本解析和依赖求解。
- 不做 glob import 作为第一阶段能力。
- 不把 `pub(package)` / `pub(crate)` 作为 module part 第一阶段的阻塞项。
- 不做运行时动态加载模块。
- 不把 `dyn Trait`、class 或继承系统混进模块系统。
- 不把所有文件系统布局都变成语言语义。

## 4. 核心语义模型

### 4.1 Package

`Package` 是编译单元和可见性边界。

短期：

```text
PackageKey = hash(root file + import paths + compiler config subset)
```

长期：

```text
PackageKey = manifest name + version + source root + dependency identity
```

`pub(package)` / `pub(crate)` 这类可见性都依赖 `PackageKey`。它们是 M3 后半段或
M3 之后的候选扩展；M3.0 的第一刀只要求 `PackageKey` 能给 module graph / exports
query 提供稳定身份。

### 4.2 Module

`Module` 是逻辑命名空间，不等于文件。

```text
module regex.vm;
```

产生：

```text
ModuleKey {
    package: PackageKey,
    path: "regex.vm",
    kind: source,
}
```

### 4.3 Module Part

一个模块可以由多个 part 组成。

建议语法：

```aurex
module regex.vm part engine;
```

或者保守一点：

```aurex
module regex.vm;
part engine;
```

我更建议第一种：`module regex.vm part engine;`。原因是 `part` 属于模块声明的一部分，parser 和 diagnostics 更容易保证“这个文件到底属于哪个逻辑模块的哪个 part”。

示例：

```aurex
// vm/core.ax
module regex.vm;

priv struct Program {
    code: []const u8;
}

pub(package) fn compile(pattern: str) -> Result[Program, Error] {
    return parse_and_emit(pattern);
}
```

```aurex
// vm/engine.ax
module regex.vm part engine;

priv fn parse_and_emit(pattern: str) -> Result[Program, Error] {
    ...
}
```

```aurex
// vm/debug.ax
module regex.vm part debug;

priv fn dump_program(program: &Program) -> void {
    ...
}
```

这三个文件共享同一个 `ModuleKey(regex.vm)`，但有不同 `ModulePartKey`。

### 4.4 Import

当前 import 继续保留：

```aurex
import regex.vm as vm;
```

语义：

- import 绑定的是模块 alias，不是把所有名字注入当前作用域。
- 未限定名字优先当前模块。
- 跨模块访问使用 `alias.name` 或完整可见模块路径。

这点应该继续保持。它比默认把所有名字拉进当前作用域更稳定，也更适合大型编译器代码。

### 4.5 Re-export

短期继续支持当前形态：

```aurex
pub import regex.api as api;
```

中期加入显式 re-export：

```aurex
pub use regex.api.Regex;
pub use regex.api.compile;
pub use regex.api.Error as RegexError;
```

设计原则：

- `import` 用来引入模块 alias。
- `use` 用来引入或转发具体定义。
- `pub use` 是 public API 的一部分，必须进入 `ModuleExports`。

## 5. 可见性模型

完整 V2 候选模型可以采用四层可见性：

```text
priv          只在同一逻辑模块内可见，包括所有 module parts
pub(package) 只在同一 package 内可见
pub(crate)   pub(package) 的别名，等 package manifest 稳定后可保留或废弃
pub          对外 public API
```

M3.0 第一阶段只要求 `priv` 能跨同一逻辑模块的所有 parts 可见，`pub` 继续表示跨模块 public API。
`pub(package)` / `pub(crate)` 用来记录后续设计方向，不阻塞 module part 和 exports query。

暂不建议第一阶段加入：

```text
pub(super)
pub(in path)
protected
friend
```

原因是 Aurex 当前没有 nested module tree 语法，也没有 class inheritance；直接引入 `pub(super)` / `protected` 会把可见性模型变复杂，但收益不明显。

### 5.1 为什么不是只有 `pub/priv`

只有 `pub/priv` 时会出现这个问题：

```aurex
// regex.vm
priv fn compile_internal(...) -> Program { ... }

// regex.api
import regex.vm as vm;

pub fn compile(pattern: str) -> Regex {
    // 不能调用 vm.compile_internal，因为它是 priv。
}
```

为了让 `regex.api` 调用，只能改成：

```aurex
pub fn compile_internal(...) -> Program { ... }
```

但这会把 `compile_internal` 暴露给用户，污染 public API。

`pub(package)` 解决的是这个中间层：

```aurex
// regex.vm
pub(package) fn compile_internal(...) -> Program { ... }

// regex.api
import regex.vm as vm;

pub fn compile(pattern: str) -> Regex {
    return Regex{ program: vm.compile_internal(pattern)? };
}
```

包外用户仍然不能调用：

```aurex
import regex.vm as vm;

fn main() -> i32 {
    vm.compile_internal("a+"); // error: package-private item is not visible here
    return 0;
}
```

### 5.2 module part 和 private 的关系

`priv` 应该是**逻辑模块私有**，不是文件私有。

```aurex
// regex.vm part core
module regex.vm;

priv fn alloc_state() -> State { ... }
```

```aurex
// regex.vm part engine
module regex.vm part engine;

fn run() -> void {
    let state = alloc_state(); // ok: same logical module
}
```

如果以后需要“文件私有”，可以再加：

```text
file priv
```

但第一阶段不建议加。过早加入文件私有会让可见性矩阵变复杂。

## 6. Query 设计

### 6.1 查询分层

模块系统 V2 应该至少拆出这些 query：

```text
FileContent(FileKey)
  -> LexFile(LexFileKey)
  -> ParseFile(ParseFileKey)
  -> ModulePart(ModulePartKey)
  -> ModuleGraph(PackageKey or root ModuleKey)
  -> ModuleExports(ModuleKey)
  -> ItemList(ModuleKey)
  -> ItemSignature(DefKey)
  -> TypeCheckBody(BodyKey)
  -> Diagnostics(QueryKey)
```

其中：

- `ModuleGraph` 负责解析模块和 import 关系。
- `ModuleExports` 负责 public / package-public / re-export 表。
- `ItemList` 负责同一逻辑模块多个 part 的 item 汇总。
- `ItemSignature` 只依赖 item signature，不依赖函数体。
- `TypeCheckBody` 只在函数体或依赖 signature 变化时重算。

### 6.2 为什么 query 对模块系统很重要

假设有三个文件：

```text
regex/vm/core.ax
regex/vm/engine.ax
regex/api.ax
```

如果只改 `engine.ax` 里一个 private 函数体：

```aurex
priv fn step(...) -> State {
    // 修改这里
}
```

理想失效范围：

```text
ParseFile(engine.ax)        red
ModulePart(regex.vm:engine) red
TypeCheckBody(step)         red
ModuleExports(regex.vm)     green
ItemSignature(compile)      green
regex.api body              green unless it depends on changed signature
```

如果只靠当前 combined AST 重跑，就很难精细地区分这些层。

## 7. 名字解析和导出表

### 7.1 命名空间

Aurex 应该显式区分 namespace：

```text
type namespace     struct / enum / type alias / trait
value namespace    fn / const / global / enum constructor
member namespace   field / method / associated item
module namespace   module alias / module path
```

这和 Rust 的经验类似：use/re-export 会影响不同 namespace，若不分开，错误诊断和歧义处理都会变脆。

### 7.2 ModuleExports

每个模块产生一张导出表：

```text
ModuleExports {
    module: ModuleKey,
    public_types: name -> DefKey
    public_values: name -> DefKey
    public_members: owner + name -> MemberKey
    package_types: name -> DefKey
    package_values: name -> DefKey
    reexports: local name -> target DefKey
}
```

`pub use` 必须写入 `reexports`。

### 7.3 歧义处理

如果两个 re-export 导出同名不同实体：

```aurex
module app.facade;

pub use a.io.Reader;
pub use b.io.Reader;
```

应当诊断：

```text
error: ambiguous re-export 'Reader'
note: first exported here
note: conflicting export here
help: rename one export with `as`
```

修复方式：

```aurex
pub use a.io.Reader as ByteReader;
pub use b.io.Reader as TextReader;
```

如果两个 re-export 最终指向同一个定义，则可以允许合并：

```aurex
pub use a.api.Regex;
pub use a.compat.Regex; // 如果最终 target DefKey 相同，可视为重复但不冲突
```

## 8. 具体例子

### 8.1 大模块拆文件

V1 写法：

```aurex
module regex.vm;

priv struct State { ... }
priv fn parse(...) -> Ast { ... }
priv fn emit(...) -> Program { ... }
priv fn optimize(...) -> Program { ... }
pub fn compile(...) -> Program { ... }
```

文件越来越大。

V2 写法：

```aurex
// regex/vm/core.ax
module regex.vm;

priv struct State { ... }

pub(package) fn compile(pattern: str) -> Result[Program, Error] {
    let ast = parse(pattern)?;
    let program = emit(ast)?;
    return optimize(program);
}
```

```aurex
// regex/vm/parser.ax
module regex.vm part parser;

priv fn parse(pattern: str) -> Result[Ast, Error] { ... }
```

```aurex
// regex/vm/emitter.ax
module regex.vm part emitter;

priv fn emit(ast: Ast) -> Result[Program, Error] { ... }
```

```aurex
// regex/vm/optimizer.ax
module regex.vm part optimizer;

priv fn optimize(program: Program) -> Result[Program, Error] { ... }
```

收益：

- 四个文件共享同一个 logical module。
- `parse` / `emit` / `optimize` 仍是 private。
- `compile` 只对 package 内可见。
- public API 不暴露 VM 内部实现。

缺点：

- compiler 必须合并多个 part 的 item list。
- 重复定义诊断要跨 part。
- part 加载顺序不能影响语义。

### 8.2 public facade

```aurex
// regex/api.ax
module regex.api;

import regex.vm as vm;

pub struct Regex {
    program: vm.Program;
}

pub fn compile(pattern: str) -> Result[Regex, Error] {
    return Regex{ program: vm.compile(pattern)? };
}
```

```aurex
// regex/mod.ax
module regex;

pub use regex.api.Regex;
pub use regex.api.compile;
```

用户只需要：

```aurex
import regex as regex;

fn main() -> i32 {
    let r = regex.compile("a+")?;
    return 0;
}
```

收益：

- `regex.vm` 不进入用户 API。
- `regex.api` 可以作为内部组织，`regex` 作为 facade。
- IDE 跳转可以从 `regex.compile` 跳到 `regex.api.compile`。

缺点：

- re-export 必须记录 source 和 target，否则 definition 跳转会乱。
- 公开 API 的稳定性不只取决于声明处，还取决于 facade 的 re-export 表。

### 8.3 package-private 类型泄漏

错误例子：

```aurex
module regex.api;

import regex.vm as vm;

pub fn compile(pattern: str) -> vm.Program {
    return vm.compile(pattern)?;
}
```

如果 `vm.Program` 是 `pub(package)` 或 `priv`，那么这个 `pub fn` 把内部类型泄漏到了 public signature。

应诊断：

```text
error: public function exposes package-private type 'regex.vm.Program'
note: return type is part of public API
help: wrap it in a public type or make the type public intentionally
```

正确写法：

```aurex
pub struct Regex {
    priv program: vm.Program;
}

pub fn compile(pattern: str) -> Result[Regex, Error] {
    return Regex{ program: vm.compile(pattern)? };
}
```

这个检查很重要。否则 `pub(package)` 只是表面上隐藏，实际会通过 public signature 泄漏出去。

### 8.4 import alias 歧义

```aurex
module app.main;

import a.io as io;
import b.io as io;

fn main() -> i32 {
    io.read(); // ambiguous
    return 0;
}
```

应该在 import 处或使用处诊断：

```text
error: ambiguous import alias 'io'
note: first import alias declared here
note: conflicting import alias declared here
help: rename one import with `as`
```

修复：

```aurex
import a.io as aio;
import b.io as bio;
```

### 8.5 circular re-export

```aurex
module a;
pub use b.X;

module b;
pub use a.X;
```

如果没有真实定义，应该诊断 re-export cycle：

```text
error: cyclic re-export while resolving 'X'
note: a exports b.X
note: b exports a.X
```

如果 cycle 中某条路径最终指向同一个 concrete `DefKey`，可以允许，但第一阶段建议保守拒绝，避免复杂性过早进入。

## 9. 方案对比

### 9.1 方案 A：继续当前文件级模块

优点：

- 实现最简单。
- 当前代码改动最小。
- 适合小项目。

缺点：

- 大模块难拆。
- 内部 API 容易被迫 `pub`。
- query 失效范围粗。
- IDE 很难做局部增量。
- public API 和内部实现边界会越来越混。

结论：不适合作为 M3 以后系统库和前端自举的长期基础。

### 9.2 方案 B：Rust 风格 nested module tree

类似：

```rust
mod vm;
mod parser;
pub(crate) fn helper() {}
```

优点：

- 工业上成熟。
- `pub(crate)`、`pub(in path)` 等表达力强。
- re-export 经验丰富。

缺点：

- Aurex 当前没有 nested `mod {}` 语法。
- 会引入更多路径规则：`self`、`super`、crate root、relative path。
- 学习成本高，parser / name resolver 要一次性扩张很多。

结论：可以借鉴可见性和 re-export 思路，但不建议照搬 nested module tree。

### 9.3 方案 C：Kotlin 风格 package + internal

类似：

```text
package regex.vm
internal fun compile(...)
```

优点：

- package-private 心智模型简单。
- 多文件同 package 很自然。
- 对大型库拆分很友好。

缺点：

- 如果 package 和文件系统关系太松，编译器需要额外 package graph。
- 默认 public 的语言不适合 Aurex 当前 default private 策略。

结论：Aurex 可以借鉴 `internal` 的边界，但保持 default private。

### 9.4 方案 D：Aurex V2 混合方案

```aurex
module regex.vm part parser;
pub(package) fn helper(...) -> ...
pub use regex.api.Regex;
```

优点：

- 保留当前 `module path;`。
- 支持一个逻辑模块多文件。
- 支持 package-private。
- 直接映射到 `ModuleKey` / `ModulePartKey` / `ModuleExports`。
- 不强迫引入 nested module 和 `super` / `self` 路径。

缺点：

- 是 Aurex 自己的模型，需要写清楚规范。
- module part 顺序、重复定义、source range 诊断都要设计。
- package root 还没有 manifest 时，只能先用编译器 invocation 推导。

结论：最适合当前阶段。

## 10. 好处

### 10.1 大型代码库更容易组织

原来一个 `regex.vm` 只能放在一个文件里，否则内部 helper 不好共享。V2 后可以按功能拆 part：

```text
regex.vm
  core
  parser
  emitter
  optimizer
  debug
```

对外仍然只有一个 `regex.vm`。

### 10.2 public API 更干净

`pub(package)` 可以让内部模块协作，不把实现细节泄漏给用户。

### 10.3 增量编译更准

改 private body 不影响 exports。改 `pub` signature 才影响下游 public API。

### 10.4 诊断更准

有了 `ModuleExports` 和 namespace-aware lookup，可以更清楚地区分：

- unknown module alias
- unknown item in visible module
- private item
- package-private item
- ambiguous import
- ambiguous re-export
- public API leaks private type

### 10.5 更适合 IDE

IDE 可以直接查询：

```text
definition(name) -> DefKey
references(DefKey)
exports(ModuleKey)
visible_names(ModuleKey)
```

而不是每次从 combined AST 和 sema side table 里反推。

### 10.6 给后续高级特性保留稳定边界

M3 当前只承诺模块系统和泛型闭环；static trait、资源语义、owned containers、class-like sugar 都是 M3 之后的独立专题。不过这些后续能力都会依赖稳定模块边界。否则 trait impl、Drop impl、associated type、generic instance key 都容易被临时路径和 display name 污染。

## 11. 缺点和风险

### 11.1 编译器复杂度上升

当前 `ModuleLoader` 递归加载后 append AST。V2 需要：

- ModuleGraph builder
- ModulePart registry
- ModuleExports resolver
- visibility checker
- re-export cycle detector
- query dependency tracking

这是实打实的复杂度。

### 11.2 规范复杂度上升

用户要理解：

- module 是逻辑命名空间
- part 是同一模块的文件片段
- package 是可见性边界
- import 绑定模块 alias
- use 绑定具体定义
- pub use 改变 public API

如果文档和诊断不清楚，会比当前系统难学。

### 11.3 package root 暂时不稳定

没有 manifest 时，`PackageKey` 只能从 root file、import paths 和 invocation 推导。这在 CLI 场景够用，但对长期 package manager 不够。

缓解方式：

- M2.5 已经把 `PackageKey` / `ModuleKey` / `ModulePartKey` 作为 query-key 基础抽象出来。
- M3 先使用这套身份模型完善 module part 和 exports query。
- 不立刻承诺 manifest 语义。
- 后续有 manifest 后替换 key 构造来源。

### 11.4 public API 泄漏检查复杂

`pub` 函数签名里不能出现 private / package-private 类型。泛型、associated type、future trait bound 都会扩大检查范围。

例如：

```aurex
pub fn f[T: internal.Trait](value: T) -> void { ... }
```

这类签名以后必须被拒绝或要求 trait 也 public。

### 11.5 re-export 会制造 API 兼容性压力

一旦用户依赖：

```aurex
regex.Regex
```

即使真实定义在 `regex.api.Regex`，删除 `pub use regex.api.Regex;` 也是 breaking change。

这不是坏事，但必须让 `ModuleExports` 和文档生成器都把 re-export 当成 public API。

### 11.6 query 失效规则更难写对

如果 `ModuleExports` 的 fingerprint 计算漏了某个 re-export，IDE 可能出现“代码已改但补全没变”的错误。

缓解方式：

- 每个 query 输出结构化 fingerprint。
- export 表用排序后的 stable key 计算 fingerprint。
- 加 red/green regression tests。

## 12. 推荐落地顺序

### Phase 0：冻结语义文档

交付：

- 本文档。
- negative / positive case 清单。
- 明确 V2 非目标。

### Phase 1：ModuleKey / ModulePartKey 接入现有加载器

不改语言表面，只把当前 `module path;` 映射到：

```text
PackageKey
ModuleKey
ModulePartKey(primary)
```

交付：

- module graph dump。
- stable module key tests。
- duplicate module diagnostics 保持兼容。

### Phase 2：ModuleGraph query

把 import 解析结果从 `ModuleLoader` side effect 里抽成结构化结果。

交付：

- `ModuleGraph(ModuleKey)` 或 `ModuleGraph(PackageKey)`。
- import edge list。
- cycle diagnostics。
- missing import diagnostics。

### Phase 3：ModulePart 语法

加入：

```aurex
module regex.vm part parser;
```

语义：

- part name 在同一 module 内唯一。
- part item 归入同一个 `ModuleKey`。
- `priv` 跨 part 可见。
- part 加载顺序不影响 lookup。

### Phase 4：ModuleExports query

先把 public exports 表结构化。package-private 和 re-export 可以使用同一张表的扩展字段，但不要求第一阶段全部实现。

交付：

- exports fingerprint。
- public API 泄漏 private type 诊断。
- IDE definition target 可从 export table 找到真实定义。

### Phase 5：package-private visibility

加入：

```aurex
pub(package)
pub(crate)
```

第一阶段可以把二者视为同义。

交付：

- package 内访问通过。
- package 外访问拒绝。
- public API 泄漏 package-private 类型拒绝。

### Phase 6：`pub use` / selective import

加入：

```aurex
use regex.api.Regex;
use regex.api.Error as RegexError;
pub use regex.api.compile;
```

暂缓：

```aurex
use regex.api::*;
```

glob import 第一阶段不要做。它会显著增加歧义和诊断复杂度。

## 13. 测试计划

### 13.1 Parser tests

- `module a.b;`
- `module a.b part c;`
- 缺少 part name。
- part name 非 identifier。
- `part` 用在非 module declaration 位置。

### 13.2 Module loader tests

- 同一 module 多 part 成功。
- 重复 part name 拒绝。
- import 指向错误 module path 拒绝。
- part 文件声明了不同 module path 拒绝。
- import cycle 诊断。

### 13.3 Sema visibility tests

- same module part 可访问 `priv`。
- sibling module 不可访问 `priv`。
- `pub fn` 泄漏 private type 拒绝。

### 13.4 Query / incremental tests

- 修改 private body：`ModuleExports` fingerprint 不变。
- 修改 public signature：`ModuleExports` fingerprint 变化。
- 修改 import list：`ModuleGraph` fingerprint 变化。
- 修改 part body：只相关 body query 变红。

### 13.5 Diagnostics tests

- ambiguous import alias。
- unknown import alias with suggestion。
- private item access。
- duplicate item across parts。
- duplicate module part。

### 13.6 后续 visibility / re-export tests

这些用例对应 Phase 5 / Phase 6，不阻塞 M3.0 module part 的第一阶段：

- same package 可访问 `pub(package)`。
- external package 不可访问 `pub(package)`。
- `pub struct` public field 泄漏 package-private type 拒绝。
- re-export cycle 诊断。
- ambiguous re-export。
- 修改 re-export：facade exports 变红。

## 14. 对照经验：Python 和现代 C++

### 14.1 Python 的做法

Python 的模块系统是**运行时导入系统**，不是编译期模块系统。它的核心机制是：

1. 先查 `sys.modules`，如果已经导入过，就直接复用缓存。
2. 再通过 `sys.meta_path`、`sys.path`、`path hooks` 找到模块。
3. loader 创建 module object，然后执行模块代码。
4. `import` 本身不会像 Aurex 那样形成编译期稳定 query key；它更多是运行时命名空间和缓存协议。

包（package）在 Python 里通常对应目录，历史上常依赖 `__init__.py`，后来还支持 namespace package。`__all__`、`__init__.py` 中的 re-export、命名约定下划线前缀，构成了 Python 的“导出边界”惯例，但它不是语言级强制的可见性系统。

对 Aurex 的启发是：

- 可以借鉴 Python 的 package facade 和 re-export 习惯。
- 不能照搬它的运行时导入语义，因为我们需要编译期稳定身份、增量失效和 IDE 级 query。
- Python 的做法适合脚本和动态生态，不适合直接承担编译器前端的模块 identity。

### 14.2 现代 C++ 的做法

现代 C++20/23 的模块系统更接近我们要的方向，但它的重点仍然是**编译期可分割的翻译单元**，不是运行时命名空间。它把模块分成：

- `export module`：模块接口单元。
- `module;` / `module M:part;`：模块实现或模块分区。
- `export`：控制哪些声明进入模块外可见的接口。
- `import`：显式导入模块依赖。

模块分区（module partitions）尤其值得参考，因为它和我们提议的 `module regex.vm part parser;` 很像：一个逻辑模块可以拆成多个编译片段，内部实现可以跨分区协作，但对外仍保持统一接口。C++ 还区分 private module fragment、exported declarations 和 implementation units，这说明“逻辑模块”和“文件”不必一一对应。

对 Aurex 的启发是：

- 可以借鉴 C++ 模块分区的思路来设计 `ModulePartKey`。
- 可以借鉴 C++ 的接口/实现分离来设计 `ModuleExports`。
- 但不要直接照搬 C++ 的语法复杂度；Aurex 当前没有那么多 legacy 兼容包袱，应该保留更简单的 `module path;` 语义，再逐步加 `part` 和 `pub(package)`。

### 14.3 三者对比

```text
Python
  runtime module object
  cache: sys.modules
  package: directory / __init__.py / namespace package
  visibility: convention + __all__
  best for: dynamic loading, scripting, plugin ecosystem

Modern C++
  compile-time module unit
  cache: compiler module BMI / build artifacts
  package-like boundary: module interface + partitions
  visibility: export / import
  best for: large compiled systems, build scalability

Aurex V2
  compile-time logical module + module part
  cache: query graph + stable keys
  boundary: priv / pub, later pub(package)
  best for: compiler frontends, system libraries, IDE-friendly incremental compilation
```

### 14.4 结论

Python 告诉我们：**facade 和 re-export 很重要**。

现代 C++ 告诉我们：**模块分区和接口/实现分离很重要**。

Aurex 需要的是把这两点放进一个更适合编译器工程的模型里：

- 逻辑模块稳定。
- 文件可以拆分成 part。
- public API 可以通过 facade 暴露。
- package-private 可以保留内部协作。
- 所有边界都能落到 query key 和 diagnostics 上。

## 15. 参考经验

Rust 的模块系统提供了几个值得借鉴的点：

- item 在模块层组织，并且 name resolution 允许 item 在使用前或使用后定义。
- `pub(crate)`、`pub(in path)` 等 scoped visibility 说明 public/private 之间需要中间层。
- `pub use` 是 re-export，会影响 public API。

但 Aurex 不应该照搬 Rust nested module tree，因为当前语法没有 `mod` item、`self` / `super` / crate-root 路径模型，直接照搬会扩大太多前端复杂度。

Kotlin 的 `internal` 说明了“同一 module 内可见”的价值。它对大型项目拆分非常实用，但 Kotlin 默认 public；Aurex 应继续保持 default private。

Python 的 import system 也提醒一点：package / module 不应该被过度绑定到文件系统。文件系统可以是默认加载策略，但语言层身份应该是逻辑身份。

参考资料：

1. [Rust Reference: items](https://doc.rust-lang.org/reference/items.html)
2. [Rust Reference: visibility and privacy](https://doc.rust-lang.org/reference/visibility-and-privacy.html)
3. [Rust Reference: use declarations](https://doc.rust-lang.org/stable/reference/items/use-declarations.html)
4. [Rust Reference: crates and source files](https://doc.rust-lang.org/stable/reference/crates-and-source-files.html)
5. [Python import system](https://docs.python.org/3.10/reference/import.html)
6. [Python tutorial: modules](https://docs.python.org/3/tutorial/modules.html)
7. [Python tutorial: packages](https://docs.python.org/3/tutorial/modules.html#packages)
8. [Python importlib](https://docs.python.org/3/library/importlib.html)
9. [C++ language modules](https://learn.microsoft.com/en-us/cpp/cpp/modules-cpp?view=msvc-170)
10. [C++ module partitions](https://learn.microsoft.com/en-us/cpp/cpp/module-partitions?view=msvc-170)
11. [C++ `export` and `import`](https://learn.microsoft.com/en-us/cpp/cpp/module-import-export?view=msvc-170)
12. [C++ modules overview](https://learn.microsoft.com/en-us/cpp/cpp/modules-cpp?view=msvc-170)

## 16. 总结

模块系统 V2 的核心不是把 import 写法做花，而是明确三条边界：

```text
文件边界：FileKey / ModulePartKey
模块边界：ModuleKey / priv / item namespace
包边界：PackageKey / public API，后续扩展 pub(package)
```

推荐结论：

1. 当前 `module path;` 保留。
2. 新增 `module path part name;` 支持逻辑模块多文件。
3. `pub(package)` / `pub(crate)` 作为 M3 后半段或 M3 之后的 visibility 扩展，不阻塞第一阶段 module part。
4. 把 `ModuleGraph` 和 `ModuleExports` 做成 query。
5. `pub use` 后置到 export table 稳定之后。
6. glob import、nested module、`pub(super)`、package manager 都后置。

这样做能支撑系统库拆分、前端自举、query 增量和 IDE，同时不会一次性把模块系统扩张成难以验证的大功能。

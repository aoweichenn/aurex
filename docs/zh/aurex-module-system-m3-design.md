# Aurex M3 模块系统设计稿

状态：M3.0 当前设计基线，Phase 1-5 与 Phase 6A-1/2/3/4/5/6 已进入实现闭环
日期：2026-05-26
适用范围：同一 package 内的 logical module / source-file part 分离、module graph、exports query、跨 part `priv` 可见性

## 1. 设计结论

Aurex M3.0 的模块系统采用：

```text
显式 primary module 文件
显式 part list
part 文件自声明所属 logical module 和 part name
```

推荐语法固定为：

```aurex
module regex.vm;

part parser;
part emitter;
part optimizer;
```

```aurex
module regex.vm part parser;
```

这意味着 Aurex 的模块不是 Python 式运行时 import，也不是 Rust 式嵌套 `mod` 文件树，
也不是 C++ 式 translation-unit / BMI 体系。Aurex 的模块是：

```text
静态、显式、query-friendly、IDE-friendly 的 logical namespace。
```

文件只是 logical module 的一个 source-file part。模块身份来自 `PackageKey + ModuleKey`，
不是来自物理路径。源文件身份来自 `ModulePartKey`，用于 diagnostics、debug info 和
incremental invalidation，不直接决定 public API identity。

## 2. 当前项目约束

M3.0 建立在 R5 Compilation Pipeline / Driver Action core 之后。所有模块实现必须复用：

- `CompilationSession`
- `CompilationPipeline`
- `FrontendPipeline`
- `LoweringPipeline`
- `BackendPipeline`
- `PipelineStage`
- query key / diagnostics / profile / tooling contract

M3.0 不允许在 driver、session、query、diagnostics 之外另开一套 module compilation path。

当前实现仍然是：

```text
source file
  -> lexer
  -> parser
  -> AstModule
  -> ModuleLoader recursive import
  -> combined AstModule
  -> sema / lowering / backend
```

M3.0 的落地策略不是推倒重写，而是在保持 combined AST 兼容路径可用的同时，把权威语义边界
逐步迁移到：

```text
PackageKey
  -> ModuleKey
      -> ModulePartKey
      -> ModuleGraph
      -> ModuleExports
      -> ItemList
      -> ItemSignature
```

## 3. 外部语言调研取舍

### 3.1 C++ / Clang

C++20 modules 的价值在于明确区分 module interface、module implementation、module partition
和 import。Clang / MSVC 的实践说明：一个 logical module 可以拆成多个 module units，partition
可以成为编译期边界，public API 和 implementation 可以分层。

Aurex 采纳：

- 一个 logical module 可以由多个 part 组成。
- part name 在同一 module 内唯一。
- module 对外只暴露显式 public exports。
- 后端 symbol identity 不应由文件路径决定。

Aurex 不采纳：

- header unit。
- macro / textual inclusion 兼容包袱。
- global module fragment / private module fragment 的复杂翻译单元语义。
- 把 BMI 当作语言级信息隐藏机制。
- interface / implementation 强制分文件作为 M3.0 第一阶段要求。

原因：Aurex 当前没有 C/C++ 头文件历史包袱，也没有宏展开兼容需求。M3.0 的关键不是预编译
module artifact，而是先把 logical module、part、exports 和 query identity 做稳。

### 3.2 Python

Python 的优点是 dotted path、package 层级和 `import x as y` 的 ergonomics。它适合人写，
也适合工具展示。

Aurex 采纳：

- dotted module path，例如 `regex.vm`。
- alias import，例如 `import regex.vm as vm;`。
- 文件系统可以作为查找策略的一部分。

Aurex 不采纳：

- runtime import side effect。
- 动态 `sys.path` 语义。
- import 时执行模块顶层代码。
- `__all__` / underscore convention 作为可见性语义。
- namespace package 的动态合并。

原因：Aurex 是静态编译器。module graph 必须在 frontend 阶段确定，不能由运行时路径、运行时副作用
或 convention 决定。

### 3.3 Rust

Rust 的优点是强 visibility、`pub use` re-export、module tree 的明确 ownership，以及
`pub(crate)` / `pub(super)` / `pub(in path)` 这类 scoped visibility。

Aurex 采纳：

- public API 必须显式。
- re-export 必须进入 module exports。
- duplicate item 应在同一 namespace 内稳定报错。
- `pub(package)` 可以作为后续 package-private visibility。

Aurex 不采纳为 M3.0 第一阶段能力：

- nested `mod` tree。
- `mod.rs` / inline module / path attribute 历史规则。
- `pub(super)` / `pub(in path)`。
- glob import。

原因：Aurex 当前没有 Rust 式嵌套 module tree 语义。过早引入 `super` / `in path` 会让
visibility 绑定到 tree topology，而 M3.0 的目标是先稳定 `PackageKey`、`ModuleKey` 和
`ModulePartKey`。

### 3.4 Kotlin

Kotlin 的 package header 不要求匹配目录，imports 是 file-local，`internal` 以同一 compilation
module 为边界。这给 Aurex 很有价值的启发：logical identity 和 physical layout 可以分离。

Aurex 采纳：

- logical module path 不强制等于目录结构。
- imports 是 part-local declaration。
- package-private / internal visibility 以后以 `PackageKey` 为边界。

Aurex 不采纳：

- 默认 public。
- star import 作为默认常用路径。
- 把 JVM / build artifact 语义混进语言核心。

原因：Aurex 需要对 public API、query invalidation 和 diagnostics source range 有更强的静态控制。

## 4. 核心语义模型

### 4.1 PackageKey

`PackageKey` 是 compilation package 的稳定身份，也是未来 `pub(package)` 的边界。

M3.0 第一阶段不做 package manager、版本求解或外部 dependency resolver。当前实现已经支持
manifest-backed package identity，但只把 manifest 作为 `PackageKey` 的稳定输入，不把它扩展成依赖
解析系统。可参与 package identity 的输入是：

```text
root file
import paths
compiler config subset
source root
manifest package name/version/root
```

长期再把 dependency source、lockfile 和版本求解纳入完整 package identity。

M3.0 对 `PackageKey` 的承诺必须保守：

- 它用于同一次 compilation session 内的 query grouping、module identity 和 `pub(package)` visibility 边界。
- 它承诺当前编译会话内 manifest name/version/root 级 package identity，但不承诺 dependency graph 级
  package identity。
- 同一份源码如果用不同 root file、import path 或 compiler config 编译，短期内可以得到不同 `PackageKey`。
- `pub(package)` 使用当前编译会话内 `PackageKey`；如果存在有效 `aurex.toml`，则优先由 manifest
  identity 决定。

当前 Phase 6A-3 到 6A-6 的实现状态：

- root package identity 可由 `CompilerInvocation::package_identity` / CLI `--package` 指定；空值继续映射到
  历史默认 package key。
- 当没有显式 `--package` 时，driver 会从 root input 所在目录向上查找 `aurex.toml`，读取 `[package]`
  下的 `name` 和可选 `version`，并以 manifest root 一起派生 root `PackageKey`。
- 显式 `-I` / `--import-path` import root 如果存在可识别 `aurex.toml`，外部模块使用该 manifest
  identity；否则继续回退到 canonical import root identity。
- CLI `--package` 仍是最高优先级 override，便于测试、工具集成和非 manifest 单文件场景。
- 从当前文件目录解析到的 local-relative import 继承 importer 的 `PackageKey`。
- 从显式 `-I` / `--import-path` import root 解析到的模块派生独立 `PackageKey`，避免未来
  `pub(package)` 把外部依赖误判为同包。
- `ModuleRecord`、`ModuleImportRecord`、sema `SemanticOptions::module_packages` 和增量缓存 source /
  module query subjects 已消费同一套 package key。
- cache header 记录解析后的 root package identity；同一源码用不同 `--package` 或不同 manifest identity
  编译时不会被旧 coarse source reuse 直接复用。
- Parser 已接入 `pub(package)`，same-package access 和 package-visible surface 泄漏检查消费同一套
  `PackageKey`。

仍未承诺的部分：

- manifest 暂只识别 `[package] name/version` 和 manifest root，不支持 workspace、dependency、profile、
  feature 或 source-root override。
- 没有 dependency package lock 或版本求解。
- 没有跨 package 同名 logical module 的完整 namespace UX；loader 已先把 logical module cache key
  做成 package-aware，用户级 package import 语法仍在后续阶段设计。

### 4.2 ModuleKey

`ModuleKey` 是 logical module identity。

```aurex
module regex.vm;
```

产生：

```text
ModuleKey {
  package: PackageKey,
  path: "regex.vm",
  kind: source
}
```

`ModuleKey` 不包含文件路径。移动一个 item 到同一 module 的另一个 part，不应改变它的 public
definition identity。

### 4.3 ModulePartKey

`ModulePartKey` 是 source-file part identity。

```aurex
module regex.vm part parser;
```

产生：

```text
ModulePartKey {
  module: ModuleKey("regex.vm"),
  file: FileKey("regex/vm.parts/parser.ax"),
  name: "parser",
  stable_index: 1,
  kind: fragment
}
```

primary 文件产生：

```text
ModulePartKey {
  module: ModuleKey("regex.vm"),
  file: FileKey("regex/vm.ax"),
  name: "<primary>",
  stable_index: 0,
  kind: primary
}
```

`stable_index` 来自 primary part list 的声明顺序，仅用于 deterministic dumps、diagnostics 和
incremental bookkeeping，不影响 language semantics。

### 4.4 DefKey

`DefKey` 来自：

```text
ModuleKey + namespace + logical item path + kind + disambiguator
```

不来自 file path。这样可以保证：

- 同一 module 内移动定义不制造新 public symbol。
- debug info 仍然能指向原始 source range。
- query invalidation 可以区分 body edit、signature edit 和 module graph edit。

`DefKey` 的 `disambiguator` 不能来自 AST 遍历顺序、part list 顺序或文件加载顺序。M3.0 第一阶段
不设计 overload set 的稳定 disambiguation；同一 `ModuleKey + namespace + name` 的重复定义直接报错，
普通 top-level def 使用 `disambiguator = 0`。泛型实例 identity 继续由 `GenericInstanceKey`
一类 stable key 派生，不借用 source traversal index。

## 5. 语法设计

### 5.1 Primary module file

```aurex
module regex.vm;

part parser;
part emitter;
part optimizer;

priv struct Program {
    code: []u8;
}

pub fn compile(pattern: str) -> Program {
    let ast = parse(pattern);
    let program = emit(ast);
    return optimize(program);
}
```

规则：

- primary 文件必须使用 `module path;`。
- primary 文件可以声明零个或多个 `part name;`。
- 如果没有 `part` 声明，则这个 module 只有 primary part。
- primary 文件结构固定为 `module declaration -> part declarations -> import declarations -> items`。
- `part` 声明只允许出现在 primary module 的顶层 module declaration 之后、import declaration 之前。
- 同一 primary 文件中 part name 不可重复。
- `part` 声明不是 import，也不创建 nested module path；它只声明同一 logical module 的 source-file part membership。
- `part` 在 M3.0 中是 module declaration 区域的上下文关键字，不作为全局保留字。普通代码中的
  `part` identifier 不应因为模块系统落地而被破坏。
- part name 必须是 simple identifier，不能是 reserved keyword、不能包含 `.` 或 path separator，
  也不能使用保留的 primary sentinel 名称。

### 5.2 Module part file

```aurex
module regex.vm part parser;

priv fn parse(pattern: str) -> Ast {
    ...
}
```

规则：

- part 文件必须使用 `module path part name;`。
- `module path` 必须等于 primary 文件的 module path。
- `part name` 必须出现在 primary 文件的 part list 中。
- part 文件结构固定为 `module part declaration -> import declarations -> items`。
- part 文件不能再声明 `part name;` 列表。
- part 文件不能作为 codegen / native artifact 的 root input 独立编译。
- part 文件不能被 `import` 当成独立 module。

CLI 和 tooling 需要区分：

- `aurexc --check path/to/vm.parts/parser.ax` 这类检查型命令可以从 part header 反查 owning primary，
  然后检查完整 owning module；诊断必须说明实际检查的是 `regex.vm`，而不是把 part 当成独立 module。
- `--emit=tokens` / `--emit=lossless` 这类 source-local inspection 可以直接作用于任意 source file，
  因为它们不建立 module identity。
- `--emit=ast` / `--emit=modules` / `--dump-checked` 这类 frontend / sema inspection 如果以 part
  文件作为输入，也应通过 owning primary 构建完整 module graph，再输出带 part source ranges 的结果。
- codegen / native artifact 输出，例如 `--emit=ir`、`--emit=llvm-ir`、`--emit=asm`、`--emit=obj`
  和默认 executable，不能以 part 文件作为 root input。诊断应提示用户改为编译 owning primary module，
  避免“从 implementation fragment 生成哪个 module artifact”的身份歧义。
- IDE / tooling 可以临时解析 part buffer，但必须通过 `module path part name;` 反查 owning primary，
  再构建完整 module graph 后进行语义诊断；找不到 primary 时只能进入 degraded parsing，不能给出
  “该 part 独立可编译”的假象。

### 5.3 Import

M3.0 保留现有 import：

```aurex
import regex.vm as vm;
```

语义：

- import 绑定 module alias。
- import 不把目标 module 的 item glob 注入当前 scope。
- imports 是 part-local declaration。
- 当前 part 要使用 alias，就必须在当前 part 里 import。
- 同一 logical module 的顶层 item 跨 parts 可直接可见，不需要 import。
- part-local import 只影响当前 part 源码里的 name resolution；它不会把 alias 传播给同一 module 的其他 parts。
- resolved item signatures 在同一 `ModuleKey` 内共享，但 signature 中引用的 external `DefKey` 不会让对应 import alias 在其他 parts 可见。
- import path resolution 必须产生唯一 module candidate；同一 import path 如果能从 importer-relative
  路径和多个 `-I` import paths 解析到不同文件，必须诊断 ambiguous import，而不是按搜索顺序静默选择。
- import candidate 比较使用 canonical path 去重；同一 canonical file 通过多个 search roots 命中时只算一个候选。
- import resolution 不搜索 module part sidecar 目录，除非目标 path 本身通过正常 module path
  规则解析到 primary module 文件。

如果现有语法支持省略 alias：

```aurex
import regex.vm;
```

则 alias 默认为最后一个 path segment，即 `vm`。如果发生 alias 冲突，按当前 part 的 import
diagnostics 报错。

module part 不创建可导入的 module path。例如 `regex.vm` 的 `parser` part 不是
`regex.vm.parser` module：

```aurex
import regex.vm.parser as parser; // error if this resolves to a part file
```

诊断应提示：

```text
module part `parser` is not an importable module
note: import the owning module `regex.vm` instead
```

### 5.4 Re-export

M3.0 兼容当前：

```aurex
pub import regex.api as api;
```

后续扩展：

```aurex
pub use regex.api.Regex;
pub use regex.api.compile as compile_regex;
```

M3.0 第一阶段不实现 glob import / glob re-export：

```aurex
use regex.api.*;
pub use regex.api.*;
```

原因：glob 会显著增加 ambiguity、incremental invalidation 和 diagnostics 难度，不应阻塞
module part 主线。

`pub import` 和未来 `pub use` 的边界必须固定：

```text
pub import module as alias  -> module re-export
pub use module.Item as Name -> item re-export
```

`pub import` 不等价于 glob export，不把目标 module 的所有 public items 注入当前 module exports。

## 6. 文件发现策略

M3.0 固定原则：

```text
不使用隐式目录扫描定义语言语义。
```

primary 文件的 `part name;` 是唯一的 module membership 来源。loader 只能按下面固定的
primary-sidecar 规则查找 part 文件，不能把目录下所有 `.ax` 自动纳入 module。

第一阶段固定查找策略：

```text
primary file: path/to/vm.ax
part base dir: path/to/vm.parts/
part parser -> path/to/vm.parts/parser.ax
part emitter -> path/to/vm.parts/emitter.ax
```

也就是：

```text
part base dir = primary file path without extension + ".parts" directory
part file     = part base dir / part_name.ax
```

part lookup 只相对 primary 文件，不相对 `module path`。因此 logical module path 不需要匹配目录：

```text
primary file: src/runtime/vm.ax
module path:  regex.vm
part parser:  src/runtime/vm.parts/parser.ax
```

用户可见的查找规则必须足够可解释：

- primary 声明 `part parser;` 后，缺失文件时诊断必须直接给出期望路径：
  `src/runtime/vm.parts/parser.ax`。
- 发现 `src/runtime/vm.parts/parser.ax` 但 primary 没有声明 `part parser;` 时，不能静默纳入；
  诊断或 IDE quick fix 应提示在 `src/runtime/vm.ax` 的 module declaration 区域添加
  `part parser;`。
- part 文件里的 `module regex.vm part parser;` 和 primary 的 module path / part list 不一致时，
  诊断必须同时指出 part 文件 header 和 owning primary declaration。
- `.parts` 目录是 implementation sidecar，不是 import search root。用户如果写
  `import regex.vm.parser;`，编译器只应查找 importable module `regex/vm/parser.ax`，
  不应把 `regex/vm.parts/parser.ax` 当作 fallback。

采用 `.parts` sidecar 目录是第三轮缺陷审查后的修正。Aurex 当前 import path 规则会把
`import regex.vm.parser` 映射到 `regex/vm/parser.ax`。如果 module parts 也使用 `regex/vm/parser.ax`，
那么 logical part `parser` 会和合法 dotted module `regex.vm.parser` 发生物理路径冲突，重演
Python/Rust/Java 中“路径查找和逻辑身份混杂”的历史问题。`.parts` 目录把 implementation parts
和 importable dotted modules 分开：

```text
regex/vm.ax                 -> importable module regex.vm
regex/vm/parser.ax          -> importable module regex.vm.parser
regex/vm.parts/parser.ax    -> part parser of module regex.vm
```

这条规则也降低了 Java split-package 类问题：同一个 importable module path 不应由多个物理文件或
part 文件共同定义。

跨平台文件系统还需要额外约束。Aurex 语言层面的 module path 和 part name 是大小写敏感的，
但 macOS 默认文件系统和 Windows 常见配置可能大小写不敏感。loader 必须在 canonical path
比较之外增加 case-fold collision diagnostics：

```text
regex/vm.parts/parser.ax
regex/vm.parts/Parser.ax
```

如果这两个文件在某个平台上可能指向同一物理路径，或同一 primary 同时声明了只靠大小写区分的
part name，M3.0 应给出 portability diagnostic。否则团队成员在不同 OS 上会看到不同 module graph。

如果后续需要非约定布局，可以扩展：

```aurex
part parser from "parts/parser.ax";
```

但 M3.0 第一阶段不引入 `from`。这样可以把语言语义、driver lookup policy 和未来 package manifest
分开，避免过早把任意路径选择写进语言核心。

## 7. 可见性

M3.0 第一阶段：

```text
priv    同一 ModuleKey 内可见，包括 primary 和所有 parts
pub     跨 module public API
默认值  沿用当前 Aurex 默认可见性，不在 M3.0 顺手改语义
```

后续扩展：

```text
pub(package)    同一 PackageKey 内可见
pub(crate)      可以作为 pub(package) 别名或后续废弃
```

暂不支持：

```text
pub(super)
pub(in path)
protected
friend
file-private default
```

M3.0 的 `priv` 是 module-private，不是 file-private。也就是说，同一 `ModuleKey` 的所有
parts 都能看到 `priv` top-level items。只希望当前文件内部可见的 helper 暂时没有语言级修饰符。
`file priv` 或类似能力是已知后续设计点，不混入 M3.0 第一阶段。

### 7.0 Phase 6A：独立可见性层级

Phase 6A 的优先级从 `ModulePartKey` 之外单独切出：先把 visibility 从二值
`priv` / `pub` 提升为可扩展层级，但不引入 Rust 式 nested module tree。

目标层级：

```text
module-private   <   package-visible   <   public
priv                 pub(package)          pub
```

语义边界：

- `priv`：同一 `ModuleKey` 内可见，包括 primary 和所有 parts。
- `pub(package)`：同一 `PackageKey` 内可见，跨 module 可见，但不进入 public API。
- `pub`：跨 package 可见，是真正 public API surface。
- 默认可见性保持现状：顶层 item、field、method、import 默认 `priv`。

`pub(crate)` 不作为 Aurex 的首选语法。Aurex 没有 Rust crate/module tree 语义，长期命名应以
`PackageKey` 为权威。如果要兼容用户习惯，只能把 `pub(crate)` 作为 `pub(package)` 的语法别名，
并在文档中标记为非首选；不能让 `crate` 获得独立语义。

访问上下文由三元组决定：

```text
AccessContext {
  current_package: PackageKey
  current_module: ModuleKey
  current_part: ModulePartKey
}
```

访问判断：

```text
can_access(decl_owner, visibility, access_context):
  visibility == pub          -> true
  visibility == pub(package) -> decl_owner.package == access_context.current_package
  visibility == priv         -> decl_owner.module == access_context.current_module
```

member 的有效可见性必须取 owner 和 member 的 meet：

```text
effective_visibility(owner, member) = min(owner.visibility, member.visibility)
```

因此：

- `pub fn` 放在 `priv struct` 的 `impl` 里，不是 public method surface。
- `pub fn` 放在 `pub(package) struct` 的 `impl` 里，最多是 package-visible method surface。
- `pub field` 所在 struct 如果是 `pub(package)`，该 field 也最多是 package-visible。
- `pub(package)` item 可以引用 `pub(package)` type，但不能把 `priv` type 泄漏给 package 其他 module。
- `pub` item 不能引用 `pub(package)` 或 `priv` type。

导出面需要从单一 public surface 拆成分层 surface：

```text
ModulePublicExports(ModuleKey)     // pub surface，跨 package 稳定
ModulePackageExports(ModuleKey)    // pub + pub(package) surface，同 package 内使用
ItemList(ModuleKey)                // 仍只记录 def identity
ItemSignature(DefKey)              // 记录 item 自身 signature
```

当前实现已经采用分层 query 边界：`ModuleExports(ModuleKey)` 是跨 package public API 的唯一权威，
`ModulePackageExports(ModuleKey)` 是同 package facade 和同包增量失效使用的 package surface。二者
不能合并成“带 visibility rank 的同一个结果”，否则同一 facade 在同包和跨包消费者下会得到不同可见
闭包，污染只按 exporting module 缓存的 public export 查询。

import visibility 也使用同一层级，但语义分开看：

- `priv import`：只给当前 part 的 name resolution 使用。
- `pub(package) import`：同 package 内 re-export，可供 package 内 facade 使用。
- `pub import`：public re-export，进入 public `ModuleExports`。

re-export traversal 的访问规则是按边判断，而不是只看最终目标：

```text
public edge        -> 所有消费者都可穿过
package edge       -> 只有 access.package == exporter.package 时可穿过
private edge       -> 不形成 re-export；只给 exporter 本地解析使用
```

因此，如果当前包 `P` 的 facade `A` 通过 `pub import` 指向外部包 `Q` 的模块 `B`，同包消费者可以
穿过 `A -> B`，但只能看到 `B` 对 `P` 可见的 public surface，不能因为 `A` 是同包 facade 就拿到
`Q` 内部的 `pub(package)` surface。增量查询也按这个约束建边：

```text
ModulePackageExports(A)
  -> ModulePackageExports(B)   // B.package == A.package 且 B 有 package surface
  -> ModuleExports(B)          // B 是外部 package，或 B 没有 package surface
```

Phase 6A 暂不做：

- `pub(super)` / `pub(in path)`：没有 nested module tree，不能绑定到不存在的 topology。
- file-private：`priv` 已经是 module-private；文件私有以后用独立语法设计。
- protected/friend：与 Aurex 当前模块和类型系统不匹配。
- package manager / external dependency resolver：`PackageKey` 先作为编译会话内稳定身份。

Phase 6A-1 实现收口状态（2026-05-26）：

- 已落地内部 `Visibility` 三层级：`private_`、`package_`、`public_`。
- 已提供统一 helper：`visibility_rank`、`visibility_at_least`、`effective_visibility`、
  `visibility_is_public`、`visibility_is_module_private`、`visibility_name`。
- Parser 仍只接受现有 `priv` / `pub`，不会从源码产生 `pub(package)`；现有测试和用户行为保持不变。
- AST / checked dump 已改为通过 `visibility_name` 输出，内部 package visibility 的 dump 稳定为
  `pub(package)`。
- Sema lookup 的 access 判断已经按层级入口收束；`pub(package)` 在 6A-1 仍只是内部占位，
  真正上下文化 access 判断由后续 6A-2 承接。
- Public API surface 检查会把低于 `pub` 的类型视为不可泄漏到 public API；这为后续
  `pub(package)` 不进入跨 package API 提前建立了防线。
- Module loader 记录 import 的原始 visibility；ModuleGraph fingerprint 写入 import visibility rank；
  ModuleExports signature entry 写入 export entry visibility rank。这样后续拆出
  `ModulePackageExports` 时，不需要改变当前 stable semantics。
- 正常测试已覆盖 visibility lattice、package visibility dump、sema access/internal surface helper、
  import visibility 对 ModuleGraph / ModuleExports / ItemList 的增量影响。

Phase 6A-2 实现收口状态（2026-05-26）：

- 已新增 `sema::DeclContext{PackageKey, ModuleKey, ModulePartKey}` 和
  `sema::AccessContext{PackageKey, ModuleKey, ModulePartKey}`。
- 已新增 `sema::VisibilityPolicy`，集中处理 `can_access` 和 `can_expose_type`。
- `SemanticAnalyzerCore::can_access` 已改为只接受 `DeclContext`；旧的 module-id 入口降级为
  `can_access_module` 桥接函数，调用点不再继续复用旧函数名。
- `DeclContext` / `AccessContext` 的 `PackageKey` 来自现有 query key 体系；6A-2 只先完成 policy
  抽象，真实 package 来源由 6A-3 承接。
- `priv` 仍按 module 边界判断，保持当前 module-part 设计；`ModulePartKey` 字段先进入上下文模型，
  但不改变现有 `priv` 跨 part 语义。
- Export surface 检查改为通过 `VisibilityPolicy::can_expose_type` 判断，避免后续在 sema 内继续散落
  visibility rank 比较。
- 正常白盒测试已覆盖 same-module、same-package、cross-package、invalid-current-context、
  package surface 和 public surface 的 policy 行为。

Phase 6A-3 实现收口状态（2026-05-26）：

- 已新增 root package identity 通道：`CompilerInvocation::package_identity` 和 CLI `--package`。
- `ModuleLoader` 已把 root/local-relative imports 与显式 import path 命中的外部 root 区分成不同
  `PackageKey`；同一 physical import root 的 package identity 由 canonical import root 派生。
- `ModuleRecord` 保存 owner package 与 syntax `ModuleId`，`ModuleImportRecord` 保存 imported module
  package；re-export dependency key 不再默认落到空 package。
- `SemanticOptions::module_packages` 按 `ModuleId` 传入 sema；`query_module_key`、泛型 template
  query key 和 access policy 的 package 判断已消费真实 package 表。
- 增量缓存 source rows 写入 source package key，`ModuleGraph` / `ModuleExports` / `ItemList`
  query record 使用 package-aware `ModuleKey`；语义 subject 优先按 syntax `ModuleId` 绑定 package，
  防止不同包内同名 logical module 的 item signature 混用 package；cache header 写入 root package
  identity，防止不同 `--package` 编译复用同一 coarse source cache。
- 已补正常测试覆盖 CLI 解析、stable-id package-aware adapter、ModuleLoader import-path package
  隔离、sema package access 和 package-aware incremental cache module key。

Phase 6A-4 实现收口状态（2026-05-26）：

- Parser 已接受 `pub(package)` contextual visibility scope，不把 `package` 提升为保留关键字。
- 顶层 item、struct field、impl method 和 import 都能从源码产生 `Visibility::package_`，AST /
  checked dump 使用既有 `visibility_name` 稳定输出 `pub(package)`。
- `pub(crate)`、`pub(in path)` 和其他 scoped visibility 仍拒绝，避免把 Rust 的 crate/path visibility
  语义过早复制进 Aurex。
- Sema exported surface 检查已覆盖 `pub(package)`：package-visible surface 不能泄漏 `priv` type；
  public surface 泄漏 package-visible type 时诊断文案会明确写出 `package-visible type`，不再笼统称为
  private type。
- 正常测试已覆盖 parser positive/negative、same-package 访问、cross-package import-path 隔离，以及
  package-visible surface 泄漏诊断。

Phase 6A-5 实现收口状态（2026-05-26）：

- Sema 保留 public-only `module_export_modules` 缓存；qualified lookup、module path lookup、
  suggestions 和 generic selector 改用 access-aware export closure，避免同一 exporting module 的缓存
  在同包/跨包消费者之间串用。
- `pub(package) import` 已具备 package-level re-export 语义：同一 `PackageKey` 内可通过 facade
  访问，被 `-I` import path 切到不同 package 的消费者不可见。
- `priv import` 不进入任何 re-export closure，只作为当前模块本地解析输入。
- 增量缓存新增 `module_package_exports` query kind；`ModulePackageExports(ModuleKey)` 只在模块存在
  package-visible surface 或需要传播同包 package surface 时生成，减少无包级导出的旧代码缓存 churn。
- package exports query 记录 `pub + pub(package)` surface；同包 re-export dependency 指向
  `ModulePackageExports`，外部 package 或无 package surface 的目标指向 public `ModuleExports`。
- public `ModuleExports` fingerprint 和 dependency edge 保持 public-only，不包含 `pub(package) import`。
- 正常测试覆盖 same-package / cross-package package re-export、public re-export、private import 不重导出、
  public cache 与 access-aware lookup 分离，以及 package import visibility 对 package exports query 的
  增量影响。

Phase 6A-6 实现收口状态（2026-05-26）：

- driver 新增 manifest-backed package identity：当 `CompilerInvocation::package_identity` 为空时，
  root input 从所在目录向上查找 `aurex.toml`，读取 `[package] name/version` 形成 root `PackageKey`。
- import root identity 同样优先使用 import root 或其祖先中的有效 `aurex.toml`；没有 manifest 时保持
  旧的 canonical import root 派生策略。
- CLI `--package` 保持最高优先级 override，确保单文件、测试和工具调用不被附近 manifest 隐式改写。
- incremental cache header 改为记录解析后的 root package identity，因此 manifest name/version/root
  变化会阻止旧 cache 复用。
- 正常测试覆盖 root/import manifest package 分离、CLI override 优先级，以及 manifest identity 进入
  incremental cache query key。

Phase 6A-6 之后仍未完成的部分：

- `pub(crate)` 是否作为 `pub(package)` 别名的最终语言决策。
- `pub(in path)` / nested module tree 如果进入语言，需要另起作用域拓扑设计。
- manifest source-root、workspace、dependency graph、lockfile 和版本求解仍未进入 M3.0。

### 7.1 `priv` 跨 part

```aurex
// regex/vm.ax
module regex.vm;

part parser;

priv struct Ast {
    kind: i32;
}
```

```aurex
// regex/vm.parts/parser.ax
module regex.vm part parser;

priv fn parse(pattern: str) -> Ast {
    ...
}
```

`Ast` 和 `parse` 都属于 `ModuleKey(regex.vm)`，所以可以互相访问。

### 7.2 跨 module 访问

```aurex
module regex.api;

import regex.vm as vm;

pub fn compile(pattern: str) -> Regex {
    return vm.parse(pattern); // error: parse is private to module regex.vm
}
```

即使 `regex.api` 和 `regex.vm` 在同一个 package 内，也不能访问 `priv`；需要把 API 明确提升为
`pub(package)` 才能被同包其他 module 使用。

### 7.3 Public API 泄漏 private type

```aurex
module regex.vm;

priv struct Program {
    code: []u8;
}

pub fn compile(pattern: str) -> Program {
    ...
}
```

应该诊断：

```text
public function `compile` exposes private type `Program`
```

这是 `ModuleExports` 和 item signature 检查必须覆盖的语义，不应该等到 backend 才失败。

泄漏检查不能只看函数返回类型。M3.0 需要定义统一的 `ExportedSignatureSurface`：

```text
pub function parameter / return type
pub method receiver / parameter / return type
pub struct field type
pub enum case payload type
pub type alias target
pub const/global type
pub generic signature and bounds
pub re-export target
```

出现在 exported signature surface 中的每个 referenced type / def，都必须至少达到对应 export
visibility。否则 diagnostics 应该落在 public declaration 和被泄漏的 private declaration 上。

## 8. 名字解析

### 8.1 Namespace

M3.0 至少区分：

```text
module namespace
type namespace
value namespace
member namespace
```

trait / impl / associated type 相关 namespace 只作为 query key 预留，不进入 M3.0 语义。

### 8.2 Unqualified lookup

未限定名字 lookup 顺序：

```text
1. local scope
2. generic/type parameter scope
3. current ModuleKey 的 item namespace，跨所有 parts
4. builtin/prelude，如果后续启用
```

imported module 的 item 不进入 unqualified lookup，除非未来显式设计 `use`。

M3.0 不新增 implicit imports / prelude。Python 的 shadowing、Kotlin 的 implicit import 和 star import
经验都说明：默认可见集合越大，unknown name、ambiguous name 和 IDE completion 的解释成本越高。
如果未来引入 prelude，必须作为独立 query-visible import set，而不是硬编码到 name lookup。

### 8.3 Qualified lookup

```aurex
vm.Program
regex.vm.Program
```

规则：

- `vm` 必须是当前 part 的 import alias。
- `regex.vm` 必须通过当前 part 的 import 或可见 re-export 进入 visible module set。
- qualified lookup 先解析 module path / alias，再进入 type/value/member namespace。

### 8.4 Duplicate item

同一 `ModuleKey` 下：

```text
same namespace + same name => duplicate item error
```

跨 part 也一样。

```aurex
// regex/vm.parts/parser.ax
module regex.vm part parser;

priv fn lower() -> i32 { 1 }
```

```aurex
// regex/vm/emitter.ax
module regex.vm part emitter;

priv fn lower() -> i32 { 2 }
```

必须报错，并给两个 source ranges。

## 9. ModuleGraph

M3.0 的 module graph 应该能表达：

```text
ModuleNode {
  key: ModuleKey
  primary_part: ModulePartKey
  parts: [ModulePartKey]
  imports: [ImportEdge]
  reexports: [ReExportEdge]
}

PartEdge {
  owner: ModuleKey
  part: ModulePartKey
  declared_at: SourceRange
}

ImportEdge {
  from_part: ModulePartKey
  to_module: ModuleKey
  alias: Ident
  visibility: priv | pub
  declared_at: SourceRange
}

ReExportEdge {
  from_module: ModuleKey
  to_module_or_def: ModuleKey | DefKey
  alias: optional Ident
  declared_at: SourceRange
}
```

Graph invariants：

- 一个 `ModuleKey` 只能有一个 primary part。
- 同一 `ModuleKey` 下 part name 唯一。
- named part 必须被 primary part list 声明。
- primary part list 中每个 part 必须解析到一个文件。
- part file 的 `module path part name;` 必须和 primary declaration 匹配。
- part file 不能被 import 成 module。
- import graph cycle 在 M3.0 直接拒绝。这是保守策略；后续版本可以再区分 signature
  dependency cycle 和 body dependency cycle。
- part membership 没有搜索 cycle，因为 membership 来自 explicit part list。
- part order 不影响语义。

module loading 不执行 top-level code。part 顺序不影响语义的前提是：

- top-level item collection 使用 deterministic ordering。
- const/global initializer dependency graph 独立分析。
- const/global cycle diagnostics 不依赖 part load order。
- 任何 future top-level side effect 都必须作为独立语言专题设计，不能借 module loading 顺手引入。

Phase 5 当前实现把 `ModuleGraph(ModuleKey)` 固定为逻辑模块级 query record。fingerprint
只混入当前 module 的结构化身份：primary path、显式 named parts、part-local import edges、import alias
和 `pub` / `priv` import visibility。它不混入 source text，也不把其他 module 的 graph 塞进当前
module 的结果；part list 重排在 part 集合不变时保持 green，新增 / 删除 part 才让对应
`ModuleGraph` red。

## 10. ModuleExports

`ModuleExports(ModuleKey)` 是 module public surface 的权威结果。

至少包含：

```text
public item defs
public type defs
public value defs
public methods / fields
pub import reexports
future pub use reexports
visibility leakage diagnostics
export fingerprint
```

M3.0 第一阶段继续支持 `pub import`，把它转成 module-level re-export edge。

后续 `pub use` 进入时，`ModuleExports` 必须能表示 selective re-export：

```text
exported_name -> original DefKey
```

删除 re-export 是 breaking API change；修改 re-export 必须让 exports fingerprint 变化。

`ModuleExports` 的 fingerprint 必须包含：

- exported def stable keys。
- module re-export edges。
- exported aliases。
- exported signature surface 中引用的 stable keys。
- visibility leakage diagnostics 的结构化结果。

不能只 hash declaration 文本或 source file path；否则移动 item、重排 parts 或格式化代码会造成错误失效。

Phase 5 当前实现的 `ModuleExports(ModuleKey)` 以 public API surface 为结果边界：

- public functions / methods，且 method owner 必须是 public surface。
- public structs、public struct fields、public enum cases 和 payload type surface。
- public type aliases、generic templates 和 top-level public const。
- primary 文件中的 `pub import` 作为 module re-export entry。
- part-local `pub import` 只影响当前 part name resolution，不进入 module-level re-export。

`ModuleExports(A)` 对 primary-level re-export 的目标 module 记录 `ModuleExports(B)` dependency edge；
provider 会排序、去重并过滤 self dependency。这样 re-exported module 的 public API 改变可以通过
query graph 传播，而普通 import 或 part-local public import 不会错误扩大失效面。

## 11. Query 边界和失效

M3.0 模块相关 query 目标：

```text
FileContent(FileKey)
LexFile(FileKey)
ParseFile(FileKey)
ModuleGraph(ModuleKey)
ModulePart(ModulePartKey)  // 后续独立 part-level query 入口
ItemList(ModuleKey)
ItemSignature(DefKey)
ModuleExports(ModuleKey)
TypeCheckBody(BodyKey)
Diagnostics(QueryKey)
LowerFunctionIR(BodyKey)
```

推荐失效规则：

```text
只改 private function body
  -> TypeCheckBody / LowerFunctionIR red
  -> ModuleExports green

改 private function signature
  -> ItemSignature red
  -> 同 ModuleKey 内依赖 body red
  -> ItemList green
  -> ModuleExports 通常 green

改 public function signature
  -> ItemSignature red
  -> ItemList green
  -> ModuleExports red
  -> dependent modules red

新增 / 删除 part
  -> ModuleGraph red
  -> ItemList red
  -> affected ModuleExports red

移动 item 到同 ModuleKey 的另一个 part
  -> source location / debug info red
  -> DefKey 和 public ABI green，除非 signature 变化
```

## 12. Diagnostics 清单

M3.0 必须覆盖：

- duplicate primary module file。
- duplicate module part name。
- part declared with wrong module path。
- part declared with wrong part name。
- primary lists missing / unresolved part。
- part declaration appears after import or item。
- part file declares a nested part list。
- part name is not a valid simple identifier。
- part file used as artifact-producing root input。
- check / frontend inspection starts from part file and resolves owning primary。
- part file imported as module。
- import path resolved to part file。
- import path resolves to multiple candidate module files。
- importable module path collides with module part sidecar path。
- module / part path differs only by filesystem case。
- import cycle。
- duplicate item across parts。
- private item accessed from another module。
- exported signature surface leaks private type。
- public re-export points to invisible item。
- ambiguous re-export。
- import alias collision。
- unknown import alias with suggestion。

诊断必须尽量落到 declaration source range，而不是只报文件路径字符串。

Parser / module declaration recovery 也属于 M3.0 用户体验边界：

- `module path part ;` 缺少 part name 时，同步到 semicolon 后继续解析。
- `part ;` 缺少 part name 时，同步到 semicolon 后继续解析。
- `part name;` 出现在非法位置时，报错但继续解析后续 item。
- module declaration 错误不应拖垮整文件 AST；AST 可以保留 invalid module / part declaration
  marker，供 IDE 和 diagnostics 使用。

Loader diagnostics 需要区分 CLI root 和 tooling buffer：

- CLI root 是 part file 且 emit kind 只做检查 / frontend inspection 时，反查 owning primary，
  检查完整 module，并添加 note 说明 part 属于哪个 module。
- CLI root 是 part file 且 emit kind 会生成 IR / LLVM IR / native artifact 时，报错并指出
  owning primary module；不能把 part 当作独立 artifact identity。
- tooling buffer 是 part file 时，可以进入 degraded graph recovery，但不能把 part 当独立 module。

用户最常见的误用需要有面向行动的诊断文案：

```text
part `parser` is declared by module `regex.vm`, but `regex/vm.parts/parser.ax` was not found
note: create `regex/vm.parts/parser.ax` or remove `part parser;` from `regex/vm.ax`

module part file `regex/vm.parts/parser.ax` is not listed by primary module `regex.vm`
note: add `part parser;` before imports in `regex/vm.ax`

unknown import alias `ast`
note: imports are part-local; add `import regex.ast as ast;` to this part

cannot emit LLVM IR from module part `parser`
note: compile owning primary module `regex.vm` instead
```

## 13. Lowering / Backend / ABI

M3.0 不改变 IR 和 LLVM backend 的核心语义，但要固定 identity 规则：

```text
backend symbol identity = ModuleKey + DefKey + generic instance identity
debug/source identity   = ModulePartKey + SourceRange
```

因此：

- 同 module 内移动 function 不应改变 public ABI symbol。
- 改 module path 会改变 `ModuleKey`，因此是 API identity change。
- 改 part 文件路径只影响 source/debug/incremental metadata，不应改变 public API。
- lowering 必须消费 checked module / checked body 的 logical owner，不直接从 file path 推导 owner。

## 14. 被拒绝的方案

### 14.1 隐式目录扫描

拒绝：

```text
扫描 module 目录下所有 .ax，自动加入 module。
```

原因：

- 语义随文件系统偶然内容变化。
- IDE / build cache / query invalidation 难以稳定。
- 临时文件、生成文件、未提交文件容易污染 module graph。

M3.0 用 primary-sidecar part lookup 代替目录扫描：

```text
primary path/to/vm.ax + part parser -> path/to/vm.parts/parser.ax
```

这条 sidecar layout 也拒绝把 part 文件和 importable dotted module 文件混在同一目录层级。

### 14.2 Python 式 runtime import

拒绝运行时加载、副作用执行和动态 path 规则。Aurex module graph 必须在 frontend 阶段确定。

module loading 永远不执行顶层代码。任何顶层 initializer 都是 sema / lowering 的依赖分析对象，
不是 import side effect。

### 14.3 Rust 式 nested module tree

暂缓 `mod` tree、`pub(super)`、`pub(in path)`。等 Aurex 确认是否需要 nested namespace 后再作为独立专题设计。

### 14.4 C++ interface / implementation 强制分离

暂缓强制 interface file / implementation file。M3.0 的 public API 由 `pub`、`pub import` 和
`ModuleExports` 决定，不要求用户先维护一套 header-like interface 文件。

### 14.5 Kotlin package-only

只靠 package declaration 不足以表达 Aurex 的 query identity、part membership 和 cross-part private
semantics。Aurex 需要 explicit `part` 作为 language-level membership。

## 15. 实现阶段

### Phase 0：文档定稿

交付：

- 本文档。
- 更新 M3 路线图。
- 更新旧 V2 草案，使其成为背景材料而不是待决策入口。

### Phase 1：Parser / AST

交付：

- `module path part name;` 解析。
- primary 文件 `part name;` 解析。
- primary 文件强制 `module -> parts -> imports -> items` 顺序。
- part 文件强制 `module part -> imports -> items` 顺序。
- `part` 作为上下文关键字，不破坏普通 identifier。
- AST 增加 module part declaration shape。
- AST dump 覆盖 primary / part。
- module / part declaration recovery。
- parser negative tests。

### Phase 2：ModuleLoader graph

交付：

- `LoadedLogicalModule` / `LoadedModulePart` 内部结构。
- 同一 `ModuleKey` 下多个 `ModulePartKey`。
- primary part list 驱动 part loading。
- primary-sidecar part lookup。
- import resolution ambiguous candidate diagnostics。
- duplicate primary / duplicate part / mismatch diagnostics。
- part root check / inspection owning-primary discovery。
- part artifact root rejection 和 import-to-part diagnostics。
- canonical import candidate de-duplication。
- case-fold path collision diagnostics。
- 保留 combined AST 兼容输出。

### Phase 3：Sema item merge

交付：

- 按 `ModuleKey` 汇总 item list。
- 跨 part duplicate item diagnostics。
- current module item lookup 跨 parts。
- part-local imports 不污染其他 parts。
- resolved signatures 跨 parts 共享，但 import aliases 不跨 parts 传播。

### Phase 4：Visibility

交付：

- `priv` 以 `ModuleKey` 为边界。
- exported signature surface 泄漏 private type diagnostics。
- `pub import` 继续作为 re-export。

### Phase 5：Query records

交付：

- `ModuleGraph(ModuleKey)` query 结果结构化，基于当前 logical module 的 primary / parts / imports。
- `ModuleExports(ModuleKey)` query 结果结构化，基于 public API surface 和 primary re-export edges。
- `ModulePackageExports(ModuleKey)` query 结果结构化，基于 package-visible API surface 和 primary
  `pub(package)` / same-package re-export edges。
- `ItemList(ModuleKey)` 只记录稳定 def identity，不混入签名 fingerprint。
- `ModuleExports -> ModuleExports` query edge 用于 primary-level `pub import` re-export dependency。
- `ModulePackageExports -> ModulePackageExports` / `ModulePackageExports -> ModuleExports` query edge 用于
  package-level re-export dependency。
- `DefKey` 不依赖 traversal order。
- query fingerprint 和 red/green tests。

当前实现状态：

- private body edit 保持 `ItemList` 和 `ModuleExports` green，只打红 body/type-check 相关 query。
- private signature edit 保持 `ItemList` 和 `ModuleExports` green，但对应 `ItemSignature` red。
- public signature edit 让 `ModuleExports` red，`ItemList` green。
- part list 重排保持 `ModuleGraph` green；新增 part 让 `ModuleGraph` red。
- primary `pub import` 生成 `ModuleExports -> ModuleExports` edge；part-local `pub import` 不生成该 edge。
- primary `pub(package) import` 生成 package exports query surface；part-local `pub(package) import` 不生成
  re-export edge。

### Phase 6：M3.0-late / M3.2 候选

候选：

- `pub(crate)` alias 是否保留，以及 `pub(in path)` 是否进入语言。
- `pub use` / selective re-export。
- non-conventional part path，例如 `part parser from "parts/parser.ax";`。

不进入第一阶段：

- glob import。
- package manager。
- external dependency resolver。
- trait / protocol / dynamic dispatch。
- RAII / Drop / borrow checker。

## 16. 测试计划

Parser:

- `module a.b;`
- `module a.b part c;`
- primary `part c;`
- missing part name。
- non-identifier part name。
- `part` 出现在非法位置。
- `part` 出现在 import 之后或 item 之后。
- part 文件里再次声明 `part name;`。
- module / part declaration recovery 后仍解析后续 item。

Loader:

- primary + one part 成功。
- primary + multiple parts 成功。
- part lookup 使用 primary `.parts` sidecar directory。
- `regex/vm/parser.ax` 仍可作为 importable module `regex.vm.parser`，不被 `regex.vm` 的 part 抢占。
- duplicate part name 拒绝。
- missing part file 拒绝。
- part declares wrong module path 拒绝。
- part declares wrong part name 拒绝。
- import points to part file 拒绝。
- import path resolves to multiple candidates 拒绝。
- same canonical import file through multiple search roots is not ambiguous。
- `--check` / frontend inspection 从 part file 启动时检查 owning module。
- codegen / native artifact 从 part file 启动时拒绝，并提示 owning primary。
- module / part path case-fold collision 给出 portability diagnostic。
- tooling buffer part recovery 不把 part 当独立 module。

Sema:

- same module part 可访问 `priv` type / fn / method / field。
- sibling module 不可访问 `priv`。
- duplicate item across parts。
- part-local import alias 只在当前 part 可见。
- exported signature surface leaks private type。
- public field / enum case / type alias / generic signature 泄漏 private type。
- resolved signature 跨 parts 可用但 alias 不跨 parts 传播。
- const/global dependency cycle diagnostics 不依赖 part order。

Query:

- private body edit 不改变 `ModuleExports` fingerprint。
- public signature edit 改变 `ModuleExports` fingerprint。
- part list edit 改变 `ModuleGraph` fingerprint。
- item moved between parts keeps stable `DefKey` when logical identity unchanged。
- reordering part list 不改变不依赖 order 的 semantic results；只允许 deterministic dump / source metadata 变化。
- duplicate top-level def 不使用 traversal-order disambiguator 静默改名。
- adding an unrelated file under an importable module directory does not change module graph unless explicitly imported or listed as part。

Integration:

- compile primary-only module 不回退。
- compile primary + parts 输出行为一致。
- existing import / `pub import` tests 全部通过。

## 17. 第二轮审视结论

当前设计主干保持不变：

```text
显式 primary module 文件
显式 part list
part 文件自声明
ModuleKey / ModulePartKey 分离
ModuleGraph / ModuleExports query 化
```

第二轮审视后，已经收敛的关键风险：

- part lookup 已从“约定查找”收紧为 primary-sidecar 固定规则。
- CLI root compile 和 IDE/tooling buffer recovery 已分开。
- part-local import 和跨 part resolved signature sharing 已分开。
- `pub import` 和未来 `pub use` 的 re-export 语义已分开。
- `priv` 已明确为 module-private，不是 file-private。
- public leakage 已扩展成 exported signature surface 检查。
- `DefKey` 已明确不能依赖 traversal order。
- module import cycle 在 M3.0 明确采用保守拒绝策略。
- top-level code 已明确不会因 module loading 执行。
- `PackageKey` 已明确是 M3.0 临时 package grouping，不承诺 manifest 级身份。
- parser / module declaration recovery 已纳入第一阶段交付。

仍需在实现阶段重点盯住的风险：

- combined AST 兼容路径不能重新把 file path 当作 module identity。
- `stable_index` 只能影响 deterministic metadata，不能进入 semantic lookup。
- current sema lookup cache 如果仍按 `syntax::ModuleId` 工作，需要确保一个 logical module 的多个 parts
  不被误认为多个可见 module。
- `ModuleExports` fingerprint 必须基于 stable keys 和 exported signature surface，而不是 source text。
- IDE degraded graph recovery 不能悄悄把 part 当作独立 module，也不能放宽 artifact-producing emit 的
  root identity 规则。
- `pub(package)` 已在 6A-4 作为独立阶段进入；`pub use`、custom `part from` 和更复杂 scoped
  visibility 仍必须后置，不能混入 M3.0 第一阶段。

进入实现前的结论：可以开始 Phase 1 Parser / AST，但每个阶段都必须带对应 negative tests；
尤其是 part lookup、part root check discovery、part artifact root rejection、import-to-part、
part-local alias、duplicate item、exported surface leakage 和 traversal-order stability 不能后补。

## 18. 第三轮缺陷驱动审视

第三轮审视专门参考各语言模块系统的历史缺陷，而不是只参考它们的能力：

- Python 的绝对/相对 import 迁移说明了 local module shadowing 会让 `import foo` 变得不可预测。
- Rust 2018 的 path / module 调整说明了文件布局规则和 logical path 混在一起后，迁移成本会很高。
- Kotlin 的 file-local imports 和 star imports 说明了 alias / implicit visibility 如果跨文件传播，会放大歧义。
- Java JPMS 的 reliable configuration 说明了同一 logical package/module 由多个来源共同定义会破坏可预测性。
- C++ modules 的 partition / BMI / ODR 经验说明了 module part、importable unit、build artifact 和 ABI identity
  如果没有明确边界，会在构建系统和诊断上变得很脆。
- Go 的 package initialization 经验说明了顶层初始化顺序必须由 dependency graph 决定，不能借文件顺序或
  loader 顺序偷懒。

第三轮后新增或修正的约束：

- 默认 part 物理布局从 `path/to/vm/parser.ax` 改为 `path/to/vm.parts/parser.ax`，避免和
  importable module `regex.vm.parser` 的 `regex/vm/parser.ax` 冲突。
- `part` 是上下文关键字，不是全局保留字，避免因为模块系统落地破坏既有普通 identifier。
- import resolution 必须产生唯一 module file candidate；多个 `-I` 或 importer-relative 路径命中不同文件时
  直接报 ambiguous import。
- module part sidecar 目录不参与普通 import search。
- M3.0 不新增 implicit imports / prelude；未来如果引入，必须作为 query-visible import set。

第三轮后的实现结论：

```text
可以进入 Phase 1 Parser / AST；
但 Loader 阶段必须优先实现 sidecar layout 和 ambiguous import diagnostics，
否则会复制 Python shadowing / Java split-package / Rust path migration 的问题。
```

## 19. 第四轮使用者视角审视

第四轮审视从 Aurex 使用者的日常动作出发：新建模块、拆文件、在编辑器里保存当前文件、运行
`aurexc --check`、读错误信息、迁移目录、在不同操作系统协作。结论是：M3.0 的核心语义仍应保持
显式和静态，但 CLI / diagnostics / tooling 必须让这些规则可发现、可恢复、可迁移。

### 19.1 当前设计对使用者友好的部分

- `module regex.vm;` 和 dotted path 容易阅读，也和现有 import 习惯一致。
- `part parser;` 明确告诉读者一个大 module 被拆成哪些实现文件，不需要猜目录里哪些文件属于模块。
- `priv` 是 module-private，移动 helper 到另一个 part 后不需要扩大 visibility。
- imports 是 part-local，读单个文件时能看见该文件依赖了哪些外部 module。
- `.parts` sidecar 把 implementation part 和 importable module 分开，避免 `regex.vm.parser`
  到底是 nested module 还是 implementation fragment 的歧义。
- `DefKey` 不依赖文件路径，使“整理代码、移动函数到另一个 part”不会无意义地制造 ABI / cache churn。

### 19.2 使用者最可能踩的坑

1. 用户会在编辑器或终端里对当前 part 文件运行 `aurexc --check path/to/vm.parts/parser.ax`。
   如果一律报错，体验会比现有单文件编译差；如果把 part 当独立 module 编译，则破坏语义。
   因此 M3.0 采用中间策略：检查类和 frontend inspection 可以从 part 反查 owning primary，
   但 artifact-producing emit 必须要求 primary。
2. `.parts` 目录不是大多数语言的常见写法。它解决了路径冲突，但会降低初学者可发现性。
   所以 missing / unlisted part diagnostics 必须直接说出要创建哪个文件、要在 primary 哪个位置添加
   `part name;`。
3. imports 是 part-local 会让从 Kotlin / Python 迁移来的用户困惑：他们可能以为 primary 的 import
   对所有 parts 生效。诊断需要在 unknown alias 时主动提示“imports are part-local”。
4. `priv` 不是 file-private。拆文件后 helper 对同 module 所有 parts 可见，这是有意设计。
   未来如果用户需要文件私有 helper，应作为 `file priv` 或类似能力单独设计，不能让 M3.0 的 `priv`
   在不同上下文里变成双重含义。
5. part list 显式带来 boilerplate。M3.0 接受这个成本，因为它换来 deterministic module graph；
   但 tooling 必须提供 quick fix / scaffold，否则大模块拆分会显得笨重。
6. 大小写敏感语义和大小写不敏感文件系统会让协作变脆。M3.0 必须诊断只靠大小写区分的 module / part
   路径冲突，避免 macOS / Windows / Linux 上结果不同。

### 19.3 CLI 行为收口

M3.0 把 CLI root input 分成三类：

```text
source-local inspection:
  --emit=tokens
  --emit=lossless

module check / frontend inspection:
  --check
  --emit=ast
  --emit=modules
  --dump-checked / --emit=checked
  --emit=typed

artifact-producing emit:
  --emit=ir
  --emit=llvm-ir
  --emit=asm
  --emit=obj
  --emit=exe / default executable
```

规则：

- source-local inspection 可以直接读取 part 文件，不建立 module identity。
- module check / frontend inspection 如果输入是 part 文件，必须反查 owning primary，并对完整 module graph
  运行；输出里仍保留 part source ranges。
- artifact-producing emit 如果输入是 part 文件，必须拒绝，并提示 owning primary。这样避免用户误以为
  `parser.ax` 会生成一个独立 module artifact。

这个规则参考了现代 IDE / compiler 的实际分层：编辑器需要当前文件检查能力，build 系统需要稳定的
module artifact identity，两者不能混成一条隐式语义。

### 19.4 Tooling 和 IDE 必须承担的体验责任

M3.0 的语言核心不做隐式目录扫描，但 IDE 可以在不改变语义的前提下提供辅助：

- 新建 part scaffold：在 primary 中插入 `part name;`，并创建 `.parts/name.ax`。
- 对未列出的 `.parts/name.ax` 提供 quick fix：添加 `part name;`。
- 对 missing part 提供 quick fix：创建期望路径，或删除 primary 中的 part declaration。
- 对 unknown alias 提供 quick fix：在当前 part 顶部添加 import，而不是加到 primary 后假装全局生效。
- 对 `priv` 被其他 module 访问的错误，提示“same package is not enough in M3.0”，避免用户误解
  package-private 已存在。
- 对大小写冲突给出跨平台说明，避免把它当成普通 duplicate file error。

这些属于 tooling contract，不是语义放宽。实现时可以先做高质量 diagnostics，再逐步加 IDE quick fix。

### 19.5 仍然拒绝的“更省事”方案

从使用者角度看，以下方案短期更省事，但长期会伤害编译器和大型项目：

- 隐式扫描 `.parts` 目录：新文件一保存就改变 module graph，临时文件和生成文件会污染语义。
- primary import 自动传播到所有 parts：读单个 part 时依赖不完整，name lookup cache 和 diagnostics 也更脆。
- part 文件既能作为 module part 又能作为 importable module：会制造 `regex.vm.parser` 的双重身份。
- artifact emit 从 part root 自动推断输出 module：build artifact identity 不清晰，也会让增量缓存和链接行为不稳定。
- 默认 prelude / glob import：更省输入，但会增加 unknown name 和 ambiguous name 的解释成本。

### 19.6 第四轮后的设计结论

本轮不改变 M3.0 的主线：

```text
显式 primary
显式 part list
part 自声明
ModuleKey / ModulePartKey 分离
ModuleGraph / ModuleExports query 化
```

但新增三条使用者视角的硬约束：

- `--check` 和 frontend inspection 必须能从 part 文件反查 owning primary，不能只给生硬拒绝。
- `.parts` / part-local imports / `priv` module-private / case-fold collision 都必须有行动导向的 diagnostics。
- tooling 可以提升体验，但不能通过隐式扫描、隐式 import 传播或 part-as-module 放宽语言语义。

进入实现时，Phase 1 / Phase 2 不能只做 AST 和 loader happy path；必须把这些用户路径作为第一批
negative / integration tests。否则 M3.0 会在内部结构上现代化，但在使用体验上显得割裂。

## 20. 参考资料

- Python import system: <https://docs.python.org/3/reference/import.html>
- Python modules tutorial: <https://docs.python.org/3/tutorial/modules.html>
- Python PEP 328 absolute / relative imports: <https://peps.python.org/pep-0328/>
- Rust Reference modules: <https://doc.rust-lang.org/reference/items/modules.html>
- Rust Reference visibility: <https://doc.rust-lang.org/reference/visibility-and-privacy.html>
- Rust 2018 path changes: <https://doc.rust-lang.org/edition-guide/rust-2018/path-changes.html>
- Kotlin packages and imports: <https://kotlinlang.org/spec/packages-and-imports.html>
- Kotlin visibility modifiers: <https://kotlinlang.org/docs/visibility-modifiers.html>
- Clang Standard C++ Modules: <https://clang.llvm.org/docs/StandardCPlusPlusModules.html>
- Clang Modules: <https://clang.llvm.org/docs/Modules.html>
- MSVC module partitions: <https://learn.microsoft.com/en-us/cpp/cpp/module-partitions>
- OpenJDK Project Jigsaw state of the module system: <https://openjdk.org/projects/jigsaw/spec/sotms/>
- Go specification package initialization: <https://go.dev/ref/spec#Package_initialization>

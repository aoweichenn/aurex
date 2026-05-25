# Aurex M3 模块系统使用案例

状态：M3.0 设计样例，供评估语言体验和实现验收，不代表当前编译器已经支持全部语法。  
日期：2026-05-25

## 1. 案例目标

这个案例用一个小型 `regex.vm` 模块评估 M3 模块系统的实际使用体验：

- 一个 logical module 拆成多个 source-file parts。
- `priv` 在同一个 `ModuleKey` 内跨 parts 可见。
- imports 是 part-local，不从 primary 自动传播到 parts。
- `.parts` sidecar 和 importable dotted module 不冲突。
- `--check` 可以从 part 文件反查 owning primary。
- IR / LLVM IR / native artifact 不能从 part root 隐式生成。
- 常见错误能给出可操作 diagnostics。

## 2. 文件布局

```text
src/
  app.ax
  regex/
    ast.ax
    vm.ax
    vm.parts/
      parser.ax
      emitter.ax
      optimizer.ax
    vm/
      parser.ax
```

含义：

```text
src/regex/vm.ax              -> importable module regex.vm
src/regex/vm.parts/parser.ax -> part parser of module regex.vm
src/regex/vm.parts/emitter.ax
src/regex/vm.parts/optimizer.ax
src/regex/vm/parser.ax       -> importable module regex.vm.parser
```

这里故意同时放了 `regex.vm` 的 `parser` part 和 importable module `regex.vm.parser`，用来验证
`.parts` sidecar 是否真正消除了路径歧义。

## 3. Source Files

### 3.1 `src/regex/ast.ax`

```aurex
module regex.ast;

pub struct Ast {
    kind: i32;
}

pub fn empty() -> Ast {
    return Ast { kind: 0 };
}
```

### 3.2 `src/regex/vm.ax`

```aurex
module regex.vm;

part parser;
part emitter;
part optimizer;

import regex.ast as ast;

pub struct Regex {
    program: Program;
}

priv struct Program {
    instruction_count: i32;
}

pub fn compile(pattern: str) -> Regex {
    let tree = parse(pattern);
    let raw = emit(tree);
    let optimized = optimize(raw);
    return Regex { program: optimized };
}

priv fn empty_program() -> Program {
    return Program { instruction_count: 0 };
}

priv fn empty_ast() -> ast.Ast {
    return ast.empty();
}
```

评估点：

- `part parser;` / `part emitter;` / `part optimizer;` 是唯一 membership 来源。
- `Program` 是 `priv`，但 `parser`、`emitter`、`optimizer` 三个 part 都能访问。
- primary 里的 `import regex.ast as ast;` 只对 primary 文件本身生效，不自动传播到 part。

### 3.3 `src/regex/vm.parts/parser.ax`

```aurex
module regex.vm part parser;

import regex.ast as ast;

priv fn parse(pattern: str) -> ast.Ast {
    if pattern == "" {
        return empty_ast();
    }
    return ast.empty();
}
```

评估点：

- 这个文件属于 `ModuleKey(regex.vm)`，不是 `ModuleKey(regex.vm.parser)`。
- `parse` 是 `priv`，但 primary 的 `compile` 可以直接调用，不需要 import。
- 这里必须自己写 `import regex.ast as ast;`，因为 imports 是 part-local。
- `empty_ast()` 定义在 primary 文件里，作为同 module item 可以直接 unqualified lookup。

### 3.4 `src/regex/vm.parts/emitter.ax`

```aurex
module regex.vm part emitter;

import regex.ast as ast;

priv fn emit(tree: ast.Ast) -> Program {
    return Program { instruction_count: tree.kind };
}
```

评估点：

- `Program` 是 primary 里的 `priv struct`，同 module part 可见。
- `ast.Ast` 依赖当前 part 自己的 import alias；不能依赖 primary 的 import。

### 3.5 `src/regex/vm.parts/optimizer.ax`

```aurex
module regex.vm part optimizer;

priv fn optimize(program: Program) -> Program {
    if program.instruction_count == 0 {
        return empty_program();
    }
    return program;
}
```

评估点：

- 不需要 import `regex.vm`；同 module 的 top-level item 跨 parts 可见。
- `empty_program()` 来自 primary，`Program` 也是 module-private item。

### 3.6 `src/regex/vm/parser.ax`

```aurex
module regex.vm.parser;

pub fn debug_name() -> str {
    return "regex.vm.parser";
}
```

评估点：

- 这是 importable module `regex.vm.parser`。
- 它不是 `regex.vm` 的 `parser` part。
- 它和 `src/regex/vm.parts/parser.ax` 可以同时存在，且身份完全不同。

### 3.7 `src/app.ax`

```aurex
module app;

import regex.vm as vm;
import regex.vm.parser as parser_debug;

pub fn main() -> i32 {
    let compiled = vm.compile("a*");
    let name = parser_debug.debug_name();
    return 0;
}
```

评估点：

- `import regex.vm as vm;` 导入的是 primary module `src/regex/vm.ax`。
- `import regex.vm.parser as parser_debug;` 导入的是 `src/regex/vm/parser.ax`。
- 外部 module 不能访问 `vm.parse`、`vm.emit`、`vm.Program`，因为它们是 `priv`。

## 4. 预期 CLI 行为

### 4.1 编译入口是 primary

```bash
aurexc --check src/regex/vm.ax
```

预期：

```text
ok
```

loader 行为：

```text
load primary: src/regex/vm.ax
read part list: parser, emitter, optimizer
load parts:
  src/regex/vm.parts/parser.ax
  src/regex/vm.parts/emitter.ax
  src/regex/vm.parts/optimizer.ax
build ModuleGraph(ModuleKey(regex.vm))
run sema on merged ItemList(regex.vm)
```

### 4.2 `--check` 从 part 文件启动

```bash
aurexc --check src/regex/vm.parts/parser.ax
```

预期：

```text
note: `src/regex/vm.parts/parser.ax` is part `parser` of module `regex.vm`
note: checking owning primary module `src/regex/vm.ax`
ok
```

这个行为是为了适配编辑器和终端里的“检查当前文件”。关键约束是：编译器检查的是完整
`regex.vm` module graph，不是把 `parser.ax` 当独立 module。

### 4.3 Artifact 输出从 part 文件启动

```bash
aurexc --emit=llvm-ir src/regex/vm.parts/parser.ax
```

预期：

```text
error: cannot emit LLVM IR from module part `parser`
note: part `parser` belongs to module `regex.vm`
note: compile owning primary module `src/regex/vm.ax` instead
```

原因：IR / LLVM IR / object / executable 需要稳定 artifact identity。`parser` 是 implementation
fragment，不是独立 module artifact。

### 4.4 Importable module 不被 `.parts` 抢占

```bash
aurexc --check src/app.ax
```

预期：

```text
ok
```

解析：

```text
import regex.vm as vm
  -> src/regex/vm.ax

import regex.vm.parser as parser_debug
  -> src/regex/vm/parser.ax
  -> not src/regex/vm.parts/parser.ax
```

## 5. 预期错误案例

### 5.1 Missing Part File

如果 `src/regex/vm.ax` 里声明：

```aurex
part optimizer;
```

但文件不存在：

```text
src/regex/vm.parts/optimizer.ax
```

预期诊断：

```text
error: part `optimizer` is declared by module `regex.vm`, but `src/regex/vm.parts/optimizer.ax` was not found
note: create `src/regex/vm.parts/optimizer.ax` or remove `part optimizer;` from `src/regex/vm.ax`
```

### 5.2 Unlisted Part File

如果存在文件：

```text
src/regex/vm.parts/scanner.ax
```

内容是：

```aurex
module regex.vm part scanner;
```

但 primary 没有：

```aurex
part scanner;
```

当用户检查该 part：

```bash
aurexc --check src/regex/vm.parts/scanner.ax
```

预期诊断：

```text
error: module part file `src/regex/vm.parts/scanner.ax` is not listed by primary module `regex.vm`
note: add `part scanner;` before imports in `src/regex/vm.ax`
```

### 5.3 Part-Local Import 误用

如果 `emitter.ax` 忘记写：

```aurex
import regex.ast as ast;
```

但仍使用：

```aurex
priv fn emit(tree: ast.Ast) -> Program {
    return Program { instruction_count: tree.kind };
}
```

预期诊断：

```text
error: unknown import alias `ast`
note: imports are part-local; add `import regex.ast as ast;` to this part
```

### 5.4 Private Type 泄漏

如果 primary 暴露：

```aurex
pub fn compile_raw(pattern: str) -> Program {
    return emit(parse(pattern));
}
```

预期诊断：

```text
error: public function `compile_raw` exposes private type `Program`
note: `Program` is private to module `regex.vm`
```

### 5.5 外部访问 Module-Private Item

如果 `src/app.ax` 写：

```aurex
let tree = vm.parse("a*");
```

预期诊断：

```text
error: private function `parse` is not visible from module `app`
note: `parse` is private to module `regex.vm`
```

### 5.6 大小写冲突

如果同一个 module 同时声明：

```aurex
part parser;
part Parser;
```

或者文件系统里同时出现：

```text
src/regex/vm.parts/parser.ax
src/regex/vm.parts/Parser.ax
```

预期诊断：

```text
error: module parts `parser` and `Parser` differ only by filesystem case
note: this can produce different module graphs on case-insensitive filesystems
```

## 6. 这个案例检验的设计问题

如果这个案例读起来合理，说明当前设计的几个核心判断基本站得住：

- 显式 part list 虽然多一行 boilerplate，但 module membership 可读、可诊断、可缓存。
- `.parts` sidecar 牺牲一点目录直觉，换来了 importable module 和 implementation part 的身份隔离。
- part-local imports 增加一点重复，但单文件依赖清晰，IDE completion 和 diagnostics 更稳定。
- `priv` 作为 module-private 适合大 module 拆文件，后续如果需要 file-private，应单独加新修饰符。
- CLI 分层能同时照顾编辑器当前文件检查和构建系统 artifact identity。

如果这个案例让用户感觉最别扭的地方是 `.parts` 名称或显式 part list，那么后续优先应做 tooling：

- `aurexc new-part regex.vm parser`
- IDE quick fix：创建 missing part file。
- IDE quick fix：把 unlisted part 加到 primary part list。
- IDE quick fix：给当前 part 添加缺失 import。

不建议为了减少样板而改成隐式目录扫描或 import 自动传播，因为那会破坏 M3.0 最核心的
deterministic module graph。

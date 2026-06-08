# 使用文档

本文描述当前 **M11b Principal-Set Composition Query Prototype Gate** 下已经落地的用法。标准库仍保持冻结并移除，所有示例和测试都应围绕语言语法、语义、IR、后端、static trait、borrowed dyn trait 和 borrowed dyn supertrait upcast 表面本身展开。M11a/M11b 新增的是 principal-set borrowed dyn composition 的 design/query facts，不新增用户可写 `dyn A + B` 语法。

## 构建

```sh
cmake -S . -B build/full-llvm
cmake --build build/full-llvm -j
```

## 运行 hello

```sh
build/full-llvm/bin/aurexc examples/hello.ax -o build/tests/hello
build/tests/hello
```

期望输出：

```text
hello from Aurex M2
```

## 输出模式

```sh
build/full-llvm/bin/aurexc --emit=tokens examples/hello.ax
build/full-llvm/bin/aurexc --emit=lossless examples/hello.ax
build/full-llvm/bin/aurexc --emit=ast examples/hello.ax
build/full-llvm/bin/aurexc --emit=checked examples/hello.ax
build/full-llvm/bin/aurexc --emit=ir examples/hello.ax
build/full-llvm/bin/aurexc --emit=llvm-ir examples/hello.ax
build/full-llvm/bin/aurexc -S examples/hello.ax -o build/tests/hello.s
build/full-llvm/bin/aurexc -c examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=asm examples/hello.ax -o build/tests/hello.s
build/full-llvm/bin/aurexc --emit=obj examples/hello.ax -o build/tests/hello.o
build/full-llvm/bin/aurexc --emit=exe examples/hello.ax -o build/tests/hello
```

`--emit=exe` 是默认模式。`--emit=asm`、`--emit=obj` 和 `--emit=exe` 形式的
native 输出需要 `-o`。driver 风格的 `-S` 和 `-c` 在省略 `-o` 时会推导
`input.s` 和 `input.o`。
`--emit=lossless` / `--dump-lossless` 会输出保留空白、行注释和块注释的
结构化 lossless syntax tree。当前 dump 已有 `source_file` root、`module_decl` /
`import_decl` / `function_decl` 等顶层声明节点、直接 trivia/eof token leaves，以及
`block`、`paren_group`、`bracket_group`、`brace_group` 这类分隔符组节点，可重建原始源码文本；
`token_stream` 现在只作为非单调手造 token span 的保守兜底。lossless tree 还记录 parent、
连续 token span 和稳定 node key，支持按 offset 找最深语法节点、重建任意子树文本，并可通过
`parse::lower_lossless_syntax_to_ast` 进入现有 AST parser。内存 buffer 的 IDE 消费入口在
`aurex/application/tooling/ide.hpp`，会复用这条 lossless/query 主线暴露 diagnostics、hover、definition、
references 和编辑影响 node。这个 snapshot 现在会优先使用 checked module 里的全局符号，
在 checked 没有长期保留符号时再回退到 AST 局部参数和 `let` 绑定；diagnostics 也会先转成
结构化 event stream，再交给 CLI 渲染或 query fingerprint。

命令语法仍保持 clang 风格的扁平 flags，但 `--help` 会按一级动作选项和二级
修饰选项分组。`--clang`、`--clang-arg` 这类 native backend 修饰选项只适用
于 native 输出模式；如果和 `--emit=ir` 或 `--check` 这类 frontend-only 模式
组合，会作为参数错误拒绝。

## 增量缓存

```sh
build/full-llvm/bin/aurexc --check --incremental-cache build/main.axic examples/hello.ax
```

`--incremental-cache` 默认使用 query-key pruning：source fingerprint 不完全匹配时，
driver 会先尝试 query-key source-stage green reuse，再在 cache write 阶段记录
red/green provider-skip profile。`--query-pruning` 只是显式确认默认行为；
`--no-query-pruning` 才会退回 coarse source-fingerprint 兼容路径。

## Trait / Protocol 表面

当前支持 nominal static trait、显式 impl、generic trait predicate、static trait method call、associated-type
equality constraint、trait 内部的 default method body，以及 borrowed dyn trait runtime dispatch：

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;

    fn fallback(self: &Self, value: Self.Item) -> Self.Item {
        return value;
    }
}

struct Bytes {
    value: i32;
}

impl Source for Bytes {
    type Item = i32;

    fn get(self: &Bytes) -> i32 {
        return self.value;
    }
}

fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {
    return value.fallback(value.get());
}
```

default method body 会在 trait context 中检查；static trait call 中被选中的 inherited default 会在单态化后
lowering 为 internal direct-call function。显式 impl method 在替换后的签名匹配时仍是 override。

borrowed dyn view 使用显式 `dyn` 边界：

```aurex
trait Draw {
    fn draw(self: &Self) -> i32;
}

struct File {
    value: i32;
}

impl Draw for File {
    fn draw(self: &File) -> i32 {
        return self.value;
    }
}

fn render(drawable: &dyn Draw) -> i32 {
    return drawable.draw();
}

fn main() -> i32 {
    let file: File = File { value: 7 };
    return render(&file);
}
```

`&T` / `&mut T` 可以在满足可见 nominal impl 时 coercion 到 `&dyn Trait` / `&mut dyn Trait`。运行时表示是
`{data*, vtable*}` fat view，method call 会从 checked vtable slot 加载函数指针并间接派发。`dyn Trait[Item = i32]`
这类 associated equality 已支持，trait default method slot 也可以进入 vtable。

常规 sample suite 中的 `tests/samples/positive/traits/trait_dyn_borrowed_dispatch.ax` 展示了一个完整 borrowed
dyn 用例：`&dyn Source[Item = i32]` shared dispatch、`&mut dyn Accumulate` mutable receiver 写回、default
method slot 和 associated equality 在同一个程序中运行。

M10d release baseline 支持 borrowed dyn supertrait upcast runtime：`trait Child: Parent` 会进入 checked supertrait graph，
`&dyn Child` 可以在需要 `&dyn Parent` 的 contextual coercion 中记录 upcast fact，并 lowering 为
`trait_object_upcast` IR 和 LLVM parent vtable projection。`dyn Child` receiver 上 inherited parent method
call 会先投影到 parent vtable，再按 parent slot 动态派发。IDE/query tooling 会把 upcast 投影到
`FunctionDynAbiFacts::upcasts`、lower-IR query invalidation、semantic fact 和 hover。

M11a 已把后续 advanced dyn 主线选为 principal-set borrowed dyn composition，并固定
`m11a_dyn_advanced_design_gate_baseline`、`principal_set_metadata_v1`、`principal_set_identity_fact`、
`composition_witness_set_fact`、`principal_method_namespace_fact`、`associated_equality_merge_fact` 和
`composition_projection_fact`。M11b 已把这些事实落成 `PrincipalSetCompositionFacts` query prototype，支持
`principal_set_identity_fact()`、`principal_set_composition_facts_fingerprint()`、
`summarize_principal_set_composition_facts()` 和 `dump_principal_set_composition_facts()`，用于校验 principal-set
identity、witness set、principal-qualified method namespace、associated equality merge 和 projection facts。
这些目前只属于设计/query facts；当前仍不实现 owning dyn、`Box<dyn Trait>`、allocator、标准库、dynamic Drop
dispatch、用户可写多 trait object composition、`dyn A + B` parser syntax、composition sema/runtime、
specialization、default associated type、generic associated type 或 associated const。

## import

模块查找顺序：

1. 导入者所在目录。
2. 显式传入的 `-I path`。

本分支没有隐式标准库路径。需要共享模块时显式传入 import root：

```sh
build/full-llvm/bin/aurexc -I tests/samples/imports --emit=checked tests/samples/positive/modules/import_path.ax
build/full-llvm/bin/aurexc --import-path tests/samples/imports --emit checked tests/samples/positive/modules/import_path.ax
```

## C FFI

本分支不再提供 std C FFI 包装。语言核心测试如需 C 函数，应在样例中声明最小 `extern c` 边界：

```aurex
extern c {
    @name("puts")
    fn puts(text: *const u8) -> i32;
}
```

## 测试

```sh
tools/run_tests.sh
```

测试覆盖 lexer/parser、模块、可见性、泛型、sum type、pattern matching、`?`、`defer`、`for`、普通值语义、IR、LLVM lowering、native execution 和安装后 compiler 执行。标准库 API、std host support、M1 system/build-tool 样例不在本分支验证范围内。

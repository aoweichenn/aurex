# 使用文档

本文描述当前 **M20d Runtime Lowering ABI Design Closure** 下已经落地的用法。标准库仍保持冻结并移除，所有示例和测试都应围绕语言语法、语义、IR、后端、static trait、borrowed dyn trait、borrowed dyn supertrait upcast、borrowed dyn composition 和 compiler/query/IR verifier/admission/shape/identity/runtime-ABI-design facts 本身展开。

M11c 新增用户可写 `dyn (A + B)` borrowed composition annotation/coercion；M11d 新增 `&dyn (A + B) -> &dyn A` /
`&mut dyn (A + B) -> &mut dyn A` 显式 runtime projection；M11e 新增 composition runtime facts 的
query/cache/tooling/verifier release closure；M12a 新增无歧义 `combo.method()` direct dispatch；M12b 固定
receiver-access binding、associated equality direct dispatch、direct/explicit projection 去重和 query/cache
fingerprint drift；M13a 新增 `m13a_dyn_advanced_design_gate_baseline` query gate，选择 borrowed
composition-to-supertrait explicit projection 作为下一条主线；M13b 新增
`dynproject[SourcePrincipal, TargetSupertrait](view)` 显式投影并记录 `composition_to_supertrait` fact；M13c
已把它 lowering 为 `trait_object_composition_project` + `trait_object_upcast` runtime；M13d 新增
`composition_supertrait_chains` query/tooling/verifier release closure；M14 已支持唯一 path 的 expected-type
projection 和 direct supertrait method dispatch。

M15 新增 `m15_dyn_advanced_design_gate_baseline()` 和 `m15_const_generic_design_gate_baseline()` 两个 query design
gate，用于固定 owning dyn/runtime boundary 与 const generic 后续实现路线。M16 已打开用户可写 const generic
check-only 子集。M17 新增 `m17_dyn_ownership_runtime_preparation_baseline()`、`DynOwnershipRuntimeFacts`、
owned container / erased drop glue / allocator / cleanup-dropck boundary facts、summary/dump/fingerprint 和 validation，
用于固定 future runtime/standard-library boundary。M18 新增 `m18_dyn_ownership_runtime_boundary_gate_baseline()`、
`DynOwnershipRuntimeBoundaryGate`、`DynOwnershipRuntimeBoundaryCheckpointFact`、
`DynOwnershipRuntimeLoweringDesignGateFact` 和 `dyn_ownership_runtime_boundary_gate` project-level query，把 M17 facts
接入 query/cache/tooling/reuse/workspace index，并把 future IR/verifier/runtime lowering prerequisites 固定为可验证
facts。当前仍不实现 bare `dyn A + B`、标准库、owning dyn 用户值、`Box<dyn Trait>`、allocator API、runtime ABI
lowering、dynamic Drop dispatch、歧义 composition-to-supertrait 多步自动选择、generic const arithmetic、const where
predicate、associated const 或 dyn const equality dispatch。

M19 新增 `m19_dyn_ownership_runtime_ir_verifier_baseline()`、
`DynOwnershipRuntimeIrVerifierFact`、`FunctionDynOwnershipRuntimeIrVerifierFacts`、
`function_dyn_ownership_runtime_ir_verifier_facts()`、`TraitObjectVTableLayout::destructor_slot_blocked` 和
`CleanupAbiPolicy::dynamic_erased_drop_blocked` blocked negative sentinel。它只让 borrowed vtable destructor-free、
static cleanup-only、erased drop identity prerequisite、allocator identity prerequisite、owned dyn object placeholder
blocker 和 runtime-lowering blocker 成为 IR/verifier 可见 facts；不新增任何标准库或 executable runtime。

M20a 新增 `m20_owned_dyn_runtime_admission_gate_baseline()`、`OwnedDynRuntimeAdmissionGate`、
`OwnedDynRuntimeAdmissionFact`、`OwnedDynRuntimeAdmissionCapability`、`OwnedDynRuntimeAdmissionStage` 和
`OwnedDynRuntimeAdmissionPolicy`。它把 M17/M18/M19 facts 汇总成 owned dyn runtime admission gate，固定
owned object layout、erased drop identity、allocator identity、runtime lowering ABI、`Box<dyn Trait>` surface 和
borrowed dyn ABI separation 的后续顺序；仍不新增任何标准库 API、`Box<dyn Trait>` runtime、allocator API 或
executable runtime。

M20b 新增 `m20b_owned_dyn_ir_shape_prototype_gate_baseline()`、`OwnedDynIrShapePrototypeGate`、
`OwnedDynIrShapePrototypeFact`、`OwnedDynObjectLayoutPrototype` 和
`ir::owned_dyn_ir_shape_prototype_gate()`。Compiler-owned prototype 固定为 `{*mut u8 data, *const u8 vtable}` two-field
handle；IR dump/fingerprint/verifier 和 query adapter 都能看见该形状。它不是用户可写 Aurex 语法，也不允许用户构造
owning dyn value；drop identity、allocator identity、runtime ABI lowering、backend helper、标准库和
`Box<dyn Trait>` 仍保持 blocked。

M20c 新增 `m20c_owned_dyn_drop_allocator_identity_gate_baseline()`、
`OwnedDynDropAllocatorIdentityGate`、`OwnedDynDropAllocatorIdentityFact`、
`OwnedDynDropAllocatorIdentitySummary` 和 `ir::owned_dyn_drop_allocator_identity_gate()`。Compiler-owned
`OwnedDynObjectLayoutPrototype` 现在带有 `erased_drop_identity_key` 和 `allocator_identity_key`；IR
dump/fingerprint/verifier 和 query adapter 都能看见这两个 prerequisite identity。它们不是 runtime slot，也不是
allocator API；标准库、`Box<dyn Trait>`、owning dyn value、runtime ABI lowering、backend helper 和 dynamic Drop
runtime 仍保持 blocked。

M20d 新增 `m20d_owned_dyn_runtime_lowering_abi_gate_baseline()`、
`OwnedDynRuntimeLoweringAbiGate`、`OwnedDynRuntimeLoweringAbiFact`、
`OwnedDynRuntimeLoweringAbiSummary` 和 `ir::owned_dyn_runtime_lowering_abi_gate()`。M20d 把 runtime ABI descriptor、
blocked-to-admitted transition guard、backend helper prerequisite、drop/allocator runtime bridge 和 dynamic Drop
blocker 固定为 stable query/IR facts。`runtime_abi_descriptor_key` 和 `backend_helper_identity_key` 不是 runtime
slot，也不会生成 helper call；标准库、`Box<dyn Trait>`、owning dyn value、allocator API、executable runtime ABI
lowering、backend helper call 和 dynamic Drop runtime 仍保持 blocked。

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
M11c 已在此基础上支持 borrowed composition annotation/coercion，M11d 进一步支持显式 projection 后 runtime dispatch；
M12a/M12b 支持唯一 principal method 的 direct dispatch，并固定 projection facts、ABI descriptors 和 receiver
access hardening；M13c 支持显式 composition-to-supertrait runtime projection：

```aurex
trait Draw { fn draw(self: &Self) -> i32; }
trait Debug { fn debug(self: &Self) -> i32; }

struct File { value: i32; }

impl Draw for File { fn draw(self: &File) -> i32 { return self.value; } }
impl Debug for File { fn debug(self: &File) -> i32 { return self.value + 1; } }

fn score(view: &dyn (Draw + Debug)) -> i32 {
    let draw: &dyn Draw = view;
    let debug: &dyn Debug = view;
    return draw.draw() + debug.debug();
}

fn score_direct(view: &dyn (Draw + Debug)) -> i32 {
    return view.draw() + view.debug();
}

trait Render: Draw {
    fn render(self: &Self) -> i32;
}

impl Render for File {
    fn render(self: &File) -> i32 {
        return self.value + 2;
    }
}

fn score_supertrait(view: &dyn (Render + Debug)) -> i32 {
    let draw: &dyn Draw = dynproject[Render, Draw](view);
    let inferred: &dyn Draw = view;
    return view.draw() + draw.draw() + inferred.draw();
}

fn main() -> i32 {
    let file: File = File { value: 7 };
    let view: &dyn (Debug + Draw) = &file;
    return score(view);
}
```

这段程序在 `--check` / `--emit=checked` 阶段会生成 principal-set identity、canonical principal order、
composition witness set、projection fact 和 checked dump/fingerprint；在 `--emit=ir` / `--emit=llvm-ir` /
native execution 中会生成 `dyn.composition.pack`、`dyn.composition.project` 和 `principal_set_metadata_v1`
metadata global。M11e 后，`FunctionDynAbiFacts`、lower-IR query invalidation、IDE semantic fact 和 hover 也会展示
`principal_sets`、`composition_projections`、projection principal index、borrow kind 和
`composition_metadata=principal_set_metadata_v1`。M12a 后，`view.draw()` 会选择唯一提供 `draw` 的 principal，
隐式记录 composition-to-principal projection，并继续使用 ordinary single-trait dyn vtable dispatch；M12b 后，
direct dispatch 与显式 `let draw: &dyn Draw = view;` 混用会去重 principal projection fact 和 function-level
composition projection ABI descriptor，associated equality direct call 会使用 selected principal 的 equality
substitution。如果多个 principal 暴露同名 method，仍必须先显式投影来消除歧义。M14 后，唯一
source-principal path 的 composition-to-supertrait expected-type projection 和 direct supertrait dispatch 也可用；
歧义 path 仍必须写 `dynproject[...]` 来消除来源。当前仍不实现 owning dyn、`Box<dyn Trait>`、allocator、
标准库、dynamic Drop dispatch、bare `dyn A + B` parser syntax、specialization、default associated type、
generic associated type、associated const 或 generic const arithmetic。

## Const Generic 状态

M16 已打开 const generic frontend / query / sema check-only 子集。当前仍可以写普通 type generic 和 origin generic：

```aurex
struct Pair[T, U] {
    first: T;
    second: U;
}

fn view[origin data](value: &[data] i32) -> &[data] i32 {
    return value;
}
```

当前也可以写 typed scalar const parameter、mixed generic arguments 和 `[N]T`：

```aurex
struct ArrayView[T, const N: usize] {
    value: T;
}

fn len[T, const N: usize](value: [N]T) -> usize {
    return N;
}

fn main() -> usize {
    let value: ArrayView[i32, 4] = ArrayView[i32, 4] { value: 1 };
    return len[i32, 4]([1]);
}
```

当前规则：

- const parameter 必须写成 `const Name: Type`，不支持 untyped const parameter。
- const parameter type 只接受 integer、`bool` 和 `char` 标量类型。
- const argument 只接受 scalar literal 或当前 generic context 中的 const parameter name；const parameter name
  转发时必须和目标 const parameter type 一致。
- `ArrayView[i32, 4]` 与 `ArrayView[i32, 5]` 会产生不同 generic instance key；const argument 不再只是 display
  name 文本。
- `[N]T` 当前是 check-only array length 集成点；它会保留 const param identity 和 fingerprint，但 unresolved
  const-param array 不会 lowering 到 runtime layout。

仍不支持 generic const arithmetic、`N + 1`、user function comptime evaluation、const where predicate、
const associated value、dyn const equality、runtime const generic ABI 和 standard-library const generic API。

M13c 后，`dynproject[Render, Draw](view)` 会在 sema 层检查并在 IR/backend 层运行：

- `view` 是 `&dyn (Render + Debug)` / `&mut dyn (Render + Debug)` 这类 borrowed composition。
- `Render` 是 source principal，并且存在于 composition 中。
- `Draw` 是 `Render` 的 supertrait。
- 成功后记录 `CompositionProjectionFact{kind=composition_to_supertrait}` 和 `supertrait_projections` summary。
- lowering 时先生成 `dyn.composition.project` 取出 source principal，再生成 `dyn.upcast` 取出 target supertrait vtable。

M13d 后，`FunctionDynAbiFacts`、lower-IR query invalidation、IDE semantic fact 和 hover 会额外展示
`composition_supertrait_chains`，把 source composition view、projected principal view 和 target supertrait view 明确
串起来。M14 后，`let inferred: &dyn Draw = view;` 和 `view.draw()` 在 `Render -> Draw` 唯一路径下会复用同一条
runtime lowering，并额外记录 `BorrowedDynViewPathFact{use=expected_type_projection}` 或
`BorrowedDynViewPathFact{use=method_dispatch}`；如果 composition 中有多个 principal 都能到达目标 supertrait，
sema 仍会拒绝并要求显式 `dynproject[SourcePrincipal, TargetSupertrait](view)`。

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

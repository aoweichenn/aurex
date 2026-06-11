# Aurex 当前语法与特性清单

日期：2026-06-11
阶段：M21a Macro System Design Gate
状态：当前实现清单、M10d 完成面、M11 principal-set composition release、M12 direct composition dispatch release closure、M13a advanced dyn remaining policy design gate、M13b explicit dynproject frontend/query/sema、M13c runtime lowering、M13d release closure、M14 borrowed dyn view path inference release、M15 advanced dyn / const generic design baseline、M16 const generic check-only release、M17 dyn ownership runtime preparation release、M18 dyn ownership runtime boundary gate release、M19 dyn ownership runtime IR/verifier preparation release、M20a owned dyn runtime admission gate release、M20b owned dyn IR shape prototype gate release、M20c drop / allocator identity prerequisite gate release、M20d runtime lowering ABI design closure release、M20e builtin derive attribute closure、M20f struct field reference borrow closure、M20g default / named call arguments closure、M21a macro design gate 与后续非目标

本文记录当前仓库真实支持的 Aurex 语言表面、已具备的高级能力、未完成能力、M10d hardening/release closure 的完成范围、M11 principal-set composition release closure、M12 direct composition dispatch release closure、M13a advanced dyn remaining policy design gate、M13b explicit dynproject frontend/query/sema 子集、M13c runtime lowering 子集、M13d release closure、M14 borrowed dyn view path release、M15 advanced dyn / const generic design baseline、M16 const generic check-only release、M17 dyn ownership runtime preparation release、M18 dyn ownership runtime boundary gate release、M19 dyn ownership runtime IR/verifier preparation release、M20a owned dyn runtime admission gate release、M20b owned dyn IR shape prototype gate release、M20c drop / allocator identity prerequisite gate release、M20d runtime lowering ABI design closure release、M20e builtin derive attribute closure、M20f struct field reference borrow closure 和 M20g default / named call arguments closure。本文以 `include/aurex/frontend/syntax/core/token.hpp`、`include/aurex/frontend/syntax/ast/*`、`src/frontend/parse/*`、`src/frontend/sema/*`、`src/midend/ir/*`、`src/backend/llvm/*`、`tests/samples/**` 为准；M20e derive 另以 `tests/gtest/integration/regression_tests.cpp` 中的 `BuiltinDeriveCapability*` 回归和 `tests/gtest/frontend/parse/parser_tests.cpp` 中的 item attribute 恢复测试为准；M20f field reference borrow 另以 `src/frontend/sema/facade/sema_types.cpp`、`src/frontend/sema/internal/borrow/sources/flow_graph.cpp`、`src/frontend/sema/internal/borrow/sources/loan_checker.cpp`、`src/midend/ir/lowering/sources/lower_ast_expr.cpp`、`src/midend/ir/lowering/sources/lower_ast_stmt.cpp` 和 `tests/gtest/integration/struct_field_reference_tests.cpp` 为准；M20g default / named call arguments 另以 `include/aurex/frontend/sema/call_arguments.hpp`、`src/frontend/parse/grammar/parser_fn.cpp`、`src/frontend/parse/grammar/parser_postfix.cpp`、`src/frontend/sema/facade/sema_call.cpp`、`src/frontend/sema/internal/declarations/sources/sema_declaration_analyzer.cpp`、`src/frontend/sema/internal/declarations/sources/sema_generic_analyzer.cpp`、`src/frontend/sema/internal/borrow/sources/flow_graph.cpp`、`src/frontend/sema/internal/borrow/sources/loan_checker.cpp`、`src/frontend/sema/internal/place/sources/move_analysis.cpp`、`src/frontend/sema/internal/place/sources/place_state.cpp`、`src/midend/ir/lowering/sources/lower_ast_expr.cpp`、`tests/gtest/frontend/sema/default_named_argument_tests.cpp`、`tests/gtest/frontend/parse/parser_tests.cpp` 和 `tests/gtest/integration/native_execution_tests.cpp` 为准；M11a/M13a/M15 dyn gate 另以 `include/aurex/infrastructure/query/dyn_advanced_design_gate.hpp`、`src/infrastructure/query/dyn_advanced_design_gate.cpp` 和 `tests/gtest/infrastructure/query/dyn_advanced_design_gate_tests.cpp` 为准；M17 dyn ownership runtime prep 另以 `include/aurex/infrastructure/query/dyn_ownership_runtime_facts.hpp`、`src/infrastructure/query/dyn_ownership_runtime_facts.cpp` 和 `tests/gtest/infrastructure/query/dyn_ownership_runtime_facts_tests.cpp` 为准；M18 dyn ownership runtime boundary gate 另以 `include/aurex/infrastructure/query/dyn_ownership_runtime_boundary_gate.hpp`、`src/infrastructure/query/dyn_ownership_runtime_boundary_gate.cpp` 和 `tests/gtest/infrastructure/query/dyn_ownership_runtime_boundary_gate_tests.cpp` 为准；M19 dyn ownership runtime IR/verifier facts 另以 `include/aurex/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts.hpp`、`include/aurex/midend/ir/ir_dyn_ownership_runtime_ir_verifier_facts.hpp`、`src/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts.cpp`、`src/midend/ir/core/ir_dyn_ownership_runtime_ir_verifier_facts.cpp` 和 `tests/gtest/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts_tests.cpp` 为准；M20a owned dyn runtime admission gate 另以 `include/aurex/infrastructure/query/owned_dyn_runtime_admission_gate.hpp`、`src/infrastructure/query/owned_dyn_runtime_admission_gate.cpp` 和 `tests/gtest/infrastructure/query/owned_dyn_runtime_admission_gate_tests.cpp` 为准；M20b owned dyn IR shape prototype gate 另以 `include/aurex/infrastructure/query/owned_dyn_ir_shape_prototype_gate.hpp`、`include/aurex/midend/ir/ir_owned_dyn_ir_shape_prototype_gate.hpp`、`src/infrastructure/query/owned_dyn_ir_shape_prototype_gate.cpp`、`src/midend/ir/core/ir_owned_dyn_ir_shape_prototype_gate.cpp`、`tests/gtest/infrastructure/query/owned_dyn_ir_shape_prototype_gate_tests.cpp` 和 `tests/gtest/midend/ir/owned_dyn_ir_shape_prototype_tests.cpp` 为准；M20c drop / allocator identity prerequisite gate 另以 `include/aurex/infrastructure/query/owned_dyn_drop_allocator_identity_gate.hpp`、`include/aurex/midend/ir/ir_owned_dyn_drop_allocator_identity_gate.hpp`、`src/infrastructure/query/owned_dyn_drop_allocator_identity_gate.cpp`、`src/midend/ir/core/ir_owned_dyn_drop_allocator_identity_gate.cpp`、`tests/gtest/infrastructure/query/owned_dyn_drop_allocator_identity_gate_tests.cpp` 和 `tests/gtest/midend/ir/owned_dyn_drop_allocator_identity_tests.cpp` 为准；M20d runtime lowering ABI design closure 另以 `include/aurex/infrastructure/query/owned_dyn_runtime_lowering_abi_gate.hpp`、`include/aurex/midend/ir/ir_owned_dyn_runtime_lowering_abi_gate.hpp`、`src/infrastructure/query/owned_dyn_runtime_lowering_abi_gate.cpp`、`src/midend/ir/core/ir_owned_dyn_runtime_lowering_abi_gate.cpp`、`tests/gtest/infrastructure/query/owned_dyn_runtime_lowering_abi_gate_tests.cpp` 和 `tests/gtest/midend/ir/owned_dyn_runtime_lowering_abi_tests.cpp` 为准；M15 const generic gate 另以 `include/aurex/infrastructure/query/const_generic_design_gate.hpp`、`src/infrastructure/query/const_generic_design_gate.cpp` 和 `tests/gtest/infrastructure/query/const_generic_design_gate_tests.cpp` 为准；M16 const generic frontend/sema 另以 `src/frontend/parse/grammar/parser_fn.cpp`、`src/frontend/parse/grammar/parser_type.cpp`、`src/frontend/parse/grammar/parser_postfix.cpp`、`src/frontend/sema/internal/declarations/sources/sema_generic_analyzer.cpp`、`src/frontend/sema/internal/services/sources/sema_type_services.cpp`、`tests/gtest/frontend/parse/parser_tests.cpp`、`tests/gtest/frontend/syntax/ast_dump_tests.cpp` 和 `tests/gtest/frontend/sema/sema_generics_whitebox_tests.cpp` 为准；M11/M12/M13/M14 principal-set facts 另以 `include/aurex/infrastructure/query/principal_set_composition_facts.hpp`、`include/aurex/infrastructure/query/dyn_abi_facts.hpp`、`src/infrastructure/query/dyn_abi_facts.cpp`、`src/infrastructure/query/lower_function_ir_query.cpp`、`src/application/tooling/ide/ide.cpp`、`src/midend/ir/core/ir_dyn_abi_facts.cpp`、`src/frontend/sema/facade/sema_call.cpp`、`tests/gtest/frontend/sema/dyn_trait_composition_tests.cpp`、`tests/gtest/frontend/sema/dyn_trait_composition_supertrait_tests.cpp`、`tests/gtest/midend/ir/lower_ast_dyn_trait_tests.cpp`、`tests/gtest/infrastructure/query/principal_set_composition_facts_tests.cpp`、`tests/gtest/infrastructure/query/trait_object_upcast_key_tests.cpp` 和 `tests/gtest/integration/native_execution_tests.cpp` 为准。

M2 当前原则：

- M1 已舍弃，不再沿着 std/selfhost/build-tool 路线修补。
- 标准库不在当前树中，语言样例必须自包含。
- 语言级 `move(...)`、`noncopy struct` 和旧 use-after-move 追踪已从 M2 基线删除。
- 当前资源语义以 M6 的 compiler-owned `Copy`、内部 resource summary、whole-local move analysis、cleanup
  lowering、stable drop-glue identity 和 target-independent drop-glue planner 为入口；M7a 已在其上加入
  CFG-sensitive local loan checker、borrow summary、query/tooling projection 和 borrow diagnostics。用户编写
  destructor syntax/body lowering、完整 Rust-style lifetime surface、raw pointer alias safe proof 和 partial move
  继续后移。

## 总体判断

Aurex 现在已经具备一个小型系统语言核心：模块、import、可见性、函数、默认参数、命名参数、C FFI、struct、enum payload、泛型、method、raw pointer、safe reference、array、slice、tuple、`str`、block expression、`if` expression、`match` expression、`defer`、C-style `for`、结构化 Result/Option shape 识别、`?`、nominal static trait、显式 trait impl、trait predicate、静态 trait method dispatch、第一版 associated type model、trait default method body、内建 `#[derive(Copy, Eq, Hash)]` item attribute、compiler-owned `Copy` capability、内部 resource summary、whole-local move analysis、cleanup lowering、drop flags、稳定 drop-glue identity、target-independent drop-glue planner 和第一版 IDE/LSP resource projection 都已经存在。

M21a 已开启宏系统主线，但当前只完成 design gate，不是完整 macro/proc-macro 实现。现有真实用户可见宏能力仍只有
M20e 的编译器内建 `#[derive(Copy, Eq, Hash)]`。M21a 新增 `m21a_macro_design_gate_baseline()`，固定后续宏系统
必须使用 token tree / attribute surface、hygiene、source map、debug trace、declared generated names 和
query-backed expansion；typed expression macro、external procedural macro、文本替换宏、宏生成用户代码和标准库仍未实现。

但它还不是“基础语法冻结”的语言。最需要先修的是基础层剩余不一致，而不是继续添加更高级特性：

- block statement 和 block expression 主体规则已统一；expression context 额外要求 final expression。
- const initializer 已补齐纯标量运算，但还不是完整 comptime。
- compound assignment 已补齐；M2 采用 Rust/Zig 风格，不提供 `++` / `--` 自增自减语法。
- trailing separator 策略已冻结：圆括号/方括号列表允许 trailing comma，comma 分隔的花括号列表允许但不强制最后一个 comma。
- 普通 enum 已切换为 ADT-first：base type 和 case discriminant 可省略，显式 base/discriminant 保留给 C-like/repr enum。
- borrowed slice 已落地：类型语法为 `[]const T` / `[]mut T`，表达式语法为 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`，当前表示为 `ptr + len`。
- tuple 和 M2 pattern ergonomics 已落地：类型 `(A, B)` / `(A,)`，字面量 `(a, b)` / `(a,)`，数字字段访问 `value.0` / `value . 1`，局部 `let (a, _) = value;` 解构，match tuple/struct pattern，nested enum payload destructuring，以及 `if value is pattern` / `while value is pattern` / if 表达式 pattern condition；匿名 tuple 不支持 `.first` / `.second` 这类命名字段；空 tuple 暂不属于 M2。
- 顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`。
- `pub fn` 必须显式返回类型；private helper 仍可做返回类型推导。
- 默认参数 / 命名参数已由 M20g 落地：普通函数、inherent method、泛型函数 / 泛型 method 支持参数默认值；普通函数、inherent method、trait/static/dyn method call 支持命名参数，sema 会归一化为 checked `ordered_args`，后续 IR lowering 和 borrow/place/move 分析都按参数声明顺序消费。
- raw pointer、unchecked string、bit cast 等已经有最小 `unsafe` 边界；safe reference `&T` / `&mut T` 已落地并接入 M7 local loan checker。M20f 已固定 source-level field reference borrow：`&record.field` / `&mut record.field`、不同已知 struct 字段 loan 分离、同字段和 whole-parent invalidation diagnostics、local field reference escape rejection，以及 IR `field_addr` lowering。resource field partial move/overwrite helper、完整 Rust-style lifetime surface、raw pointer alias safe proof 和跨函数高级 alias model 仍暂缓。
- 泛型已有 `where` / trait predicate 约束，支持内建能力 `Sized`、`Eq`、`Ord`、`Hash`、compiler-owned `Copy` 和用户定义 nominal static trait；associated type declaration、impl assignment、projection、equality predicate 和 trait default method body 已进入 M5 release baseline；M8 已加入 `&dyn Trait` / `&mut dyn Trait` borrowed dyn view、checked vtable facts、IR/backend vtable dispatch 和 `dyn Trait[Assoc = Type]` associated equality dispatch；M10 已加入 borrowed dyn supertrait upcast runtime；M11a 已选择 principal-set borrowed dyn composition design/query gate，M11b 已新增 `PrincipalSetCompositionFacts` query prototype，M11c 已支持 `dyn (A + B)` borrowed composition annotation/coercion，M11d 已支持显式 composition-to-principal runtime projection，M11e 已完成 composition runtime facts/query/tooling/verifier hardening，M12a 已支持唯一 principal method 的 direct composition dispatch，M12b 已完成 receiver-access binding、associated equality direct dispatch、projection/ABI descriptor 去重和 query/cache fingerprint hardening，M13a 已选择 borrowed composition-to-supertrait explicit projection 作为后续主线并固定 query design gate，M13b 已支持 `dynproject[Child, Parent](view)` 显式投影并记录 `composition_to_supertrait` fact，M13c/M13d 已支持并收口该显式投影的 IR/backend runtime lowering 和 query/tooling/verifier facts，M14 已支持唯一 path 的 expected-type projection 和 direct supertrait method dispatch。M16 已支持 typed scalar const generic check-only：`const N: usize`、mixed generic args、const argument fingerprint、`GenericInstanceKey::const_args` 和 `[N]T` array length identity；M17 已新增 `DynOwnershipRuntimeFacts`，固定 future owning dyn / erased drop glue / allocator / cleanup-dropck runtime boundary；M18 已新增 `DynOwnershipRuntimeBoundaryGate` 和 `dyn_ownership_runtime_boundary_gate` project-level query，把这些 runtime boundary facts 接入 query/cache/tooling/reuse/workspace index，并固定 future IR/verifier lowering design gate；M19 已新增 `DynOwnershipRuntimeIrVerifierFact`、`FunctionDynOwnershipRuntimeIrVerifierFacts`、`function_dyn_ownership_runtime_ir_verifier_facts()`、borrowed vtable destructor-free verifier guard 和 `CleanupAbiPolicy::dynamic_erased_drop_blocked` blocked negative sentinel；M20a 已新增 `OwnedDynRuntimeAdmissionGate` 和 `m20_owned_dyn_runtime_admission_gate_baseline()`，把 owned object layout、erased drop identity、allocator identity、runtime lowering ABI、`Box<dyn Trait>` surface 和 borrowed dyn ABI separation 固定成分阶段准入顺序；M20b 已新增 `OwnedDynObjectLayoutPrototype` 和 `OwnedDynIrShapePrototypeGate`，固定 compiler-owned `{*mut u8 data, *const u8 vtable}` two-field IR shape、blocked drop/allocator slots 和 verifier/query negative matrix；M20c 已新增 `OwnedDynDropAllocatorIdentityGate`，并把 `erased_drop_identity_key` / `allocator_identity_key` 接入 IR verifier、dump、layout ABI fingerprint 和 query adapter；M20d 已新增 `OwnedDynRuntimeLoweringAbiGate`，把 runtime ABI descriptor、blocked-to-admitted transition guard、backend helper prerequisite、drop/allocator runtime bridge 和 dynamic Drop blocker 固定成 query/IR facts。generic const arithmetic、owning dyn 用户值、`Box<dyn Trait>`、allocator API、trait-object Drop dispatch、executable runtime ABI lowering、backend runtime helper call、bare `dyn A + B`、歧义 composition-to-supertrait 自动选择和用户 `Drop` bound 仍暂缓。
- `str` 已有内建雏形和 no-std checked UTF-8 边界；`strvalid(bytes)` / `strfromutf8(bytes)` 接受 `[]const u8` 或 `[]mut u8`，`strfromutf8` 返回 `str`，失败时返回空 `str`；`text[l:r]` 按 byte offset 做 checked slicing，越界或非 UTF-8 code point boundary 返回空 `str`；`strraw(data, len)` 继续是 unsafe-only unchecked 后门。

## 当前完整语法库存

### 词法

当前关键字：

```text
module import as pub priv extern export
fn struct opaque enum trait const type impl match
let var if else for in while break continue defer return where
true false null
void bool i8 u8 i16 u16 i32 u32 i64 u64 isize usize f32 f64 str
char
mut cast ptrcast bitcast sizeof alignof
ptraddr ptrat strptr strblen strvalid strfromutf8 strraw
```

`c` 是 C ABI 语法里的上下文标记，不是全局关键字。`fn f(a: i32, b: i32, c: i32)`
这类参数名、局部名、普通函数名和模块路径段都可以使用 `c`。

当前标点和操作符：

```text
( ) { } [ ] , . ... ; : -> => @ ?
+ - * / % & | ^ ~ !
= += -= *= /= %= &= |= ^= <<= >>=
== != < <= > >= << >> && ||
```

词法层仍保留 `++` / `--` 为独立 token，用于给出明确诊断并避免 `--x` 被误解析成两次一元负号；语法层不支持自增自减。

注释：

- `//` 行注释。
- `/* ... */` 块注释，支持嵌套。

标识符：

```text
identifier = [A-Za-z_][A-Za-z0-9_]*
```

当前是显式 ASCII 规则，不使用 locale。Unicode identifier 暂缓。

### 字面量

整数：

```aurex
0
123
1_000
0xff
0Xff
0b1010
0B1010
1u8
42usize
1i32
```

规则：

- `_` 只能出现在两个合法数字之间。
- 不允许 `1_`、`1__2`、`0x_FF`、`0b_1010`。
- 整数后缀支持 `i8`、`i16`、`i32`、`i64`、`isize`、`u8`、`u16`、`u32`、`u64`、`usize`。
- 浮点后缀不能用于整数字面量，例如 `1f32` 会诊断。
- 整数字面量可根据期望整数类型做范围检查。

浮点：

```aurex
1.0
0.5
.5
1.
1e3
1.0e-3
1.0f32
1.0f64
```

规则：

- 没有期望类型时默认 `f64`。
- 期望类型是 `f32` / `f64` 时按期望类型检查。
- 浮点后缀支持 `f32` / `f64`。
- 整数后缀不能用于浮点字面量，例如 `1.0u8` 会诊断。
- 当前不支持 underscore suffix 形式，例如 `1.0_f32`。

字符串和 byte：

```aurex
"hello"       // str
c"hello"      // *const u8, 用于 C FFI
r"raw\n"      // str，escape 不解释，可跨行
b"abc\n"      // [N]u8
b'a'          // u8
b'\n'         // u8
'λ'           // char
'\u{03BB}'    // char
```

普通字符串会校验 UTF-8。C 字符串拒绝内部 NUL。raw string 不解释 escape，并允许换行。byte string 只接受 ASCII raw byte 和简单 byte escape，结果类型是固定长度 `[N]u8`。`char` 是 Unicode scalar value，不是 `u8` 或 C `char`。

布尔和空指针：

```aurex
true
false
null
```

`null` 需要期望类型是 pointer。

### 模块与导入

```aurex
module app.main;

import common.result as result;
pub import common.status as status;
pub use common.status.Status;
pub(package) use common.status.PackageStatus as PackageStatus;
priv import internal.detail as detail;
```

规则：

- 每个文件声明一个 `module` path。
- module path 用 `.`。
- import 通过导入者目录和显式 `-I` 查找。
- import 默认 private。
- `pub import` 可 re-export。
- primary-level `pub use module.Item [as Alias]` / `pub(package) use ...` 可 selective re-export 单个 item。
- 使用 `alias.item` 做限定查找；语法统一使用 `.`，语义按 base kind
  区分 module / type / value / member。
- 当前没有 glob import/use、general selective import、workspace、dependency resolver、lockfile、
  version solving 或 package manager。

### 可见性

```aurex
pub fn api() -> i32 { return 1; }
priv fn helper() -> i32 { return 2; }

struct User {
    pub id: i32;
    priv secret: i32;
}
```

当前行为：

- 顶层 item 默认 private，跨模块可访问 API 必须显式 `pub`。
- struct field 默认 private，跨模块字段读取和 struct literal 初始化只允许访问 public field。
- impl method 默认 private，跨模块 method 调用必须显式 `pub`。
- import 默认 private，`pub import` 可 re-export。
- `export c fn` 强制 public，不能写成 private。
- `impl` block 和 `extern` block 不能显式写成 private；未写可见性时按普通 block 解析。

### 类型语法

基础类型：

```aurex
void bool
i8 u8 i16 u16 i32 u32 i64 u64 isize usize
f32 f64
str
```

命名类型和限定类型：

```aurex
Point
Box[i32]
result.ResultI32I32
```

指针：

```aurex
*const T
*mut T
*mut *mut u8
```

数组：

```aurex
[4]u8
[16]Point
```

当前限制：

- 数组类型可以作为字段、局部变量和 const 存在，并可用 `[1, 2, 3]` / `[0; 128]` 构造固定长度数组值。
- 数组和含数组类型不能作为普通 by-value 参数、返回值、赋值目标或 enum payload。
- 这些限制来自当前 ABI/lowering 能力，不是最终语言设计。

tuple：

```aurex
(i32, bool)
(i32,)
```

规则：

- tuple 是匿名 product type，元素按声明顺序排列。
- tuple literal 写作 `(1, true)` 或 `(value,)`。
- 匿名 tuple 支持数字字段访问：`pair.0`、`pair.1` 和带空格的 `pair . 0`；数字字段越界会报 `tuple field index is out of range`。
- 匿名 tuple 不支持 `.first` / `.second` 这类命名字段；需要命名字段时使用 named struct。
- 局部声明可做 tuple destructuring：`let (left, _) = pair;`，`var (a, b) = pair;` 会把绑定设为 mutable。
- `match pair { (left, right) => ... }` 已支持 tuple pattern；bool / no-payload enum 叶子组成的 tuple、struct 和小 fixed array 可做结构化穷尽性检查，其他开放形状仍需要 irrefutable arm。
- 空 tuple 类型、空 tuple 字面量和空 tuple pattern 不属于 M2。

safe reference 已作为 M2 基础类型落地：

```aurex
&T
&mut T
```

规则：

- `&place` 只产生 `&T`；需要 raw pointer 地址时必须显式使用 `ptraddr(...)`、`ptrat[T](...)` 等 raw/unsafe 边界，不存在按 expected type 退化成 raw pointer 的兼容路径。
- `&mut place` 产生 `&mut T`，要求 operand 是 writable place，且不会退化成 raw pointer。
- `*ref` 是 safe 解引用；`*raw_pointer` 仍需要 `unsafe`。
- `&mut T` 可赋给 `&T`，反向不允许。
- reference pointee 必须是 valid storage；`&void` 和 opaque value reference 被拒绝。
- M7 已接入 CFG-sensitive local loan checker、borrowed-return summary、projection-aware conflict 和
  lifetime/dropck facts；M20f 已固定 struct field reference borrow。当前仍不暴露完整 Rust-style lifetime
  surface，不证明 raw pointer alias safe，不实现 index/slice 精确 disjoint proof 或跨函数高级 alias model。

当前没有：

- never type。
- typed scalar const generic check-only 已在 M16 打开：当前 AST/parser 支持 type parameter、typed const parameter
  和 origin parameter；未完成的是 generic const arithmetic、const where predicate、associated const、dyn const
  equality 和 unresolved const-param array runtime ABI。

函数指针类型、lambda 字面量和 M20 捕获闭包核心子集已作为基础函数式能力落地：

```aurex
type BinaryOp = fn(i32, i32) -> i32;
type CCallback = extern c fn(*mut void, ...) -> i32;
let inc: fn(i32) -> i32 = fn(value: i32) -> i32 => value + 1;
let base: i32 = 40;
let add_base = fn(value: i32) -> i32 => value + base;
```

当前语义是非捕获函数指针：函数名和无捕获 lambda 字面量可以作为值赋给 `fn(...) -> T`，函数名也可以作为值赋给
`extern c fn(...) -> T`；局部变量、参数和 struct 字段中的函数指针可以用普通调用语法间接调用。调用约定、参数类型、
variadic 标记和返回类型都是类型身份的一部分；variadic 函数类型只允许 `extern c fn`。lambda 当前要求显式参数类型和
显式 `-> T`，支持 `=> expr` 表达式体和 `{ ... }` 块体。

M20 捕获闭包核心子集当前支持捕获外层非泛型依赖、非 borrowed-view 的 `Copy` 局部或参数，按值复制进编译器生成的匿名
environment record；闭包可局部存储、直接调用、嵌套调用、在 match guard 中使用捕获值，也可从函数返回并由
return inference 推断为内部环境类型。捕获闭包不是 `fn(...) -> T`，不能赋给薄函数指针；非 `Copy` 捕获会报
`capturing a non-Copy value in a closure is not supported yet`，borrowed-view 捕获会报
`capturing a borrowed-view value in a closure is not supported yet`，generic-dependent 捕获会报
`capturing a generic-dependent value in a closure is not supported yet`。当前仍不实现 `Fn`/`FnMut`/`FnOnce`
trait、标准库 adapter、heap/allocator closure box、mutable/consuming capture mode、generic closure environment ABI
或 borrowed closure environment escape 求解。

### 顶层 item

const：

```aurex
const ANSWER: i32 = 42;
```

type alias：

```aurex
type Count = usize;
```

struct：

```aurex
struct Point {
    x: i32;
    y: i32;
}
```

generic struct：

```aurex
struct Pair[T, U] {
    first: T;
    second: U;
}
```

M2 基础泛型语法：

```text
GenericParams = "[" GenericParam ("," GenericParam)* ","? "]"
GenericParam  = identifier
TypeArgs      = "[" Type ("," Type)* ","? "]"
GenericApply  = Callee TypeArgs  // parser emits an explicit postfix AST node
WhereClause   = "where" Identifier ":" TraitPredicate ("+" TraitPredicate)*
TraitPredicate = Identifier AssociatedTypeEqualities?
AssociatedTypeEqualities = "[" Identifier "=" Type ("," Identifier "=" Type)* ","? "]"
```

规则：

- `[]` 不能为空；`fn f[]`、`struct Box[]`、`Box[]`、`id[](...)` 都是 parser 错误。
- `GenericParam` 只能是类型参数名；inline bound `T: Bound` 仍被拒绝。
- `where` 支持内建非资源 capability：`Sized`、`Eq`、`Ord`、`Hash`，也支持 M5 用户 trait predicate 和 associated-type equality，例如 `where T: Source[Item = i32]`。
- `#[derive(Copy, Eq, Hash)]` 是当前唯一内建 item attribute surface。它只支持 `struct` / `enum`，其中
  `Eq` / `Hash` 记录 checked capability fact，`Copy` 仍服从资源语义分类和 `impl Drop` custom destructor
  规则；它不生成标准库方法，也不是完整 macro/proc-macro 系统。
- M5 当前只支持 identifier trait predicate；qualified where predicate 和 generic trait predicate argument 留给后续 solver 阶段。
- 命名类型后的 `TypeArgs` 用于泛型类型实例化，例如 `Pair[i32, bool]`。
- 泛型类型声明覆盖 `struct Pair[T, U]`、`enum Option[T]`、`type Ptr[T] = *const T`。
- `impl[T] Box[T]` 这类 owner generic impl 支持；method-local generic parameter 仍不支持。
- parser 阶段直接生成显式 postfix AST：`generic_apply`、`index`、`slice`、`field`、`call`、`struct_literal` 和 `try_expr`。
- `id[i32](value)` 解析为 `generic_apply` callee 后接 `call`；`Option[i32].some` 和 `Option[T].some` 这类 type-shaped selector 解析为泛型类型选择后接字段/enum case 选择；`values[0].field` 和 `values[index].field` 解析为 value `index` 后接字段选择。
- `name[index]` 默认是 value index；只有 type-only argument、明显的 call / struct literal 延续，或满足 M2.1 type-shaped selector 契约的 selector 延续会把 bracket 作为 type args。type-shaped 是语法契约：base 或裸 type arg 以大写 identifier 开头，或者 type arg 使用 primitive / pointer / reference / tuple / slice / array / function type 等 type-only 形式；lowercase `name[index].field` 不依赖 sema 猜测，固定解析为 value index。
- 类型注解支持 `Name`、`alias.Name` 和可见模块路径形式 `core.mem.File` / `core.mem.Box[i32]`；表达式侧同样支持 `core.mem.PAGE_SIZE`、`core.mem.make()`、`core.mem.File.new()` 这类多段模块 selector。一段 qualifier 仍按 import alias 解析，多段 qualifier 按 visible module path 解析。

enum：

```aurex
enum OptionI32 {
    some(i32),
    none,
}

enum Token {
    ident(str),
    span(usize, usize),
    eof,
}

enum Status: u8 {
    ok = 0,
    err = 1,
}
```

普通 enum 默认是 ADT：base type 和 case discriminant 可省略，tag 从 0 自动分配；payload 支持单字段和多字段。显式 base/discriminant 仍保留给 C-like/repr enum。generic enum 已纳入 M2 基线，可写 `enum Option[T] { some(T), none }` 和 `enum Result[T, E] { ok(T), err(E) }`。

opaque struct：

```aurex
opaque struct FILE;
```

function：

```aurex
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn inferred() {
    return 42;
}

fn mix(a: i32, b: i32 = 10, c: i32 = 100) -> i32 {
    return a + b + c;
}
```

private 普通函数可以省略返回类型并由 `return` 推导。`pub fn`、`extern c fn`、`export c fn` 和 prototype 必须显式返回类型。

默认参数 / 命名参数：

```aurex
fn mix(a: i32, b: i32 = 10, c: i32 = 100) -> i32 {
    return a + b + c;
}

struct Acc {
    base: i32;
}

impl Acc {
    fn add(self: &Acc, value: i32, scale: i32 = 1) -> i32 {
        return self.base + value * scale;
    }
}

fn main() -> i32 {
    let acc: Acc = Acc { base: 5 };
    let a: i32 = mix(1);
    let b: i32 = mix(1, 2);
    let c: i32 = mix(a: 1, c: 3);
    let d: i32 = mix(1, c: 3);
    let e: i32 = acc.add(value: 2);
    let f: i32 = acc.add(scale: 3, value: 2);
    return a + b + c + d + e + f;
}
```

规则：

- 默认参数写在参数类型之后：`name: Type = expr`。
- required 参数不能出现在 default 参数之后。
- 默认表达式按该参数类型做期望类型分析。
- 默认表达式当前不把前序参数放入作用域；`fn f(a: i32, b: i32 = a)` 暂不支持。
- 普通函数、inherent method、泛型函数和 owner generic method 可以声明默认参数。
- C ABI / variadic 函数不能声明默认参数。
- trait requirement 不能声明默认参数；trait impl method / static dispatch / dyn dispatch 的调用侧可以使用命名参数。
- call-site 可以混用 positional 和 named 参数，但 named 参数之后不能再出现 positional 参数。
- named 参数按目标参数名重排，缺失的 defaulted 参数由声明默认值填充。
- sema 会在 checked binding 中保存 `ordered_args`；IR lowering、borrow summary、body flow graph、body loan
  precheck、place-state precheck、move analysis 和 borrow escape / lambda capture 扫描都消费归一化顺序。
- named 参数不支持 enum constructor、function value / lambda 间接调用或 variadic C call。

function prototype：

```aurex
fn declared(a: i32) -> i32;
```

prototype 必须有定义，且签名必须一致。

C FFI：

```aurex
extern c {
    opaque struct FILE;
    @name("puts")
    fn puts(s: *const u8) -> i32;
    @name("printf")
    fn printf(format: *const u8, ...) -> i32;
}

@name("main")
export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 {
    return 0;
}
```

限制：

- variadic 只允许 `extern c`。
- C ABI 函数必须显式返回类型。
- generic extern/export C 函数不支持。
- `@name("...")` 是函数声明前 ABI 名称装饰器。

impl / method：

```aurex
impl Point {
    fn len2(self: *const Point) -> i32 {
        return self.x * self.x + self.y * self.y;
    }
}
```

规则：

- impl target 必须是 named struct / enum / opaque struct 类型。
- method 的 `self` 如果存在，必须是第一个参数。
- `self` 类型必须是 impl 类型、其 pointer 或其 safe reference。
- `impl[T] Box[T]` 这类 owner generic impl 支持；method-local generic parameter 仍不支持。
- receiver 可以使用值、raw pointer 或 safe reference；raw pointer receiver 仍主要服务 FFI/unsafe 边界。

### 语句

局部绑定：

```aurex
let x: i32 = 1;
let y = 2;
var z: i32 = 3;
```

规则：

- `let` 不可赋值。
- `var` 可赋值。
- initializer 必填。
- 类型可省略并推导。
- 同一 lexical scope 内不允许重复定义 local/param。
- 内层 lexical scope 可以 shadow 外层 local。
- local/param 不能 shadow import alias、visible root module、generic type parameter 或当前可见类型名。

赋值：

```aurex
z = z + 1;
z += 1;
point.x = 2;
ptr.value = 3;
array[index] = 4;
```

支持简单赋值和复合赋值：

```text
= -= *= /= %= <<= >>= &= ^= |=
```

不支持 `++` / `--`。计数更新必须显式写成 `x += 1` / `x -= 1`；`x++`、`++x`、`x--`、`--x` 都是语法错误。这个选择与 Rust/Zig 类似，避免 C/C++/Java/JavaScript 的“更新表达式返回旧值或新值”心智负担，也避免副作用表达式进入 M2 基础语法。

控制流：

```aurex
if cond {
    return 1;
} else if other {
    return 2;
} else {
    return 3;
}

while i < n {
    i = i + 1;
}

for var i: i32 = 0; i < 10; i = i + 1 {
    continue;
}

for i in range(10) {
    continue;
}

for i in range(2, 10) {
    continue;
}

for i in range(10, 0, -2) {
    continue;
}
```

当前 `for i in range(...)` 是 M2 基础语法糖，不是通用 iterator：

- `range(end)` 是半开区间 `[0, end)`。
- `range(start, end)` 是半开区间 `[start, end)`。
- `range(start, end, step)` 支持显式步长；`step > 0` 时向上迭代，`step < 0` 时向下迭代，`step == 0` 时零次迭代。
- `start` / `end` / `step` 只求值一次，必须是同一种整数类型。
- loop 变量默认不可变，作用域只在循环体内。
- 当前没有 `for item in container`、数组/切片/字符串迭代或 iterator protocol。

`break` / `continue` 只能在 loop 中使用；在 range-for 中，`continue` 跳到隐藏的 cursor 更新点，隐式 step 使用 `1`，显式 step 使用用户给定的步长。

defer：

```aurex
defer cleanup();
```

当前 `defer` 只允许 call expression。

return：

```aurex
return;
return value;
```

expression statement：

```aurex
foo();
foo()?;
```

语义层只允许 call 和 try expression 作为 expression statement。`x + y;` 会诊断。

### 表达式

name / qualified name：

```aurex
value
math.add
Status.ok
remote.Status.ok
```

grouping：

```aurex
(a + b)
```

unary：

```aurex
!ok
-value
~mask
&place
&mut place
*ptr
```

binary precedence 从高到低：

| 层级 | 操作符 |
| --- | --- |
| postfix | `.`, `[]`, `()`, `?`, sema-resolved generic apply |
| unary | `!`, `-`, `~`, `&`, `*`, builtins |
| multiplicative | `*`, `/`, `%` |
| additive | `+`, `-` |
| shift | `<<`, `>>` |
| comparison | `<`, `<=`, `>`, `>=` |
| equality | `==`, `!=` |
| bit and | `&` |
| bit xor | `^` |
| bit or | `|` |
| logical and | `&&` |
| logical or | `||` |

postfix：

```aurex
value.field
array[index]
ptr[index]
function(arg)
generic_fn[T](arg)
value.method(arg)
expr?
```

struct literal：

```aurex
Point { x: 1, y: 2 }
Pair[i32, bool] { first: 1, second: true }
remote.Point { x: 1, y: 2 }
```

歧义规则：

- parser 不提前判定 `name[index]` 是 index 还是泛型实参；sema 根据 base kind materialize。局部 value base 走 index，泛型函数/type base 走 type args。
- 显式函数泛型实参写成 `name[T](arg)` 或 `module.name[T](arg)`。
- generic struct literal 只有在 `Name[Args] { ... }` 形态中解析为类型实参。
- `fn f[]`、`Box[]`、`id[](...)` 均非法，空 `[]` 不表示推导。
- `<` / `>` 只作为比较相关 token，不再作为泛型 delimiter。

if expression：

```aurex
let value = if ok {
    1
} else if fallback {
    2
} else {
    3
};
```

规则：

- condition 必须是 bool。
- expression form 必须有 else。
- 分支类型必须一致。
- result 不能是 void。
- const initializer 中不能使用。

block expression：

```aurex
let value = {
    let x = 1;
    if x > 0 {
        cleanup();
    }
    x + 1
};
```

当前规则：

- block body 语法与普通 statement block 共用同一套 statement 解析。
- block expression 可以包含 `let` / `var`、赋值、允许的 expression statement、普通 `if` statement、`while`、`for`、嵌套 block、`defer`、`return`、`break`、`continue`。
- expression context 要求 tail expression，且 result 不能是 void。
- `return`、`break`、`continue` 的合法性由语义阶段判断。
- 如果 block body 已经保证不 fallthrough，tail expression 会被诊断为不可达。
- 普通函数体和普通 statement block 暂不把最后表达式当作隐式 `return`。

match expression：

```aurex
let value = match result {
    .ok(x) => x,
    .err(code) => code,
};
```

当前支持：

- enum match。
- integer / bool literal match。
- wildcard `_`。
- enum case payload binding；多字段 payload 可用 `.case(a, b)` 按字段解构。
- nested enum payload destructuring：`.case((a, b))`。
- tuple pattern 和 struct pattern：`(a, b)`、`Point { x, y }`。
- pattern condition：`if value is pattern`、`while value is pattern`，以及 if 表达式中的 `if value is pattern { ... } else { ... }`。
- 局部 struct destructuring：`let Point { x, y } = point;`。
- 局部 slice/enum destructuring 和 `let ... else`：`let [head, ..] = array;`、`let .some(value) = opt else { return 0; };`。
- guarded arm：`pattern if guard => value`。
- or-pattern：`a | b | c`，绑定 alternative 必须绑定同名且同类型的名字。
- slice pattern：`[head, .., tail]`。
- exhaustiveness 和 unreachable 检查。

当前限制：

- payload pattern 的绑定数量必须和 case payload 字段数一致；无 payload case 不能写 binding。
- or-pattern alternatives 绑定名字时必须在每个 alternative 中保持同名同类型。
- 没有 Rust-style `if let` / `while let`；M2 使用 `if value is pattern` / `while value is pattern`。
- match 不能用于 const initializer。

try expression：

```aurex
let value = result?;
```

规则：

- operand 必须是形状匹配的 payload enum；当前样例使用 `ResultI32I32` / `OptionI32` 这类具体 enum。
- `Result` 形状是结构化检查，不依赖 enum 类型名：enum 必须精确只有 `ok(payload)` 和 `err(payload)` 两个 case，当前函数返回类型也必须是 result-like enum，且 `err` payload 类型一致。
- `Option` 形状是结构化检查，不依赖 enum 类型名：enum 必须精确只有 `some(payload)` 和无 payload `none` 两个 case，当前函数返回类型也必须是 option-like enum。
- 用户自定义同名 case 但 payload 形状不匹配或额外 case 不会被 `?` 误识别为 Result/Option。
- const initializer 中不能使用。

内建表达式：

```aurex
cast[i32](value)
ptrcast[*const u8](ptr)
bitcast[u32](value)
sizeof[T]
alignof[T]
ptraddr(ptr)
ptrat[*mut T](address)
strptr(text)
strblen(text)
strvalid(bytes)
strfromutf8(bytes)
strraw(data, len)
```

其中 `ptrcast`、`bitcast`、`ptrat` 和 `strraw` 是 unsafe-only；safe context 下直接使用会诊断。`cast`、`sizeof`、`alignof`、`ptraddr`、`strptr`、`strblen`、`strvalid` 和 `strfromutf8` 仍是 safe 内建。`strvalid(bytes)` 返回 `bool`；`strfromutf8(bytes)` 返回 `str`，成功时借用原 byte slice，失败时是空 `str`，因此 checked 调用失败时不会把无效输入包装成 UTF-8 文本。需要区分合法空输入和非法输入时调用 `strvalid(bytes)`。

### Pattern 语法

```aurex
_
case_name
EnumName.case_name
.case_name
case_name(binding)
EnumName.case_name(binding)
.case_name(binding)
case_name(first, second)
EnumName.case_name(first, second)
.case_name(first, second)
42
true
false
p1 | p2 | p3
```

限制：

- enum match pattern 必须是 enum case 或 `_`。
- integer/bool match pattern 必须是 literal 或 `_`。
- payload binding 数量必须和 enum case payload 字段数一致，例如 `.some(value)`、`.span(start, end)`。
- or-pattern 可以绑定 payload，但每个 alternative 必须绑定同名且同类型的名字。
- wildcard 后续 arm 会诊断 unreachable。

### const initializer

当前允许：

- integer / float / bool / null / string / c-string / raw string / byte string / byte literal / char literal。
- const 引用。
- enum case 常量。
- struct literal，只要字段 initializer 都是 const evaluable。
- `!`、`-`、`~`。
- `+`、`-`、`*`、`/`、`%`、`<<`、`>>`、比较、相等、bitwise `&` / `^` / `|`、logical `&&` / `||`。
- `cast`、`ptrcast`、`bitcast`、`ptraddr`、`ptrat`。
- `sizeof`、`alignof`。

当前不允许：

- 函数调用。
- `if` / block / match / `?`。
- `strptr` / `strblen` / `strvalid` / `strfromutf8` / `strraw`。

## 已支持的高级特性

当前已经具备的高级能力：

1. 手写 lexer/parser、错误恢复和 ID-backed AST。
2. 多文件 module/import/re-export。
3. C FFI：`extern c`、`export c fn`、`@name`、variadic extern。
4. 泛型 struct / function / enum / type alias，采用单态化实例化或结构化 alias 替换。
5. 泛型函数参数推断、显式 `id[T](x)` 调用、generic struct literal、owner generic impl，以及 `where` capability / trait predicate 约束。
6. enum payload 和 enum constructor。
7. pattern matching：enum/integer/bool、payload binding、guard、or-pattern、exhaustiveness。
8. 结构化 Result/Option shape 识别和 `?` lowering。
9. method / associated function 查找，支持跨模块可见性。
10. block expression 和 `if` expression。
11. `defer` 和早返回路径中的 lowering 支持。
12. typed Aurex IR、IR verifier、pass pipeline、LLVM backend 和 native 输出。
13. 普通 `fn main` 入口：无参数或 `(argc: i32, argv: *mut *mut u8)`，返回 `i32` 或 `void`。
14. M5 nominal static trait：`trait` declaration、显式 `impl Trait for Type`、generic trait predicate、static trait method dispatch、associated type declaration/assignment/projection/equality、trait default method body，以及 IDE/tooling/diagnostics 投影。
15. M6-WP2/WP3/WP4/WP5/WP6/WP7 资源基线：compiler-owned `Copy`、内部 resource summary、deterministic resource dump、whole-local move analysis、move 后重新初始化、use-after-move diagnostics、cleanup action stack、drop flag、IR `drop` / `drop_if` cleanup 节点、destructor body identity、stable drop-glue key、target-independent drop-glue planner、IDE hover resource summary 和 `aurex-lsp` stdio 入口。
16. M7a borrow/lifetime 内部事实基线：`BodyFlowGraph`、`BodyLoanCheckResult`、`FunctionBorrowSummary`、projection-aware conflict matrix、direct call borrowed-return summary propagation、borrow summary / loan check query fingerprint、IDE `borrow_summary` / `body_loan_check` semantic facts，以及 primary conflict / loan creation / invalidating action / later carrier use diagnostics。
17. M7c lifetime/dropck facts：origin-qualified reference、lifetime facts、deterministic outlives solver、return-origin subset、storage escape、type-outlives、dropck facts 和 IDE/query projection。
18. M7d-B struct field place-state 子集：本地 owned struct field partial move/reinit、字段级 cleanup/drop flag、generic template body-flow 类型读取、IR lowering 字段 drop flag，以及 owned generic return summary 与 place-state 的分层处理。
19. M7d-F tuple element place-state 子集：tuple 数字字段访问、本地 owned tuple element partial move/reinit、tuple element borrow disjoint proof、tuple 元素级 cleanup/drop flag，以及嵌套 tuple/struct droppable leaves 的结构化 cleanup 展开。
20. M8 borrowed dyn trait：`dyn Trait` syntax/sema、object-callability diagnostics、`&T` / `&mut T` 到 `&dyn Trait` / `&mut dyn Trait` 的 coercion、checked vtable layout/method slot/coercion facts、IR trait-object pack/extract/vtable-slot、LLVM vtable global / indirect call、default method slot 和 associated equality dispatch。
21. M13/M14 borrowed composition-to-supertrait runtime projection 和 view path inference：`dynproject[SourcePrincipal, TargetSupertrait](view)` 在 sema 层显式选择 composition 中的 source principal，再检查 target 是该 source 的 supertrait，并记录 `CompositionProjectionFact{kind=composition_to_supertrait}`；IR/backend 会 lowering 为 `trait_object_composition_project` + `trait_object_upcast`，不新增 runtime metadata policy。M14 进一步支持唯一 source-principal path 下的 `let parent: &dyn Parent = view;` 和 `view.parent()`，并记录 `BorrowedDynViewPathFact`。
22. M18 project-level dyn ownership runtime boundary gate：`DynOwnershipRuntimeBoundaryGate`、checkpoint facts、lowering design gate facts 和 `dyn_ownership_runtime_boundary_gate` query 已把 M17 runtime-preparation facts 接入 query/cache/tooling/reuse/workspace index；它只记录 future IR/verifier/runtime lowering prerequisites，并继续阻止标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、dynamic Drop dispatch 和 borrowed vtable destructor slot 被声明为已实现。
23. M19 dyn ownership runtime IR / verifier preparation：`DynOwnershipRuntimeIrVerifierFact`、`FunctionDynOwnershipRuntimeIrVerifierFacts`、`function_dyn_ownership_runtime_ir_verifier_facts()`、`TraitObjectVTableLayout::destructor_slot_blocked` 和 `CleanupAbiPolicy::dynamic_erased_drop_blocked` blocked negative sentinel 已把 M18 prerequisites 落成 verifier-visible IR facts、dump/fingerprint 和负例矩阵；它仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。
24. M20a owned dyn runtime admission design gate：`OwnedDynRuntimeAdmissionGate`、`OwnedDynRuntimeAdmissionFact`、`OwnedDynRuntimeAdmissionCapability`、`OwnedDynRuntimeAdmissionStage`、`OwnedDynRuntimeAdmissionPolicy` 和 `m20_owned_dyn_runtime_admission_gate_baseline()` 已把 M17/M18/M19 facts 组合成 owned dyn runtime admission summary/fingerprint/dump。它只决定后续 M20b/M20c/M20d/M21 的准入顺序，仍不实现标准库、`Box<dyn Trait>`、allocator API、owning dyn 用户值、runtime ABI lowering、backend runtime helper call 或 dynamic Drop runtime。
25. M20b owned dyn IR shape prototype gate：`OwnedDynObjectLayoutPrototype`、
    `OwnedDynIrShapePrototypeGate`、`m20b_owned_dyn_ir_shape_prototype_gate_baseline()` 和
    `ir::owned_dyn_ir_shape_prototype_gate()` 已把 compiler-owned two-field `{*mut u8 data, *const u8 vtable}`
    prototype 接入 Module copy/move、IR dump、layout ABI fingerprint、verifier、query summary/dump/fingerprint 和
    negative matrix。drop identity slot、allocator identity slot、标准库、`Box<dyn Trait>`、owning dyn 用户值、
    allocator API、runtime lowering、backend helper 和 dynamic Drop runtime 仍保持 blocked。
26. M20c drop / allocator identity prerequisite gate：`OwnedDynDropAllocatorIdentityGate`、
    `OwnedDynDropAllocatorIdentityFact`、`m20c_owned_dyn_drop_allocator_identity_gate_baseline()` 和
    `ir::owned_dyn_drop_allocator_identity_gate()` 已把 compiler-owned erased drop identity 与 allocator identity
    固定成 stable IR/query prerequisites。`OwnedDynObjectLayoutPrototype::erased_drop_identity_key` 和
    `OwnedDynObjectLayoutPrototype::allocator_identity_key` 已接入 verifier、dump、layout ABI fingerprint 和
    M20b/M20c adapters；它们仍不是 runtime slot、allocator API 或用户可写 owning dyn 语法。
27. M20d runtime lowering ABI design closure：`OwnedDynRuntimeLoweringAbiGate`、
    `OwnedDynRuntimeLoweringAbiFact`、`m20d_owned_dyn_runtime_lowering_abi_gate_baseline()` 和
    `ir::owned_dyn_runtime_lowering_abi_gate()` 已把 compiler-owned runtime ABI descriptor、blocked-to-admitted
    transition guard、backend helper prerequisite、drop/allocator runtime bridge 和 dynamic Drop blocker 固定成
    stable query/IR facts。`runtime_abi_descriptor_key` 和 `backend_helper_identity_key` 只是 stable fingerprints；
    它们仍不是 executable runtime lowering、backend helper call、allocator API 或用户可写 owning dyn 语法。
28. M20g default / named call arguments：函数和 method 参数默认值、call-site named argument label、
    checked `ordered_args`、默认值填充、命名实参重排、diagnostics、IR lowering、borrow summary、body flow graph、
    body loan precheck、place-state precheck、move analysis、borrow escape / lambda capture ordered traversal 和
    native execution regression 已收口。它仍不是标准库能力，也不是 ABI 级 optional/default parameter metadata。

这些能力说明 Aurex 已经超过“基础表达式语言”。仍需继续收口的是更高阶的抽象边界，例如 `?` 目前已有结构化 Result/Option shape 检查但还没有绑定到未来标准库定义；完整 Rust-style lifetime surface、raw pointer alias safe proof、indexed move-out、array/slice/index 精确 disjoint proof 和 replace/take/swap primitives 尚未落地。

## 已删除或明确不在 M2 基线的能力

M2 已删除：

- `std/` 源树。
- host C support 自动链接。
- driver std 查找和隐式 import path 注入。
- `--stdlib`、`--std-backend`、`--no-stdlib`。
- M1 std/system/build-tool/selfhost 样例。
- 标准库名字特判。
- `move(...)`。
- `noncopy struct`。
- use-after-move 追踪。

这些不是永久禁止，而是不能作为当前语言地基继续推进。旧 M1 资源语义已移除；当前资源语义入口是 M6 的
compiler-owned `Copy`、内部 resource summary、whole-local move analysis、cleanup lowering 和 stable drop-glue identity。

## 未完成特性总清单

### 类型系统未完成

- tuple struct / anonymous record。
- owning dyn / `Box<dyn Trait>` / allocator / trait-object Drop dispatch / 多 trait object composition。
- 标准库 dyn ownership surface、owning trait-object coercion、runtime ABI lowering 和 dynamic Drop runtime；M18 已把这些能力固定为 future lowering design gate，M19 已落成 IR/verifier blocked facts，M20a 已固定 admission order，M20b 已落成 compiler-owned IR shape prototype，M20c 已落成 drop / allocator identity prerequisites，M20d 已落成 runtime ABI design closure facts，但这些仍不是当前语言能力。
- `Drop` 用户 bound 和 generic Drop impl 仍暂缓；`Copy` 已作为 compiler-owned capability 进入 M6-WP2，destructor body identity、drop-glue key 和窄 `impl Drop for T` custom destructor lowering 已进入 M7 基线。
- generic associated type。
- associated const。
- specialization / negative impl / unsafe trait / auto trait。
- generic const arithmetic、const where predicate、associated const、dyn const equality 和 runtime const-param array ABI；
  M16 已完成用户可写 typed scalar const generic check-only。
- never type。

### 资源与安全未完成

- 用户 destructor syntax / custom destructor lowering / partial aggregate rollback codegen。
- 完整 Rust-style lifetime surface、HRTB、variance 和 full Polonius Datalog。
- raw pointer alias safe proof。
- indexed move-out、array/slice/index 精确 disjoint proof、pattern payload move 和 non-`Copy` `?` payload transfer；本地 tuple element partial move/reinit 已由 M7d-F 补上。
- borrowed/reference base 的 resource field overwrite。
- `replace` / `take` / `swap` compiler-known primitives。
- unsafe/concurrency aliasing model。
- owned resource API 约束。

### 表达式与 pattern 未完成

- range pattern。
- string/byte pattern。
- 默认参数 / 命名参数的第一版已完成；仍未完成的是默认表达式引用前序参数、function value / lambda 参数名元数据、
  enum constructor 命名 payload、ABI 级 optional/default metadata 和 C ABI / variadic default support。
- match guard 已区分无 guard、字面量 true/false 和动态表达式；dynamic slice 代表长度 witness 和 open integer literal usefulness 已进入穷尽/不可达主路径。
- shared / mutable / consuming closure capture、generic closure environment ABI、borrowed closure environment escape、
  `Fn` / `FnMut` / `FnOnce` 风格能力和 heap/runtime closure box；M20 已支持无捕获 lambda 与 Copy-by-value
  捕获闭包核心子集。
- operator overloading。
- user-defined implicit conversion。
- `for item in container` / iterator protocol。

### 模块、包和工程能力未完成

- package manifest。
- package root 和 package/module 映射。
- public surface dump。
- selective import。
- glob import。
- versioned dependency。
- 库层重建。
- selfhost / Stage1。
- build tool。

### 高级能力暂缓

- class / inheritance / dynamic dispatch。
- owning dyn / 多 trait object composition。
- 完整 macro / proc-macro / 用户自定义 derive；当前只有编译器内建 `#[derive(Copy, Eq, Hash)]` 和
  M21a macro design gate facts。
- comptime。
- async / coroutine。
- effect system。
- concurrency capability：`Send` / `Sync` 等。
- GC 或托管运行时。

## 基础语法优先级评估

M2 接下来应先完善基础语法。建议不要先做 trait、borrow、class、macro、async 或 std。

### P0：现代基础语法第一优先级

这一组是下一阶段第一优先级。对照面不只限 Rust、Go、Zig、Kotlin、C++，还包括 Swift、C#、TypeScript/JavaScript、Python、Dart、Scala、OCaml/F#、Haskell、Julia、Nim、D、V，以及 ML/ADT、pattern matching exhaustiveness、null safety、type soundness、unsafe boundary 相关研究。共同结论是：Aurex 的函数、block、if/while/for、module/import、struct、method、match、C FFI 这些大语法骨架已经具备；真正需要先补的是现代语言共同证明过的基础表达能力，而不是 trait、borrow checker、macro、async、std 或 iterator protocol。

1. 最小 `unsafe` block / `unsafe fn`。已补

   raw pointer dereference、`ptrcast`、`bitcast`、`ptrat`、`strraw` 可能破坏 aliasing、layout、UTF-8、pointer validity 等语言不变量，当前已经必须被语法显式圈起来：

   ```aurex
   unsafe {
       let p = ptrat[*mut Header](address);
       *p = Header { len: 4 };
   }

   unsafe fn from_raw(data: *const u8, len: usize) -> str {
       return strraw(data, len);
   }
   ```

   已冻结规则：unsafe-only builtin 和 raw pointer dereference 在 safe context 下诊断；调用 `unsafe fn` 或 unsafe 函数指针必须处于 unsafe context。当前不包含 borrow checker、lifetime、unsafe trait、unsafe impl、unsafe extern block 或用户 destructor surface。

2. ADT-first enum 语法。已补

   普通 enum 已经让 ADT 成为默认形态：base type 和 case discriminant 可省略，tag 自动分配；显式 `enum Status: u8 { ok = 0, err = 1 }` 仍保留给 C-like/repr enum。generic enum 已纳入 M2 基线，下面的 `Option[T]` / `Result[T, E]` 是当前可通过的类型参数 ADT：

   ```aurex
   enum Option[T] {
       some(T),
       none,
   }

   enum Result[T, E] {
       ok(T),
       err(E),
   }
   ```

  当前仍不包含 GADT、generic const arithmetic、qualified where predicate 或 generic trait predicate argument。

3. array literal / repeat literal。已补

   数组类型 `[N]T` 已经存在，现在固定长度数组值语法也已补齐：

   ```aurex
   let bytes: [4]u8 = [1, 2, 3, 4];
   let zeroes: [128]u8 = [0; 128];
   let matrix: [2][2]i32 = [[1, 2], [3, 4]];
   ```

   这不需要 std 容器，也不等同于 iterator protocol。

### 已补齐：borrowed slice type / slice expression

slice 是数组、`str`、buffer、FFI wrapper 的共同地基。当前 M2 已经先落地 borrowed slice，不提前做容器迭代：

```aurex
[]const u8
[]mut T

let part = bytes[start:end];
let tail = bytes[start:];
let head = bytes[:end];
let all = bytes[:];
```

当前规则：slice 是 `ptr + len` fat value；可从 array 或 slice 产生；`[]mut T` 可赋给 `[]const T`，反向不允许；`slice[i]` 沿用 index 表达式；不包含 iterator protocol 或运行时 bounds check。

### P1：基础可用性补齐

1. raw string / multiline raw string / bytes string / Unicode scalar `char`。已补

   M2 当前已有 `"..." -> str`、`c"..." -> *const u8`、`r"..." -> str`、`b"..." -> [N]u8`、`b'a' -> u8` 和 `'λ' -> char`：

   ```aurex
   r"c:\tmp\file.txt"
   r"line 1
   line 2"
   b"abc"
   'λ'
   ```

   规则已经冻结为：raw string 不解释 escape 且允许换行；byte string 结果类型是固定数组 `[N]u8`，拒绝 Unicode escape 和非 ASCII raw bytes；`char` 表示 Unicode scalar value，不是 `u8` 或 C `char`。

2. function pointer / function type（已补入 M2 基线）

   M20 已支持非泛型依赖、非 borrowed-view 的 Copy-by-value 捕获闭包核心子集；完整 shared/mutable/consuming
   capture mode、generic closure environment ABI 和 borrowed closure environment escape 继续暂缓。非捕获函数类型、
   无捕获 lambda 和 C callback 已作为基础系统能力落地：

   ```aurex
   type Cmp = fn(a: *const void, b: *const void) -> i32;
   type Callback = extern c fn(ctx: *mut void) -> void;
   let inc: fn(i32) -> i32 = fn(value: i32) -> i32 => value + 1;
   ```

   函数名和无捕获 lambda 可作为函数指针值，局部变量、参数和 struct 字段里的函数指针可直接调用；`...` variadic
   只允许出现在 `extern c fn` 类型中。捕获闭包当前是内部匿名环境 record 值，不是薄函数指针；闭包调用经由
   hidden-env thunk。尚未完成的是 mutable/consuming capture mode、generic-dependent 捕获、borrowed closure
   environment escape、`Fn`/`FnMut`/`FnOnce` 风格能力、标准库 adapter 和 heap/runtime closure box。

3. 已补齐：tuple / destructuring declaration

   tuple 和 destructuring 是 Rust、Swift、Python、Dart、Scala、ML-family、C++ structured binding 等共同采用的基础人体工程学。Aurex 当前已经先做有限形态：

   ```aurex
   let pair: (i32, bool) = (1, true);
   let (count, ok) = pair;
   let single: (i32,) = (count,);
   return count;
   ```

   当前边界：空 tuple 不支持；tuple struct 和 anonymous record 暂缓。

4. 已补齐：pattern 扩展到 `let` / `match` / `if value is pattern` / `while value is pattern` / struct pattern

   ADT 如果没有轻量 destructuring，使用成本会过高：

   ```aurex
   if opt is .some(value) {
       return value;
   }

   match point {
       Point { x: 0, y } => y,
       Point { x, y } => x + y,
   }
   ```

   `let ... else`、slice pattern、binding or-pattern alternatives、guard literal truth 建模、dynamic slice 代表长度 witness、open integer literal usefulness 和有限结构化穷尽性已补齐。

### P2：已冻结或中等优先级

1. 统一 block 语法：已补

   block expression 已经可以完整承载普通 statement，主体规则与普通 block 共用。当前冻结为：

   ```text
   block = "{" stmt* tail_expr? "}"
   ```

   expression context 要求 tail；statement/function body context 暂不引入隐式 return。

2. 冻结 expression statement 规则：已补

   当前语义已收口到 call / try expression，并已有负例保持 `x + y;` 这类无效语句的诊断。

3. trailing separator 策略已冻结

   当前规则：圆括号列表和方括号列表允许 trailing comma；comma 分隔的花括号列表允许 trailing comma，但最后一个元素不强制 comma；struct field declaration 继续使用 `;`，最后一个 field 仍按 statement-like 风格保留 `;`。这条已由 parser 单测和 `tests/samples/positive/core/trailing_separator_policy.ax` 覆盖。

4. public 函数返回类型显式化：已补

   `pub fn`、`extern c fn`、`export c fn` 和 prototype 必须显式返回类型。private helper 可以暂时保留推导。

5. namespace 规则冻结

   `.` 统一用于 module path、import alias item、type、const/function、enum constructor、field 和 method；语义按 base kind 区分 module / type / value / member。表达式侧已支持 `samplelib.visibility.answer`、`samplelib.visibility.PublicBox.new()`、`samplelib.visibility.PublicChoice.yes` 这类 visible module path selector。`::` 不再是 Aurex 源语法。

   shadowing 规则保持严格：local/parameter 不能 shadow import alias、visible root module、generic type parameter 或 visible type name；内层 local 仍可 shadow 外层 local。

6. `str` 安全边界：已补 M2 no-std checked 构造和切片

   保留 `str` 语言内建，unchecked 构造 `strraw(data, len)` 放入 unsafe；checked UTF-8 构造先作为语言内建冻结，不依赖旧 std：`strvalid(bytes) -> bool`、`strfromutf8(bytes) -> str`。两者接受 `[]const u8` / `[]mut u8`；失败时 `strfromutf8` 返回空 `str`，不会把无效 byte slice 包成 `str`。`text[l:r]` 已作为基础语法落地，按 byte offset 检查 bounds 和 UTF-8 boundary；owned `String`、`Bytes`、`CStr` 等继续后置到库层/资源语义之后。

7. safe reference：已补最小 M2 版本

   `&T` / `&mut T`、`&place` / `&mut place`、reference 安全解引用和 `&mut` 可写性检查已落地。M7a 已加入
   CFG-sensitive local loan checker、borrowed-return `BorrowSummary`、projection-aware conflict、query/tooling
   projection 和多点 borrow diagnostics；M7c 已加入 lifetime/dropck facts，M7d-B 已加入本地 owned struct field
   partial move/reinit/drop flag 子集；当前仍不暴露完整 Rust-style lifetime surface，不证明 raw pointer alias safe，
   不实现 index/slice 精确 disjoint proof、indexed move-out 或 replace/take/swap 的完整 place-level resource semantics；本地 tuple element partial move/reinit 已由 M7d-F 补上。

### P3：M5 后独立设计流

- labeled break/continue。
- match/switch statement context。
- octal / hex float。
- doc comment。
- user destructor syntax / partial aggregate rollback codegen。
- owning dyn / trait-object Drop dispatch / 歧义 composition-to-supertrait 自动选择；M11a 完成
  principal-set composition design/query gate，M11b 完成 `PrincipalSetCompositionFacts` query prototype，M11c 完成
  `dyn (A + B)` frontend/sema，M11d 完成显式 composition-to-principal IR/backend runtime projection，M11e 完成
  composition runtime facts/query/tooling/verifier hardening，M12a 完成唯一 principal method direct dispatch，M12b 完成
  receiver-access binding、associated equality direct dispatch、projection/ABI descriptor 去重和 query/cache
  fingerprint hardening，M13a 完成 borrowed composition-to-supertrait design/query gate，M13b 完成
  `dynproject[SourcePrincipal, TargetSupertrait](view)` frontend/query/sema check-only，M13c 已完成该显式投影的
  IR/backend runtime lowering，M13d 已完成 `composition_supertrait_chains`、query/cache/tooling hover、verifier
  negative matrix 和 docs/tests/coverage release closure，M14 已完成唯一 path 的 expected-type projection、
  direct supertrait method dispatch 和 `BorrowedDynViewPathFact` release closure。M17/M18/M19/M20a/M20b/M20c/M20d 已完成 dyn ownership
  runtime preparation、runtime boundary gate、IR/verifier preparation、owned dyn runtime admission gate、compiler-owned
  IR shape prototype gate、drop / allocator identity prerequisite gate 和 runtime lowering ABI design closure，把 owning dyn、
  erased drop glue、allocator、cleanup/dropck、query/cache/tooling/reuse/workspace index、future IR/verifier lowering
  prerequisites 与 admission order 固定为 facts。
- specialization / default associated type / minimal implementation annotation / 更强 trait solver。
- borrow checker。
- package。
- 库层重建。

这些都重要，但不应重新打开 M5 static default-method baseline。default private、ADT-first enum、generic enum、generic type alias、owner generic impl、`where` capability / trait predicate、array literal / repeat literal、slice type/expression、checked `str` slicing、tuple/destructuring、function pointer / function type、最小 unsafe、最小 safe reference、nominal static trait、第一版 associated type model 和 trait default method body 已完成，不再作为未完成前置项。

## 近期执行建议

建议按这个顺序开工：

1. 保持当前 grammar 的 EBNF、syntax matrix、parser/sema tests 同步。
2. 继续保持 `where` capability / trait predicate 和 borrowed dyn trait 的文档、测试和诊断同步；M9 dyn ABI / tooling release closure 已完成，M10a 已选择 supertrait upcasting 并固定设计基线，M10b 已完成
   frontend/query/sema check-only 实现，M10c 已完成 IR/backend runtime，M10d 已完成 hardening/release closure；M11a 已选择
   principal-set borrowed dyn composition 并固定 `principal_set_metadata_v1` design/query gate，M11b 已完成
   `PrincipalSetCompositionFacts` query prototype，M11c 已完成 `dyn (A + B)` frontend/sema，M11d 已完成显式
   IR/backend runtime projection，M11e 已完成 hardening/release closure，M12a/M12b 已完成 direct composition
   dispatch release closure，M13a 已完成 remaining advanced dyn design gate，M13b 已完成
   `dynproject[...]` check-only 显式 supertrait projection，M13c 已完成 runtime lowering，M13d 已完成 release closure，
   M14 已完成唯一 borrowed dyn view path inference；M17/M18/M19/M20a/M20b/M20c/M20d 已完成 dyn ownership runtime facts、
   project-level boundary gate、IR/verifier preparation、owned dyn runtime admission gate、owned dyn IR shape
   prototype gate、drop / allocator identity prerequisite gate 和 runtime lowering ABI design closure；下一步应进入标准库 / owning dyn runtime surface 入口评估。owning dyn 用户值、trait-object
   Drop dispatch、allocator、
   principal-set owning composition、标准库 API 和完整 Rust-style lifetime surface 继续作为独立后续包。
3. 继续保持 match witness、dynamic slice/open integer 回归样例和 guard 精确覆盖规则的测试同步。
4. 继续把 unsafe 维持在最小边界，不扩展到 unsafe trait/impl/extern block；FFI ownership adoption 等资源 unsafe API 等 M6 后续专门设计。

## 参考

这些资料只作为 M2 设计参考，不表示 Aurex 要照搬：

- Rust Reference: Block expressions: https://doc.rust-lang.org/reference/expressions/block-expr.html
- Rust Reference: The unsafe keyword: https://doc.rust-lang.org/reference/unsafe-keyword.html
- Rust Compiler Development Guide: Unsafety checking: https://rustc-dev-guide.rust-lang.org/unsafety-checking.html
- Zig Language Reference: https://ziglang.org/documentation/master/
- Go Language Specification: https://go.dev/ref/spec
- Swift API Design Guidelines: https://www.swift.org/documentation/api-design-guidelines/
- Swift Programming Language: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/
- Microsoft C# Language Reference: https://learn.microsoft.com/dotnet/csharp/language-reference/
- TypeScript Handbook: https://www.typescriptlang.org/docs/handbook/intro.html
- Python Language Reference: https://docs.python.org/3/reference/
- Dart Language: https://dart.dev/language
- Scala 3 Reference: https://docs.scala-lang.org/scala3/reference/
- OCaml Manual: https://ocaml.org/manual/
- F# Language Reference: https://learn.microsoft.com/dotnet/fsharp/language-reference/
- Luc Maranget, Warnings for pattern matching.
- Robin Milner, A Theory of Type Polymorphism in Programming.
- Wright and Felleisen, A Syntactic Approach to Type Soundness.

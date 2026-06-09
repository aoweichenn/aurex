# Aurex M16 Const Generic Frontend / Query / Sema Check-Only Release Baseline

状态：M16 已完成 const generic 的 parser / AST / query identity / sema check-only 子集。M16 不实现标准库、不实现 runtime ABI lowering、不实现 generic const arithmetic，也不引入 user function comptime evaluation。

## 完成范围

M16 把 M15 const generic design gate 选定的 typed scalar const parameter 路线落到当前编译器前端和语义层：

- `syntax::GenericParamKind::const_` 已加入 AST，和已有 type parameter、`origin` parameter 共同组成 ordered generic parameter list。
- `syntax::GenericArgKind::{type,const_expr}` 已加入 AST，泛型实参可以按声明顺序混合 type argument 和 const argument。
- `syntax::ArrayLengthKind::{literal,const_expr}` 已加入 AST，`[N]T` 可以保留 const-param array length 的语法身份。
- parser 已支持 `const N: usize` 这类 typed const generic parameter、`ArrayView[i32, 4]` 这类 mixed generic args，以及 `[N]T` array length。
- AST dump 会打印 `struct ArrayView[T, const N: usize]`、`ArrayView[i32, 4]` 和 `[N]T`。
- sema 已支持 integer / bool / char 标量 const parameter type；`str`、struct、slice、dyn 等非标量 const parameter type 会被拒绝。
- generic template side table 已纳入 const parameter declared type、mixed generic args 和 `[N]T` length expression 的 reachable AST span。
- generic instance key 已加入 `const_args` fingerprint，`ArrayView[i32, 4]` 和 `ArrayView[i32, 5]` 不再只能靠 display text 区分。
- generic parameter query key 已区分 `query::GenericParamKind::type` 和 `query::GenericParamKind::const_`，且 origin parameter 不会污染 ordered type / const param identity。
- generic context 已绑定 const parameter type、identity 和 const argument fingerprint；函数体内未限定名称 `N` 可以解析为当前 const generic parameter value。
- type system 已加入 `sema::ArrayLengthKind::const_param` 和 `ArrayLengthInfo`，`[N]T` 会保留 const param identity、name、type 和 fingerprint。

## 当前可写语法

M16 当前可写的 const generic 子集如下：

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

规则：

- const parameter 必须写成 `const Name: Type`，不支持 untyped const parameter。
- 当前 const parameter type 只接受 integer、`bool` 和 `char` 标量类型。
- const argument 当前接受标量 literal 或当前 generic context 中的 const parameter name；const parameter name
  转发时必须和目标 const parameter type 一致。
- `[N]T` 当前是 check-only array length 集成点；它能进入 canonical type / fingerprint，但 unresolved const-param array 不会 lowering 到 runtime layout。
- mixed generic argument 的顺序按声明参数检查：type parameter 必须拿 type argument，const parameter 必须拿 const argument。

## 明确非目标

M16 仍然不实现：

- 标准库 API、`Box`、allocator API 或拥有型容器。
- runtime ABI / LLVM lowering for unresolved const-param arrays。
- generic const arithmetic，例如 `N + 1`。
- user function comptime evaluation。
- const where predicate、const equality trait solver、associated const、const generic trait predicate argument。
- dyn trait const equality dispatch。
- specialization、GADT、generic associated type。

这些不是永久禁止，而是需要后续 comptime engine、trait solver extension、runtime layout policy 或标准库阶段分别设计。

## Query 和增量边界

M16 没有删除 M15 的 `m15_const_generic_design_gate_baseline()`；它仍作为历史设计门保留，说明 const generic 选择 typed scalar / canonical value / generic instance key / `[N]T` integration 路线。

当前实现新增的关键查询边界是：

- const parameter identity 必须稳定，不能由 source display text 或 transient `TypeHandle` 代替。
- const argument fingerprint 必须进入 `GenericInstanceKey::const_args`。
- array const length fingerprint 必须混入 canonical type identity。
- generic template AST span 必须包含 const parameter type、const argument expression 和 `[N]T` length expression，否则 incremental/cache/tooling 可能漏失效。

## 测试覆盖

M16 已补齐 focused 覆盖：

- parser 正例：const parameter、mixed generic args、`[N]T`。
- AST dump 正例：const parameter、const argument、const array length。
- sema 正例：const generic struct instance carry const args、generic function body 使用 `N`、`[N]T` 解析为 const-param array length。
- sema 负例：非标量 const parameter type 被拒绝；const parameter name 转发到不同类型的目标 const parameter 会被拒绝并给出 expected/actual type note。
- query/whitebox：generic template node span 覆盖 mixed generic args 和 const length expr；ordered generic identity 跳过 origin params。

M16 完成时仍要求完整 gtest、query tests、coverage gate 和 `git diff --check` 通过。

## 后续阶段

下一阶段建议进入 **M17 Dyn Ownership Runtime Preparation**。M17 仍不实现标准库 API；它应该先补 owning dyn facts DTO、erased drop glue identity、cleanup/dropck boundary facts 和 allocator boundary facts，为 future owner container / `Box` / dynamic cleanup ABI 留出稳定 compiler facts。

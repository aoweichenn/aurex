# Aurex M15 Advanced Dyn Ownership / Const Generic Boundary Design Baseline

日期：2026-06-09
状态：M15 design baseline 已落到 query gate、文档和测试；不实现标准库、不实现 runtime owning dyn、不打开用户可用 const generic 语法。

## 目标

M15 不是继续在 borrowed dyn 上叠小功能，而是把两个已经反复阻塞后续设计的边界一次性定清楚：

1. advanced dyn ownership / runtime boundary：owning dyn、`Box<dyn Trait>`、dynamic Drop dispatch 和 allocator policy
   的职责边界、事实形状和非目标。
2. const generic boundary：typed const parameter、canonical const argument、generic instance key、array length
   集成、comptime 子集和 trait/dyn 边界。

M15 的代码入口是两个 query design gate：

- `m15_dyn_advanced_design_gate_baseline()`
- `m15_const_generic_design_gate_baseline()`

这两个 gate 都是 release-quality design facts：有 validation、summary、dump、stable fingerprint 和 focused query
tests。后续真正实现 parser/sema/lowering 时必须保持这些 gate 的语义约束，不能绕过 query/cache identity。

## Dyn Ownership / Runtime Boundary

M8 到 M14 已经把 borrowed dyn 的主路径收口：

- `borrowed_view_v1` 和 `borrowed_methods_only_v1` 固定 borrowed erased view。
- `supertrait_vptr_metadata_v1` 固定 borrowed supertrait upcast。
- `principal_set_metadata_v1` 固定 borrowed principal-set composition。
- M13/M14 的 composition-to-supertrait path 只复用现有 metadata，不新增 runtime metadata policy。

M15 因此明确：owning dyn 不能改写 borrowed dyn 的 ABI，也不能把 destructor slot 偷偷塞进 borrowed vtable。

### M15 Dyn Gate 决策

`m15_dyn_advanced_design_gate_baseline()` 包含 6 个 candidate：

| capability | M15 stage | decision | 说明 |
| --- | --- | --- | --- |
| `supertrait_upcasting` | `completed_release_baseline` | `requires_new_metadata_policy` | M10 已完成，M15 不重开 |
| `multi_trait_composition` | `completed_release_baseline` | `requires_new_metadata_policy` | M11/M12 已完成，M15 不重开 |
| `borrowed_composition_supertrait_projection` | `completed_release_baseline` | `composes_existing_metadata_policies` | M13/M14 已完成，M15 不重开 |
| `owning_dyn` | `design_gate` | `requires_standard_library_stage` | 只固定 owner/container/drop/allocator facts，不实现 runtime |
| `dynamic_drop_dispatch` | `design_gate` | `requires_runtime_stage` | 只固定 erased drop glue 和 cleanup boundary，不生成动态 drop |
| `allocator_policy` | `design_gate` | `requires_standard_library_stage` | 只固定 allocator identity / placement / deallocation boundary |

M15 dyn gate 固定的 policy / non-goal identifier：

- `owning_dyn_container_v1`
- `owning_dyn_metadata_v1`
- `dynamic_drop_metadata_v1`
- `allocator_placement_policy_v1`
- `allocator_metadata_v1`
- `standard_library_runtime_not_in_m15`
- `no_new_dyn_surface_in_m15`
- `owning_dyn_runtime_not_in_m15`
- `do_not_implement_box_dyn_trait_in_m15`
- `do_not_add_allocator_api_in_m15`
- `do_not_lower_owning_dyn_to_runtime_in_m15`
- `do_not_add_destructor_slot_to_borrowed_vtables_in_m15`
- `do_not_emit_dynamic_drop_dispatch_in_m15`
- `do_not_define_allocator_trait_or_std_module_in_m15`

### Owning Dyn 边界

M15 选择 future owning dyn 必须先有显式 owner/container policy。当前不接受如下 shortcuts：

- 不把 borrowed `{data*, vtable*}` 当作 owning value。
- 不把 `Box<dyn Trait>` 当作编译器内建标准库替身。
- 不在 M15 加 allocator API、allocator trait 或标准库模块。
- 不把 dynamic Drop dispatch 作为普通 method slot。
- 不让 borrowed dyn coercion 获得所有权。

M15 gate 固定后续必须补的 facts：

- `owned_dyn_container_layout_fact`
- `owned_dyn_move_boundary_fact`
- `owned_dyn_drop_obligation_fact`
- `owned_dyn_allocator_requirement_fact`
- `owned_dyn_tooling_boundary_fact`

这些 facts 的含义是：owning dyn 是一个资源 owner，会参与 move/drop/dropck/query identity；它不是 borrowed view 的
可变体。

### Dynamic Drop 边界

M15 固定 dynamic Drop dispatch 是 runtime cleanup ABI 问题，而不是 trait method dispatch 问题。

后续必须先有：

- `erased_drop_glue_identity_fact`
- `dynamic_drop_slot_layout_fact`
- `dropck_erased_receiver_fact`
- `cleanup_runtime_boundary_fact`

当前不会生成 dynamic drop dispatch，也不会给 borrowed vtable 增加 destructor slot。静态 drop glue、M6/M7 的 resource
和 cleanup lowering 继续是当前可执行基线。

### Allocator 边界

Allocator policy 归 future standard library / resource surface，不属于 M15 实现范围。

M15 只固定：

- `allocator_identity_fact`
- `allocator_placement_policy_fact`
- `owned_dyn_deallocation_policy_fact`
- `allocator_tooling_boundary_fact`

这避免把 allocator 烧进某个单一 runtime/global policy，也避免后续 `Box` / arena / placement owner 无法共存。

## Const Generic Boundary

当前仓库已经有：

- type generic：`fn id[T](value: T) -> T`
- origin generic：`fn f[origin data](value: &[data] T) -> &[data] T`
- array literal / array type：`[4]u8`
- const initializer 的标量子集
- query-level `GenericParamKind::const_` 预留

但当前还没有用户可用 const generic。AST 的 `syntax::GenericParamKind` 只有 `type` 和 `origin`；generic side table
当前把 generic parameter 映射到 `TypeHandle` placeholder；`GenericInstanceKey` 当前主要面向 canonical type
arguments。M15 的 const generic 工作就是把下一步实现顺序固定下来。

### 语法选择

M15 选择 typed const parameter surface，后续建议语法：

```aurex
struct ArrayView[T, const N: usize] {
    data: *const T;
}

fn len[T, const N: usize](value: &[N]T) -> usize {
    return N;
}
```

设计理由：

- `Name[T]` 已是 Aurex 泛型调用/类型实参语法；继续使用 `[]`，不引入 `<T>`。
- `const N: usize` 明确区分 type parameter 和 value parameter，不靠大小写或表达式猜测。
- 第一阶段只允许 scalar const arguments：bool、char、整数、usize/isize 和可规范化的 literal const。
- array length 可以成为首个消费点，因为 `[N]T` 已经是现有语言形状。

M15 不选择：

- Rust 风格的 `{N + 1}` const expression surface。
- Zig 风格任意 comptime 参数和值级函数执行。
- C++ 风格无类型模板参数推断。
- 把 const generic 参数拼进 display name 当 identity。

### M15 Const Generic Gate 决策

`m15_const_generic_design_gate_baseline()` 包含 6 个 candidate：

| capability | M15 stage | decision | 说明 |
| --- | --- | --- | --- |
| `typed_const_parameter_surface` | `ready_for_implementation` | `selected_m15_frontend_query_path` | 下一阶段可先做 parser/AST/sema check-only |
| `canonical_const_argument_identity` | `ready_for_implementation` | `selected_m15_frontend_query_path` | 必须先有 typed canonical value key |
| `generic_instance_key_integration` | `ready_for_implementation` | `selected_m15_frontend_query_path` | instance key 必须混入 const args |
| `array_length_type_integration` | `ready_for_implementation` | `selected_m15_frontend_query_path` | `[N]T` 是第一批实际消费点 |
| `const_expression_evaluation_subset` | `blocked_by_dependency` | `requires_comptime_engine` | `N + 1` 等泛型算术后移 |
| `trait_predicate_and_dyn_boundary` | `future_stage` | `requires_trait_solver_extension` | const where predicate / dyn const equality 后移 |

M15 const generic gate 固定的 selected policy identifier：

- `typed_const_param_v1`
- `canonical_const_value_v1`
- `generic_instance_const_arg_key_v1`
- `array_length_const_param_v1`
- `scalar_const_eval_subset_v1`
- `const_generic_trait_solver_boundary_v1`

### Required Facts

下一阶段实现 const generic 时必须先补这些 facts：

- `const_generic_param_decl_fact`
- `const_generic_param_type_fact`
- `const_generic_param_identity_fact`
- `canonical_const_value_key`
- `const_argument_type_key`
- `const_argument_value_fingerprint`
- `generic_param_kind_const_key`
- `generic_instance_const_arg_key`
- `generic_param_env_const_binding_fact`
- `array_length_const_param_fact`
- `array_type_const_length_key`
- `array_layout_const_length_fingerprint`

这些 facts 保证：

- `Array[i32, 4]` 和 `Array[i32, 5]` 是不同 instance。
- `4usize`、在期望 `usize` 下的 `4`、以及等价 canonical scalar value 可以稳定归一。
- query/cache 在 const argument value 或 type 改变时会失效。
- IR lowering 只接收已经解析出 concrete length 的 array type。

### M15 Const Generic 非目标

M15 不实现：

- untyped const params。
- generic const arithmetic，例如 `N + 1`。
- user function comptime evaluation。
- const where predicates。
- const associated values。
- dyn trait const equality dispatch。
- runtime const generic。
- 标准库。

对应的 query non-goal identifier：

- `standard_library_runtime_not_in_m15`
- `runtime_const_generic_not_in_m15`
- `dyn_const_generic_not_in_m15`
- `do_not_support_untyped_const_params`
- `do_not_accept_full_const_expressions_as_params_in_m15`
- `do_not_key_const_arguments_by_display_text`
- `do_not_allow_non_scalar_const_arguments_in_m15`
- `do_not_reuse_type_placeholder_for_const_params`
- `do_not_make_const_args_part_of_display_name_only`
- `do_not_support_generic_arithmetic_array_lengths_in_m15`
- `do_not_lower_unresolved_array_lengths_to_ir`
- `do_not_support_generic_const_arithmetic_in_m15`
- `do_not_evaluate_user_functions_in_const_generic_args`
- `do_not_support_const_where_predicates_in_m15`
- `do_not_add_const_associated_values_to_dyn_trait_in_m15`

## 与参考语言的取舍

M15 只借鉴成熟语言的工程边界，不照抄语法：

- Rust 的 const generics 证明 typed const parameter + value identity 是必要边界，但 Aurex 暂不引入 `{expr}` 泛型实参。
- Zig 的 comptime 证明编译期值能力很强，但 Aurex 现阶段没有完整 comptime engine，不能把任意函数执行放进泛型参数。
- C++ constant template parameters 证明 value-level template identity 会影响 ABI/cache，但 Aurex 不采用 `<...>` 和隐式无类型参数。

因此 M15 选择小而稳定的路线：typed scalar const generic + query identity + array length 集成优先；comptime 算术、trait solver 和 dyn 交互后移。

## 下一步

M15 结束后，下一阶段建议拆成两个可实现方向，不能混在一个提交里：

1. M16 Const Generic Frontend / Query / Sema Check-Only：
   - `syntax::GenericParamKind::const_`
   - typed const param parser
   - canonical const value key
   - generic instance key const argument
   - `[N]T` check-only
2. M17 Dyn Ownership Runtime Preparation：
   - owning dyn facts DTO
   - erased drop glue identity
   - cleanup/dropck boundary facts
   - allocator boundary facts

标准库仍后移。`Box`、allocator trait、owning container API 和 dynamic Drop runtime 不能在 M16/M17 之前偷跑。

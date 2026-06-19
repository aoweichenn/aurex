#include <gtest/integration/sample_suite/sample_suite_support.hpp>

namespace aurex::test {

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pointer_field_write)
{
    run_positive_runtime_smoke_sample("pointers", "pointer_field_write.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_eval_order_assign)
{
    run_positive_runtime_smoke_sample("evaluation", "eval_order_assign.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_const_binary)
{
    run_positive_runtime_smoke_sample("types", "const_binary.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_builtins)
{
    run_positive_runtime_smoke_sample("core", "builtins.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_compound_assignment)
{
    run_positive_runtime_smoke_sample("expressions", "compound_assignment.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_block_expression)
{
    run_positive_runtime_smoke_sample("expressions", "block_expression.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_closure_capture)
{
    run_positive_runtime_smoke_sample("functions", "lambda_closure_capture.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_closure_return)
{
    run_positive_runtime_smoke_sample("functions", "lambda_closure_return.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_reference_capture)
{
    run_positive_runtime_smoke_sample("functions", "lambda_reference_capture.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_mutable_reference_capture)
{
    run_positive_runtime_smoke_sample("functions", "lambda_mutable_reference_capture.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_default_capture)
{
    run_positive_runtime_smoke_sample("functions", "lambda_default_capture.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_init_capture)
{
    run_positive_runtime_smoke_sample("functions", "lambda_init_capture.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_lambda_move_capture)
{
    run_positive_runtime_smoke_sample("functions", "lambda_move_capture.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_array_slice)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_array_slice.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_range_value)
{
    run_positive_runtime_smoke_sample("control_flow", "for_range_value.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_str)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_str.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_protocol_direct)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_protocol_direct.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_protocol_direct_mut_ref)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_protocol_direct_mut_ref.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_protocol_iter_method)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_protocol_iter_method.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_protocol_iter_mut_method)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_protocol_iter_mut_method.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_protocol_trait_direct)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_protocol_trait_direct.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_for_in_protocol_generic_trait)
{
    run_positive_runtime_smoke_sample("control_flow", "for_in_protocol_generic_trait.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_tuple_basic)
{
    run_positive_runtime_smoke_sample("types", "tuple_basic.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_str_checked)
{
    run_positive_runtime_smoke_sample("types", "str_checked.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_str_slice)
{
    run_positive_runtime_smoke_sample("types", "str_slice.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pattern_ergonomics)
{
    run_positive_runtime_smoke_sample("pattern_matching", "pattern_ergonomics.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_pattern_remaining)
{
    run_positive_runtime_smoke_sample("pattern_matching", "pattern_remaining.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_const_pattern)
{
    run_positive_runtime_smoke_sample("pattern_matching", "const_pattern.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_generic_basic)
{
    run_positive_runtime_smoke_sample("generics", "basic_m2.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_generic_builtins)
{
    run_positive_runtime_smoke_sample("generics", "builtins_m3_1.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_method_local_generics)
{
    run_positive_runtime_smoke_sample("generics", "method_local_m3_1.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_method_local_identity_closure_generics)
{
    run_positive_runtime_smoke_sample("generics", "method_local_identity_closure_m3_1.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_static_dispatch)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_static_dispatch.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_associated_static_dispatch)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_associated_static_dispatch.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_associated_type_where_equality)
{
    run_positive_runtime_smoke_sample("traits", "trait_associated_type_where_equality.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_inherent_precedence)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_inherent_precedence.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_method_function_field_precedence)
{
    run_positive_runtime_smoke_sample("traits", "trait_method_function_field_precedence.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_trait_dyn_borrowed_dispatch)
{
    run_positive_runtime_smoke_sample("traits", "trait_dyn_borrowed_dispatch.ax");
}

TEST_F(AurexIntegrationTest, SampleSuite_PositiveRuntime_imported_samples)
{
    verify_import_runtime_samples();
}

} // namespace aurex::test

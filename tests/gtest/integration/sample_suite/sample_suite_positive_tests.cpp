#include <gtest/integration/sample_suite/sample_suite_support.hpp>

namespace aurex::test {

TEST_F(AurexIntegrationTest, SampleSuite_PositiveSamples)
{
    verify_positive_samples_llvm_ir();
    verify_const_enum_lowering();
    verify_generic_builtin_ir();
}

} // namespace aurex::test

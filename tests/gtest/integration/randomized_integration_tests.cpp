#include <support/randomized_source.hpp>
#include <support/test_support.hpp>

#include <aurex/driver/invocation.hpp>

#include <fstream>
#include <string>

namespace aurex::test {
namespace {

void write_source(const fs::path& path, const std::string& source) {
    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output) << "failed to create " << path;
    output << source;
    ASSERT_TRUE(output) << "failed to write " << path;
}

driver::CompilerInvocation randomized_invocation(const fs::path& source, const driver::EmitKind emit_kind) {
    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.tool_path = aurexc_path();
    invocation.emit_kind = emit_kind;
    invocation.import_paths.push_back(imports_root());
    return invocation;
}

} // namespace

TEST_F(AurexIntegrationTest, RandomizedGeneratedModulesPassDriverCheckAndIrEmission) {
    randomized::DeterministicRandom random(randomized::RANDOM_SOURCE_LEGAL_PROGRAM_SEED);
    for (base::usize index = 0; index < randomized::RANDOM_SOURCE_INTEGRATION_PROGRAM_COUNT; ++index) {
        SCOPED_TRACE("randomized driver integration case " + std::to_string(index));
        const fs::path source = tmp_root() / ("randomized_" + std::to_string(index) + ".ax");
        write_source(source, randomized::legal_program(random, index));

        require_compiler_success(randomized_invocation(source, driver::EmitKind::check));
        const CommandResult ir = require_compiler_success(randomized_invocation(source, driver::EmitKind::ir));
        expect_contains(ir.output, "fn main");
    }
}

TEST_F(AurexIntegrationTest, RandomizedMalformedModulesFailCleanlyThroughDriverCheck) {
    randomized::DeterministicRandom random(randomized::RANDOM_SOURCE_PARSER_RECOVERY_SEED);
    for (base::usize index = 0; index < randomized::RANDOM_SOURCE_INTEGRATION_PROGRAM_COUNT; ++index) {
        SCOPED_TRACE("randomized malformed driver case " + std::to_string(index));
        const fs::path source = tmp_root() / ("randomized_bad_" + std::to_string(index) + ".ax");
        write_source(source, randomized::parser_recovery_source(random, index));

        const CommandResult result = run_compiler(randomized_invocation(source, driver::EmitKind::check));
        EXPECT_NE(result.exit_code, 0) << result.output;
        EXPECT_FALSE(result.output.empty());
    }
}

} // namespace aurex::test

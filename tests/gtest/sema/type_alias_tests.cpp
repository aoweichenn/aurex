#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, TypeAlias) {
    const fs::path alias = positive_sample("types", "type_alias.ax");
    const std::string alias_tokens = require_success(aurexc() + " --dump-tokens " + q(alias)).output;
    expect_contains(alias_tokens, "kw_type `type`");

    const std::string alias_ast = require_success(aurexc() + " --emit=ast " + q(alias)).output;
    expect_contains_all(alias_ast, {
        "item #0 type_alias Count",
        "alias i32",
        "item #1 type_alias CountPtr",
        "alias *mut Count",
        "item #2 type_alias Bytes4",
        "alias [4]u8",
        "item #3 type_alias PacketAlias",
        "alias Packet",
    });

    const std::string alias_checked = require_success(aurexc() + " --emit=checked " + q(alias)).output;
    expect_contains_all(alias_checked, {
        "type_aliases 4",
        "type Count = i32",
        "type CountPtr = *mut i32",
        "type Bytes4 = [4]u8",
        "type PacketAlias = type_alias.Packet",
    });

    const std::string alias_ir = require_success(aurexc() + " --emit=ir " + q(alias)).output;
    expect_contains_all(alias_ir, {
        "fn read_count(value: *mut i32)",
        "fn main()",
        "record Packet @m0_type_alias_Packet",
        ".len: i32",
    });

    const std::string alias_llvm = require_success(aurexc() + " --emit=llvm-ir " + q(alias)).output;
    expect_contains(alias_llvm, "%m0_type_alias_Packet = type { i32 }");

    const fs::path alias_bin = test_bin_root() / "type_alias";
    require_success(aurexc() + " " + q(alias) + " -o " + q(alias_bin));
    EXPECT_EQ(require_success(q(alias_bin)).output, "");

    const fs::path imported = positive_sample("types", "type_alias_import.ax");
    const fs::path imported_bin = test_bin_root() / "type_alias_import";
    require_success(aurexc() + " -I " + q(imports_root()) + " " + q(imported) + " -o " + q(imported_bin));
    EXPECT_EQ(require_success(q(imported_bin)).output, "");

    const fs::path cycle = negative_sample("types", "type_alias_cycle.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(cycle)).output, "cyclic type alias");

    const fs::path duplicate = negative_sample("types", "type_alias_duplicate.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(duplicate)).output, "duplicate type definition");

    const fs::path opaque_value = negative_sample("types", "type_alias_opaque_value.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(opaque_value)).output, "opaque struct can only be used as a pointer target");
}

} // namespace aurex::test

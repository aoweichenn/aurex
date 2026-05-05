#include "support/test_support.hpp"

namespace aurex::test {

TEST_F(AurexIntegrationTest, ModuleVisibility) {
    const fs::path source = positive_sample("visibility", "visibility_import.ax");
    const std::string import_flags = sample_import_flags();

    const fs::path library = imports_root() / "samplelib" / "visibility.ax";
    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(library)).output;
    expect_contains_all(tokens, {
        "kw_pub `pub`",
        "kw_priv `priv`",
    });

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(library)).output;
    expect_contains_all(ast, {
        "item #0 const answer",
        "item #1 priv const hidden_answer",
        "item #4 struct PublicBox",
        "field value : i32",
        "field priv secret : i32",
    });

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn exported -> i32",
        "fn priv add_secret -> i32",
        "struct PublicBox",
        "type PublicInt = i32",
        "type priv HiddenInt = i32",
    });

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn make_box(value: i32)",
        "call m0_samplelib_visibility_exported",
        "call m0_samplelib_visibility_make_box",
        ".value: i32",
        ".secret: i32",
    });

    const fs::path bin = test_bin_root() / "visibility_import";
    require_success(aurexc() + " " + import_flags + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path private_function = negative_sample("visibility", "private_function_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_function)).output, "unknown function: add_secret");

    const fs::path private_qualified_function = negative_sample("visibility", "private_qualified_function.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_qualified_function)).output,
        "function is private: samplelib.visibility.add_secret"
    );

    const fs::path private_const = negative_sample("visibility", "private_const_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_const)).output, "unknown name: hidden_answer");

    const fs::path private_type = negative_sample("visibility", "private_type_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_type)).output, "unknown type: HiddenInt");

    const fs::path private_field = negative_sample("visibility", "private_field_access.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_field)).output, "field is private: secret");

    const fs::path private_struct = negative_sample("visibility", "private_struct_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_struct)).output, "unknown type: HiddenBox");

    const fs::path private_enum = negative_sample("visibility", "private_enum_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output, "unknown name: HiddenChoice_yes");

    const fs::path unknown_alias = negative_sample("modules", "unknown_import_alias.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_alias)).output, "unknown import alias: missing");

    const fs::path alias_source = positive_sample("modules", "import_alias_qualified_call.ax");
    const std::string alias_tokens = require_success(aurexc() + " " + import_flags + " --dump-tokens " + q(alias_source)).output;
    expect_contains_all(alias_tokens, {
        "kw_as `as`",
        "colon_colon `::`",
    });

    const std::string alias_ast = require_success(aurexc() + " " + import_flags + " --emit=ast " + q(alias_source)).output;
    expect_contains_all(alias_ast, {
        "import samplelib.visibility as vis",
        "let answer : vis::PublicInt",
        "name `vis::answer`",
        "let boxed : vis::PublicBox",
        "name `vis::make_box`",
    });

    const std::string alias_ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(alias_source)).output;
    expect_contains_all(alias_ir, {
        "const_ref @m0_samplelib_visibility_answer",
        "call m0_samplelib_visibility_make_box",
        "call m0_samplelib_visibility_exported",
    });

    const fs::path alias_bin = test_bin_root() / "import_alias_qualified_call";
    require_success(aurexc() + " " + import_flags + " " + q(alias_source) + " -o " + q(alias_bin));
    EXPECT_EQ(require_success(q(alias_bin)).output, "");
}

TEST_F(AurexIntegrationTest, PublicImportReexport) {
    const fs::path source = positive_sample("visibility", "reexport_import.ax");
    const std::string import_flags = sample_import_flags();
    const fs::path facade = imports_root() / "samplelib" / "reexport_facade.ax";
    const fs::path private_facade = imports_root() / "samplelib" / "private_facade.ax";

    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(facade)).output;
    expect_contains_all(tokens, {
        "kw_pub `pub`",
        "kw_import `import`",
    });

    const std::string ast = require_success(aurexc() + " " + import_flags + " --emit=ast " + q(facade)).output;
    expect_contains(ast, "pub import samplelib.reexport_inner");
    const std::string private_ast = require_success(aurexc() + " " + import_flags + " --emit=ast " + q(private_facade)).output;
    expect_contains(private_ast, "priv import samplelib.reexport_inner");

    const std::string modules = require_success(aurexc() + " " + import_flags + " --dump-modules " + q(source)).output;
    expect_contains_all(modules, {
        "samplelib.reexport_facade",
        "samplelib.reexport_inner",
        "reexport_import",
    });

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn add_two -> i32",
        "fn add_base -> i32",
        "struct samplelib.reexport_inner.Pair<i32> fields=2",
        "type Count = i32",
        "case Mode_ready : samplelib.reexport_inner.Mode",
    });
    expect_contains(checked, "type priv SecretCount = i32");

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "call m0_samplelib_reexport_facade_add_two",
        "call m0_samplelib_reexport_inner_add_base",
        "record samplelib.reexport_inner.Pair<i32>",
        "const base @m0_samplelib_reexport_inner_base",
        "const_ref @m0_samplelib_reexport_inner_base",
    });

    const fs::path bin = test_bin_root() / "reexport_import";
    require_success(aurexc() + " " + import_flags + " " + q(source) + " -o " + q(bin));
    EXPECT_EQ(require_success(q(bin)).output, "");

    const fs::path private_import = negative_sample("visibility", "private_reexport_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_import)).output, "unknown type: Count");

    const fs::path private_const = negative_sample("visibility", "private_reexport_const.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_const)).output, "unknown name: hidden_base");

    const fs::path private_type = negative_sample("visibility", "private_reexport_type.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_type)).output, "unknown type: SecretCount");

    const fs::path private_enum = negative_sample("visibility", "private_reexport_enum.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output, "unknown name: SecretMode_hidden");
}

} // namespace aurex::test

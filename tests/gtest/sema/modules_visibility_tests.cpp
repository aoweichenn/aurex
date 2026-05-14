#include <support/test_support.hpp>

#include <fstream>

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
        "item #0 pub const answer",
        "item #1 priv const hidden_answer",
        "item #4 pub struct PublicBox",
        "field pub value : i32",
        "field priv secret : i32",
    });

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked, {
        "fn exported -> i32",
        "fn priv add_secret -> i32",
        "fn method samplelib.visibility.PublicBox.bump -> i32",
        "fn method samplelib.visibility.PublicBox.read -> i32",
        "fn priv method samplelib.visibility.PublicBox.secret_value -> i32",
        "struct PublicBox",
        "type PublicInt = i32",
        "type priv HiddenInt = i32",
    });

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "fn make_box(value: i32)",
        "call m0_samplelib_visibility_exported",
        "call m0_samplelib_visibility_make_box",
        "call m0_samplelib_visibility_PublicBox_read",
        "call m0_samplelib_visibility_PublicBox_bump",
        ".value: i32",
        ".secret: i32",
    });

    const std::string llvm_ir = require_success(aurexc() + " " + import_flags + " --emit=llvm-ir " + q(source)).output;
    expect_contains_all(llvm_ir, {
        "%m0_samplelib_visibility_PublicBox = type { i32, i32 }",
        "@m0_visibility_import_main",
    });

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

    const fs::path private_qualified_type = negative_sample("visibility", "private_qualified_type.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_qualified_type)).output,
        "type is private: samplelib.visibility.HiddenInt"
    );

    const fs::path private_field = negative_sample("visibility", "private_field_access.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_field)).output, "field is private: secret");

    const fs::path private_method = negative_sample("visibility", "private_method_access.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_method)).output,
        "method is private: samplelib.visibility.PublicBox.secret_value"
    );

    const fs::path private_struct = negative_sample("visibility", "private_struct_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_struct)).output, "unknown type: HiddenBox");

    const fs::path private_enum = negative_sample("visibility", "private_enum_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output, "unknown name: HiddenChoice");

    const fs::path private_qualified_const = negative_sample("visibility", "private_qualified_const.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(private_qualified_const)).output,
        "name is private: samplelib.visibility.hidden_answer"
    );

    const fs::path unknown_alias = negative_sample("modules", "unknown_import_alias.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_alias)).output, "unknown import alias: missing");

    const fs::path ambiguous_alias = negative_sample("modules", "ambiguous_import_alias.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(ambiguous_alias)).output,
        "ambiguous import alias: lib"
    );

    const fs::path unknown_qualified_function = negative_sample("modules", "unknown_qualified_function.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(unknown_qualified_function)).output,
        "unknown function in module samplelib.visibility: missing"
    );

    const fs::path unknown_qualified_type = negative_sample("modules", "unknown_qualified_type.ax");
    expect_contains(
        require_failure(aurexc() + " " + import_flags + " --check " + q(unknown_qualified_type)).output,
        "unknown type in module samplelib.visibility: Missing"
    );

    const fs::path alias_source = positive_sample("modules", "import_alias_qualified_call.ax");
    const std::string alias_tokens = require_success(aurexc() + " " + import_flags + " --dump-tokens " + q(alias_source)).output;
    expect_contains_all(alias_tokens, {
        "kw_as `as`",
        "dot `.`",
    });

    const std::string alias_ast = require_success(aurexc() + " " + import_flags + " --emit=ast " + q(alias_source)).output;
    expect_contains_all(alias_ast, {
        "import samplelib.visibility as vis",
        "let answer : vis.PublicInt",
        "field .answer",
        "var boxed : vis.PublicBox",
        "field .make_box",
        "field .value",
        "field .read",
        "field .bump",
    });

    const std::string alias_ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(alias_source)).output;
    expect_contains_all(alias_ir, {
        "const_ref @m0_samplelib_visibility_answer",
        "call m0_samplelib_visibility_make_box",
        "call m0_samplelib_visibility_PublicBox_read",
        "call m0_samplelib_visibility_PublicBox_bump",
        "call m0_samplelib_visibility_exported",
    });

    const std::string alias_llvm =
        require_success(aurexc() + " " + import_flags + " --emit=llvm-ir " + q(alias_source)).output;
    expect_contains_all(alias_llvm, {
        "@m0_import_alias_qualified_call_main",
        "call i32 @m0_samplelib_visibility_exported",
    });
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
        "struct PairI32 fields=2",
        "type Count = i32",
        "case Mode_ready : samplelib.reexport_inner.Mode",
    });
    expect_contains(checked, "type priv SecretCount = i32");

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir, {
        "call m0_samplelib_reexport_facade_add_two",
        "call m0_samplelib_reexport_inner_add_base",
        "record PairI32 @m0_samplelib_reexport_inner_PairI32",
        "const base @m0_samplelib_reexport_inner_base",
        "const_ref @m0_samplelib_reexport_inner_base",
    });

    require_success(aurexc() + " " + import_flags + " --emit=llvm-ir " + q(source));

    const fs::path private_import = negative_sample("visibility", "private_reexport_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_import)).output, "unknown type: Count");

    const fs::path private_const = negative_sample("visibility", "private_reexport_const.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_const)).output, "unknown name: hidden_base");

    const fs::path private_type = negative_sample("visibility", "private_reexport_type.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_type)).output, "unknown type: SecretCount");

    const fs::path private_enum = negative_sample("visibility", "private_reexport_enum.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output, "unknown name: SecretMode");
}

TEST_F(AurexIntegrationTest, DefaultPrivateVisibility) {
    const fs::path import_dir = tmp_root() / "default_private_imports";
    fs::create_directories(import_dir);
    const fs::path library = import_dir / "default_private_lib.ax";
    {
        std::ofstream out(library, std::ios::binary);
        out <<
            "module default_private_lib;\n"
            "const hidden_value: i32 = 7;\n"
            "fn hidden_fn() -> i32 { return hidden_value; }\n"
            "pub struct Box { value: i32; }\n"
            "pub fn make_box() -> Box { return Box { value: hidden_fn() }; }\n"
            "impl Box {\n"
            "  fn hidden_method(self: *const Box) -> i32 { return self.value; }\n"
            "  pub fn read(self: *const Box) -> i32 { return self.hidden_method(); }\n"
            "}\n";
    }

    const std::string library_ast = require_success(aurexc() + " --emit=ast " + q(library)).output;
    expect_contains_all(library_ast, {
        "item #0 priv const hidden_value",
        "item #1 priv fn hidden_fn",
        "item #2 pub struct Box",
        "field priv value : i32",
        "item #4 priv fn hidden_method",
        "item #5 pub fn read",
    });

    const fs::path use_public = tmp_root() / "default_private_public_use.ax";
    {
        std::ofstream out(use_public, std::ios::binary);
        out <<
            "module default_private_public_use;\n"
            "import default_private_lib as lib;\n"
            "fn main() -> i32 {\n"
            "  let box: lib.Box = lib.make_box();\n"
            "  return box.read() - 7;\n"
            "}\n";
    }
    require_success(aurexc() + " -I " + q(import_dir) + " --check " + q(use_public));

    const fs::path private_value = tmp_root() / "default_private_value.ax";
    {
        std::ofstream out(private_value, std::ios::binary);
        out <<
            "module default_private_value;\n"
            "import default_private_lib as lib;\n"
            "fn main() -> i32 { return lib.hidden_value; }\n";
    }
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_value)).output,
        "name is private: default_private_lib.hidden_value"
    );

    const fs::path private_function = tmp_root() / "default_private_function.ax";
    {
        std::ofstream out(private_function, std::ios::binary);
        out <<
            "module default_private_function;\n"
            "import default_private_lib as lib;\n"
            "fn main() -> i32 { return lib.hidden_fn(); }\n";
    }
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_function)).output,
        "function is private: default_private_lib.hidden_fn"
    );

    const fs::path private_field = tmp_root() / "default_private_field.ax";
    {
        std::ofstream out(private_field, std::ios::binary);
        out <<
            "module default_private_field;\n"
            "import default_private_lib as lib;\n"
            "fn main() -> i32 {\n"
            "  let box: lib.Box = lib.make_box();\n"
            "  return box.value;\n"
            "}\n";
    }
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_field)).output,
        "field is private: value"
    );

    const fs::path private_method = tmp_root() / "default_private_method.ax";
    {
        std::ofstream out(private_method, std::ios::binary);
        out <<
            "module default_private_method;\n"
            "import default_private_lib as lib;\n"
            "fn main() -> i32 {\n"
            "  let box: lib.Box = lib.make_box();\n"
            "  return box.hidden_method();\n"
            "}\n";
    }
    expect_contains(
        require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_method)).output,
        "method is private: default_private_lib.Box.hidden_method"
    );
}

} // namespace aurex::test

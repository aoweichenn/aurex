#include <support/test_support.hpp>

#include <fstream>

namespace aurex::test {

namespace {

fs::path write_visibility_test_source(const fs::path& path, const std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
    return path;
}

} // namespace

TEST_F(AurexIntegrationTest, ModuleVisibility)
{
    const fs::path source = positive_sample("visibility", "visibility_import.ax");
    const std::string import_flags = sample_import_flags();

    const fs::path library = imports_root() / "samplelib" / "visibility.ax";
    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(library)).output;
    expect_contains_all(tokens,
        {
            "kw_pub `pub`",
            "kw_priv `priv`",
        });

    const std::string ast = require_success(aurexc() + " --emit=ast " + q(library)).output;
    expect_contains_all(ast,
        {
            "item #0 pub const answer",
            "item #1 priv const hidden_answer",
            "item #4 pub struct PublicBox",
            "field pub value : i32",
            "field priv secret : i32",
        });

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
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
    expect_contains_all(ir,
        {
            "fn make_box(value: i32)",
            "call m0_samplelib_visibility_exported",
            "call m0_samplelib_visibility_make_box",
            "call m0_samplelib_visibility_PublicBox_read",
            "call m0_samplelib_visibility_PublicBox_bump",
            ".value: i32",
            ".secret: i32",
        });

    const std::string llvm_ir = require_success(aurexc() + " " + import_flags + " --emit=llvm-ir " + q(source)).output;
    expect_contains_all(llvm_ir,
        {
            "%m0_samplelib_visibility_PublicBox = type { i32, i32 }",
            "@m0_visibility_import_main",
        });

    const fs::path private_function = negative_sample("visibility", "private_function_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_function)).output,
        "unknown function: add_secret");

    const fs::path private_qualified_function = negative_sample("visibility", "private_qualified_function.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_qualified_function)).output,
        "function is private: samplelib.visibility.add_secret");

    const fs::path private_const = negative_sample("visibility", "private_const_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_const)).output,
        "unknown name: hidden_answer");

    const fs::path private_type = negative_sample("visibility", "private_type_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_type)).output,
        "unknown type: HiddenInt");

    const fs::path private_qualified_type = negative_sample("visibility", "private_qualified_type.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_qualified_type)).output,
        "type is private: samplelib.visibility.HiddenInt");

    const fs::path private_field = negative_sample("visibility", "private_field_access.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_field)).output,
        "field is private: secret");

    const fs::path private_method = negative_sample("visibility", "private_method_access.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_method)).output,
        "method is private: samplelib.visibility.PublicBox.secret_value");

    const fs::path private_struct = negative_sample("visibility", "private_struct_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_struct)).output,
        "unknown type: HiddenBox");

    const fs::path private_enum = negative_sample("visibility", "private_enum_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output,
        "unknown import alias: HiddenChoice");

    const fs::path private_qualified_const = negative_sample("visibility", "private_qualified_const.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_qualified_const)).output,
        "name is private: samplelib.visibility.hidden_answer");

    const fs::path unknown_alias = negative_sample("modules", "unknown_import_alias.ax");
    expect_contains(require_failure(aurexc() + " --check " + q(unknown_alias)).output, "unknown import alias: missing");

    const fs::path ambiguous_alias = negative_sample("modules", "ambiguous_import_alias.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(ambiguous_alias)).output,
        "ambiguous import alias: lib");

    const fs::path unknown_qualified_function = negative_sample("modules", "unknown_qualified_function.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(unknown_qualified_function)).output,
        "unknown function in module samplelib.visibility: missing");

    const fs::path unknown_qualified_type = negative_sample("modules", "unknown_qualified_type.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(unknown_qualified_type)).output,
        "unknown type in module samplelib.visibility: Missing");

    const fs::path alias_source = positive_sample("modules", "import_alias_qualified_call.ax");
    const std::string alias_tokens =
        require_success(aurexc() + " " + import_flags + " --dump-tokens " + q(alias_source)).output;
    expect_contains_all(alias_tokens,
        {
            "kw_as `as`",
            "dot `.`",
        });

    const std::string alias_ast =
        require_success(aurexc() + " " + import_flags + " --emit=ast " + q(alias_source)).output;
    expect_contains_all(alias_ast,
        {
            "import samplelib.visibility as vis",
            "let answer : vis.PublicInt",
            "field .answer",
            "var boxed : vis.PublicBox",
            "field .make_box",
            "field .value",
            "field .read",
            "field .bump",
        });

    const std::string alias_ir =
        require_success(aurexc() + " " + import_flags + " --emit=ir " + q(alias_source)).output;
    expect_contains_all(alias_ir,
        {
            "const_ref @m0_samplelib_visibility_answer",
            "call m0_samplelib_visibility_make_box",
            "call m0_samplelib_visibility_PublicBox_read",
            "call m0_samplelib_visibility_PublicBox_bump",
            "call m0_samplelib_visibility_exported",
        });

    const std::string alias_llvm =
        require_success(aurexc() + " " + import_flags + " --emit=llvm-ir " + q(alias_source)).output;
    expect_contains_all(alias_llvm,
        {
            "@m0_import_alias_qualified_call_main",
            "call i32 @m0_samplelib_visibility_exported",
        });
}

TEST_F(AurexIntegrationTest, PublicImportReexport)
{
    const fs::path source = positive_sample("visibility", "reexport_import.ax");
    const std::string import_flags = sample_import_flags();
    const fs::path facade = imports_root() / "samplelib" / "reexport_facade.ax";
    const fs::path private_facade = imports_root() / "samplelib" / "private_facade.ax";

    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(facade)).output;
    expect_contains_all(tokens,
        {
            "kw_pub `pub`",
            "kw_import `import`",
        });

    const std::string ast = require_success(aurexc() + " " + import_flags + " --emit=ast " + q(facade)).output;
    expect_contains(ast, "pub import samplelib.reexport_inner as inner");
    const std::string private_ast =
        require_success(aurexc() + " " + import_flags + " --emit=ast " + q(private_facade)).output;
    expect_contains(private_ast, "priv import samplelib.reexport_inner as inner");

    const std::string modules = require_success(aurexc() + " " + import_flags + " --dump-modules " + q(source)).output;
    expect_contains_all(modules,
        {
            "samplelib.reexport_facade",
            "samplelib.reexport_inner",
            "reexport_import",
        });

    const std::string checked = require_success(aurexc() + " " + import_flags + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "fn add_two -> i32",
            "fn add_base -> i32",
            "struct PairI32 fields=2",
            "type Count = i32",
            "case Mode_ready : samplelib.reexport_inner.Mode",
        });
    expect_contains(checked, "type priv SecretCount = i32");

    const std::string ir = require_success(aurexc() + " " + import_flags + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "call m0_samplelib_reexport_facade_add_two",
            "call m0_samplelib_reexport_inner_add_base",
            "record PairI32 @m0_samplelib_reexport_inner_PairI32",
            "const base @m0_samplelib_reexport_inner_base",
            "const_ref @m0_samplelib_reexport_inner_base",
        });

    require_success(aurexc() + " " + import_flags + " --emit=llvm-ir " + q(source));

    const fs::path private_import = negative_sample("visibility", "private_reexport_import.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_import)).output,
        "unknown type in module samplelib.private_facade: Count");

    const fs::path private_const = negative_sample("visibility", "private_reexport_const.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_const)).output,
        "unknown name in module samplelib.reexport_facade: hidden_base");

    const fs::path private_type = negative_sample("visibility", "private_reexport_type.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_type)).output,
        "unknown type in module samplelib.reexport_facade: SecretCount");

    const fs::path private_enum = negative_sample("visibility", "private_reexport_enum.ax");
    expect_contains(require_failure(aurexc() + " " + import_flags + " --check " + q(private_enum)).output,
        "unknown type in module samplelib.reexport_facade: SecretMode");
}

TEST_F(AurexIntegrationTest, DefaultPrivateVisibility)
{
    const fs::path import_dir = tmp_root() / "default_private_imports";
    fs::create_directories(import_dir);
    const fs::path library = import_dir / "default_private_lib.ax";
    {
        std::ofstream out(library, std::ios::binary);
        out << "module default_private_lib;\n"
               "const hidden_value: i32 = 7;\n"
               "fn hidden_fn() -> i32 { return hidden_value; }\n"
               "pub struct Box { value: i32; }\n"
               "pub fn make_box() -> Box { return Box { value: hidden_fn() }; }\n"
               "impl Box {\n"
               "  fn hidden_method(self: &Box) -> i32 { return self.value; }\n"
               "  pub fn read(self: &Box) -> i32 { return self.hidden_method(); }\n"
               "}\n";
    }

    const std::string library_ast = require_success(aurexc() + " --emit=ast " + q(library)).output;
    expect_contains_all(library_ast,
        {
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
        out << "module default_private_public_use;\n"
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
        out << "module default_private_value;\n"
               "import default_private_lib as lib;\n"
               "fn main() -> i32 { return lib.hidden_value; }\n";
    }
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_value)).output,
        "name is private: default_private_lib.hidden_value");

    const fs::path private_function = tmp_root() / "default_private_function.ax";
    {
        std::ofstream out(private_function, std::ios::binary);
        out << "module default_private_function;\n"
               "import default_private_lib as lib;\n"
               "fn main() -> i32 { return lib.hidden_fn(); }\n";
    }
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_function)).output,
        "function is private: default_private_lib.hidden_fn");

    const fs::path private_field = tmp_root() / "default_private_field.ax";
    {
        std::ofstream out(private_field, std::ios::binary);
        out << "module default_private_field;\n"
               "import default_private_lib as lib;\n"
               "fn main() -> i32 {\n"
               "  let box: lib.Box = lib.make_box();\n"
               "  return box.value;\n"
               "}\n";
    }
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_field)).output,
        "field is private: value");

    const fs::path private_method = tmp_root() / "default_private_method.ax";
    {
        std::ofstream out(private_method, std::ios::binary);
        out << "module default_private_method;\n"
               "import default_private_lib as lib;\n"
               "fn main() -> i32 {\n"
               "  let box: lib.Box = lib.make_box();\n"
               "  return box.hidden_method();\n"
               "}\n";
    }
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(private_method)).output,
        "method is private: default_private_lib.Box.hidden_method");
}

TEST_F(AurexIntegrationTest, ExportedSignatureSurfacesRejectPrivateTypes)
{
    const fs::path work = tmp_root() / "public_api_private_type_leaks";

    const fs::path function_return = write_visibility_test_source(work / "function_return.ax",
        "module public_leak_function_return;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub fn expose() -> Secret {\n"
        "  return Secret { value: 1 };\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(function_return)).output,
        "public function `expose` exposes private type `public_leak_function_return.Secret`");

    const fs::path struct_field = write_visibility_test_source(work / "struct_field.ax",
        "module public_leak_struct_field;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub struct Wrapper {\n"
        "  pub secret: Secret;\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(struct_field)).output,
        "public struct field `public_leak_struct_field.Wrapper.secret` exposes private type "
        "`public_leak_struct_field.Secret`");

    const fs::path enum_case = write_visibility_test_source(work / "enum_case.ax",
        "module public_leak_enum_case;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub enum Choice: u8 {\n"
        "  ready(Secret) = 1,\n"
        "  none = 2,\n"
        "}\n");
    const std::string enum_output = require_failure(aurexc() + " --check " + q(enum_case)).output;
    expect_contains(enum_output, "public enum case `public_leak_enum_case.Choice_ready` exposes private type");
    expect_contains(enum_output, "`public_leak_enum_case.Secret`");

    const fs::path type_alias = write_visibility_test_source(work / "type_alias.ax",
        "module public_leak_type_alias;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub type PublicSecret = Secret;\n");
    expect_contains(require_failure(aurexc() + " --check " + q(type_alias)).output,
        "public type alias `PublicSecret` exposes private type `public_leak_type_alias.Secret`");

    const fs::path public_const = write_visibility_test_source(work / "const.ax",
        "module public_leak_const;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub const SECRET: Secret = Secret { value: 1 };\n");
    expect_contains(require_failure(aurexc() + " --check " + q(public_const)).output,
        "public const `SECRET` exposes private type `public_leak_const.Secret`");

    const fs::path public_method = write_visibility_test_source(work / "method.ax",
        "module public_leak_method;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub struct Box {\n"
        "  pub value: i32;\n"
        "}\n"
        "impl Box {\n"
        "  pub fn take(self: &Box, secret: Secret) -> i32 {\n"
        "    return self.value + secret.value;\n"
        "  }\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(public_method)).output,
        "public method `public_leak_method.Box.take` exposes private type `public_leak_method.Secret`");
}

TEST_F(AurexIntegrationTest, PackageVisibilityControlsPackageSurface)
{
    const fs::path same_package_work = tmp_root() / "package_visibility_same_package";
    static_cast<void>(write_visibility_test_source(same_package_work / "lib" / "shared.ax",
        "module lib.shared;\n"
        "pub(package) fn package_value() -> i32 {\n"
        "  return 7;\n"
        "}\n"));
    const fs::path same_package_root = write_visibility_test_source(same_package_work / "main.ax",
        "module package_visibility_same;\n"
        "import lib.shared as shared;\n"
        "fn main() -> i32 {\n"
        "  return shared.package_value() - 7;\n"
        "}\n");
    const std::string same_package_ir = require_success(aurexc() + " --emit=llvm-ir " + q(same_package_root)).output;
    expect_contains(same_package_ir, "@m0_lib_shared_package_value");

    const fs::path cross_package_work = tmp_root() / "package_visibility_cross_package";
    const fs::path import_dir = cross_package_work / "imports";
    static_cast<void>(write_visibility_test_source(import_dir / "lib" / "shared.ax",
        "module lib.shared;\n"
        "pub(package) fn package_value() -> i32 {\n"
        "  return 7;\n"
        "}\n"));
    const fs::path cross_package_root = write_visibility_test_source(cross_package_work / "main.ax",
        "module package_visibility_cross;\n"
        "import lib.shared as shared;\n"
        "fn main() -> i32 {\n"
        "  return shared.package_value();\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(cross_package_root)).output,
        "function is private: lib.shared.package_value");
}

TEST_F(AurexIntegrationTest, PackageVisibilityControlsPackageReexports)
{
    const fs::path same_package_work = tmp_root() / "package_reexport_same_package";
    static_cast<void>(write_visibility_test_source(same_package_work / "lib" / "inner.ax",
        "module lib.inner;\n"
        "pub(package) fn package_value() -> i32 {\n"
        "  return 11;\n"
        "}\n"
        "pub fn public_value() -> i32 {\n"
        "  return 13;\n"
        "}\n"));
    static_cast<void>(write_visibility_test_source(same_package_work / "lib" / "facade.ax",
        "module lib.facade;\n"
        "pub(package) import lib.inner as inner;\n"
        "pub fn facade_value() -> i32 {\n"
        "  return inner.public_value();\n"
        "}\n"));
    const fs::path same_package_root = write_visibility_test_source(same_package_work / "main.ax",
        "module package_reexport_same;\n"
        "import lib.facade as facade;\n"
        "fn main() -> i32 {\n"
        "  return facade.package_value() + facade.facade_value() - 24;\n"
        "}\n");
    const std::string same_package_ir = require_success(aurexc() + " --emit=llvm-ir " + q(same_package_root)).output;
    expect_contains_all(same_package_ir,
        {
            "@m0_lib_inner_package_value",
            "@m0_lib_facade_facade_value",
        });

    const fs::path cross_package_work = tmp_root() / "package_reexport_cross_package";
    const fs::path import_dir = cross_package_work / "imports";
    static_cast<void>(write_visibility_test_source(import_dir / "lib" / "inner.ax",
        "module lib.inner;\n"
        "pub(package) fn package_value() -> i32 {\n"
        "  return 11;\n"
        "}\n"
        "pub fn public_value() -> i32 {\n"
        "  return 13;\n"
        "}\n"));
    static_cast<void>(write_visibility_test_source(import_dir / "lib" / "facade.ax",
        "module lib.facade;\n"
        "pub(package) import lib.inner as inner;\n"
        "pub fn facade_value() -> i32 {\n"
        "  return inner.public_value();\n"
        "}\n"));
    const fs::path cross_package_root = write_visibility_test_source(cross_package_work / "main.ax",
        "module package_reexport_cross;\n"
        "import lib.facade as facade;\n"
        "fn main() -> i32 {\n"
        "  return facade.package_value();\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " -I " + q(import_dir) + " --check " + q(cross_package_root)).output,
        "unknown function in module lib.facade: package_value");

    static_cast<void>(write_visibility_test_source(import_dir / "lib" / "public_facade.ax",
        "module lib.public_facade;\n"
        "pub import lib.inner as inner;\n"));
    const fs::path public_cross_root = write_visibility_test_source(cross_package_work / "public_main.ax",
        "module package_reexport_public_cross;\n"
        "import lib.public_facade as facade;\n"
        "fn main() -> i32 {\n"
        "  return facade.public_value() - 13;\n"
        "}\n");
    require_success(aurexc() + " -I " + q(import_dir) + " --check " + q(public_cross_root));

    static_cast<void>(write_visibility_test_source(same_package_work / "lib" / "private_facade.ax",
        "module lib.private_facade;\n"
        "import lib.inner as inner;\n"));
    const fs::path private_same_root = write_visibility_test_source(same_package_work / "private_main.ax",
        "module package_reexport_private_same;\n"
        "import lib.private_facade as facade;\n"
        "fn main() -> i32 {\n"
        "  return facade.public_value();\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(private_same_root)).output,
        "unknown function in module lib.private_facade: public_value");
}

TEST_F(AurexIntegrationTest, PackageVisibilitySurfaceLeaksUseVisibilityAwareDiagnostics)
{
    const fs::path package_leak = write_visibility_test_source(tmp_root() / "package_visibility_surface_leak.ax",
        "module package_visibility_surface_leak;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "pub(package) fn expose(secret: Secret) -> i32 {\n"
        "  return secret.value;\n"
        "}\n"
        "pub(package) struct Wrapper {\n"
        "  pub(package) secret: Secret;\n"
        "}\n");
    const std::string package_output = require_failure(aurexc() + " --check " + q(package_leak)).output;
    expect_contains(package_output,
        "package-visible function `expose` exposes private type `package_visibility_surface_leak.Secret`");
    expect_contains(package_output,
        "package-visible struct field `package_visibility_surface_leak.Wrapper.secret` exposes private type "
        "`package_visibility_surface_leak.Secret`");

    const fs::path public_leak = write_visibility_test_source(tmp_root() / "public_package_visibility_surface_leak.ax",
        "module public_package_visibility_surface_leak;\n"
        "pub(package) struct PackageOnly {\n"
        "  pub(package) value: i32;\n"
        "}\n"
        "pub fn expose() -> PackageOnly {\n"
        "  return PackageOnly { value: 1 };\n"
        "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(public_leak)).output,
        "public function `expose` exposes package-visible type "
        "`public_package_visibility_surface_leak.PackageOnly`");
}

TEST_F(AurexIntegrationTest, PublicMethodsOnPrivateTypesAreNotExportedSurfaces)
{
    const fs::path source = write_visibility_test_source(tmp_root() / "private_method_owner_surface.ax",
        "module private_method_owner_surface;\n"
        "priv struct Secret {\n"
        "  pub value: i32;\n"
        "}\n"
        "impl Secret {\n"
        "  pub fn read(self: &Secret) -> i32 {\n"
        "    return self.value;\n"
        "  }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let secret: Secret = Secret { value: 7 };\n"
        "  return secret.read() - 7;\n"
        "}\n");
    require_success(aurexc() + " --check " + q(source));
}

} // namespace aurex::test

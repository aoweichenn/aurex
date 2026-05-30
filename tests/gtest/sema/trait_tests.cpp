#include <aurex/base/diagnostic.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/parse/parser.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/sema/sema.hpp>

#include <support/test_support.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::test {
namespace {

[[nodiscard]] syntax::AstModule parse_trait_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer({91}, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

[[nodiscard]] sema::CheckedModule analyze_trait_source(const std::string_view source)
{
    syntax::AstModule module = parse_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();
    if (!result) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << result.error().message;
        return {};
    }
    return result.take_value();
}

[[nodiscard]] std::string analyze_trait_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();

    std::string output;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        output += diagnostic.message;
        output += '\n';
    }
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
        return output;
    }
    output += result.error().message;
    output += '\n';
    return output;
}

void expect_trait_source_diagnostic(const std::string_view source, const std::string_view diagnostic)
{
    expect_contains(analyze_trait_source_failure(source), diagnostic);
}

void expect_negative_trait_sample(const std::string_view filename, const std::string_view diagnostic)
{
    const fs::path source = negative_sample("traits", std::string(filename));
    const std::string output = require_failure(aurexc() + " --check " + sample_import_flags() + " " + q(source)).output;
    expect_contains(output, diagnostic);
}

} // namespace

TEST(CoreUnit, TraitSemaRegistryRecordsTraitAndImplFacts)
{
    const std::string_view source =
        "module trait_registry_whitebox;\n"
        "pub trait Reader[T] {\n"
        "  fn read(self: &Self, value: T) -> i32;\n"
        "}\n"
        "struct File { handle: i32; }\n"
        "impl Reader[i32] for File {\n"
        "  fn read(self: &File, value: i32) -> i32 {\n"
        "    return value;\n"
        "  }\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 1U);
    ASSERT_EQ(checked.trait_impls.size(), 1U);

    const sema::TraitSignature& trait = checked.traits.begin()->second;
    EXPECT_EQ(trait.name, "Reader");
    EXPECT_EQ(trait.generic_params.size(), 1U);
    ASSERT_EQ(trait.requirements.size(), 1U);
    EXPECT_EQ(trait.requirements.front().name, "read");
    EXPECT_EQ(trait.requirements.front().param_types.size(), 2U);

    const sema::TraitImplInfo& impl = checked.trait_impls.begin()->second;
    EXPECT_EQ(impl.trait_name, "Reader");
    EXPECT_EQ(checked.types.display_name(impl.self_type), "trait_registry_whitebox.File");
    ASSERT_EQ(impl.trait_args.size(), 1U);
    EXPECT_EQ(checked.types.display_name(impl.trait_args.front()), "i32");
    ASSERT_EQ(impl.methods.size(), 1U);
    EXPECT_EQ(impl.methods.front().name, "read");

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "traits 1",
            "trait Reader[T0] params=1 requirements=1",
            "requirement read(&Self, T) -> i32",
            "trait_impls 1",
            "impl Reader[i32] for trait_registry_whitebox.File methods=1",
        });

    sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.traits.size(), 1U);
    ASSERT_EQ(copied.trait_impls.size(), 1U);
    EXPECT_EQ(copied.traits.begin()->second.requirements.front().name, "read");
    EXPECT_EQ(copied.trait_impls.begin()->second.methods.front().name, "read");

    sema::CheckedModule assigned;
    assigned = checked;
    ASSERT_EQ(assigned.traits.size(), 1U);
    ASSERT_EQ(assigned.trait_impls.size(), 1U);
    EXPECT_EQ(assigned.traits.begin()->second.requirements.front().name, "read");
    EXPECT_EQ(assigned.trait_impls.begin()->second.methods.front().name, "read");

    sema::CheckedModule moved = std::move(copied);
    ASSERT_EQ(moved.traits.size(), 1U);
    ASSERT_EQ(moved.trait_impls.size(), 1U);
    EXPECT_EQ(moved.traits.begin()->second.requirements.front().name, "read");
    EXPECT_EQ(moved.trait_impls.begin()->second.methods.front().name, "read");
}

TEST(CoreUnit, TraitSemaRegistrySubstitutesCompositeRequirementTypes)
{
    const std::string_view source =
        "module trait_registry_composite_whitebox;\n"
        "struct Token { value: i32; }\n"
        "enum Mode { fast, slow }\n"
        "pub trait Shape[T] {\n"
        "  fn ptr(self: *const Self, value: *mut T) -> *const T;\n"
        "  fn slice(self: &Self, values: []const T) -> T;\n"
        "  fn pair(self: &Self, value: (Self, T)) -> T;\n"
        "  fn callback(self: &Self, op: fn(T) -> T) -> T;\n"
        "  fn concrete(self: &Self, token: Token, mode: Mode) -> Token;\n"
        "}\n"
        "struct Box { value: i32; }\n"
        "impl Shape[i32] for Box {\n"
        "  fn ptr(self: *const Box, value: *mut i32) -> *const i32 { return null; }\n"
        "  fn slice(self: &Box, values: []const i32) -> i32 { return values[0]; }\n"
        "  fn pair(self: &Box, value: (Box, i32)) -> i32 {\n"
        "    let (_, right) = value;\n"
        "    return right;\n"
        "  }\n"
        "  fn callback(self: &Box, op: fn(i32) -> i32) -> i32 { return op(1); }\n"
        "  fn concrete(self: &Box, token: Token, mode: Mode) -> Token { return token; }\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 1U);
    ASSERT_EQ(checked.trait_impls.size(), 1U);
    EXPECT_EQ(checked.traits.begin()->second.requirements.size(), 5U);
    EXPECT_EQ(checked.trait_impls.begin()->second.methods.size(), 5U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait Shape[T0] params=1 requirements=5",
            "requirement ptr(*const Self, *mut T) -> *const T",
            "requirement slice(&Self, []const T) -> T",
            "requirement pair(&Self, (Self, T)) -> T",
            "requirement callback(&Self, fn(T) -> T) -> T",
            "requirement concrete(&Self, trait_registry_composite_whitebox.Token, "
            "trait_registry_composite_whitebox.Mode) -> trait_registry_composite_whitebox.Token",
            "impl Shape[i32] for trait_registry_composite_whitebox.Box methods=5",
        });
}

TEST(CoreUnit, TraitSemaRegistryAcceptsEnumSelfTargets)
{
    const std::string_view source =
        "module trait_registry_self_targets_whitebox;\n"
        "trait Marker {}\n"
        "trait Extra {}\n"
        "trait UnsafeReader { unsafe fn read(self: &Self) -> i32; }\n"
        "struct Box { value: i32; }\n"
        "enum Mode { fast, slow }\n"
        "impl Marker for Mode {}\n"
        "impl Marker for Box {}\n"
        "impl UnsafeReader for Box {\n"
        "  unsafe fn read(self: &Box) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 { return 0; }\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 3U);
    EXPECT_EQ(checked.trait_impls.size(), 3U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "traits 3",
            "trait priv Extra params=0 requirements=0",
            "trait priv Marker params=0 requirements=0",
            "trait priv UnsafeReader params=0 requirements=1",
            "requirement unsafe read(&Self) -> i32",
            "trait_impls 3",
            "impl Marker for trait_registry_self_targets_whitebox.Box methods=0",
            "impl Marker for trait_registry_self_targets_whitebox.Mode methods=0",
            "impl UnsafeReader for trait_registry_self_targets_whitebox.Box methods=1",
        });
}

TEST(CoreUnit, TraitSemaRegistryRejectsBoundaryCases)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases = {
        {
            "module trait_duplicate_name_whitebox;\n"
            "struct Reader { value: i32; }\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate trait definition in module trait_duplicate_name_whitebox: Reader",
        },
        {
            "module trait_duplicate_requirement_whitebox;\n"
            "trait Reader {\n"
            "  fn read(self: &Self) -> i32;\n"
            "  fn read(self: &Self) -> i32;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate trait requirement: Reader.read",
        },
        {
            "module trait_self_generic_whitebox;\n"
            "trait Reader[Self] { fn read(self: &Self) -> i32; }\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate generic parameter `Self`",
        },
        {
            "module trait_requirement_generic_whitebox;\n"
            "trait Reader { fn read[T](self: &Self, value: T) -> i32; }\n"
            "fn main() -> i32 { return 0; }\n",
            "method-local generic parameters are not supported",
        },
        {
            "module trait_requirement_missing_return_whitebox;\n"
            "trait Reader { fn read(self: &Self); }\n"
            "fn main() -> i32 { return 0; }\n",
            "function prototype return type must be explicit",
        },
        {
            "module trait_requirement_array_param_whitebox;\n"
            "trait Reader[T] { fn read(self: &Self, values: [2]T) -> T; }\n"
            "struct File { value: i32; }\n"
            "impl Reader[i32] for File {\n"
            "  fn read(self: &File, values: [2]i32) -> i32 { return values[0]; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "array type cannot be used as a function parameter",
        },
        {
            "module trait_impl_generic_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl[T] Reader for File {\n"
            "  fn read(self: &File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "generic trait impl blocks are not supported",
        },
        {
            "module trait_impl_where_unsupported_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File where File: Reader {\n"
            "  fn read(self: &File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "generic trait impl blocks are not supported",
        },
        {
            "module trait_impl_unknown_module_path_whitebox;\n"
            "struct File { value: i32; }\n"
            "impl missing.Visible for File {\n"
            "  fn read(self: &File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "unknown trait",
        },
        {
            "module trait_generic_arity_missing_whitebox;\n"
            "trait Reader[T] { fn read(self: &Self, value: T) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File, value: i32) -> i32 { return value; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait type arguments for Reader: expected 1, got 0",
        },
        {
            "module trait_generic_arity_mismatch_whitebox;\n"
            "trait Reader[A, B] { fn read(self: &Self, value: A) -> B; }\n"
            "struct File { value: i32; }\n"
            "impl Reader[i32] for File {\n"
            "  fn read(self: &File, value: i32) -> i32 { return value; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait type arguments for Reader: expected 2, got 1",
        },
        {
            "module trait_type_args_on_plain_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader[i32] for File {\n"
            "  fn read(self: &File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait Reader is not generic",
        },
        {
            "module trait_duplicate_impl_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File) -> i32 { return 1; }\n"
            "}\n"
            "impl Reader for File {\n"
            "  fn read(self: &File) -> i32 { return 2; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate trait impl: Reader for trait_duplicate_impl_whitebox.File",
        },
        {
            "module trait_impl_builtin_syntax_self_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "impl Reader for i32 {\n"
            "  fn read(self: &i32) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "impl target must be a named type",
        },
        {
            "module trait_impl_alias_self_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "type Count = i32;\n"
            "impl Reader for Count {\n"
            "  fn read(self: &Count) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "impl target must be a named type",
        },
        {
            "module trait_impl_unknown_self_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "impl Reader for Missing {\n"
            "  fn read(self: &Missing) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "unknown type: Missing",
        },
        {
            "module trait_impl_param_count_mismatch_whitebox;\n"
            "trait Reader { fn read(self: &Self, value: i32) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl method signature does not match requirement",
        },
        {
            "module trait_impl_param_type_mismatch_whitebox;\n"
            "trait Reader { fn read(self: &Self, value: i32) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File, value: u32) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl method signature does not match requirement",
        },
        {
            "module trait_impl_unsafe_shape_mismatch_whitebox;\n"
            "trait Reader { unsafe fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl method signature does not match requirement",
        },
        {
            "module trait_impl_variadic_shape_mismatch_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File, ...) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "variadic functions are only supported for extern c declarations",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_trait_source_diagnostic(source, diagnostic);
    }
}

TEST_F(AurexIntegrationTest, TraitImplRegistrySamples)
{
    const fs::path source = positive_sample("traits", "trait_impl_registry.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "trait Reader params=0 requirements=1",
            "requirement read(&Self) -> i32",
            "impl Reader for trait_impl_registry.File methods=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path qualified_source = positive_sample("traits", "trait_impl_qualified_registry.ax");
    const std::string qualified_checked =
        require_success(aurexc() + " --emit=checked " + sample_import_flags() + " " + q(qualified_source)).output;
    expect_contains_all(qualified_checked,
        {
            "trait_impls 1",
            "impl Visible for trait_impl_qualified_registry.File methods=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + sample_import_flags() + " " + q(qualified_source));

    const fs::path selective_source = positive_sample("traits", "trait_impl_selective_registry.ax");
    const std::string selective_checked =
        require_success(aurexc() + " --emit=checked " + sample_import_flags() + " " + q(selective_source)).output;
    expect_contains_all(selective_checked,
        {
            "trait_impls 1",
            "impl Visible for trait_impl_selective_registry.File methods=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + sample_import_flags() + " " + q(selective_source));

    expect_negative_trait_sample("trait_impl_missing_method.ax", "trait impl missing method");
    expect_negative_trait_sample("trait_impl_duplicate_method.ax", "duplicate trait impl method");
    expect_negative_trait_sample("trait_impl_signature_mismatch.ax",
        "trait impl method signature does not match requirement");
    expect_negative_trait_sample("trait_impl_unknown_method.ax", "trait impl method is not required");
    expect_negative_trait_sample("trait_impl_unknown_trait.ax", "unknown trait: Missing");
    expect_negative_trait_sample("trait_impl_target_not_named.ax", "trait impl target must be a named trait");
    expect_negative_trait_sample("trait_impl_self_type_not_named.ax", "impl target must be a named type");
    expect_negative_trait_sample("trait_impl_private_trait.ax", "trait is private: samplelib.traits.Hidden");
    expect_negative_trait_sample("trait_impl_unknown_qualified_trait.ax",
        "unknown trait in module samplelib.traits: Missing");
}

} // namespace aurex::test

#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

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

[[nodiscard]] sema::CheckedModule analyze_trait_source(
    const std::string_view source, const sema::SemanticOptions options)
{
    syntax::AstModule module = parse_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics, options);
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

[[nodiscard]] sema::CheckedModule analyze_trait_source(const std::string_view source)
{
    return analyze_trait_source(source, sema::SemanticOptions{});
}

struct TraitAnalysisWithAst {
    syntax::AstModule module;
    sema::CheckedModule checked;
};

[[nodiscard]] TraitAnalysisWithAst analyze_trait_source_with_ast(
    const std::string_view source, const sema::SemanticOptions options = {})
{
    TraitAnalysisWithAst analysis;
    analysis.module = parse_trait_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(analysis.module, diagnostics, options);
    auto result = analyzer.analyze();
    if (!result) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << result.error().message;
        return analysis;
    }
    analysis.checked = result.take_value();
    return analysis;
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
    const std::string_view source = "module trait_registry_whitebox;\n"
                                    "pub trait Reader<T> {\n"
                                    "  fn read(self: &Self, value: T) -> i32;\n"
                                    "}\n"
                                    "struct File { handle: i32; }\n"
                                    "impl Reader<i32> for File {\n"
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
            "trait Reader<T0> params=1 associated_types=0 requirements=1",
            "requirement read(&Self, T) -> i32",
            "trait_impls 1",
            "impl Reader<i32> for trait_registry_whitebox.File associated_types=0 methods=1",
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

TEST(CoreUnit, DropSemaRegistersReservedDestructorImpl)
{
    const std::string_view source = "module drop_valid_whitebox;\n"
                                    "struct File { fd: i32; }\n"
                                    "fn observe(value: i32) -> void {}\n"
                                    "impl Drop for File {\n"
                                    "  fn drop(self: deinit File) -> void { observe(self.fd); }\n"
                                    "}\n"
                                    "fn main() -> void {}\n";

    const TraitAnalysisWithAst analysis = analyze_trait_source_with_ast(source);
    const sema::CheckedModule& checked = analysis.checked;
    ASSERT_EQ(checked.destructors.size(), 1U);
    EXPECT_TRUE(checked.traits.empty());
    EXPECT_TRUE(checked.trait_impls.empty());

    const sema::DestructorInfo& destructor = checked.destructors.begin()->second;
    EXPECT_EQ(checked.types.display_name(destructor.self_type), "drop_valid_whitebox.File");
    ASSERT_TRUE(syntax::is_valid(destructor.impl_item));
    ASSERT_TRUE(syntax::is_valid(destructor.method_item));
    ASSERT_LT(destructor.impl_item.value, analysis.module.items.size());
    ASSERT_LT(destructor.method_item.value, analysis.module.items.size());
    const syntax::ItemNode impl_item = analysis.module.items[destructor.impl_item.value];
    const syntax::ItemNode method_item = analysis.module.items[destructor.method_item.value];
    ASSERT_EQ(impl_item.kind, syntax::ItemKind::impl_block);
    ASSERT_EQ(method_item.kind, syntax::ItemKind::fn_decl);
    EXPECT_EQ(method_item.name, "drop");
    bool impl_contains_method = false;
    for (const syntax::ItemId impl_child : impl_item.impl_items) {
        impl_contains_method = impl_contains_method || impl_child.value == destructor.method_item.value;
    }
    EXPECT_TRUE(impl_contains_method);
    EXPECT_NE(destructor.fingerprint.byte_count, 0U);

    const auto function = checked.functions.find(destructor.function_key);
    ASSERT_NE(function, checked.functions.end());
    EXPECT_TRUE(function->second.is_destructor);
    EXPECT_EQ(function->second.name, "drop");
    ASSERT_EQ(function->second.param_types.size(), 1U);
    EXPECT_TRUE(checked.types.same(function->second.param_types.front(), destructor.self_type));
    EXPECT_TRUE(checked.types.is_void(function->second.return_type));

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "traits 0",
            "trait_impls 0",
            "destructors 1",
            "destructor drop_valid_whitebox.File ->",
            "fn method drop_valid_whitebox.File.drop -> void",
            "destructor",
        });

    sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.destructors.size(), 1U);
    EXPECT_EQ(copied.destructors.begin()->second.fingerprint, destructor.fingerprint);
    EXPECT_EQ(sema::checked_destructors_fingerprint(copied), sema::checked_destructors_fingerprint(checked));
}

TEST(CoreUnit, DropSemaRejectsUnsupportedDestructorSurfaces)
{
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {
            "module drop_reserved_trait_whitebox;\n"
            "trait Drop {}\n"
            "fn main() -> void {}\n",
            "Drop is a reserved destructor trait and cannot be declared by user code",
        },
        {
            "module drop_missing_deinit_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(self: File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop method self parameter must be marked deinit",
        },
        {
            "module drop_wrong_self_name_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(value: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop method signature must be fn drop(self: deinit T) -> void",
        },
        {
            "module drop_pointer_self_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(self: deinit *mut File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop method self parameter must be the impl type by value",
        },
        {
            "module drop_wrong_self_type_whitebox;\n"
            "struct File { fd: i32; }\n"
            "struct Other { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(self: deinit Other) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop method self parameter must be the impl type by value",
        },
        {
            "module drop_nonvoid_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(self: deinit File) -> i32 { return 0; }\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop method must explicitly return void",
        },
        {
            "module drop_duplicate_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "impl Drop for File {\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "duplicate Drop impl for type",
        },
        {
            "module drop_generic_impl_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl<T> Drop for File {\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "generic Drop impl blocks are not supported",
        },
        {
            "module drop_associated_type_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  type Item = i32;\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "associated types are not supported in Drop impls",
        },
        {
            "module drop_extra_method_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "  fn close(self: File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop impl must define exactly one method",
        },
        {
            "module drop_wrong_method_name_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  fn close(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop impl method must be named drop",
        },
        {
            "module drop_borrow_contract_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop for File {\n"
            "  @borrow(return = [self])\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "borrow contracts are not supported on Drop methods",
        },
        {
            "module drop_scoped_surface_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl pkg.Drop for File {\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop impl must use unqualified Drop without type arguments",
        },
        {
            "module drop_type_args_surface_whitebox;\n"
            "struct File { fd: i32; }\n"
            "impl Drop<i32> for File {\n"
            "  fn drop(self: deinit File) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop impl must use unqualified Drop without type arguments",
        },
        {
            "module drop_builtin_target_whitebox;\n"
            "impl Drop for i32 {\n"
            "  fn drop(self: deinit i32) -> void {}\n"
            "}\n"
            "fn main() -> void {}\n",
            "Drop impl target must be a named struct, enum, or opaque struct",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_trait_source_diagnostic(source, diagnostic);
    }
}

TEST(CoreUnit, BorrowContractSemaRecordsDeclaredAndInferredFacts)
{
    const std::string_view source = "module borrow_contract_facts;\n"
                                    "@borrow(return = [left, right])\n"
                                    "fn choose(left: &i32, right: &i32, take_left: bool) -> &i32 {\n"
                                    "  if take_left { return left; }\n"
                                    "  return right;\n"
                                    "}\n"
                                    "fn inferred(value: &i32) -> &i32 {\n"
                                    "  return value;\n"
                                    "}\n"
                                    "extern c {\n"
                                    "  @borrow(return = [value])\n"
                                    "  fn external_view(value: &i32) -> &i32;\n"
                                    "  @borrow(return = [static, unknown])\n"
                                    "  fn external_text() -> str;\n"
                                    "}\n"
                                    "fn main() -> i32 { return 0; }\n";
    const sema::CheckedModule checked = analyze_trait_source(source);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "borrow_contracts",
            "source=declared return=&i32 selectors=2 unknown=false local_escape=false mismatch=false",
            "source=inferred return=&i32 selectors=1 unknown=false local_escape=false mismatch=false",
            "source=declared return=&i32 selectors=1 unknown=false local_escape=false mismatch=false",
            "source=declared return=str selectors=2 unknown=true local_escape=false mismatch=false",
        });
}

TEST(CoreUnit, BorrowContractSemaRejectsInvalidDeclaredSelectors)
{
    expect_trait_source_diagnostic("module borrow_contract_redundant;\n"
                                   "@borrow(return = [value])\n"
                                   "fn bad(value: i32) -> i32 { return value; }\n"
                                   "fn main() -> i32 { return 0; }\n",
        "borrow contract requires a return type that can contain a borrow");
    expect_trait_source_diagnostic("module borrow_contract_unknown_selector;\n"
                                   "@borrow(return = [missing])\n"
                                   "fn bad(value: &i32) -> &i32 { return value; }\n"
                                   "fn main() -> i32 { return 0; }\n",
        "borrow contract return selector does not name a parameter");
    expect_trait_source_diagnostic("module borrow_contract_self_selector;\n"
                                   "@borrow(return = [self])\n"
                                   "fn bad(value: &i32) -> &i32 { return value; }\n"
                                   "fn main() -> i32 { return 0; }\n",
        "borrow contract 'self' selector requires a first self parameter");
    expect_trait_source_diagnostic("module borrow_contract_non_borrowing_selector;\n"
                                   "extern c {\n"
                                   "  @borrow(return = [value])\n"
                                   "  fn bad(value: i32) -> &i32;\n"
                                   "}\n"
                                   "fn main() -> i32 { return 0; }\n",
        "borrow contract return selector must name a parameter that can carry a borrow");
    expect_trait_source_diagnostic("module borrow_contract_duplicate_selector;\n"
                                   "@borrow(return = [value, value])\n"
                                   "fn bad(value: &i32) -> &i32 { return value; }\n"
                                   "fn main() -> i32 { return 0; }\n",
        "duplicate borrow contract return selector");
}

TEST(CoreUnit, BorrowContractSemaRejectsBodyOutsideDeclaredReturnSources)
{
    expect_trait_source_diagnostic("module borrow_contract_mismatch;\n"
                                   "@borrow(return = [left])\n"
                                   "fn bad(left: &i32, right: &i32) -> &i32 {\n"
                                   "  return right;\n"
                                   "}\n"
                                   "fn main() -> i32 { return 0; }\n",
        "function body returns a borrow source outside the declared borrow contract");
}

TEST(CoreUnit, TraitImplBorrowContractUsesBodyInferredContract)
{
    const std::string_view valid = "module trait_borrow_contract_valid;\n"
                                   "struct Box { value: i32; }\n"
                                   "trait View {\n"
                                   "  @borrow(return = [self])\n"
                                   "  fn view(self: &Self) -> &Self;\n"
                                   "}\n"
                                   "impl View for Box {\n"
                                   "  fn view(self: &Box) -> &Box { return self; }\n"
                                   "}\n"
                                   "fn main() -> i32 { return 0; }\n";
    const sema::CheckedModule checked = analyze_trait_source(valid);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains(dump, "requirement view(&Self) -> &Self borrow_contract=declared/selectors=1/unknown=false");

    const std::string_view renamed_param = "module trait_borrow_contract_param_rename;\n"
                                           "struct Box { value: i32; }\n"
                                           "trait Pick {\n"
                                           "  @borrow(return = [other])\n"
                                           "  fn pick(self: &Self, other: &Self) -> &Self;\n"
                                           "}\n"
                                           "impl Pick for Box {\n"
                                           "  fn pick(self: &Box, rhs: &Box) -> &Box { return rhs; }\n"
                                           "}\n"
                                           "fn main() -> i32 { return 0; }\n";
    const sema::CheckedModule renamed_checked = analyze_trait_source(renamed_param);
    EXPECT_FALSE(renamed_checked.trait_impls.empty());

    expect_trait_source_diagnostic("module trait_borrow_contract_mismatch;\n"
                                   "struct Box { value: i32; }\n"
                                   "trait Pick {\n"
                                   "  @borrow(return = [self])\n"
                                   "  fn pick(self: &Self, other: &Self) -> &Self;\n"
                                   "}\n"
                                   "impl Pick for Box {\n"
                                   "  fn pick(self: &Box, other: &Box) -> &Box { return other; }\n"
                                   "}\n"
                                   "fn main() -> i32 { return 0; }\n",
        "function body returns a borrow source outside the declared borrow contract");
}

TEST(CoreUnit, TraitSemaRegistrySubstitutesCompositeRequirementTypes)
{
    const std::string_view source = "module trait_registry_composite_whitebox;\n"
                                    "struct Token { value: i32; }\n"
                                    "enum Mode { fast, slow }\n"
                                    "pub trait Shape<T> {\n"
                                    "  fn ptr(self: *const Self, value: *mut T) -> *const T;\n"
                                    "  fn slice(self: &Self, values: []const T) -> T;\n"
                                    "  fn pair(self: &Self, value: (Self, T)) -> T;\n"
                                    "  fn callback(self: &Self, op: fn(T) -> T) -> T;\n"
                                    "  fn concrete(self: &Self, token: Token, mode: Mode) -> Token;\n"
                                    "}\n"
                                    "struct Box { value: i32; }\n"
                                    "impl Shape<i32> for Box {\n"
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
            "trait Shape<T0> params=1 associated_types=0 requirements=5",
            "requirement ptr(*const Self, *mut T) -> *const T",
            "requirement slice(&Self, []const T) -> T",
            "requirement pair(&Self, (Self, T)) -> T",
            "requirement callback(&Self, fn(T) -> T) -> T",
            "requirement concrete(&Self, trait_registry_composite_whitebox.Token, "
            "trait_registry_composite_whitebox.Mode) -> trait_registry_composite_whitebox.Token",
            "impl Shape<i32> for trait_registry_composite_whitebox.Box associated_types=0 methods=5",
        });
}

TEST(CoreUnit, TraitSemaRegistryAcceptsEnumSelfTargets)
{
    const std::string_view source = "module trait_registry_self_targets_whitebox;\n"
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
            "trait priv Extra params=0 associated_types=0 requirements=0",
            "trait priv Marker params=0 associated_types=0 requirements=0",
            "trait priv UnsafeReader params=0 associated_types=0 requirements=1",
            "requirement unsafe read(&Self) -> i32",
            "trait_impls 3",
            "impl Marker for trait_registry_self_targets_whitebox.Box associated_types=0 methods=0",
            "impl Marker for trait_registry_self_targets_whitebox.Mode associated_types=0 methods=0",
            "impl UnsafeReader for trait_registry_self_targets_whitebox.Box associated_types=0 methods=1",
        });
}

TEST(CoreUnit, TraitPredicatesLowerWhereBoundsAndValidateGenericCandidates)
{
    const std::string_view source = "module trait_predicate_where_whitebox;\n"
                                    "trait Reader { fn read(self: &Self) -> i32; }\n"
                                    "struct File { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "fn use_reader<T>(value: T) -> i32 where T: Reader {\n"
                                    "  return 0;\n"
                                    "}\n"
                                    "fn forward_reader<T>(value: T) -> i32 where T: Reader {\n"
                                    "  return use_reader<T>(value);\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let file = File { value: 7 };\n"
                                    "  return forward_reader<File>(file);\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 1U);
    ASSERT_EQ(checked.trait_impls.size(), 1U);
    EXPECT_EQ(checked.trait_predicates.size(), 3U);
    EXPECT_EQ(checked.trait_obligations.size(), 2U);
    EXPECT_EQ(checked.trait_evidence.size(), 3U);
    EXPECT_EQ(checked.param_envs.size(), 2U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_predicates 3",
            "T: Reader origin=where",
            "trait_predicate_where_whitebox.File: Reader origin=impl",
            "trait_obligations 2",
            "trait_evidence 3",
            "param_envs 2",
            "param_env forward_reader predicates=1",
            "param_env use_reader predicates=1",
        });

    sema::CheckedModule copied = checked;
    EXPECT_EQ(copied.trait_predicates.size(), 3U);
    EXPECT_EQ(copied.trait_obligations.size(), 2U);
    EXPECT_EQ(copied.trait_evidence.size(), 3U);
    EXPECT_EQ(copied.param_envs.size(), 2U);
}

TEST(CoreUnit, TraitPredicatesRejectUnsatisfiedGenericArguments)
{
    const std::string_view source = "module trait_predicate_unsatisfied_whitebox;\n"
                                    "trait Reader { fn read(self: &Self) -> i32; }\n"
                                    "struct File { value: i32; }\n"
                                    "struct Other { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "fn use_reader<T>(value: T) -> i32 where T: Reader {\n"
                                    "  return 0;\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let other = Other { value: 9 };\n"
                                    "  return use_reader<Other>(other);\n"
                                    "}\n";

    expect_trait_source_diagnostic(
        source, "type trait_predicate_unsatisfied_whitebox.Other does not satisfy trait predicate `Reader`");
}

TEST(CoreUnit, TraitPredicatesLowerBuiltinAndDeclaredBoundsTogether)
{
    const std::string_view source = "module trait_predicate_mixed_bounds_whitebox;\n"
                                    "trait Reader { fn read(self: &Self) -> i32; }\n"
                                    "enum Flag { no, yes }\n"
                                    "impl Reader for Flag {\n"
                                    "  fn read(self: &Flag) -> i32 { return 1; }\n"
                                    "}\n"
                                    "fn use_flag<T>(value: T) -> i32 where T: Eq + Reader {\n"
                                    "  return 0;\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let flag: Flag = Flag.yes;\n"
                                    "  return use_flag<Flag>(flag);\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    EXPECT_EQ(checked.trait_predicates.size(), 3U);
    EXPECT_EQ(checked.trait_obligations.size(), 2U);
    EXPECT_EQ(checked.trait_evidence.size(), 3U);
    ASSERT_EQ(checked.param_envs.size(), 1U);
    EXPECT_EQ(checked.param_envs.front().predicate_indices.size(), 2U);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_predicates 3",
            "T: Eq origin=where",
            "T: Reader origin=where",
            "trait_predicate_mixed_bounds_whitebox.Flag: Reader origin=impl",
            "trait_obligations 2",
            "evidence #0 builtin",
            "evidence #1 param_env",
            "param_env use_flag predicates=2",
        });
}

TEST(CoreUnit, TraitMethodCallsRecordParamEnvAndImplDispatchBindings)
{
    const std::string_view source = "module trait_method_dispatch_whitebox;\n"
                                    "trait Reader { fn read(self: &Self) -> i32; }\n"
                                    "struct File { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "fn use_reader<T>(value: &T) -> i32 where T: Reader {\n"
                                    "  return value.read();\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let file = File { value: 7 };\n"
                                    "  return use_reader<File>(&file);\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.trait_method_calls.size(), 2U);
    const sema::TraitMethodCallBinding& param_env_call = checked.trait_method_calls[0];
    EXPECT_EQ(param_env_call.dispatch, sema::TraitMethodDispatchKind::param_env);
    EXPECT_EQ(param_env_call.predicate_index, 0U);
    EXPECT_EQ(param_env_call.method_name, "read");
    EXPECT_EQ(checked.types.display_name(param_env_call.self_type), "T");
    EXPECT_EQ(checked.types.display_name(param_env_call.return_type), "i32");

    const sema::TraitMethodCallBinding& impl_call = checked.trait_method_calls[1];
    EXPECT_EQ(impl_call.dispatch, sema::TraitMethodDispatchKind::impl_override);
    EXPECT_EQ(impl_call.predicate_index, 1U);
    EXPECT_EQ(impl_call.method_name, "read");
    EXPECT_EQ(checked.types.display_name(impl_call.self_type), "trait_method_dispatch_whitebox.File");
    EXPECT_EQ(checked.types.display_name(impl_call.return_type), "i32");

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait_method_calls 2",
            "trait_call #0 param_env T.read -> i32 predicate=0",
            "trait_call #1 impl_override trait_method_dispatch_whitebox.File.read -> i32 predicate=1",
            "fn method trait_method_dispatch_whitebox.File.read -> i32 "
            "@c_name=m0_trait_method_dispatch_whitebox_File_trait_impl_Reader__read",
        });

    const sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.trait_method_calls.size(), 2U);
    EXPECT_EQ(copied.trait_method_calls[0].method_name, "read");
    EXPECT_EQ(copied.trait_method_calls[1].dispatch, sema::TraitMethodDispatchKind::impl_override);
}

TEST(CoreUnit, TraitAssociatedTypesRecordProjectionEqualitiesAndDispatch)
{
    const std::string_view source = "module trait_associated_type_whitebox;\n"
                                    "trait Source {\n"
                                    "  type Item;\n"
                                    "  fn get(self: &Self) -> Self.Item;\n"
                                    "}\n"
                                    "struct Bytes { value: i32; }\n"
                                    "impl Source for Bytes {\n"
                                    "  type Item = i32;\n"
                                    "  fn get(self: &Bytes) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "fn use_i32<T>(value: &T) -> i32 where T: Source<Item = i32> {\n"
                                    "  return value.get();\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let bytes = Bytes { value: 5 };\n"
                                    "  return use_i32<Bytes>(&bytes);\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 1U);
    ASSERT_EQ(checked.trait_impls.size(), 1U);
    ASSERT_EQ(checked.trait_predicates.size(), 2U);
    ASSERT_EQ(checked.trait_method_calls.size(), 2U);

    const sema::TraitSignature& trait = checked.traits.begin()->second;
    ASSERT_EQ(trait.associated_types.size(), 1U);
    EXPECT_EQ(trait.associated_types.front().name, "Item");
    EXPECT_EQ(trait.associated_types.front().ordinal, 0U);
    EXPECT_TRUE(query::is_valid(trait.associated_types.front().member_key));
    ASSERT_EQ(trait.requirements.size(), 1U);
    EXPECT_EQ(checked.types.display_name(trait.requirements.front().return_type), "Self.Item");

    const sema::TraitImplInfo& impl = checked.trait_impls.begin()->second;
    ASSERT_EQ(impl.associated_types.size(), 1U);
    EXPECT_EQ(impl.associated_types.front().name, "Item");
    EXPECT_EQ(checked.types.display_name(impl.associated_types.front().value_type), "i32");
    EXPECT_EQ(impl.associated_types.front().member_key, trait.associated_types.front().member_key);

    const sema::TraitPredicate& where_predicate = checked.trait_predicates.front();
    ASSERT_EQ(where_predicate.associated_type_equalities.size(), 1U);
    EXPECT_EQ(where_predicate.associated_type_equalities.front().name, "Item");
    EXPECT_EQ(checked.types.display_name(where_predicate.associated_type_equalities.front().value_type), "i32");

    const sema::TraitMethodCallBinding& param_env_call = checked.trait_method_calls.front();
    EXPECT_EQ(param_env_call.dispatch, sema::TraitMethodDispatchKind::param_env);
    EXPECT_EQ(param_env_call.method_name, "get");
    EXPECT_EQ(checked.types.display_name(param_env_call.return_type), "i32");

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait priv Source params=0 associated_types=1 requirements=1",
            "assoc_type Item",
            "requirement get(&Self) -> Self.Item",
            "impl Source for trait_associated_type_whitebox.Bytes associated_types=1 methods=1",
            "assoc_type Item = i32 requirement=0",
            "T: Source origin=where",
            "assoc_eq Item = i32",
            "trait_associated_type_whitebox.Bytes: Source origin=impl",
            "trait_call #0 param_env T.get -> i32 predicate=0",
            "trait_call #1 impl_override trait_associated_type_whitebox.Bytes.get -> i32 predicate=1",
        });

    const sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.traits.begin()->second.associated_types.size(), 1U);
    ASSERT_EQ(copied.trait_impls.begin()->second.associated_types.size(), 1U);
    ASSERT_EQ(copied.trait_predicates.front().associated_type_equalities.size(), 1U);
    EXPECT_EQ(copied.trait_predicates.front().associated_type_equalities.front().name, "Item");
}

TEST(CoreUnit, TraitAssociatedTypeEqualityNormalizesGenericProjectionTypes)
{
    const std::string_view source = "module trait_associated_projection_equality_whitebox;\n"
                                    "trait Source { type Item; }\n"
                                    "struct Bytes { value: i32; }\n"
                                    "impl Source for Bytes { type Item = i32; }\n"
                                    "fn project<T>(value: T) -> T.Item where T: Source<Item = i32> {\n"
                                    "  return 1;\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  return 0;\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait priv Source params=0 associated_types=1 requirements=0",
            "assoc_type Item",
            "T: Source origin=where",
            "assoc_eq Item = i32",
            "template priv value project params=1",
        });
}

TEST(CoreUnit, TraitAssociatedProjectionStaysAbstractWithoutEquality)
{
    const std::string_view source = "module trait_associated_projection_abstract_whitebox;\n"
                                    "trait Source {\n"
                                    "  type Item;\n"
                                    "  fn get(self: &Self) -> Self.Item;\n"
                                    "}\n"
                                    "fn project<T>(value: &T) -> T.Item where T: Source {\n"
                                    "  return value.get();\n"
                                    "}\n"
                                    "fn main() -> i32 { return 0; }\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait priv Source params=0 associated_types=1 requirements=1",
            "requirement get(&Self) -> Self.Item",
            "T: Source origin=where",
            "trait_call #0 param_env T.get -> T.Item predicate=0",
        });
}

TEST(CoreUnit, TraitAssociatedTypeRequirementsSupportMultipleOutputs)
{
    const std::string_view source = "module trait_associated_multiple_outputs_whitebox;\n"
                                    "trait Source {\n"
                                    "  type Item;\n"
                                    "  type Error;\n"
                                    "  fn convert(self: &Self, value: Self.Item) -> Self.Error;\n"
                                    "}\n"
                                    "struct Bytes { value: i32; }\n"
                                    "impl Source for Bytes {\n"
                                    "  type Item = i32;\n"
                                    "  type Error = u8;\n"
                                    "  fn convert(self: &Bytes, value: i32) -> u8 {\n"
                                    "    return 1u8;\n"
                                    "  }\n"
                                    "}\n"
                                    "fn main() -> i32 { return 0; }\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 1U);
    ASSERT_EQ(checked.trait_impls.size(), 1U);
    const sema::TraitSignature& trait = checked.traits.begin()->second;
    ASSERT_EQ(trait.associated_types.size(), 2U);
    EXPECT_EQ(trait.associated_types[0].name, "Item");
    EXPECT_EQ(trait.associated_types[1].name, "Error");

    const sema::TraitImplInfo& impl = checked.trait_impls.begin()->second;
    ASSERT_EQ(impl.associated_types.size(), 2U);
    EXPECT_EQ(checked.types.display_name(impl.associated_types[0].value_type), "i32");
    EXPECT_EQ(checked.types.display_name(impl.associated_types[1].value_type), "u8");

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait priv Source params=0 associated_types=2 requirements=1",
            "assoc_type Item",
            "assoc_type Error",
            "requirement convert(&Self, Self.Item) -> Self.Error",
            "impl Source for trait_associated_multiple_outputs_whitebox.Bytes associated_types=2 methods=1",
            "assoc_type Item = i32 requirement=0",
            "assoc_type Error = u8 requirement=1",
        });
}

TEST(CoreUnit, TraitAssociatedTypeEqualityAcceptsDifferentProjection)
{
    const std::string_view source = "module trait_associated_projection_equality_chain_whitebox;\n"
                                    "trait Source {\n"
                                    "  type Item;\n"
                                    "  type Error;\n"
                                    "}\n"
                                    "fn project<T>(value: T) -> i32 where T: Source<Item = T.Error> {\n"
                                    "  return 0;\n"
                                    "}\n"
                                    "fn main() -> i32 { return 0; }\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.trait_predicates.size(), 1U);
    const sema::TraitPredicate& predicate = checked.trait_predicates.front();
    ASSERT_EQ(predicate.associated_type_equalities.size(), 1U);
    EXPECT_EQ(predicate.associated_type_equalities.front().name, "Item");
    EXPECT_EQ(checked.types.display_name(predicate.associated_type_equalities.front().value_type), "T.Error");

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait priv Source params=0 associated_types=2 requirements=0",
            "T: Source origin=where",
            "assoc_eq Item = T.Error",
        });
}

TEST(CoreUnit, TraitAssociatedProjectionTypeTableInternsDisplayAndCopies)
{
    sema::TypeTable types;
    const sema::TypeHandle base = types.generic_param("T");
    query::MemberKey member;
    member.global_id = 42;

    const sema::TypeHandle first = types.associated_projection(base, member, "Item");
    const sema::TypeHandle second = types.associated_projection(base, member, "Item");
    EXPECT_EQ(first.value, second.value);
    EXPECT_EQ(types.display_name(first), "T.Item");

    const sema::TypeTable copied = types;
    EXPECT_EQ(copied.display_name(first), "T.Item");
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
            "duplicate trait associated item: Reader.read",
        },
        {
            "module trait_self_generic_whitebox;\n"
            "trait Reader<Self> { fn read(self: &Self) -> i32; }\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate generic parameter `Self`",
        },
        {
            "module trait_requirement_generic_whitebox;\n"
            "trait Reader { fn read<T>(self: &Self, value: T) -> i32; }\n"
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
            "trait Reader<T> { fn read(self: &Self, values: [2]T) -> T; }\n"
            "struct File { value: i32; }\n"
            "impl Reader<i32> for File {\n"
            "  fn read(self: &File, values: [2]i32) -> i32 { return values[0]; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "array type cannot be used as a function parameter",
        },
        {
            "module trait_impl_generic_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl<T> Reader for File {\n"
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
            "trait Reader<T> { fn read(self: &Self, value: T) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader for File {\n"
            "  fn read(self: &File, value: i32) -> i32 { return value; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait type arguments for Reader: expected 1, got 0",
        },
        {
            "module trait_generic_arity_mismatch_whitebox;\n"
            "trait Reader<A, B> { fn read(self: &Self, value: A) -> B; }\n"
            "struct File { value: i32; }\n"
            "impl Reader<i32> for File {\n"
            "  fn read(self: &File, value: i32) -> i32 { return value; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait type arguments for Reader: expected 2, got 1",
        },
        {
            "module trait_type_args_on_plain_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "struct File { value: i32; }\n"
            "impl Reader<i32> for File {\n"
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
        {
            "module trait_predicate_duplicate_bound_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "fn use_reader<T>(value: T) -> i32 where T: Reader + Reader {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate capability `Reader` for generic parameter `T`",
        },
        {
            "module trait_predicate_generic_arity_whitebox;\n"
            "trait Needs<T> {}\n"
            "fn use_needs<U>(value: U) -> i32 where U: Needs {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "too few trait predicate type arguments for Needs: expected 1, got 0",
        },
        {
            "module trait_predicate_forward_missing_bound_whitebox;\n"
            "trait Reader { fn read(self: &Self) -> i32; }\n"
            "fn use_reader<T>(value: T) -> i32 where T: Reader {\n"
            "  return 0;\n"
            "}\n"
            "fn forward_reader<T>(value: T) -> i32 {\n"
            "  return use_reader<T>(value);\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "does not satisfy trait predicate `Reader`",
        },
        {
            "module trait_associated_type_duplicate_trait_whitebox;\n"
            "trait Source {\n"
            "  type Item;\n"
            "  type Item;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate trait associated item: Source.Item",
        },
        {
            "module trait_associated_type_generic_unsupported_whitebox;\n"
            "trait Source {\n"
            "  type Item<T>;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "generic associated types are not supported",
        },
        {
            "module trait_associated_type_requirement_target_whitebox;\n"
            "trait Source {\n"
            "  type Item = i32;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait associated type requirement must not have a target type",
        },
        {
            "module trait_impl_target_not_named_inprocess_whitebox;\n"
            "struct File { value: i32; }\n"
            "impl *const File for File {\n"
            "  fn read(self: &File) -> i32 { return self.value; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl target must be a named trait",
        },
        {
            "module trait_associated_type_impl_generic_unsupported_whitebox;\n"
            "trait Source { type Item; }\n"
            "struct Bytes { value: i32; }\n"
            "impl Source for Bytes {\n"
            "  type Item<T> = i32;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "generic associated types are not supported",
        },
        {
            "module trait_associated_type_missing_impl_whitebox;\n"
            "trait Source { type Item; }\n"
            "struct Bytes { value: i32; }\n"
            "impl Source for Bytes {}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl missing associated type: Source for trait_associated_type_missing_impl_whitebox.Bytes.Item",
        },
        {
            "module trait_associated_type_unknown_impl_whitebox;\n"
            "trait Source { type Item; }\n"
            "struct Bytes { value: i32; }\n"
            "impl Source for Bytes {\n"
            "  type Other = i32;\n"
            "  type Item = i32;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl associated type is not required: Source for "
            "trait_associated_type_unknown_impl_whitebox.Bytes.Other",
        },
        {
            "module trait_associated_type_duplicate_impl_whitebox;\n"
            "trait Source { type Item; }\n"
            "struct Bytes { value: i32; }\n"
            "impl Source for Bytes {\n"
            "  type Item = i32;\n"
            "  type Item = i32;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate trait impl associated type: Source for "
            "trait_associated_type_duplicate_impl_whitebox.Bytes.Item",
        },
        {
            "module trait_associated_type_signature_mismatch_whitebox;\n"
            "trait Source {\n"
            "  type Item;\n"
            "  fn get(self: &Self) -> Self.Item;\n"
            "}\n"
            "struct Bytes { value: i32; }\n"
            "impl Source for Bytes {\n"
            "  type Item = i32;\n"
            "  fn get(self: &Bytes) -> u8 { return 0u8; }\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait impl method signature does not match requirement",
        },
        {
            "module trait_associated_type_unknown_equality_whitebox;\n"
            "trait Source { type Item; }\n"
            "fn use_source<T>(value: T) -> i32 where T: Source<Missing = i32> {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "trait Source has no associated type `Missing`",
        },
        {
            "module trait_associated_type_duplicate_equality_whitebox;\n"
            "trait Source { type Item; }\n"
            "fn use_source<T>(value: T) -> i32 where T: Source<Item = i32, Item = i32> {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "duplicate associated type equality for Source.Item",
        },
        {
            "module trait_associated_type_builtin_equality_whitebox;\n"
            "fn use_eq<T>(value: T) -> i32 where T: Eq<Item = i32> {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "builtin capability `Eq` has no associated type `Item`",
        },
        {
            "module trait_associated_type_missing_bound_whitebox;\n"
            "trait Source { type Item; }\n"
            "fn use_item<T>(value: T) -> T.Item {\n"
            "  return value;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "associated type projection T.Item requires a trait bound",
        },
        {
            "module trait_associated_type_unknown_projection_whitebox;\n"
            "fn use_item<T>(value: T) -> T.Item {\n"
            "  return value;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "unknown associated type projection T.Item",
        },
        {
            "module trait_associated_type_ambiguous_projection_whitebox;\n"
            "trait SourceA { type Item; }\n"
            "trait SourceB { type Item; }\n"
            "fn use_item<T>(value: T) -> T.Item where T: SourceA + SourceB {\n"
            "  return value;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "ambiguous associated type projection T.Item",
        },
        {
            "module trait_associated_type_projection_cycle_whitebox;\n"
            "trait Source { type Item; }\n"
            "fn use_item<T>(value: T) -> i32 where T: Source<Item = T.Item> {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 { return 0; }\n",
            "associated type equality forms a projection cycle: Source.Item",
        },
        {
            "module trait_associated_type_equality_unsatisfied_whitebox;\n"
            "trait Source { type Item; }\n"
            "struct Bytes { value: i32; }\n"
            "impl Source for Bytes { type Item = u8; }\n"
            "fn use_i32<T>(value: T) -> i32 where T: Source<Item = i32> {\n"
            "  return 0;\n"
            "}\n"
            "fn main() -> i32 {\n"
            "  let bytes = Bytes { value: 1 };\n"
            "  return use_i32<Bytes>(bytes);\n"
            "}\n",
            "trait associated type equality is not satisfied: Source for "
            "trait_associated_type_equality_unsatisfied_whitebox.Bytes.Item expected i32, got u8",
        },
    };

    for (const auto& [source, diagnostic] : cases) {
        expect_trait_source_diagnostic(source, diagnostic);
    }
}

TEST(CoreUnit, TraitDefaultMethodsTypeCheckAndRecordInheritedOrigin)
{
    const std::string_view source = "module trait_default_method_inherited_call;\n"
                                    "trait Reader {\n"
                                    "  fn read(self: &Self) -> i32;\n"
                                    "  fn is_empty(self: &Self) -> bool {\n"
                                    "    return self.read() == 0;\n"
                                    "  }\n"
                                    "}\n"
                                    "struct File { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> i32 {\n"
                                    "    return self.value;\n"
                                    "  }\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let file = File { value: 0 };\n"
                                    "  if file.is_empty() {\n"
                                    "    if file.is_empty() { return 0; }\n"
                                    "  }\n"
                                    "  return 1;\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 1U);
    const sema::TraitSignature& trait = checked.traits.begin()->second;
    ASSERT_EQ(trait.requirements.size(), 2U);
    EXPECT_FALSE(trait.requirements[0].has_default_body);
    EXPECT_TRUE(trait.requirements[1].has_default_body);

    ASSERT_EQ(checked.trait_impls.size(), 1U);
    const sema::TraitImplInfo& impl = checked.trait_impls.begin()->second;
    ASSERT_EQ(impl.methods.size(), 2U);
    bool saw_read_override = false;
    bool saw_empty_default = false;
    for (const sema::TraitImplMethodInfo& method : impl.methods) {
        if (method.name == "read") {
            saw_read_override = method.origin == sema::TraitImplMethodOrigin::impl_override;
        }
        if (method.name == "is_empty") {
            saw_empty_default = method.origin == sema::TraitImplMethodOrigin::trait_default;
        }
    }
    EXPECT_TRUE(saw_read_override);
    EXPECT_TRUE(saw_empty_default);

    ASSERT_EQ(checked.trait_default_method_instances.size(), 1U);
    const sema::TraitDefaultMethodInstanceInfo& default_instance = checked.trait_default_method_instances.front();
    EXPECT_EQ(default_instance.requirement_ordinal, 1U);
    EXPECT_EQ(default_instance.signature.name, "is_empty");
    EXPECT_TRUE(default_instance.signature.is_trait_default_method_instance);
    EXPECT_EQ(checked.types.display_name(default_instance.signature.method_owner_type),
        "trait_default_method_inherited_call.File");
    const auto default_function = checked.functions.find(default_instance.key);
    ASSERT_NE(default_function, checked.functions.end());
    EXPECT_TRUE(default_function->second.is_trait_default_method_instance);
    EXPECT_EQ(default_function->second.c_name, default_instance.signature.c_name);

    const sema::CheckedModule copied = checked;
    ASSERT_EQ(copied.trait_default_method_instances.size(), 1U);
    EXPECT_TRUE(copied.trait_default_method_instances.front().signature.is_trait_default_method_instance);
    EXPECT_EQ(copied.trait_default_method_instances.front().signature.c_name.view(),
        default_instance.signature.c_name.view());

    sema::SemanticOptions transient_options;
    transient_options.retain_generic_side_tables = false;
    const sema::CheckedModule transient_checked = analyze_trait_source(source, transient_options);
    EXPECT_TRUE(transient_checked.trait_default_method_instances.empty());
    bool saw_transient_default_function = false;
    for (const sema::TraitMethodCallBinding& call : transient_checked.trait_method_calls) {
        if (call.method_name != "is_empty" || call.dispatch != sema::TraitMethodDispatchKind::trait_default) {
            continue;
        }
        const auto function = transient_checked.functions.find(call.function_key);
        ASSERT_NE(function, transient_checked.functions.end());
        saw_transient_default_function = function->second.is_trait_default_method_instance;
    }
    EXPECT_TRUE(saw_transient_default_function);

    bool saw_default_body_param_env_call = false;
    bool saw_concrete_trait_default_call = false;
    sema::FunctionLookupKey concrete_default_call_key;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "read" && call.dispatch == sema::TraitMethodDispatchKind::param_env) {
            saw_default_body_param_env_call = true;
        }
        if (call.method_name == "is_empty" && call.dispatch == sema::TraitMethodDispatchKind::trait_default) {
            saw_concrete_trait_default_call = true;
            concrete_default_call_key = call.function_key;
            EXPECT_TRUE(sema::is_valid(call.function_key));
        }
    }
    EXPECT_TRUE(saw_default_body_param_env_call);
    EXPECT_TRUE(saw_concrete_trait_default_call);
    EXPECT_EQ(concrete_default_call_key, default_instance.key);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "requirement is_empty(&Self) -> bool default",
            "impl Reader for trait_default_method_inherited_call.File associated_types=0 methods=2",
            "method read requirement=0 origin=impl_override",
            "method is_empty requirement=1 origin=trait_default",
            "Self: Reader origin=trait_self",
            "param_env Self.read -> i32",
            "trait_default trait_default_method_inherited_call.File.is_empty -> bool",
            "trait_default_method_instances 1",
            "trait_default_instance #0 trait_default_method_inherited_call.File.is_empty -> bool requirement=1",
        });
}

TEST(CoreUnit, TraitDefaultMethodsInstantiateAssociatedTypeProjectionBody)
{
    const std::string_view source = "module trait_default_method_associated_body;\n"
                                    "trait Source {\n"
                                    "  type Item;\n"
                                    "  fn get(self: &Self) -> Self.Item;\n"
                                    "  fn fallback(self: &Self, value: Self.Item) -> Self.Item {\n"
                                    "    let copy: Self.Item = value;\n"
                                    "    return copy;\n"
                                    "  }\n"
                                    "}\n"
                                    "struct Bytes { value: i32; }\n"
                                    "impl Source for Bytes {\n"
                                    "  type Item = i32;\n"
                                    "  fn get(self: &Bytes) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let bytes = Bytes { value: 7 };\n"
                                    "  return bytes.fallback(12) - 12;\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.trait_default_method_instances.size(), 1U);
    const sema::TraitDefaultMethodInstanceInfo& instance = checked.trait_default_method_instances.front();
    EXPECT_EQ(instance.signature.name, "fallback");
    EXPECT_TRUE(instance.signature.is_trait_default_method_instance);
    ASSERT_EQ(instance.signature.param_types.size(), 2U);
    EXPECT_EQ(checked.types.display_name(instance.signature.param_types[1]), "i32");
    EXPECT_EQ(checked.types.display_name(instance.signature.return_type), "i32");

    bool saw_concrete_fallback = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "fallback" && call.dispatch == sema::TraitMethodDispatchKind::trait_default) {
            saw_concrete_fallback = true;
            EXPECT_EQ(call.function_key, instance.key);
            EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        }
    }
    EXPECT_TRUE(saw_concrete_fallback);
}

TEST(CoreUnit, TraitDefaultMethodInstanceBodyViewsValidateSyntaxBackedInstances)
{
    const std::string_view source = "module trait_default_method_body_view;\n"
                                    "trait Reader {\n"
                                    "  fn read(self: &Self) -> i32;\n"
                                    "  fn is_empty(self: &Self) -> bool {\n"
                                    "    return self.read() == 0;\n"
                                    "  }\n"
                                    "}\n"
                                    "struct File { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let file = File { value: 0 };\n"
                                    "  if file.is_empty() { return 0; }\n"
                                    "  return 1;\n"
                                    "}\n";

    TraitAnalysisWithAst analysis = analyze_trait_source_with_ast(source);
    ASSERT_EQ(analysis.checked.trait_default_method_instances.size(), 1U);
    const sema::TraitDefaultMethodInstanceInfo& instance = analysis.checked.trait_default_method_instances.front();

    const sema::TraitDefaultMethodInstanceBodyView view =
        analysis.checked.trait_default_method_instance_body_view(analysis.module, 0U);
    ASSERT_TRUE(sema::is_valid(view));
    ASSERT_NE(view.instance, nullptr);
    ASSERT_NE(view.signature, nullptr);
    ASSERT_NE(view.side_tables, nullptr);
    ASSERT_NE(view.item, nullptr);
    EXPECT_EQ(view.instance, &instance);
    EXPECT_EQ(view.signature, &instance.signature);
    EXPECT_EQ(view.body.value, instance.body.value);
    EXPECT_TRUE(view.item->is_trait_default_method);

    const sema::TraitDefaultMethodInstanceBodyView out_of_range =
        analysis.checked.trait_default_method_instance_body_view(
            analysis.module, analysis.checked.trait_default_method_instances.size());
    EXPECT_FALSE(sema::is_valid(out_of_range));

    sema::TraitDefaultMethodInstanceInfo missing_item = instance;
    missing_item.item = syntax::INVALID_ITEM_ID;
    EXPECT_FALSE(
        sema::is_valid(analysis.checked.trait_default_method_instance_body_view(analysis.module, missing_item)));

    sema::TraitDefaultMethodInstanceInfo missing_body = instance;
    missing_body.body = syntax::INVALID_STMT_ID;
    EXPECT_FALSE(
        sema::is_valid(analysis.checked.trait_default_method_instance_body_view(analysis.module, missing_body)));

    sema::TraitDefaultMethodInstanceInfo conflicted = instance;
    conflicted.signature.has_conflict = true;
    EXPECT_FALSE(sema::is_valid(analysis.checked.trait_default_method_instance_body_view(analysis.module, conflicted)));

    sema::TraitDefaultMethodInstanceInfo invalid_impl = instance;
    invalid_impl.impl_key = {};
    EXPECT_FALSE(
        sema::is_valid(analysis.checked.trait_default_method_instance_body_view(analysis.module, invalid_impl)));

    sema::TraitDefaultMethodInstanceInfo plain_function_signature = instance;
    plain_function_signature.signature.is_trait_default_method_instance = false;
    EXPECT_FALSE(sema::is_valid(
        analysis.checked.trait_default_method_instance_body_view(analysis.module, plain_function_signature)));

    sema::TraitDefaultMethodInstanceInfo plain_function_item = instance;
    bool found_plain_function = false;
    for (base::usize index = 0; index < analysis.module.items.size(); ++index) {
        const syntax::ItemNode* const item = analysis.module.items.ptr(index);
        if (item != nullptr && item->kind == syntax::ItemKind::fn_decl && !item->is_trait_default_method
            && syntax::is_valid(item->body)) {
            plain_function_item.item = syntax::ItemId{static_cast<base::u32>(index)};
            plain_function_item.body = item->body;
            found_plain_function = true;
            break;
        }
    }
    ASSERT_TRUE(found_plain_function);
    EXPECT_FALSE(
        sema::is_valid(analysis.checked.trait_default_method_instance_body_view(analysis.module, plain_function_item)));
}

TEST(CoreUnit, TraitDefaultMethodsExplicitOverrideWins)
{
    const std::string_view source = "module trait_default_method_override_call;\n"
                                    "trait Reader {\n"
                                    "  fn read(self: &Self) -> i32;\n"
                                    "  fn is_empty(self: &Self) -> bool {\n"
                                    "    return false;\n"
                                    "  }\n"
                                    "}\n"
                                    "struct File { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> i32 {\n"
                                    "    return self.value;\n"
                                    "  }\n"
                                    "  fn is_empty(self: &File) -> bool {\n"
                                    "    return self.value == 0;\n"
                                    "  }\n"
                                    "}\n"
                                    "fn main() -> i32 {\n"
                                    "  let file = File { value: 0 };\n"
                                    "  if file.is_empty() { return 0; }\n"
                                    "  return 1;\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.trait_impls.size(), 1U);
    const sema::TraitImplInfo& impl = checked.trait_impls.begin()->second;
    ASSERT_EQ(impl.methods.size(), 2U);
    for (const sema::TraitImplMethodInfo& method : impl.methods) {
        EXPECT_EQ(method.origin, sema::TraitImplMethodOrigin::impl_override);
    }
    EXPECT_TRUE(checked.trait_default_method_instances.empty());

    bool saw_override_call = false;
    bool saw_trait_default_call = false;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "is_empty" && call.dispatch == sema::TraitMethodDispatchKind::impl_override) {
            saw_override_call = true;
        }
        if (call.method_name == "is_empty" && call.dispatch == sema::TraitMethodDispatchKind::trait_default) {
            saw_trait_default_call = true;
        }
    }
    EXPECT_TRUE(saw_override_call);
    EXPECT_FALSE(saw_trait_default_call);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "requirement is_empty(&Self) -> bool default",
            "method is_empty requirement=1 origin=impl_override",
            "impl_override trait_default_method_override_call.File.is_empty -> bool",
        });
}

TEST(CoreUnit, TraitDefaultMethodsApplyGenericTraitWherePredicates)
{
    const std::string_view source = "module trait_default_method_generic_where;\n"
                                    "trait Source {\n"
                                    "  type Item;\n"
                                    "  fn get(self: &Self) -> Self.Item;\n"
                                    "}\n"
                                    "trait Adapter<T> where T: Source<Item = i32> {\n"
                                    "  fn value(self: &Self, input: &T) -> i32 {\n"
                                    "    return input.get();\n"
                                    "  }\n"
                                    "}\n"
                                    "struct Bytes { value: i32; }\n"
                                    "impl Source for Bytes {\n"
                                    "  type Item = i32;\n"
                                    "  fn get(self: &Bytes) -> i32 { return self.value; }\n"
                                    "}\n"
                                    "struct Box { value: i32; }\n"
                                    "impl Adapter<Bytes> for Box {}\n"
                                    "fn main() -> i32 {\n"
                                    "  let box = Box { value: 0 };\n"
                                    "  let bytes = Bytes { value: 9 };\n"
                                    "  return box.value(&bytes);\n"
                                    "}\n";

    const sema::CheckedModule checked = analyze_trait_source(source);
    ASSERT_EQ(checked.traits.size(), 2U);
    ASSERT_EQ(checked.trait_impls.size(), 2U);

    bool saw_generic_trait_self_predicate = false;
    bool saw_source_where_predicate = false;
    for (const sema::TraitPredicate& predicate : checked.trait_predicates) {
        if (predicate.origin == sema::TraitPredicateOrigin::trait_self && predicate.trait_name == "Adapter") {
            saw_generic_trait_self_predicate = true;
            EXPECT_EQ(predicate.trait_args.size(), 1U);
            ASSERT_FALSE(predicate.trait_args.empty());
            EXPECT_EQ(checked.types.display_name(predicate.trait_args.front()), "T");
        }
        if (predicate.origin == sema::TraitPredicateOrigin::explicit_where && predicate.trait_name == "Source") {
            saw_source_where_predicate = true;
            ASSERT_EQ(predicate.associated_type_equalities.size(), 1U);
            EXPECT_EQ(predicate.associated_type_equalities.front().name, "Item");
            EXPECT_EQ(checked.types.display_name(predicate.associated_type_equalities.front().value_type), "i32");
        }
    }
    EXPECT_TRUE(saw_generic_trait_self_predicate);
    EXPECT_TRUE(saw_source_where_predicate);

    ASSERT_EQ(checked.trait_default_method_instances.size(), 1U);
    const sema::TraitDefaultMethodInstanceInfo& default_instance = checked.trait_default_method_instances.front();
    EXPECT_EQ(default_instance.signature.name, "value");
    EXPECT_TRUE(default_instance.signature.is_trait_default_method_instance);
    EXPECT_EQ(checked.types.display_name(default_instance.signature.method_owner_type),
        "trait_default_method_generic_where.Box");
    ASSERT_EQ(default_instance.signature.param_types.size(), 2U);
    EXPECT_EQ(checked.types.display_name(default_instance.signature.param_types[1]),
        "&trait_default_method_generic_where.Bytes");
    EXPECT_EQ(checked.types.display_name(default_instance.signature.return_type), "i32");

    bool saw_default_body_source_call = false;
    bool saw_concrete_adapter_default_call = false;
    sema::FunctionLookupKey concrete_default_call_key;
    for (const sema::TraitMethodCallBinding& call : checked.trait_method_calls) {
        if (call.method_name == "get" && call.dispatch == sema::TraitMethodDispatchKind::param_env) {
            saw_default_body_source_call = true;
            EXPECT_EQ(checked.types.display_name(call.self_type), "T");
            EXPECT_EQ(checked.types.display_name(call.return_type), "i32");
        }
        if (call.method_name == "value" && call.dispatch == sema::TraitMethodDispatchKind::trait_default) {
            saw_concrete_adapter_default_call = true;
            concrete_default_call_key = call.function_key;
            EXPECT_TRUE(sema::is_valid(call.function_key));
            EXPECT_EQ(checked.types.display_name(call.self_type), "trait_default_method_generic_where.Box");
        }
    }
    EXPECT_TRUE(saw_default_body_source_call);
    EXPECT_TRUE(saw_concrete_adapter_default_call);
    EXPECT_EQ(concrete_default_call_key, default_instance.key);

    const std::string dump = sema::dump_checked_module(checked);
    expect_contains_all(dump,
        {
            "trait priv Adapter<T0> params=1 associated_types=0 requirements=1",
            "requirement value(&Self, &T) -> i32 default",
            "T: Source origin=where",
            "Self: Adapter<T> origin=trait_self",
            "impl Adapter<trait_default_method_generic_where.Bytes> for trait_default_method_generic_where.Box "
            "associated_types=0 methods=1",
            "method value requirement=0 origin=trait_default",
            "param_env T.get -> i32",
            "trait_default trait_default_method_generic_where.Box.value -> i32",
            "trait_default_method_instances 1",
            "trait_default_instance #0 trait_default_method_generic_where.Box.value -> i32 requirement=0",
        });
}

TEST(CoreUnit, TraitDefaultMethodsRejectBadExplicitOverrideWithoutFallingBackToDefault)
{
    const std::string_view source = "module trait_default_method_bad_override;\n"
                                    "trait Reader {\n"
                                    "  fn read(self: &Self) -> i32 {\n"
                                    "    return 0;\n"
                                    "  }\n"
                                    "}\n"
                                    "struct File { value: i32; }\n"
                                    "impl Reader for File {\n"
                                    "  fn read(self: &File) -> bool {\n"
                                    "    return false;\n"
                                    "  }\n"
                                    "}\n"
                                    "fn main() -> i32 { return 0; }\n";

    const std::string output = analyze_trait_source_failure(source);
    expect_contains(output,
        "trait impl method signature does not match requirement: Reader for "
        "trait_default_method_bad_override.File.read");
    expect_contains(
        output, "trait method has a default body, but an explicit override must still match the requirement signature");
    expect_contains(output, "trait impl missing method: Reader for trait_default_method_bad_override.File.read");
}

TEST_F(AurexIntegrationTest, TraitImplRegistrySamples)
{
    const fs::path source = positive_sample("traits", "trait_impl_registry.ax");
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(source)).output;
    expect_contains_all(checked,
        {
            "trait Reader params=0 associated_types=0 requirements=1",
            "requirement read(&Self) -> i32",
            "impl Reader for trait_impl_registry.File associated_types=0 methods=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(source));

    const fs::path static_dispatch_source = positive_sample("traits", "trait_method_static_dispatch.ax");
    const std::string static_dispatch_checked =
        require_success(aurexc() + " --emit=checked " + q(static_dispatch_source)).output;
    expect_contains_all(static_dispatch_checked,
        {
            "trait_method_calls 2",
            "trait_call #0 param_env T.read -> i32 predicate=0",
            "trait_call #1 impl_override trait_method_static_dispatch.File.read -> i32 predicate=1",
            "@c_name=m0_trait_method_static_dispatch_File_trait_impl_Reader__read",
        });
    const std::string static_dispatch_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(static_dispatch_source)).output;
    expect_contains(static_dispatch_llvm, "call i32 @m0_trait_method_static_dispatch_File_trait_impl_Reader__read");

    const fs::path associated_static_source = positive_sample("traits", "trait_method_associated_static_dispatch.ax");
    const std::string associated_static_checked =
        require_success(aurexc() + " --emit=checked " + q(associated_static_source)).output;
    expect_contains_all(associated_static_checked,
        {
            "trait_method_calls 1",
            "trait_call #0 impl_override trait_method_associated_static_dispatch.File.answer -> i32 predicate=0",
            "@c_name=m0_trait_method_associated_static_dispatch_File_trait_impl_Factory__answer",
        });
    const std::string associated_static_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(associated_static_source)).output;
    expect_contains(
        associated_static_llvm, "call i32 @m0_trait_method_associated_static_dispatch_File_trait_impl_Factory__answer");

    const fs::path associated_type_source = positive_sample("traits", "trait_associated_type_where_equality.ax");
    const std::string associated_type_checked =
        require_success(aurexc() + " --emit=checked " + q(associated_type_source)).output;
    expect_contains_all(associated_type_checked,
        {
            "trait priv Source params=0 associated_types=1 requirements=1",
            "assoc_type Item",
            "requirement get(&Self) -> Self.Item",
            "impl Source for trait_associated_type_where_equality.Bytes associated_types=1 methods=1",
            "assoc_type Item = i32 requirement=0",
            "T: Source origin=where",
            "assoc_eq Item = i32",
            "trait_call #0 param_env T.get -> i32 predicate=0",
            "trait_call #1 impl_override trait_associated_type_where_equality.Bytes.get -> i32 predicate=1",
        });
    const std::string associated_type_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(associated_type_source)).output;
    expect_contains(
        associated_type_llvm, "call i32 @m0_trait_associated_type_where_equality_Bytes_trait_impl_Source__get");

    const fs::path inherent_precedence_source = positive_sample("traits", "trait_method_inherent_precedence.ax");
    const std::string inherent_precedence_checked =
        require_success(aurexc() + " --emit=checked " + q(inherent_precedence_source)).output;
    expect_contains_all(inherent_precedence_checked,
        {
            "trait_method_calls 0",
            "@c_name=m0_trait_method_inherent_precedence_File_trait_impl_Reader__read",
            "@c_name=m0_trait_method_inherent_precedence_File_read",
        });
    const std::string inherent_precedence_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(inherent_precedence_source)).output;
    expect_contains(inherent_precedence_llvm, "call i32 @m0_trait_method_inherent_precedence_File_read");

    const fs::path qualified_source = positive_sample("traits", "trait_impl_qualified_registry.ax");
    const std::string qualified_checked =
        require_success(aurexc() + " --emit=checked " + sample_import_flags() + " " + q(qualified_source)).output;
    expect_contains_all(qualified_checked,
        {
            "trait_impls 1",
            "impl Visible for trait_impl_qualified_registry.File associated_types=0 methods=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + sample_import_flags() + " " + q(qualified_source));

    const fs::path selective_source = positive_sample("traits", "trait_impl_selective_registry.ax");
    const std::string selective_checked =
        require_success(aurexc() + " --emit=checked " + sample_import_flags() + " " + q(selective_source)).output;
    expect_contains_all(selective_checked,
        {
            "trait_impls 1",
            "impl Visible for trait_impl_selective_registry.File associated_types=0 methods=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + sample_import_flags() + " " + q(selective_source));

    const fs::path inherited_default_source =
        samples_root() / "checked" / "traits" / "trait_default_method_inherited.ax";
    const std::string inherited_default_checked =
        require_success(aurexc() + " --emit=checked " + q(inherited_default_source)).output;
    expect_contains_all(inherited_default_checked,
        {
            "requirement is_empty(&Self) -> bool default",
            "impl Reader for trait_default_method_inherited.File associated_types=0 methods=2",
            "method is_empty requirement=1 origin=trait_default",
            "param_env Self.read -> i32",
        });
    const fs::path override_default_source = samples_root() / "checked" / "traits" / "trait_default_method_override.ax";
    const std::string override_default_checked =
        require_success(aurexc() + " --emit=checked " + q(override_default_source)).output;
    expect_contains_all(override_default_checked,
        {
            "requirement is_empty(&Self) -> bool default",
            "impl Reader for trait_default_method_override.File associated_types=0 methods=2",
            "method is_empty requirement=1 origin=impl_override",
        });

    const fs::path inherited_dispatch_source = positive_sample("traits", "trait_default_method_inherited_dispatch.ax");
    const std::string inherited_dispatch_checked =
        require_success(aurexc() + " --emit=checked " + q(inherited_dispatch_source)).output;
    expect_contains_all(inherited_dispatch_checked,
        {
            "trait_default trait_default_method_inherited_dispatch.File.is_empty -> bool",
            "fn method trait_default_method_inherited_dispatch.File.is_empty -> bool",
            "_trait_default_Reader_",
        });
    const std::string inherited_dispatch_ir =
        require_success(aurexc() + " --emit=ir " + q(inherited_dispatch_source)).output;
    expect_contains_all(inherited_dispatch_ir,
        {
            "trait_default_method_inherited_dispatch_File_trait_default_Reader",
            "call m0_trait_default_method_inherited_dispatch_File_trait_impl_Reader__read",
        });

    const fs::path override_dispatch_source = positive_sample("traits", "trait_default_method_override_dispatch.ax");
    const std::string override_dispatch_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(override_dispatch_source)).output;
    expect_contains(
        override_dispatch_llvm, "call i1 @m0_trait_default_method_override_dispatch_File_trait_impl_Reader__is_empty");
    expect_not_contains(override_dispatch_llvm, "_trait_default_Reader_");

    const fs::path generic_dispatch_source = positive_sample("traits", "trait_default_method_generic_dispatch.ax");
    const std::string generic_dispatch_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(generic_dispatch_source)).output;
    expect_contains_all(generic_dispatch_llvm,
        {
            "call i1 @m0_trait_default_method_generic_dispatch_File_trait_default_Reader",
            "call i32 @m0_trait_default_method_generic_dispatch_File_trait_impl_Reader__read",
        });

    const fs::path associated_default_source =
        positive_sample("traits", "trait_default_method_associated_type_dispatch.ax");
    const std::string associated_default_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(associated_default_source)).output;
    expect_contains_all(associated_default_llvm,
        {
            "Bytes_trait_default_Source",
            "call i32 @m0_trait_default_method_associated_type_dispatch_Bytes_trait_default_Source",
        });

    const fs::path inherent_default_source = positive_sample("traits", "trait_default_method_inherent_precedence.ax");
    const std::string inherent_default_llvm =
        require_success(aurexc() + " --emit=llvm-ir " + q(inherent_default_source)).output;
    expect_contains(inherent_default_llvm, "call i1 @m0_trait_default_method_inherent_precedence_File_is_empty");
    expect_not_contains(inherent_default_llvm, "_trait_default_Reader_");

    expect_negative_trait_sample("trait_impl_missing_method.ax", "trait impl missing method");
    expect_negative_trait_sample("trait_impl_duplicate_method.ax", "duplicate trait impl method");
    expect_negative_trait_sample(
        "trait_impl_signature_mismatch.ax", "trait impl method signature does not match requirement");
    expect_negative_trait_sample("trait_impl_unknown_method.ax", "trait impl method is not required");
    expect_negative_trait_sample("trait_impl_unknown_trait.ax", "unknown trait: Missing");
    expect_negative_trait_sample("trait_impl_target_not_named.ax", "trait impl target must be a named trait");
    expect_negative_trait_sample("trait_impl_self_type_not_named.ax", "impl target must be a named type");
    expect_negative_trait_sample("trait_impl_private_trait.ax", "trait is private: samplelib.traits.Hidden");
    expect_negative_trait_sample(
        "trait_impl_unknown_qualified_trait.ax", "unknown trait in module samplelib.traits: Missing");
    expect_negative_trait_sample("trait_method_ambiguous_bound.ax", "ambiguous trait method `read`");
    expect_negative_trait_sample("trait_method_ambiguous_impl.ax", "ambiguous trait method `read`");
    expect_negative_trait_sample("trait_method_associated_missing_impl.ax", "has no visible impl for trait method");
    expect_negative_trait_sample("trait_method_missing_bound.ax", "requires a trait bound");
    expect_negative_trait_sample("trait_method_missing_impl.ax", "has no visible impl for trait method");
    expect_negative_trait_sample(
        "trait_associated_type_ambiguous_projection.ax", "ambiguous associated type projection T.Item");
    expect_negative_trait_sample(
        "trait_associated_type_builtin_equality.ax", "builtin capability `Eq` has no associated type `Item`");
    expect_negative_trait_sample(
        "trait_associated_type_duplicate_equality.ax", "duplicate associated type equality for Source.Item");
    expect_negative_trait_sample("trait_associated_type_duplicate_impl.ax",
        "duplicate trait impl associated type: Source for trait_associated_type_duplicate_impl.Bytes.Item");
    expect_negative_trait_sample(
        "trait_associated_type_duplicate_trait.ax", "duplicate trait associated item: Source.Item");
    expect_negative_trait_sample("trait_associated_type_equality_unsatisfied.ax",
        "trait associated type equality is not satisfied: Source for "
        "trait_associated_type_equality_unsatisfied.Bytes.Item expected i32, got u8");
    expect_negative_trait_sample(
        "trait_associated_type_generic_unsupported.ax", "generic associated types are not supported");
    expect_negative_trait_sample(
        "trait_associated_type_missing_bound.ax", "associated type projection T.Item requires a trait bound");
    expect_negative_trait_sample("trait_associated_type_missing_impl.ax",
        "trait impl missing associated type: Source for trait_associated_type_missing_impl.Bytes.Item");
    expect_negative_trait_sample(
        "trait_associated_type_projection_cycle.ax", "associated type equality forms a projection cycle: Source.Item");
    expect_negative_trait_sample(
        "trait_associated_type_signature_mismatch.ax", "trait impl method signature does not match requirement");
    expect_negative_trait_sample(
        "trait_associated_type_unknown_equality.ax", "trait Source has no associated type `Missing`");
    expect_negative_trait_sample("trait_associated_type_unknown_impl.ax",
        "trait impl associated type is not required: Source for trait_associated_type_unknown_impl.Bytes.Other");
    expect_negative_trait_sample("trait_default_method_missing_required.ax",
        "trait impl missing method: Reader for trait_default_method_missing_required.File.read");
    expect_negative_trait_sample("trait_default_method_return_mismatch.ax", "return type mismatch");
    expect_negative_trait_sample(
        "trait_default_method_self_field.ax", "field access requires a non-opaque struct value");

    const fs::path predicate_source = positive_sample("traits", "trait_predicate_where_generic.ax");
    const std::string predicate_checked = require_success(aurexc() + " --emit=checked " + q(predicate_source)).output;
    expect_contains_all(predicate_checked,
        {
            "trait_predicates 3",
            "T: Reader origin=where",
            "trait_predicate_where_generic.File: Reader origin=impl",
            "param_env forward_reader predicates=1",
            "param_env use_reader predicates=1",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(predicate_source));

    expect_negative_trait_sample(
        "trait_predicate_unsatisfied_generic_arg.ax", "does not satisfy trait predicate `Reader`");
    expect_negative_trait_sample("trait_impl_orphan_external.ax", "orphan trait impl is not allowed");
    expect_negative_trait_sample("trait_predicate_duplicate_bound.ax", "duplicate capability `Reader`");
    expect_negative_trait_sample(
        "trait_predicate_generic_trait_arity.ax", "too few trait predicate type arguments for Needs");
    expect_negative_trait_sample(
        "trait_predicate_forward_missing_bound.ax", "does not satisfy trait predicate `Reader`");

    const fs::path mixed_source = positive_sample("traits", "trait_predicate_mixed_bounds.ax");
    const std::string mixed_checked = require_success(aurexc() + " --emit=checked " + q(mixed_source)).output;
    expect_contains_all(mixed_checked,
        {
            "trait_predicates 3",
            "T: Eq origin=where",
            "T: Reader origin=where",
            "trait_predicate_mixed_bounds.Flag: Reader origin=impl",
            "param_env use_flag predicates=2",
        });
    require_success(aurexc() + " --emit=llvm-ir " + q(mixed_source));

    const fs::path local_trait_external_self =
        positive_sample("traits", "trait_impl_orphan_local_trait_external_self.ax");
    const std::string local_trait_checked =
        require_success(aurexc() + " --emit=checked " + sample_import_flags() + " " + q(local_trait_external_self))
            .output;
    expect_contains_all(local_trait_checked,
        {
            "trait_impls 1",
            "impl LocalReader for samplelib.traits.HiddenFile associated_types=0 methods=1",
            "samplelib.traits.HiddenFile: LocalReader origin=impl",
        });
    require_success(aurexc() + " --emit=llvm-ir " + sample_import_flags() + " " + q(local_trait_external_self));
}

} // namespace aurex::test

#include <aurex/frontend/sema/identifier.hpp>

#include <array>
#include <span>
#include <sstream>
#include <string_view>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u32 IDENTIFIER_TEST_MODULE_INDEX = 7U;
constexpr base::u32 IDENTIFIER_TEST_OWNER_TYPE = 11U;
constexpr base::u64 IDENTIFIER_TEST_GENERIC_IDENTITY = 0x0000000200000001ULL;

} // namespace

TEST(CoreUnit, SemaIdentifierHelpersCoverInterningRebindingAndLookupKeys)
{
    sema::IdentifierInterner first;
    sema::IdentifierInterner second;
    const sema::IdentId alpha_id = first.intern("alpha");
    const sema::IdentId second_alpha_id = second.intern("alpha");
    const sema::IdentId beta_id = first.intern("beta");

    const sema::InternedText empty = sema::intern_text(first, "");
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0U);
    EXPECT_EQ(empty.view(), "");
    EXPECT_TRUE(empty == nullptr);
    EXPECT_TRUE(nullptr == empty);

    const sema::InternedText alpha{alpha_id, &first};
    const sema::InternedText beta{beta_id, &first};
    const sema::InternedText alpha_from_second{second_alpha_id, &second};
    EXPECT_FALSE(alpha.empty());
    EXPECT_EQ(alpha.size(), 5U);
    const std::string_view alpha_data{alpha.data(), alpha.size()};
    EXPECT_EQ(alpha_data, "alpha");
    EXPECT_EQ(alpha.view(), "alpha");
    EXPECT_EQ(static_cast<std::string_view>(alpha), "alpha");
    EXPECT_TRUE(alpha == alpha_from_second);
    EXPECT_TRUE(alpha != beta);
    EXPECT_TRUE(alpha == std::string_view{"alpha"});
    EXPECT_TRUE(std::string_view{"alpha"} == alpha);
    EXPECT_TRUE(alpha == "alpha");
    EXPECT_TRUE("alpha" == alpha);
    EXPECT_TRUE(alpha != std::string_view{"beta"});
    EXPECT_TRUE(std::string_view{"beta"} != alpha);
    EXPECT_TRUE(alpha != "beta");
    EXPECT_TRUE("beta" != alpha);
    EXPECT_FALSE(alpha == nullptr);
    EXPECT_TRUE(nullptr != alpha);

    std::ostringstream out;
    out << alpha;
    EXPECT_EQ(out.str(), "alpha");

    sema::InternedText rebound = alpha;
    sema::rebind_interned_text(rebound, first);
    EXPECT_EQ(rebound.view(), "alpha");
    sema::rebind_interned_text(rebound, &second, first);
    EXPECT_EQ(rebound.interner, &first);

    sema::InternedText rebound_to_second = alpha;
    sema::rebind_interned_text(rebound_to_second, &first, second);
    EXPECT_EQ(rebound_to_second.interner, &second);
    EXPECT_EQ(rebound_to_second.view(), "alpha");

    const std::array<std::string_view, 2> parts{"module", "name"};
    EXPECT_EQ(sema::stable_fingerprint("module").byte_count, query::stable_fingerprint("module").byte_count);
    EXPECT_EQ(sema::stable_fingerprint(std::span<const std::string_view>{parts}),
        query::stable_identity_fingerprint(std::span<const std::string_view>{parts}));

    const sema::GenericParamIdentity generic = sema::generic_param_identity_from_text("T");
    EXPECT_TRUE(sema::is_valid(generic));
    EXPECT_FALSE(sema::is_valid(sema::INVALID_GENERIC_PARAM_IDENTITY));
    EXPECT_EQ(sema::GenericParamIdentityHash{}(sema::GenericParamIdentity{IDENTIFIER_TEST_GENERIC_IDENTITY}),
        sema::GenericParamIdentityHash{}(sema::GenericParamIdentity{IDENTIFIER_TEST_GENERIC_IDENTITY}));

    const sema::ModuleLookupKey module_key{IDENTIFIER_TEST_MODULE_INDEX, alpha_id};
    EXPECT_TRUE(sema::is_valid(module_key));
    EXPECT_FALSE(sema::is_valid(sema::ModuleLookupKey{}));
    EXPECT_FALSE(sema::is_valid(sema::ModuleLookupKey{IDENTIFIER_TEST_MODULE_INDEX, sema::INVALID_IDENT_ID}));
    EXPECT_EQ(sema::ModuleLookupKeyHash{}(module_key), sema::ModuleLookupKeyHash{}(module_key));
    EXPECT_EQ(sema::IdentIdHash{}(alpha_id), sema::IdentIdHash{}(alpha_id));

    const sema::MethodLookupKey method_key{IDENTIFIER_TEST_MODULE_INDEX, IDENTIFIER_TEST_OWNER_TYPE, alpha_id};
    EXPECT_TRUE(sema::is_valid(method_key));
    EXPECT_FALSE(sema::is_valid(sema::MethodLookupKey{}));
    EXPECT_FALSE(sema::is_valid(
        sema::MethodLookupKey{IDENTIFIER_TEST_MODULE_INDEX, sema::SEMA_LOOKUP_INVALID_KEY_PART, alpha_id}));
    EXPECT_FALSE(sema::is_valid(
        sema::MethodLookupKey{IDENTIFIER_TEST_MODULE_INDEX, IDENTIFIER_TEST_OWNER_TYPE, sema::INVALID_IDENT_ID}));
    EXPECT_EQ(sema::MethodLookupKeyHash{}(method_key), sema::MethodLookupKeyHash{}(method_key));

    const sema::FunctionLookupKey function_key{IDENTIFIER_TEST_MODULE_INDEX, IDENTIFIER_TEST_OWNER_TYPE, alpha_id};
    EXPECT_TRUE(sema::is_valid(function_key));
    EXPECT_TRUE(sema::is_valid(
        sema::FunctionLookupKey{IDENTIFIER_TEST_MODULE_INDEX, sema::SEMA_LOOKUP_INVALID_KEY_PART, alpha_id}));
    EXPECT_FALSE(sema::is_valid(sema::FunctionLookupKey{}));
    EXPECT_FALSE(sema::is_valid(
        sema::FunctionLookupKey{IDENTIFIER_TEST_MODULE_INDEX, IDENTIFIER_TEST_OWNER_TYPE, sema::INVALID_IDENT_ID}));
    EXPECT_EQ(sema::FunctionLookupKeyHash{}(function_key), sema::FunctionLookupKeyHash{}(function_key));

    const sema::EnumCaseLookupKey enum_case_key{IDENTIFIER_TEST_OWNER_TYPE, alpha_id};
    EXPECT_EQ(sema::EnumCaseLookupKeyHash{}(enum_case_key), sema::EnumCaseLookupKeyHash{}(enum_case_key));
}

} // namespace aurex::test

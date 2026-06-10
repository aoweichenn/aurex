#include <aurex/infrastructure/query/dyn_ownership_runtime_facts.hpp>

#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;

void refresh(query::DynOwnershipRuntimeFacts& facts)
{
    facts.summary = query::summarize_dyn_ownership_runtime_counts(facts);
    facts.fingerprint = query::dyn_ownership_runtime_facts_fingerprint(facts);
}

[[nodiscard]] query::DynOwnershipRuntimeFacts baseline()
{
    return query::m17_dyn_ownership_runtime_preparation_baseline();
}

} // namespace

TEST(QueryUnit, DynOwnershipRuntimeFactsExposeEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_kind_name(
                  query::DynOwnershipRuntimeBoundaryKind::owned_dyn_container),
        "owned_dyn_container");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_kind_name(
                  query::DynOwnershipRuntimeBoundaryKind::erased_drop_glue),
        "erased_drop_glue");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_kind_name(
                  query::DynOwnershipRuntimeBoundaryKind::allocator_boundary),
        "allocator_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_kind_name(
                  query::DynOwnershipRuntimeBoundaryKind::cleanup_dropck_boundary),
        "cleanup_dropck_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_stage_name(
                  query::DynOwnershipRuntimeStage::preparation),
        "preparation");
    EXPECT_EQ(query::dyn_ownership_runtime_stage_name(
                  query::DynOwnershipRuntimeStage::future_standard_library_boundary),
        "future_standard_library_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_stage_name(
                  query::DynOwnershipRuntimeStage::future_runtime_lowering_boundary),
        "future_runtime_lowering_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  query::DynOwnershipRuntimePolicy::owning_dyn_container_v1),
        "owning_dyn_container_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  query::DynOwnershipRuntimePolicy::owning_dyn_metadata_v1),
        "owning_dyn_metadata_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  query::DynOwnershipRuntimePolicy::dynamic_drop_metadata_v1),
        "dynamic_drop_metadata_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  query::DynOwnershipRuntimePolicy::allocator_placement_policy_v1),
        "allocator_placement_policy_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  query::DynOwnershipRuntimePolicy::allocator_metadata_v1),
        "allocator_metadata_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  query::DynOwnershipRuntimePolicy::cleanup_dropck_boundary_v1),
        "cleanup_dropck_boundary_v1");

    EXPECT_EQ(query::dyn_ownership_runtime_boundary_kind_name(
                  static_cast<query::DynOwnershipRuntimeBoundaryKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::dyn_ownership_runtime_stage_name(
                  static_cast<query::DynOwnershipRuntimeStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::dyn_ownership_runtime_policy_name(
                  static_cast<query::DynOwnershipRuntimePolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeBoundaryKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimePolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, DynOwnershipRuntimeFactsM17BaselineSeparatesRuntimeAndStdlibBoundaries)
{
    const query::DynOwnershipRuntimeFacts facts = baseline();
    ASSERT_TRUE(query::is_valid(facts));
    EXPECT_TRUE(query::is_valid_m17_dyn_ownership_runtime_preparation_baseline(facts));
    EXPECT_EQ(facts.subject, "M17 Dyn Ownership Runtime Preparation");
    EXPECT_EQ(facts.summary.boundary_count, 4U);
    EXPECT_EQ(facts.summary.owned_container_boundary_count, 1U);
    EXPECT_EQ(facts.summary.erased_drop_glue_boundary_count, 1U);
    EXPECT_EQ(facts.summary.allocator_boundary_count, 1U);
    EXPECT_EQ(facts.summary.cleanup_dropck_boundary_count, 1U);
    EXPECT_EQ(facts.summary.standard_library_blocked_count, 2U);
    EXPECT_EQ(facts.summary.runtime_lowering_blocked_count, 4U);
    EXPECT_EQ(facts.summary.box_surface_blocked_count, 1U);
    EXPECT_EQ(facts.summary.allocator_api_blocked_count, 1U);
    EXPECT_EQ(facts.summary.dynamic_drop_dispatch_blocked_count, 2U);
    EXPECT_EQ(facts.summary.borrowed_vtable_destructor_free_count, 1U);
    EXPECT_EQ(facts.summary.cleanup_dropck_bridge_count, 1U);

    ASSERT_EQ(facts.owned_containers.size(), 1U);
    ASSERT_EQ(facts.erased_drop_glue.size(), 1U);
    ASSERT_EQ(facts.allocator_boundaries.size(), 1U);
    ASSERT_EQ(facts.cleanup_dropck_boundaries.size(), 1U);
    EXPECT_EQ(facts.owned_containers.front().stage,
        query::DynOwnershipRuntimeStage::future_standard_library_boundary);
    EXPECT_EQ(facts.erased_drop_glue.front().stage,
        query::DynOwnershipRuntimeStage::future_runtime_lowering_boundary);
    EXPECT_EQ(facts.allocator_boundaries.front().stage,
        query::DynOwnershipRuntimeStage::future_standard_library_boundary);
    EXPECT_EQ(facts.cleanup_dropck_boundaries.front().stage,
        query::DynOwnershipRuntimeStage::preparation);
    EXPECT_TRUE(facts.erased_drop_glue.front().borrowed_vtable_destructor_free);
    EXPECT_TRUE(facts.owned_containers.front().box_surface_blocked);
    EXPECT_TRUE(facts.allocator_boundaries.front().allocator_api_blocked);
}

TEST(QueryUnit, DynOwnershipRuntimeFactsRecordFunctionsUpdateSummary)
{
    const query::DynOwnershipRuntimeFacts source = baseline();
    query::DynOwnershipRuntimeFacts facts;
    facts.subject = "manual dyn ownership runtime preparation";

    query::record_dyn_owned_container_boundary_fact(facts, source.owned_containers.front());
    EXPECT_EQ(facts.summary.boundary_count, 1U);
    EXPECT_EQ(facts.summary.standard_library_blocked_count, 1U);
    EXPECT_EQ(facts.summary.runtime_lowering_blocked_count, 1U);
    EXPECT_TRUE(query::is_valid(facts));

    query::record_dyn_erased_drop_glue_boundary_fact(facts, source.erased_drop_glue.front());
    EXPECT_EQ(facts.summary.boundary_count, 2U);
    EXPECT_EQ(facts.summary.dynamic_drop_dispatch_blocked_count, 1U);
    EXPECT_EQ(facts.summary.borrowed_vtable_destructor_free_count, 1U);
    EXPECT_TRUE(query::is_valid(facts));

    query::record_dyn_allocator_boundary_fact(facts, source.allocator_boundaries.front());
    query::record_dyn_cleanup_dropck_boundary_fact(facts, source.cleanup_dropck_boundaries.front());
    facts.fingerprint = query::dyn_ownership_runtime_facts_fingerprint(facts);
    EXPECT_EQ(facts.summary.boundary_count, 4U);
    EXPECT_EQ(facts.summary.standard_library_blocked_count, 2U);
    EXPECT_EQ(facts.summary.runtime_lowering_blocked_count, 4U);
    EXPECT_EQ(facts.summary.cleanup_dropck_bridge_count, 1U);
    EXPECT_TRUE(query::is_valid(facts));
    EXPECT_NE(facts.fingerprint, source.fingerprint);
}

TEST(QueryUnit, DynOwnershipRuntimeFactsValidationRejectsBoundaryDrift)
{
    query::DynOwnershipRuntimeFacts facts = baseline();
    ASSERT_TRUE(query::is_valid_m17_dyn_ownership_runtime_preparation_baseline(facts));

    query::DynOwnershipRuntimeFacts empty_subject = facts;
    empty_subject.subject.clear();
    refresh(empty_subject);
    EXPECT_FALSE(query::is_valid(empty_subject));

    query::DynOwnershipRuntimeFacts missing_owned_name = facts;
    missing_owned_name.owned_containers.front().fact_name.clear();
    refresh(missing_owned_name);
    EXPECT_FALSE(query::is_valid(missing_owned_name));

    query::DynOwnershipRuntimeFacts missing_drop_slot = facts;
    missing_drop_slot.erased_drop_glue.front().dynamic_drop_slot_layout_fact.clear();
    refresh(missing_drop_slot);
    EXPECT_FALSE(query::is_valid(missing_drop_slot));

    query::DynOwnershipRuntimeFacts missing_deallocation = facts;
    missing_deallocation.allocator_boundaries.front().deallocation_policy_fact.clear();
    refresh(missing_deallocation);
    EXPECT_FALSE(query::is_valid(missing_deallocation));

    query::DynOwnershipRuntimeFacts missing_cleanup_bridge = facts;
    missing_cleanup_bridge.cleanup_dropck_boundaries.front().dropck_erased_receiver_fact.clear();
    refresh(missing_cleanup_bridge);
    EXPECT_FALSE(query::is_valid(missing_cleanup_bridge));

    query::DynOwnershipRuntimeFacts standard_library_claimed = facts;
    standard_library_claimed.owned_containers.front().standard_library_blocked = false;
    refresh(standard_library_claimed);
    EXPECT_FALSE(query::is_valid(standard_library_claimed));

    query::DynOwnershipRuntimeFacts allocator_api_claimed = facts;
    allocator_api_claimed.allocator_boundaries.front().allocator_api_blocked = false;
    refresh(allocator_api_claimed);
    EXPECT_FALSE(query::is_valid(allocator_api_claimed));

    query::DynOwnershipRuntimeFacts runtime_lowering_claimed = facts;
    runtime_lowering_claimed.cleanup_dropck_boundaries.front().runtime_lowering_blocked = false;
    refresh(runtime_lowering_claimed);
    EXPECT_FALSE(query::is_valid(runtime_lowering_claimed));

    query::DynOwnershipRuntimeFacts borrowed_vtable_with_destructor = facts;
    borrowed_vtable_with_destructor.erased_drop_glue.front().borrowed_vtable_destructor_free = false;
    refresh(borrowed_vtable_with_destructor);
    EXPECT_FALSE(query::is_valid(borrowed_vtable_with_destructor));

    query::DynOwnershipRuntimeFacts invalid_policy = facts;
    invalid_policy.allocator_boundaries.front().metadata_policy =
        query::DynOwnershipRuntimePolicy::owning_dyn_metadata_v1;
    refresh(invalid_policy);
    EXPECT_FALSE(query::is_valid(invalid_policy));

    query::DynOwnershipRuntimeFacts invalid_stage = facts;
    invalid_stage.cleanup_dropck_boundaries.front().stage =
        static_cast<query::DynOwnershipRuntimeStage>(QUERY_TEST_INVALID_ENUM_VALUE);
    refresh(invalid_stage);
    EXPECT_FALSE(query::is_valid(invalid_stage));

    query::DynOwnershipRuntimeFacts owned_wrong_stage = facts;
    owned_wrong_stage.owned_containers.front().stage =
        query::DynOwnershipRuntimeStage::future_runtime_lowering_boundary;
    refresh(owned_wrong_stage);
    EXPECT_FALSE(query::is_valid(owned_wrong_stage));

    query::DynOwnershipRuntimeFacts erased_drop_wrong_stage = facts;
    erased_drop_wrong_stage.erased_drop_glue.front().stage =
        query::DynOwnershipRuntimeStage::future_standard_library_boundary;
    refresh(erased_drop_wrong_stage);
    EXPECT_FALSE(query::is_valid(erased_drop_wrong_stage));

    query::DynOwnershipRuntimeFacts allocator_wrong_stage = facts;
    allocator_wrong_stage.allocator_boundaries.front().stage =
        query::DynOwnershipRuntimeStage::preparation;
    refresh(allocator_wrong_stage);
    EXPECT_FALSE(query::is_valid(allocator_wrong_stage));

    query::DynOwnershipRuntimeFacts cleanup_wrong_stage = facts;
    cleanup_wrong_stage.cleanup_dropck_boundaries.front().stage =
        query::DynOwnershipRuntimeStage::future_runtime_lowering_boundary;
    refresh(cleanup_wrong_stage);
    EXPECT_FALSE(query::is_valid(cleanup_wrong_stage));

    query::DynOwnershipRuntimeFacts stale_summary = facts;
    ++stale_summary.summary.boundary_count;
    stale_summary.fingerprint = query::dyn_ownership_runtime_facts_fingerprint(stale_summary);
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::DynOwnershipRuntimeFacts stale_fingerprint = facts;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale dyn ownership runtime facts");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));
}

TEST(QueryUnit, DynOwnershipRuntimeFactsSummaryDumpAndFingerprintAreStable)
{
    const query::DynOwnershipRuntimeFacts facts = baseline();
    ASSERT_TRUE(query::is_valid_m17_dyn_ownership_runtime_preparation_baseline(facts));
    EXPECT_EQ(facts.fingerprint, query::dyn_ownership_runtime_facts_fingerprint(facts));

    const std::string summary = query::summarize_dyn_ownership_runtime_facts(facts);
    EXPECT_NE(summary.find("dyn_ownership_runtime_facts subject=M17 Dyn Ownership Runtime Preparation"),
        std::string::npos) << summary;
    EXPECT_NE(summary.find("boundaries=4"), std::string::npos) << summary;
    EXPECT_NE(summary.find("owned_containers=1"), std::string::npos) << summary;
    EXPECT_NE(summary.find("standard_library_blocked=2"), std::string::npos) << summary;
    EXPECT_NE(summary.find("runtime_lowering_blocked=4"), std::string::npos) << summary;
    EXPECT_NE(summary.find("first_owned_container=owned_dyn_container_layout_fact"),
        std::string::npos) << summary;
    EXPECT_NE(summary.find("borrowed_vtable_destructor_free=yes"), std::string::npos) << summary;

    const std::string dump = query::dump_dyn_ownership_runtime_facts(facts);
    EXPECT_NE(dump.find("owned_dyn_container_boundary_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("erased_drop_glue_boundary_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("allocator_boundary_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("cleanup_dropck_boundary_fact"), std::string::npos) << dump;
    EXPECT_NE(dump.find("owning_dyn_container_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("owning_dyn_metadata_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("dynamic_drop_metadata_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("allocator_placement_policy_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("allocator_metadata_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("cleanup_dropck_boundary_v1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("box_surface_blocked=yes"), std::string::npos) << dump;
    EXPECT_NE(dump.find("borrowed_vtable_destructor_free=yes"), std::string::npos) << dump;
    EXPECT_NE(dump.find("dropck_erased_receiver_fact"), std::string::npos) << dump;

    query::DynOwnershipRuntimeFacts changed_tooling = facts;
    changed_tooling.owned_containers.front().tooling_boundary_fact =
        "owned_dyn_tooling_boundary_fact:v2";
    refresh(changed_tooling);
    EXPECT_TRUE(query::is_valid(changed_tooling));
    EXPECT_NE(changed_tooling.fingerprint, facts.fingerprint);

    query::DynOwnershipRuntimeFacts changed_subject = facts;
    changed_subject.subject = "M17 Dyn Ownership Runtime Preparation copy";
    refresh(changed_subject);
    EXPECT_TRUE(query::is_valid(changed_subject));
    EXPECT_FALSE(query::is_valid_m17_dyn_ownership_runtime_preparation_baseline(changed_subject));
    EXPECT_NE(changed_subject.fingerprint, facts.fingerprint);
}

} // namespace aurex::test

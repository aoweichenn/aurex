#include <aurex/infrastructure/query/dyn_ownership_runtime_boundary_gate.hpp>
#include <aurex/infrastructure/query/project_graph_query.hpp>
#include <aurex/infrastructure/query/query_context.hpp>
#include <aurex/infrastructure/query/query_executor.hpp>

#include <span>
#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;
constexpr std::string_view QUERY_TEST_M18_PROJECT = "m18-dyn-ownership-runtime-boundary-project";
constexpr std::string_view QUERY_TEST_M18_PROJECT_GRAPH = "m18-project-graph";

[[nodiscard]] query::ProjectKey test_project_key()
{
    return query::project_key(query::stable_fingerprint(QUERY_TEST_M18_PROJECT));
}

[[nodiscard]] query::QueryResultFingerprint test_project_graph_result()
{
    return query::query_result_fingerprint(query::stable_fingerprint(QUERY_TEST_M18_PROJECT_GRAPH));
}

[[nodiscard]] query::DynOwnershipRuntimeBoundaryGate baseline()
{
    return query::m18_dyn_ownership_runtime_boundary_gate_baseline();
}

void refresh_runtime(query::DynOwnershipRuntimeFacts& facts)
{
    facts.summary = query::summarize_dyn_ownership_runtime_counts(facts);
    facts.fingerprint = query::dyn_ownership_runtime_facts_fingerprint(facts);
}

void refresh(query::DynOwnershipRuntimeBoundaryGate& gate)
{
    gate.summary = query::summarize_dyn_ownership_runtime_boundary_gate_counts(gate);
    gate.fingerprint = query::dyn_ownership_runtime_boundary_gate_fingerprint(gate);
}

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

} // namespace

TEST(QueryUnit, DynOwnershipRuntimeBoundaryGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointKind::query_cache_projection),
        "query_cache_projection");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointKind::tooling_projection),
        "tooling_projection");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointKind::reuse_boundary),
        "reuse_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointKind::ir_verifier_planning),
        "ir_verifier_planning");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointKind::borrowed_abi_guard),
        "borrowed_abi_guard");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointKind::runtime_lowering_gate),
        "runtime_lowering_gate");

    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_stage_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointStage::hardened_query_boundary),
        "hardened_query_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_stage_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointStage::tooling_boundary),
        "tooling_boundary");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_stage_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointStage::design_gate),
        "design_gate");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_stage_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointStage::blocked_future_runtime),
        "blocked_future_runtime");

    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointPolicy::stable_fingerprint_projection_v1),
        "stable_fingerprint_projection_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointPolicy::semantic_fact_projection_v1),
        "semantic_fact_projection_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointPolicy::body_local_reuse_boundary_v1),
        "body_local_reuse_boundary_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointPolicy::ir_verifier_prerequisite_v1),
        "ir_verifier_prerequisite_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointPolicy::borrowed_vtable_destructor_free_v1),
        "borrowed_vtable_destructor_free_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  query::DynOwnershipRuntimeBoundaryCheckpointPolicy::runtime_lowering_not_implemented_v1),
        "runtime_lowering_not_implemented_v1");

    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_kind_name(
                  static_cast<query::DynOwnershipRuntimeBoundaryCheckpointKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_stage_name(
                  static_cast<query::DynOwnershipRuntimeBoundaryCheckpointStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::dyn_ownership_runtime_boundary_checkpoint_policy_name(
                  static_cast<query::DynOwnershipRuntimeBoundaryCheckpointPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeBoundaryCheckpointKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeBoundaryCheckpointStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeBoundaryCheckpointPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, DynOwnershipRuntimeBoundaryGateM18BaselineHardensRuntimeBoundaries)
{
    const query::DynOwnershipRuntimeBoundaryGate gate = baseline();
    ASSERT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(gate));
    EXPECT_EQ(gate.subject, "M18 Dyn Ownership Runtime Boundary Hardening");
    EXPECT_TRUE(query::is_valid_m17_dyn_ownership_runtime_preparation_baseline(gate.runtime_facts));
    EXPECT_EQ(gate.runtime_facts_fingerprint, gate.runtime_facts.fingerprint);
    EXPECT_EQ(gate.runtime_facts_fingerprint, query::dyn_ownership_runtime_facts_fingerprint(gate.runtime_facts));
    EXPECT_EQ(gate.fingerprint, query::dyn_ownership_runtime_boundary_gate_fingerprint(gate));
    EXPECT_TRUE(query::is_valid(query::dyn_ownership_runtime_boundary_gate_result_fingerprint(gate)));

    EXPECT_EQ(gate.summary.checkpoint_count, 6U);
    EXPECT_EQ(gate.summary.query_cache_checkpoint_count, 1U);
    EXPECT_EQ(gate.summary.tooling_checkpoint_count, 1U);
    EXPECT_EQ(gate.summary.reuse_checkpoint_count, 1U);
    EXPECT_EQ(gate.summary.ir_verifier_checkpoint_count, 1U);
    EXPECT_EQ(gate.summary.borrowed_abi_guard_count, 1U);
    EXPECT_EQ(gate.summary.runtime_lowering_gate_count, 1U);
    EXPECT_EQ(gate.summary.m17_reference_count, 6U);
    EXPECT_EQ(gate.summary.standard_library_blocked_count, 6U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocked_count, 6U);
    EXPECT_EQ(gate.summary.box_surface_blocked_count, 6U);
    EXPECT_EQ(gate.summary.owning_dyn_user_value_blocked_count, 6U);
    EXPECT_EQ(gate.summary.allocator_api_blocked_count, 6U);
    EXPECT_EQ(gate.summary.dynamic_drop_dispatch_blocked_count, 6U);
    EXPECT_EQ(gate.summary.borrowed_metadata_destructor_free_count, 6U);
    EXPECT_EQ(gate.summary.lowering_design_gate_count, 1U);
    EXPECT_EQ(gate.summary.lowering_runtime_implemented_count, 0U);

    ASSERT_EQ(gate.checkpoints.size(), 6U);
    ASSERT_EQ(gate.lowering_design_gates.size(), 1U);
    for (const query::DynOwnershipRuntimeBoundaryCheckpointFact& fact : gate.checkpoints) {
        EXPECT_TRUE(query::is_valid(fact));
        EXPECT_TRUE(fact.references_m17_facts);
        EXPECT_TRUE(fact.standard_library_blocked);
        EXPECT_TRUE(fact.runtime_lowering_blocked);
        EXPECT_TRUE(fact.box_surface_blocked);
        EXPECT_TRUE(fact.owning_dyn_user_value_blocked);
        EXPECT_TRUE(fact.allocator_api_blocked);
        EXPECT_TRUE(fact.dynamic_drop_dispatch_blocked);
        EXPECT_TRUE(fact.borrowed_metadata_destructor_free);
    }
    EXPECT_TRUE(gate.lowering_design_gates.front().ir_owned_object_placeholder_required);
    EXPECT_TRUE(gate.lowering_design_gates.front().ir_erased_drop_identity_required);
    EXPECT_TRUE(gate.lowering_design_gates.front().ir_allocator_identity_required);
    EXPECT_FALSE(gate.lowering_design_gates.front().lowering_runtime_implemented);
    EXPECT_FALSE(gate.lowering_design_gates.front().dynamic_drop_runtime_implemented);
    EXPECT_FALSE(gate.lowering_design_gates.front().standard_library_implemented);
}

TEST(QueryUnit, DynOwnershipRuntimeBoundaryGateRecordFunctionsUpdateSummary)
{
    const query::DynOwnershipRuntimeBoundaryGate source = baseline();
    query::DynOwnershipRuntimeBoundaryGate gate;
    gate.subject = "manual M18 dyn ownership runtime boundary";
    gate.runtime_facts = source.runtime_facts;
    gate.runtime_facts_fingerprint = source.runtime_facts_fingerprint;

    query::record_dyn_ownership_runtime_boundary_checkpoint(gate, source.checkpoints.front());
    EXPECT_EQ(gate.summary.checkpoint_count, 1U);
    EXPECT_EQ(gate.summary.query_cache_checkpoint_count, 1U);
    EXPECT_EQ(gate.summary.standard_library_blocked_count, 1U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocked_count, 1U);
    EXPECT_FALSE(query::is_valid(gate));

    query::record_dyn_ownership_runtime_lowering_design_gate(gate, source.lowering_design_gates.front());
    refresh(gate);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(gate));

    const query::StableFingerprint128 one_checkpoint = gate.fingerprint;
    query::record_dyn_ownership_runtime_boundary_checkpoint(gate, source.checkpoints[1]);
    refresh(gate);
    EXPECT_EQ(gate.summary.checkpoint_count, 2U);
    EXPECT_EQ(gate.summary.tooling_checkpoint_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_NE(gate.fingerprint, one_checkpoint);
}

TEST(QueryUnit, DynOwnershipRuntimeBoundaryGateValidationRejectsBoundaryDrift)
{
    const query::DynOwnershipRuntimeBoundaryGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(gate));

    const auto expect_invalid_checkpoint_drift = [&gate](auto&& mutate) {
        query::DynOwnershipRuntimeBoundaryGate drift = gate;
        mutate(drift.checkpoints.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.boundary_fact.clear();
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.kind = static_cast<query::DynOwnershipRuntimeBoundaryCheckpointKind>(QUERY_TEST_INVALID_ENUM_VALUE);
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.stage = query::DynOwnershipRuntimeBoundaryCheckpointStage::tooling_boundary;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.policy = query::DynOwnershipRuntimeBoundaryCheckpointPolicy::semantic_fact_projection_v1;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.query_cache_boundary_recorded = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.references_m17_facts = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.standard_library_blocked = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.runtime_lowering_blocked = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.box_surface_blocked = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.owning_dyn_user_value_blocked = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.allocator_api_blocked = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.dynamic_drop_dispatch_blocked = false;
    });
    expect_invalid_checkpoint_drift([](query::DynOwnershipRuntimeBoundaryCheckpointFact& fact) {
        fact.borrowed_metadata_destructor_free = false;
    });

    const auto expect_invalid_lowering_drift = [&gate](auto&& mutate) {
        query::DynOwnershipRuntimeBoundaryGate drift = gate;
        mutate(drift.lowering_design_gates.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_lowering_drift([](query::DynOwnershipRuntimeLoweringDesignGateFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid_lowering_drift([](query::DynOwnershipRuntimeLoweringDesignGateFact& fact) {
        fact.verifier_rejects_borrowed_vtable_destructor = false;
    });
    expect_invalid_lowering_drift([](query::DynOwnershipRuntimeLoweringDesignGateFact& fact) {
        fact.lowering_runtime_implemented = true;
    });
    expect_invalid_lowering_drift([](query::DynOwnershipRuntimeLoweringDesignGateFact& fact) {
        fact.dynamic_drop_runtime_implemented = true;
    });
    expect_invalid_lowering_drift([](query::DynOwnershipRuntimeLoweringDesignGateFact& fact) {
        fact.standard_library_implemented = true;
    });

    query::DynOwnershipRuntimeBoundaryGate empty_subject = gate;
    empty_subject.subject.clear();
    refresh(empty_subject);
    EXPECT_FALSE(query::is_valid(empty_subject));

    query::DynOwnershipRuntimeBoundaryGate stale_summary = gate;
    ++stale_summary.summary.checkpoint_count;
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::DynOwnershipRuntimeBoundaryGate stale_fingerprint = gate;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale M18 boundary gate");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));

    query::DynOwnershipRuntimeBoundaryGate duplicate_checkpoint = gate;
    duplicate_checkpoint.checkpoints[1] = duplicate_checkpoint.checkpoints.front();
    refresh(duplicate_checkpoint);
    EXPECT_TRUE(query::is_valid(duplicate_checkpoint));
    EXPECT_FALSE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(duplicate_checkpoint));

    query::DynOwnershipRuntimeBoundaryGate missing_checkpoint = gate;
    missing_checkpoint.checkpoints.pop_back();
    refresh(missing_checkpoint);
    EXPECT_TRUE(query::is_valid(missing_checkpoint));
    EXPECT_FALSE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(missing_checkpoint));

    query::DynOwnershipRuntimeBoundaryGate runtime_fact_drift = gate;
    runtime_fact_drift.runtime_facts.owned_containers.front().standard_library_blocked = false;
    refresh_runtime(runtime_fact_drift.runtime_facts);
    runtime_fact_drift.runtime_facts_fingerprint = runtime_fact_drift.runtime_facts.fingerprint;
    refresh(runtime_fact_drift);
    EXPECT_FALSE(query::is_valid(runtime_fact_drift));
}

TEST(QueryUnit, DynOwnershipRuntimeBoundaryGateSummaryDumpAndFingerprintAreStable)
{
    const query::DynOwnershipRuntimeBoundaryGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(gate));

    const std::string summary = query::summarize_dyn_ownership_runtime_boundary_gate(gate);
    EXPECT_TRUE(contains_text(summary, "dyn_ownership_runtime_boundary_gate")) << summary;
    EXPECT_TRUE(contains_text(summary, "M18 Dyn Ownership Runtime Boundary Hardening")) << summary;
    EXPECT_TRUE(contains_text(summary, "query_cache=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "tooling=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "reuse=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "ir_verifier=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "borrowed_abi_guard=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_lowering_gate=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "standard_library_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_lowering_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "box_surface_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "allocator_api_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "dynamic_drop_dispatch_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "lowering_runtime_implemented=0")) << summary;

    const std::string dump = query::dump_dyn_ownership_runtime_boundary_gate(gate);
    EXPECT_TRUE(contains_text(dump, "project_query_recorded_for_dyn_ownership_runtime_boundary_gate")) << dump;
    EXPECT_TRUE(contains_text(dump, "ide_semantic_fact_dyn_ownership_runtime_boundary_gate")) << dump;
    EXPECT_TRUE(contains_text(dump, "workspace_reuse_keeps_runtime_boundary_project_scoped")) << dump;
    EXPECT_TRUE(contains_text(dump, "future_ir_verifier_prerequisites_recorded")) << dump;
    EXPECT_TRUE(contains_text(dump, "borrowed_vtable_remains_destructor_free")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_lowering_design_gate_remains_blocked")) << dump;
    EXPECT_TRUE(contains_text(dump, "standard_library_blocked=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_lowering_blocked=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "box_surface_blocked=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_api_blocked=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "dynamic_drop_dispatch_blocked=yes")) << dump;
    EXPECT_TRUE(contains_text(dump, "lowering_runtime_implemented=no")) << dump;
    EXPECT_TRUE(contains_text(dump, "standard_library_implemented=no")) << dump;

    query::DynOwnershipRuntimeBoundaryGate changed = gate;
    changed.checkpoints.front().next_stage_fact = "m19_runtime_ir_verifier_preparation:v2";
    refresh(changed);
    EXPECT_TRUE(query::is_valid(changed));
    EXPECT_NE(changed.fingerprint, gate.fingerprint);
    EXPECT_NE(query::dyn_ownership_runtime_boundary_gate_result_fingerprint(changed),
        query::dyn_ownership_runtime_boundary_gate_result_fingerprint(gate));
}

TEST(QueryUnit, DynOwnershipRuntimeBoundaryGateProviderContextAndExecutorUseProjectBoundary)
{
    const query::ProjectKey project = test_project_key();
    const query::DynOwnershipRuntimeBoundaryGate gate = baseline();
    const query::QueryResultFingerprint graph_result = test_project_graph_result();
    const query::QueryResultFingerprint gate_result =
        query::dyn_ownership_runtime_boundary_gate_result_fingerprint(gate);
    const std::optional<query::QueryKey> project_graph_key = query::project_graph_query_key(project);
    const std::optional<query::QueryKey> gate_key = query::dyn_ownership_runtime_boundary_gate_query_key(project);
    ASSERT_TRUE(project_graph_key.has_value());
    ASSERT_TRUE(gate_key.has_value());

    const query::DynOwnershipRuntimeBoundaryGateProviderInput input{
        project,
        gate,
    };
    ASSERT_TRUE(query::is_valid(input));
    const std::optional<query::DynOwnershipRuntimeBoundaryGateProviderOutput> output =
        query::provide_dyn_ownership_runtime_boundary_gate_query(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_TRUE(query::is_valid(*output));
    EXPECT_EQ(output->record.key, *gate_key);
    EXPECT_EQ(output->record.key.kind, query::QueryKind::dyn_ownership_runtime_boundary_gate);
    EXPECT_EQ(output->record.stable_key_bytes, query::stable_serialize(project));
    EXPECT_EQ(output->record.result, gate_result);
    EXPECT_EQ(output->result, gate_result);
    EXPECT_EQ(output->dependencies, std::vector<query::QueryKey>{*project_graph_key});
    EXPECT_EQ(output->gate.fingerprint, gate.fingerprint);

    query::QueryContext context;
    const query::QueryEvaluationResult project_graph_eval =
        context.evaluate_project_graph(query::ProjectGraphProviderInput{project, graph_result});
    ASSERT_EQ(project_graph_eval.status, query::QueryEvaluationStatus::computed);
    const query::QueryEvaluationResult gate_eval = context.evaluate_dyn_ownership_runtime_boundary_gate(input);
    ASSERT_EQ(gate_eval.status, query::QueryEvaluationStatus::computed);
    ASSERT_NE(gate_eval.node, nullptr);
    EXPECT_EQ(gate_eval.node->key, *gate_key);
    EXPECT_EQ(gate_eval.node->dependencies, std::vector<query::QueryKey>{*project_graph_key});
    EXPECT_TRUE(context.has_dependency(*gate_key, *project_graph_key));

    query::QueryExecutor executor{context};
    const query::QueryRequest gate_request{input};
    EXPECT_EQ(query::query_request_key(gate_request), gate_key);
    const query::QueryBatchExecutionResult cached_batch = executor.evaluate_all(std::span<const query::QueryRequest>{
        &gate_request,
        1U,
    });
    EXPECT_EQ(cached_batch.summary.total, 1U);
    EXPECT_EQ(cached_batch.summary.cached, 1U);

    EXPECT_FALSE(query::dyn_ownership_runtime_boundary_gate_query_key(query::ProjectKey{}).has_value());
    EXPECT_FALSE(query::is_valid(query::DynOwnershipRuntimeBoundaryGateProviderInput{}));
    EXPECT_FALSE(query::provide_dyn_ownership_runtime_boundary_gate_query(
                     query::DynOwnershipRuntimeBoundaryGateProviderInput{query::ProjectKey{}, gate})
                     .has_value());

    query::DynOwnershipRuntimeBoundaryGate invalid_gate = gate;
    invalid_gate.checkpoints.clear();
    refresh(invalid_gate);
    EXPECT_FALSE(query::provide_dyn_ownership_runtime_boundary_gate_query(
                     query::DynOwnershipRuntimeBoundaryGateProviderInput{project, invalid_gate})
                     .has_value());

    query::DynOwnershipRuntimeBoundaryGateProviderOutput no_dependency_output = *output;
    no_dependency_output.dependencies.clear();
    EXPECT_FALSE(query::is_valid(no_dependency_output));

    query::DynOwnershipRuntimeBoundaryGateProviderOutput duplicate_dependency_output = *output;
    duplicate_dependency_output.dependencies.push_back(*project_graph_key);
    EXPECT_FALSE(query::is_valid(duplicate_dependency_output));

    const query::ProjectKey other_project = query::project_key(query::stable_fingerprint("other M18 project"));
    const std::optional<query::QueryKey> other_project_graph_key = query::project_graph_query_key(other_project);
    ASSERT_TRUE(other_project_graph_key.has_value());
    query::DynOwnershipRuntimeBoundaryGateProviderOutput wrong_project_dependency_output = *output;
    wrong_project_dependency_output.dependencies = {*other_project_graph_key};
    EXPECT_FALSE(query::is_valid(wrong_project_dependency_output));

    query::DynOwnershipRuntimeBoundaryGateProviderOutput wrong_kind_dependency_output = *output;
    wrong_kind_dependency_output.dependencies = {
        query::query_key(query::QueryKind::file_content, gate_key->payload),
    };
    EXPECT_FALSE(query::is_valid(wrong_kind_dependency_output));

    query::DynOwnershipRuntimeBoundaryGateProviderOutput mismatched_result_output = *output;
    mismatched_result_output.result = query::query_result_fingerprint(query::stable_fingerprint("other gate result"));
    EXPECT_FALSE(query::is_valid(mismatched_result_output));

    query::QueryContext invalid_output_context;
    invalid_output_context.set_dyn_ownership_runtime_boundary_gate_provider(
        [wrong_project_dependency_output](
            const query::DynOwnershipRuntimeBoundaryGateProviderInput&) -> std::optional<
                query::DynOwnershipRuntimeBoundaryGateProviderOutput> {
            return wrong_project_dependency_output;
        });
    const query::QueryEvaluationResult invalid_output_eval =
        invalid_output_context.evaluate_dyn_ownership_runtime_boundary_gate(input);
    EXPECT_EQ(invalid_output_eval.status, query::QueryEvaluationStatus::failed);
    EXPECT_TRUE(invalid_output_context.completed_records().empty());
}

} // namespace aurex::test

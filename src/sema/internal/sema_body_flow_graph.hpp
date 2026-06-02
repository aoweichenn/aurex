#pragma once

#include <aurex/sema/checked_module.hpp>

#include <string>
#include <string_view>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BodyFlowAnalyzer final {
public:
    explicit BodyFlowAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void collect(const syntax::ItemNode& function, const FunctionLookupKey& key);

private:
    SemanticAnalyzerCore& core_;
};

[[nodiscard]] std::string_view body_flow_point_kind_name(BodyFlowPointKind kind) noexcept;
[[nodiscard]] std::string_view body_flow_action_kind_name(BodyFlowActionKind kind) noexcept;
[[nodiscard]] std::string_view body_flow_place_root_kind_name(BodyFlowPlaceRootKind kind) noexcept;
[[nodiscard]] std::string_view body_flow_place_projection_kind_name(BodyFlowPlaceProjectionKind kind) noexcept;
[[nodiscard]] std::string dump_body_flow_graph(const BodyFlowGraph& graph);

} // namespace aurex::sema

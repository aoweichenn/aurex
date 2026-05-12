#include <aurex/parse/recovery.hpp>

#include <parse/parser_recovery_sets.hpp>

namespace aurex::parse {

using syntax::TokenKind;

bool token_matches_recovery_context(
    const TokenKind kind,
    const RecoveryContext context
) noexcept {
    switch (context) {
    case RecoveryContext::identifier:
        return detail::token_matches_identifier_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::item:
        return detail::token_starts_item(kind);
    case RecoveryContext::statement:
        return detail::token_starts_statement(kind);
    case RecoveryContext::match_arm:
        return detail::token_ends_match_arm(kind) ||
               token_starts_match_arm(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::call_argument:
        return detail::token_ends_call_argument(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::struct_field:
        return detail::token_ends_struct_field(kind) ||
               token_starts_struct_field(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::parameter:
        return detail::token_ends_parameter(kind) ||
               token_starts_parameter(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::struct_decl_field:
        return detail::token_ends_struct_decl_field(kind) ||
               token_starts_struct_decl_field(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::enum_case:
        return detail::token_ends_enum_case(kind) ||
               token_starts_enum_case(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::parameter_list_start:
        return detail::token_matches_parameter_list_start_boundary(kind) ||
               token_starts_parameter(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::abi_attribute_argument:
        return detail::token_matches_abi_attribute_argument(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::abi_attribute_start:
        return detail::token_matches_abi_attribute_start_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::builtin_argument:
        return detail::token_ends_builtin_argument(kind) ||
               detail::token_starts_expression(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::builtin_argument_list_start:
        return detail::token_matches_builtin_argument_list_start_boundary(kind) ||
               detail::token_starts_expression(kind) ||
               detail::token_starts_type(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::grouped_expression:
        return detail::token_matches_grouped_expression_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::index_expression:
        return detail::token_matches_index_expression_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::array_literal:
        return detail::token_matches_array_literal_boundary(kind) ||
               detail::token_starts_expression(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::array_type_length:
        return detail::token_matches_array_type_length_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::generic_type_argument:
        return detail::token_ends_generic_type_argument(kind) ||
               detail::token_starts_type(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::generic_parameter:
        return detail::token_ends_generic_parameter(kind) ||
               kind == TokenKind::identifier ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::pattern_payload:
        return detail::token_matches_pattern_payload_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::enum_case_payload:
        return detail::token_matches_enum_case_payload_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::path_segment:
        return token_starts_path_segment(kind) ||
               detail::token_matches_path_segment_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::import_alias:
        return detail::token_matches_import_alias_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::module_terminator:
        return detail::token_matches_module_terminator_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::item_terminator:
        return detail::token_matches_item_terminator_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::type_annotation:
        return detail::token_matches_type_annotation_boundary(kind) ||
               detail::token_starts_type(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::initializer:
        return detail::token_matches_initializer_boundary(kind) ||
               detail::token_starts_expression(kind) ||
               detail::token_starts_type(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::match_arm_arrow:
        return detail::token_matches_match_arm_arrow_boundary(kind) ||
               detail::token_starts_expression(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::if_else:
        return detail::token_matches_if_else_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::statement_terminator:
        return detail::token_matches_statement_terminator_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::for_clause_separator:
        return detail::token_matches_for_clause_separator_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_non_expression_statement(kind);
    case RecoveryContext::block_start:
        return detail::token_matches_block_start_boundary(kind) ||
               detail::token_starts_item(kind) ||
               detail::token_starts_statement(kind);
    case RecoveryContext::block_end:
        return detail::token_starts_item(kind);
    }
    return false;
}

} // namespace aurex::parse

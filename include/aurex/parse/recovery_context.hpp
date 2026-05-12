#pragma once

namespace aurex::parse {

enum class RecoveryContext {
    // Identifier boundary after a malformed declaration, member, or pattern name.
    identifier,
    // Declaration or declaration-like member boundary.
    item,
    // Block statement boundary, including expression statements.
    statement,
    // Match arm boundary after a malformed arm body or separator.
    match_arm,
    // Call argument list boundary after a malformed argument or separator.
    call_argument,
    // Struct literal field list boundary after a malformed field or separator.
    struct_field,
    // Function parameter list boundary after a malformed parameter or separator.
    parameter,
    // Struct declaration field list boundary after a malformed field or separator.
    struct_decl_field,
    // Enum case list boundary after a malformed case or separator.
    enum_case,
    // Function parameter-list opener boundary after a malformed function header.
    parameter_list_start,
    // ABI attribute argument boundary after a malformed attribute argument.
    abi_attribute_argument,
    // ABI attribute argument-list opener boundary after a malformed attribute name.
    abi_attribute_start,
    // Builtin expression argument boundary after a malformed separator.
    builtin_argument,
    // Builtin expression argument-list opener boundary after a malformed builtin name.
    builtin_argument_list_start,
    // Grouped expression boundary after a malformed parenthesized expression.
    grouped_expression,
    // Index expression boundary after a malformed index.
    index_expression,
    // Array literal boundary after a malformed element or repeat count.
    array_literal,
    // Array type length boundary after a malformed length expression.
    array_type_length,
    // Generic type argument list boundary after a malformed type argument.
    generic_type_argument,
    // Generic type parameter list boundary after a malformed parameter name.
    generic_parameter,
    // Match/enum pattern payload boundary after a malformed payload binding.
    pattern_payload,
    // Enum declaration payload type boundary after a malformed payload type.
    enum_case_payload,
    // Module/import path segment boundary after a malformed segment.
    path_segment,
    // Import alias boundary after a malformed `as` alias.
    import_alias,
    // Module declaration terminator boundary after a malformed `module` path.
    module_terminator,
    // Item terminator boundary after malformed declarations.
    item_terminator,
    // Required ':' boundary before type annotations and enum base types.
    type_annotation,
    // Required '=' boundary before declaration initializers or enum values.
    initializer,
    // Required `=>` boundary between a match pattern and arm value.
    match_arm_arrow,
    // Required `else` boundary in if expressions.
    if_else,
    // Statement terminator boundary after malformed control-statement tails.
    statement_terminator,
    // For-loop clause separator boundary after a malformed condition clause.
    for_clause_separator,
    // Block opener boundary after malformed control-flow or expression block headers.
    block_start,
    // Block closer boundary after malformed or missing block tails.
    block_end,
};

} // namespace aurex::parse

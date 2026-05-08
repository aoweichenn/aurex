#pragma once

namespace aurex::parse {

enum class RecoveryContext {
    // Declaration or declaration-like member boundary.
    item,
    // Block statement boundary, including expression statements.
    statement,
    // Type argument list boundary after a malformed argument or separator.
    type_argument,
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
    // Generic parameter list boundary after a malformed parameter or separator.
    generic_parameter,
    // ABI attribute argument boundary after a malformed attribute argument.
    abi_attribute_argument,
    // Builtin expression argument boundary after a malformed separator.
    builtin_argument,
    // Module/import path segment boundary after a malformed segment.
    path_segment,
    // Import alias boundary after a malformed `as` alias.
    import_alias,
    // Statement terminator boundary after malformed control-statement tails.
    statement_terminator,
    // For-loop clause separator boundary after a malformed condition clause.
    for_clause_separator,
    // Block opener boundary after malformed control-flow or expression block headers.
    block_start,
    // Block closer boundary after malformed or missing block tails.
    block_end,
    // Transitional default for bridge calls that have not chosen a narrower boundary.
    item_or_statement,
};

} // namespace aurex::parse

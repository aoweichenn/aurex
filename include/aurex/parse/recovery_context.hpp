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
    // Transitional default for bridge calls that have not chosen a narrower boundary.
    item_or_statement,
};

} // namespace aurex::parse

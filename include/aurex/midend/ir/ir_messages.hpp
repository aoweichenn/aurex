#pragma once

#include <string>
#include <string_view>

namespace aurex::ir {

inline constexpr std::string_view IR_VERIFY_FAILED_HEADER = "IR verification failed:";

inline constexpr std::string_view IR_VERIFY_CONSTANT_ID_INVALID = "constant id is invalid";

inline constexpr std::string_view IR_VERIFY_CYCLIC_CONSTANT_DEFINITION = "cyclic constant definition";

inline constexpr std::string_view IR_VERIFY_CONSTANT_INITIALIZER_VALUE_ID_INVALID =
    "constant initializer value id is invalid";

inline constexpr std::string_view IR_VERIFY_CONSTANT_INITIALIZER_TYPE_MISMATCH = "constant initializer type mismatch";

inline constexpr std::string_view IR_VERIFY_CYCLIC_CONSTANT_REFERENCE = "cyclic constant reference";

inline constexpr std::string_view IR_VERIFY_CONSTANT_INITIALIZER_RUNTIME_UNARY =
    "constant initializer contains a runtime-only unary operator";

inline constexpr std::string_view IR_VERIFY_CONSTANT_INITIALIZER_NOT_CONSTANT =
    "constant initializer is not compile-time constant";

inline constexpr std::string_view IR_VERIFY_AGGREGATE_RESULT_RECORD = "aggregate result is not a record";

inline constexpr std::string_view IR_VERIFY_AGGREGATE_CONSTANT_MISSING_FIELD =
    "aggregate constant does not initialize every field";

inline constexpr std::string_view IR_VERIFY_RECORD_AGGREGATE_CONSTANT_ARRAY_ELEMENTS =
    "record aggregate constant cannot contain array elements";

inline constexpr std::string_view IR_VERIFY_ARRAY_AGGREGATE_CONSTANT_NAMED_FIELDS =
    "array aggregate constant cannot contain named fields";

inline constexpr std::string_view IR_VERIFY_ARRAY_AGGREGATE_CONSTANT_ELEMENT_COUNT =
    "array aggregate constant element count mismatch";

inline constexpr std::string_view IR_VERIFY_FUNCTION_EMPTY_ABI_SYMBOL = "function has an empty ABI symbol";

inline constexpr std::string_view IR_VERIFY_INVALID_VALUE_IN_BLOCK = "invalid value in block";

inline constexpr std::string_view IR_VERIFY_STORE_RESULT_VOID = "store result must be void";

inline constexpr std::string_view IR_VERIFY_STORE_TARGET_MUTABLE = "store target must be mutable";

inline constexpr std::string_view IR_VERIFY_DROP_RESULT_VOID = "drop result must be void";

inline constexpr std::string_view IR_VERIFY_DROP_TARGET_TYPE = "drop target type mismatch";

inline constexpr std::string_view IR_VERIFY_DROP_TARGET_MUTABLE = "drop target must be mutable";

inline constexpr std::string_view IR_VERIFY_DROP_IF_CONDITION_BOOL = "conditional drop flag must be bool";

inline constexpr std::string_view IR_VERIFY_DROP_CLEANUP_POLICY_REQUIRED = "drop cleanup ABI policy is required";

inline constexpr std::string_view IR_VERIFY_DROP_CLEANUP_POLICY_TARGET =
    "drop cleanup ABI policy does not match target type";

inline constexpr std::string_view IR_VERIFY_DROP_CLEANUP_POLICY_NON_DROP =
    "cleanup ABI policy is only valid on drop markers";

inline constexpr std::string_view IR_VERIFY_DROP_DYNAMIC_ERASED_RUNTIME_BLOCKED =
    "dynamic erased drop runtime is blocked at this IR stage";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_VTABLE_DESTRUCTOR_FREE =
    "borrowed dyn trait vtable must remain destructor-free";

inline constexpr std::string_view IR_VERIFY_CAST_RESULT_TARGET = "cast result type must match cast target type";

inline constexpr std::string_view IR_VERIFY_UNARY_OPERAND_INVALID = "unary operand type is invalid";

inline constexpr std::string_view IR_VERIFY_LOGICAL_UNARY_BOOL =
    "logical unary operator requires bool operand and result";

inline constexpr std::string_view IR_VERIFY_NUMERIC_UNARY_OPERAND =
    "numeric unary operator requires matching numeric operand and result";

inline constexpr std::string_view IR_VERIFY_BITWISE_UNARY_OPERAND =
    "bitwise unary operator requires matching integer operand and result";

inline constexpr std::string_view IR_VERIFY_UNARY_PASSTHROUGH_TYPE =
    "address/dereference unary passthrough type mismatch";

inline constexpr std::string_view IR_VERIFY_BINARY_OPERAND_TYPE_MISMATCH = "binary operand type mismatch";

inline constexpr std::string_view IR_VERIFY_BINARY_OPERAND_TYPE_INVALID = "binary operand type is invalid";

inline constexpr std::string_view IR_VERIFY_COMPARISON_RESULT_BOOL = "comparison binary result must be bool";

inline constexpr std::string_view IR_VERIFY_COMPARISON_OPERANDS_NUMERIC = "comparison binary operands must be numeric";

inline constexpr std::string_view IR_VERIFY_EQUALITY_RESULT_BOOL = "equality binary result must be bool";

inline constexpr std::string_view IR_VERIFY_EQUALITY_OPERANDS_SCALAR = "equality binary operands must be scalar";

inline constexpr std::string_view IR_VERIFY_LOGICAL_BINARY_BOOL =
    "logical binary operator requires bool operands and result";

inline constexpr std::string_view IR_VERIFY_INTEGER_BINARY_RESULT = "integer binary result must match operand type";

inline constexpr std::string_view IR_VERIFY_INTEGER_BINARY_OPERANDS =
    "integer binary operator requires integer operands";

inline constexpr std::string_view IR_VERIFY_NUMERIC_BINARY_RESULT = "numeric binary result must match operand type";

inline constexpr std::string_view IR_VERIFY_NUMERIC_BINARY_OPERANDS =
    "numeric binary operator requires numeric operands";

inline constexpr std::string_view IR_VERIFY_PHI_INCOMING_REQUIRED = "phi has no incoming values";

inline constexpr std::string_view IR_VERIFY_PHI_DUPLICATE_PREDECESSOR = "phi has duplicate incoming predecessor";

inline constexpr std::string_view IR_VERIFY_PHI_PREDECESSOR_EDGE = "phi predecessor has no edge to block";

inline constexpr std::string_view IR_VERIFY_PHI_MISSING_PREDECESSOR = "phi is missing incoming predecessor";

inline constexpr std::string_view IR_VERIFY_CALL_NO_TARGET = "call has no target symbol";

inline constexpr std::string_view IR_VERIFY_CALL_TARGET_OUT_OF_RANGE = "call target out of range";

inline constexpr std::string_view IR_VERIFY_CALL_ARGUMENT_OUT_OF_RANGE = "call argument out of range";

inline constexpr std::string_view IR_VERIFY_BOOL_LITERAL_TYPE = "bool literal type must be bool";

inline constexpr std::string_view IR_VERIFY_CHAR_LITERAL_TYPE = "char literal type must be char";

inline constexpr std::string_view IR_VERIFY_NULL_LITERAL_TYPE = "null literal type must be pointer";

inline constexpr std::string_view IR_VERIFY_STRING_LITERAL_TYPE = "string literal type must be str";

inline constexpr std::string_view IR_VERIFY_C_STRING_LITERAL_TYPE = "c string literal type must be *const u8";

inline constexpr std::string_view IR_VERIFY_BYTE_LITERAL_TYPE = "byte literal type must be u8";

inline constexpr std::string_view IR_VERIFY_UNDEF_VOID = "undef value cannot have void type";

inline constexpr std::string_view IR_VERIFY_ALLOCA_POINTER = "alloca result must be a pointer";

inline constexpr std::string_view IR_VERIFY_ALLOCA_MUT_POINTER = "alloca result must be a mutable pointer";

inline constexpr std::string_view IR_VERIFY_LOAD_RESULT_NONVOID = "load result must not be void";

inline constexpr std::string_view IR_VERIFY_LOAD_RESULT_TYPE = "load result type mismatch";

inline constexpr std::string_view IR_VERIFY_STR_DATA_RESULT = "str_data result must be *const u8";

inline constexpr std::string_view IR_VERIFY_STR_BYTE_LEN_RESULT = "str_byte_len result must be usize";

inline constexpr std::string_view IR_VERIFY_STRVALID_RESULT = "strvalid result must be bool";

inline constexpr std::string_view IR_VERIFY_STR_UTF8_SLICE =
    "str UTF-8 builtin operand must be a []u8 or []mut u8 byte slice";

inline constexpr std::string_view IR_VERIFY_STRFROMUTF8_RESULT = "strfromutf8 result must be str";

inline constexpr std::string_view IR_VERIFY_STR_SLICE_RESULT = "str slice result must be str";

inline constexpr std::string_view IR_VERIFY_STR_SLICE_OBJECT = "str slice object type mismatch";

inline constexpr std::string_view IR_VERIFY_STRRAW_RESULT = "strraw result must be str";

inline constexpr std::string_view IR_VERIFY_STRRAW_ARITY = "strraw requires data and length arguments";

inline constexpr std::string_view IR_VERIFY_STRRAW_DATA = "strraw data must be *const u8";

inline constexpr std::string_view IR_VERIFY_SLICE_RESULT = "slice value result must be a slice";

inline constexpr std::string_view IR_VERIFY_SLICE_DATA_POINTER = "slice data must be pointer to slice element";

inline constexpr std::string_view IR_VERIFY_SLICE_LEN = "slice length must be usize";

inline constexpr std::string_view IR_VERIFY_SLICE_DATA_RESULT = "slice data result must be pointer to slice element";

inline constexpr std::string_view IR_VERIFY_SLICE_LEN_RESULT = "slice length result must be usize";

inline constexpr std::string_view IR_VERIFY_CONSTANT_REF_ID = "constant reference id is invalid";

inline constexpr std::string_view IR_VERIFY_CONSTANT_REF_TYPE = "constant reference type mismatch";

inline constexpr std::string_view IR_VERIFY_FUNCTION_REF_TYPE = "function reference result must be a function type";

inline constexpr std::string_view IR_VERIFY_FUNCTION_REF_SIGNATURE =
    "function reference result type does not match target signature";

inline constexpr std::string_view IR_VERIFY_INDIRECT_CALL_CALLEE_FUNCTION =
    "indirect call callee must be a function value";

inline constexpr std::string_view IR_VERIFY_INDIRECT_CALL_ARGUMENT_COUNT = "indirect call has wrong argument count";

inline constexpr std::string_view IR_VERIFY_INDIRECT_CALL_RESULT_TYPE = "indirect call result type mismatch";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_PACK_RESULT =
    "dyn pack result must be a reference to dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_PACK_DATA =
    "dyn pack data must be a pointer or reference to the concrete type";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_UPCAST_OBJECT =
    "dyn upcast object must be a reference to source dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_UPCAST_RESULT =
    "dyn upcast result must be a reference to target dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_UPCAST_EDGE =
    "dyn upcast supertrait edge is invalid or missing";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_UPCAST_LAYOUT =
    "dyn upcast source and target layouts do not match the supertrait edge";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_LAYOUT =
    "dyn trait vtable layout is invalid or missing";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_SUPERTRAIT_EDGE =
    "dyn trait vtable supertrait edge is invalid";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_LAYOUT_TYPE =
    "dyn trait vtable layout does not match concrete/object types";

inline constexpr std::string_view IR_VERIFY_PRINCIPAL_SET_METADATA_LAYOUT =
    "dyn trait principal-set metadata layout is invalid or missing";

inline constexpr std::string_view IR_VERIFY_PRINCIPAL_SET_METADATA_WITNESS =
    "dyn trait principal-set metadata witness is invalid";

inline constexpr std::string_view IR_VERIFY_PRINCIPAL_SET_METADATA_LAYOUT_TYPE =
    "dyn trait principal-set metadata layout does not match concrete/composition types";

inline constexpr std::string_view IR_VERIFY_OWNED_DYN_OBJECT_LAYOUT_PROTOTYPE =
    "owned dyn object layout prototype is invalid";

inline constexpr std::string_view IR_VERIFY_OWNED_DYN_OBJECT_LAYOUT_PROTOTYPE_TYPE =
    "owned dyn object layout prototype does not match its dyn trait object type";

inline constexpr std::string_view IR_VERIFY_OWNED_DYN_OBJECT_LAYOUT_PROTOTYPE_POINTER =
    "owned dyn object layout prototype pointer field is invalid";

inline constexpr std::string_view IR_VERIFY_OWNED_DYN_OBJECT_LAYOUT_PROTOTYPE_SHAPE =
    "owned dyn object layout prototype must remain a two-field data/vtable handle";

inline constexpr std::string_view IR_VERIFY_OWNED_DYN_OBJECT_LAYOUT_PROTOTYPE_IDENTITY =
    "owned dyn object layout prototype drop/allocator identity is invalid";

inline constexpr std::string_view IR_VERIFY_OWNED_DYN_OBJECT_LAYOUT_PROTOTYPE_BLOCKED =
    "owned dyn object layout prototype must keep stdlib/runtime surfaces blocked";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_COMPOSITION_PACK_RESULT =
    "dyn composition pack result must be a reference to a principal-set dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_COMPOSITION_PACK_DATA =
    "dyn composition pack data must be a pointer or reference to the concrete type";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_COMPOSITION_PROJECT_OBJECT =
    "dyn composition project object must be a reference to a principal-set dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_COMPOSITION_PROJECT_RESULT =
    "dyn composition project result must be a reference to a selected principal dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_COMPOSITION_PROJECT_PRINCIPAL =
    "dyn composition project principal is not present in the metadata layout";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_DATA_OBJECT =
    "dyn data object must be a reference to dyn trait";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_DATA_RESULT =
    "dyn data result must be a pointer or reference to the concrete type";

inline constexpr std::string_view IR_VERIFY_TRAIT_OBJECT_VTABLE_RESULT =
    "dyn vtable result must be a pointer";

inline constexpr std::string_view IR_VERIFY_VTABLE_SLOT_OBJECT =
    "vtable slot object must be a pointer";

inline constexpr std::string_view IR_VERIFY_VTABLE_SLOT_RANGE = "vtable slot index is out of range";

inline constexpr std::string_view IR_VERIFY_VTABLE_SLOT_RESULT =
    "vtable slot result must be the slot function type";

inline constexpr std::string_view IR_VERIFY_FIELD_ADDRESS_POINTER = "field address result is not a pointer";

inline constexpr std::string_view IR_VERIFY_FIELD_ADDRESS_TYPE = "field address result type mismatch";

inline constexpr std::string_view IR_VERIFY_FIELD_ADDRESS_CONST_MUTABLE =
    "field address cannot be mutable through const object";

inline constexpr std::string_view IR_VERIFY_INDEX_ADDRESS_POINTER = "index address result is not a pointer";

inline constexpr std::string_view IR_VERIFY_INDEX_INTEGER = "index must be an integer";

inline constexpr std::string_view IR_VERIFY_INDEX_ADDRESS_TYPE = "index address result type mismatch";

inline constexpr std::string_view IR_VERIFY_INDEX_ADDRESS_CONST_MUTABLE =
    "index address cannot be mutable through const object";

inline constexpr std::string_view IR_VERIFY_AGGREGATE_MISSING_FIELD = "aggregate does not initialize every field";

inline constexpr std::string_view IR_VERIFY_RECORD_AGGREGATE_ARRAY_ELEMENTS =
    "record aggregate cannot contain array elements";

inline constexpr std::string_view IR_VERIFY_ARRAY_AGGREGATE_NAMED_FIELDS =
    "array aggregate cannot contain named fields";

inline constexpr std::string_view IR_VERIFY_ARRAY_AGGREGATE_ELEMENT_COUNT = "array aggregate element count mismatch";

[[nodiscard]] inline std::string ir_verify_duplicate_aggregate_field_message(const std::string_view field)
{
    return "duplicate aggregate field " + std::string(field);
}

[[nodiscard]] inline std::string ir_verify_unknown_aggregate_field_message(const std::string_view field)
{
    return "unknown aggregate field " + std::string(field);
}

[[nodiscard]] inline std::string ir_verify_extern_function_c_abi_message(const std::string_view symbol)
{
    return "extern function @" + std::string(symbol) + " must use C ABI";
}

[[nodiscard]] inline std::string ir_verify_exported_function_c_abi_message(const std::string_view symbol)
{
    return "exported function @" + std::string(symbol) + " must use C ABI";
}

[[nodiscard]] inline std::string ir_verify_entry_function_internal_linkage_message(const std::string_view symbol)
{
    return "entry function @" + std::string(symbol) + " must use internal linkage";
}

[[nodiscard]] inline std::string ir_verify_entry_function_aurex_abi_message(const std::string_view symbol)
{
    return "entry function @" + std::string(symbol) + " must use Aurex ABI";
}

[[nodiscard]] inline std::string ir_verify_entry_function_parameters_message(const std::string_view symbol)
{
    return "entry function @" + std::string(symbol) + " must use no parameters or argc/argv parameters";
}

[[nodiscard]] inline std::string ir_verify_entry_function_return_message(const std::string_view symbol)
{
    return "entry function @" + std::string(symbol) + " must return i32 or void";
}

[[nodiscard]] inline std::string ir_verify_function_no_blocks_message(const std::string_view symbol)
{
    return "function @" + std::string(symbol) + " has no blocks";
}

[[nodiscard]] inline std::string ir_verify_extern_function_blocks_message(const std::string_view symbol)
{
    return "extern function @" + std::string(symbol) + " must not have blocks";
}

[[nodiscard]] inline std::string ir_verify_function_parameter_count_message(const std::string_view symbol)
{
    return "function @" + std::string(symbol) + " parameter signature/value count mismatch";
}

[[nodiscard]] inline std::string ir_verify_function_non_param_message(const std::string_view symbol)
{
    return "function @" + std::string(symbol) + " has a non-param value in parameter list";
}

[[nodiscard]] inline std::string ir_verify_function_parameter_type_message(const std::string_view symbol)
{
    return "function @" + std::string(symbol) + " parameter type mismatch";
}

[[nodiscard]] inline std::string ir_verify_duplicate_non_extern_symbol_message(const std::string_view symbol)
{
    return "duplicate non-extern function ABI symbol @" + std::string(symbol);
}

[[nodiscard]] inline std::string ir_verify_extern_function_inconsistent_message(const std::string_view symbol)
{
    return "extern function @" + std::string(symbol) + " has inconsistent declarations";
}

[[nodiscard]] inline std::string ir_verify_block_terminator_message(const std::string_view block_name)
{
    return "block ^" + std::string(block_name) + " has no terminator";
}

[[nodiscard]] inline std::string ir_verify_void_function_returns_value_message(const std::string_view symbol)
{
    return "void function @" + std::string(symbol) + " returns a value";
}

[[nodiscard]] inline std::string ir_verify_unresolved_call_target_message(const std::string_view symbol)
{
    return "call target @" + std::string(symbol) + " is unresolved";
}

[[nodiscard]] inline std::string ir_verify_call_argument_count_message(const std::string_view symbol)
{
    return "call to @" + std::string(symbol) + " has wrong argument count";
}

[[nodiscard]] inline std::string ir_verify_call_result_type_message(const std::string_view symbol)
{
    return "call to @" + std::string(symbol) + " result type mismatch";
}

[[nodiscard]] inline std::string ir_verify_literal_integer_type_message(const std::string_view type_name)
{
    return "integer literal type must be integer, got " + std::string(type_name);
}

[[nodiscard]] inline std::string ir_verify_literal_float_type_message(const std::string_view type_name)
{
    return "float literal type must be float, got " + std::string(type_name);
}

[[nodiscard]] inline std::string ir_verify_size_or_align_result_message(const std::string_view op)
{
    return std::string(op) + " result must be usize";
}

[[nodiscard]] inline std::string ir_verify_unknown_field_message(const std::string_view field)
{
    return "unknown field '" + std::string(field) + "'";
}

[[nodiscard]] inline std::string ir_verify_invalid_type_message(const std::string_view context)
{
    return std::string(context) + " type is invalid";
}

[[nodiscard]] inline std::string ir_verify_storage_type_message(const std::string_view context)
{
    return std::string(context) + " type is not valid storage";
}

[[nodiscard]] inline std::string ir_verify_block_id_message(const std::string_view context)
{
    return std::string(context) + " block id is invalid";
}

[[nodiscard]] inline std::string ir_verify_value_id_message(const std::string_view context)
{
    return std::string(context) + " value id is invalid";
}

[[nodiscard]] inline std::string ir_verify_type_invalid_message(const std::string_view context)
{
    return std::string(context) + " is invalid";
}

[[nodiscard]] inline std::string ir_verify_type_mismatch_message(const std::string_view context)
{
    return std::string(context) + " type mismatch";
}

[[nodiscard]] inline std::string ir_verify_not_pointer_message(const std::string_view context)
{
    return std::string(context) + " is not a pointer";
}

} // namespace aurex::ir

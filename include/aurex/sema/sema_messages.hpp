#pragma once

#include <aurex/base/integer.hpp>

#include <string>
#include <string_view>

namespace aurex::sema {

inline constexpr std::string_view SEMA_ANALYSIS_FAILED =
    "semantic analysis failed";

inline constexpr std::string_view SEMA_AST_ITEM_MODULE_CONTRACT =
    "semantic AST contract violation: item_modules must contain one module owner per item";

inline constexpr std::string_view SEMA_AST_ITEM_MODULE_INVALID =
    "semantic AST contract violation: item module owner is invalid";

inline constexpr std::string_view SEMA_DUPLICATE_SYMBOL =
    "duplicate symbol";

inline constexpr std::string_view SEMA_DUPLICATE_DEFINITION_OR_SHADOWING =
    "duplicate definition or shadowing is not allowed: ";

inline constexpr std::string_view SEMA_PUBLIC_FUNCTION_RETURN_TYPE_EXPLICIT =
    "public function return type must be explicit";

inline constexpr std::string_view SEMA_GENERIC_PARAMS_UNSUPPORTED_ON_ITEM =
    "generic parameters are not supported on this item by M2 semantic analysis";

inline constexpr std::string_view SEMA_GENERIC_C_ABI_OR_PROTOTYPE_UNSUPPORTED =
    "generic functions with C ABI or prototypes are not supported by M2 semantic analysis";

inline constexpr std::string_view SEMA_GENERIC_METHODS_UNSUPPORTED =
    "method-local generic parameters are not supported by M2 semantic analysis";

inline constexpr std::string_view SEMA_GENERIC_RESOURCE_CAPABILITY_UNSUPPORTED =
    "resource capabilities are not part of M2 where constraints";

inline constexpr std::string_view SEMA_VARIADIC_EXTERN_C_ONLY =
    "variadic functions are only supported for extern c declarations";

inline constexpr std::string_view SEMA_VARIADIC_FUNCTION_TYPE_EXTERN_C_ONLY =
    "variadic function types are only supported for extern c fn";

inline constexpr std::string_view SEMA_IMPL_TARGET_NAMED_TYPE =
    "impl target must be a named type";

inline constexpr std::string_view SEMA_C_ABI_RETURN_TYPE_EXPLICIT =
    "C ABI function return type must be explicit";

inline constexpr std::string_view SEMA_PROTOTYPE_RETURN_TYPE_EXPLICIT =
    "function prototype return type must be explicit";

inline constexpr std::string_view SEMA_FUNCTION_PARAMETER_STORAGE =
    "function parameter type is not valid storage";

inline constexpr std::string_view SEMA_FUNCTION_TYPE_PARAMETER_STORAGE =
    "function type parameter type is not valid storage";

inline constexpr std::string_view SEMA_FUNCTION_TYPE_RETURN_STORAGE =
    "function type return type is not valid storage";

inline constexpr std::string_view SEMA_ARRAY_PARAMETER_UNSUPPORTED =
    "array type cannot be used as a function parameter";

inline constexpr std::string_view SEMA_ARRAY_FUNCTION_TYPE_PARAMETER_UNSUPPORTED =
    "array type cannot be used as a function type parameter";

inline constexpr std::string_view SEMA_ARRAY_FUNCTION_TYPE_RETURN_UNSUPPORTED =
    "array type cannot be used as a function type return";

inline constexpr std::string_view SEMA_ARRAY_STRUCT_PARAMETER_UNSUPPORTED =
    "struct containing array cannot be passed by value";

inline constexpr std::string_view SEMA_METHOD_SELF_FIRST =
    "method self parameter must be first";

inline constexpr std::string_view SEMA_METHOD_SELF_TYPE =
    "method self parameter must use the impl type or a pointer to it";

inline constexpr std::string_view SEMA_ENUM_BASE_INTEGER =
    "enum base type must be an integer type";

inline constexpr std::string_view SEMA_ENUM_DISCRIMINANT_OUT_OF_RANGE =
    "enum discriminant literal is out of range";

inline constexpr std::string_view SEMA_ENUM_DISCRIMINANT_DOES_NOT_FIT =
    "enum discriminant does not fit enum base type";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_STORAGE =
    "enum payload type is not valid storage";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_ARRAY_UNSUPPORTED =
    "enum payload cannot contain array storage";

inline constexpr std::string_view SEMA_ORDINARY_MAIN_ABI_NAME =
    "ordinary fn main cannot use ABI name 'main'";

inline constexpr std::string_view SEMA_MAIN_PARAMETERS =
    "ordinary fn main must use either no parameters or (argc: i32, argv: *mut *mut u8)";

inline constexpr std::string_view SEMA_MAIN_PARAMETERS_EXACT =
    "ordinary fn main parameters must be (argc: i32, argv: *mut *mut u8)";

inline constexpr std::string_view SEMA_MAIN_RETURN =
    "ordinary fn main must return i32 or void";

inline constexpr std::string_view SEMA_FIELD_STORAGE =
    "field type is not valid storage";

inline constexpr std::string_view SEMA_CONST_NOT_COMPILE_TIME =
    "const initializer is not compile-time constant";

inline constexpr std::string_view SEMA_CONST_TYPE_STORAGE =
    "const type is not valid storage";

inline constexpr std::string_view SEMA_CONST_TYPE_MISMATCH =
    "const initializer type does not match declared type";

inline constexpr std::string_view SEMA_FUNCTION_NAME_VALUE =
    "function name cannot be used as a value: ";

inline constexpr std::string_view SEMA_FOR_CONDITION_BOOL =
    "for condition must be bool";

inline constexpr std::string_view SEMA_FOR_RANGE_ARITY =
    "range expects 1 to 3 arguments";

inline constexpr std::string_view SEMA_RANGE_BOUNDS_INTEGER =
    "range bounds must be integer";

inline constexpr std::string_view SEMA_RANGE_STEP_INTEGER =
    "range step must be integer";

inline constexpr std::string_view SEMA_RANGE_BOUNDS_SAME_TYPE =
    "range bounds must have the same type";

inline constexpr std::string_view SEMA_RANGE_STEP_SAME_TYPE =
    "range step must have the same type as bounds";

inline constexpr std::string_view SEMA_LOCAL_TYPE_INFER =
    "local variable type cannot be inferred";

inline constexpr std::string_view SEMA_LOCAL_STORAGE =
    "local variable type is not valid storage";

inline constexpr std::string_view SEMA_INITIALIZER_TYPE_MISMATCH =
    "initializer type does not match declared type";

inline constexpr std::string_view SEMA_ASSIGNMENT_LHS_WRITABLE =
    "left side of assignment must be writable";

inline constexpr std::string_view SEMA_COMPOUND_ASSIGNMENT_TYPE_MISMATCH =
    "compound assignment type mismatch";

inline constexpr std::string_view SEMA_ASSIGNMENT_TYPE_MISMATCH =
    "assignment type mismatch";

inline constexpr std::string_view SEMA_ARRAY_ASSIGNMENT_UNSUPPORTED =
    "array or array-containing type cannot be assigned";

inline constexpr std::string_view SEMA_IF_CONDITION_BOOL =
    "if condition must be bool";

inline constexpr std::string_view SEMA_WHILE_CONDITION_BOOL =
    "while condition must be bool";

inline constexpr std::string_view SEMA_RETURN_TYPE_MISMATCH =
    "return type mismatch";

inline constexpr std::string_view SEMA_EXPR_STMT_CALL_OR_TRY =
    "expression statement must be a function call or try expression";

inline constexpr std::string_view SEMA_BREAK_CONTINUE_IN_LOOP =
    "break and continue are only valid inside loops";

inline constexpr std::string_view SEMA_DEFER_CALL =
    "defer statement must be a function call";

inline constexpr std::string_view SEMA_RETURN_TYPE_INFER =
    "function return type cannot be inferred";

inline constexpr std::string_view SEMA_INFERRED_RETURN_TYPE_MISMATCH =
    "inferred function return types do not match";

inline constexpr std::string_view SEMA_ARRAY_RETURN_UNSUPPORTED =
    "array type cannot be used as a function return type";

inline constexpr std::string_view SEMA_ARRAY_STRUCT_RETURN_UNSUPPORTED =
    "struct containing array cannot be returned by value";

inline constexpr std::string_view SEMA_RECURSIVE_RETURN_INFER =
    "cannot infer recursive function return type without an explicit return type";

inline constexpr std::string_view SEMA_NOT_ALL_PATHS_RETURN =
    "not all control paths return a value";

inline constexpr std::string_view SEMA_EXPLICIT_GENERIC_CALL_SYNTAX =
    "explicit generic calls use '[...]', for example id[i32](...)";

inline constexpr std::string_view SEMA_CALLEE_FUNCTION_NAME =
    "callee must be a function value; explicit generic calls use '[...]', for example id[i32](...)";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_ARGUMENT_TYPE_MISMATCH =
    "enum payload constructor argument type mismatch";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_ARRAY_ARGUMENT_UNSUPPORTED =
    "array-containing type cannot be used as enum payload";

inline constexpr std::string_view SEMA_ARGUMENT_ARRAY_UNSUPPORTED =
    "array-containing type cannot be passed by value";

inline constexpr std::string_view SEMA_ARRAY_INDEX_INTEGER =
    "array index must be an integer";

inline constexpr std::string_view SEMA_ARRAY_INDEX_OUT_OF_BOUNDS =
    "array constant index is out of bounds";

inline constexpr std::string_view SEMA_SLICE_BOUND_INTEGER =
    "slice bound must be an integer";

inline constexpr std::string_view SEMA_LOGICAL_NOT_BOOL =
    "logical not requires bool operand";

inline constexpr std::string_view SEMA_NUMERIC_UNARY_OPERAND =
    "numeric unary operator requires signed integer or float operand";

inline constexpr std::string_view SEMA_BITWISE_NOT_INTEGER =
    "bitwise not requires integer operand";

inline constexpr std::string_view SEMA_DEREF_POINTER =
    "dereference requires pointer or reference operand";

inline constexpr std::string_view SEMA_DEREF_STORAGE =
    "dereference requires pointer or reference to valid storage";

inline constexpr std::string_view SEMA_UNSAFE_DEREF =
    "raw pointer dereference requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_RAW_POINTER_PROJECTION =
    "raw pointer projection requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_PTRCAST =
    "ptrcast requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_BITCAST =
    "bitcast requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_PTRAT =
    "ptrat requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_STRRAW =
    "strraw requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_BLOCK_CONST_INITIALIZER =
    "unsafe block cannot be used in const initializer";

inline constexpr std::string_view SEMA_ADDRESS_OF_PLACE =
    "address-of requires a place expression";

inline constexpr std::string_view SEMA_MUTABLE_REFERENCE_PLACE =
    "mutable reference requires a writable place expression";

inline constexpr std::string_view SEMA_REFERENCE_STORAGE =
    "reference requires a valid storage type";

inline constexpr std::string_view SEMA_BINARY_OPERANDS_SAME_TYPE =
    "binary operands must have the same type";

inline constexpr std::string_view SEMA_SHIFT_NEGATIVE =
    "shift amount cannot be negative";

inline constexpr std::string_view SEMA_SHIFT_OUT_OF_RANGE =
    "shift amount is out of range";

inline constexpr std::string_view SEMA_COMPARISON_NUMERIC =
    "comparison operator requires numeric operands";

inline constexpr std::string_view SEMA_EQUALITY_SCALAR =
    "equality operator requires scalar operands";

inline constexpr std::string_view SEMA_LOGICAL_BOOL =
    "logical operator requires bool operands";

inline constexpr std::string_view SEMA_INTEGER_OPERATOR_INTEGER =
    "integer operator requires integer operands";

inline constexpr std::string_view SEMA_BINARY_NUMERIC =
    "binary operator requires numeric operands";

inline constexpr std::string_view SEMA_FIELD_STRUCT_VALUE =
    "field access requires a non-opaque struct value";

inline constexpr std::string_view SEMA_INDEX_POINTER_ARRAY_DEREF =
    "indexing pointer to array requires explicit dereference";

inline constexpr std::string_view SEMA_INDEX_POINTER_STORAGE =
    "indexing pointer requires pointer to valid storage";

inline constexpr std::string_view SEMA_INDEX_ARRAY_OR_POINTER =
    "indexing requires array, slice, or pointer value";

inline constexpr std::string_view SEMA_SLICE_ARRAY_OR_SLICE =
    "slicing requires array, slice, or str value";

inline constexpr std::string_view SEMA_SLICE_ELEMENT_STORAGE =
    "slice element type is not valid storage";

inline constexpr std::string_view SEMA_ARRAY_LITERAL_EXPECTED_TYPE =
    "array literal requires an array expected type";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_INTEGER =
    "array repeat count must be an integer literal";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_OUT_OF_RANGE =
    "array repeat count literal is out of range";

inline constexpr std::string_view SEMA_EMPTY_ARRAY_CONTEXT =
    "empty array literal requires an array type context";

inline constexpr std::string_view SEMA_ARRAY_ELEMENT_INFER =
    "array literal element type cannot be inferred";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_TYPE_MISMATCH =
    "array repeat value type mismatch";

inline constexpr std::string_view SEMA_ARRAY_LITERAL_STORAGE =
    "array literal type is not valid storage";

inline constexpr std::string_view SEMA_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH =
    "array literal element type mismatch";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_EXPECTED_TYPE =
    "tuple literal requires a tuple expected type";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_ARITY =
    "tuple literal arity does not match expected tuple type";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_ELEMENT_TYPE_MISMATCH =
    "tuple literal element type mismatch";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_STORAGE =
    "tuple literal type is not valid storage";

inline constexpr std::string_view SEMA_TUPLE_FIELD_ACCESS_UNSUPPORTED =
    "tuple fields are not directly accessible; destructure the tuple or use a named struct";

inline constexpr std::string_view SEMA_TUPLE_DESTRUCTURE_TYPE =
    "tuple destructuring requires a tuple value";

inline constexpr std::string_view SEMA_TUPLE_DESTRUCTURE_ARITY =
    "tuple destructuring pattern arity does not match tuple type";

inline constexpr std::string_view SEMA_LOCAL_PATTERN_REFUTABLE =
    "local destructuring pattern must be irrefutable";

inline constexpr std::string_view SEMA_LET_ELSE_PATTERN =
    "let-else requires a pattern";

inline constexpr std::string_view SEMA_LET_ELSE_FALLTHROUGH =
    "let-else else block must not fall through";

inline constexpr std::string_view SEMA_STRUCT_LITERAL_TYPE =
    "struct literal requires a non-opaque struct type";

inline constexpr std::string_view SEMA_STRUCT_LITERAL_FIELD_TYPE_MISMATCH =
    "struct literal field type mismatch";

inline constexpr std::string_view SEMA_INVALID_CONVERSION =
    "invalid explicit conversion";

inline constexpr std::string_view SEMA_GENERIC_SIZEOF_ALIGNOF =
    "generic type parameter cannot be queried by sizeof or alignof";

inline constexpr std::string_view SEMA_OPAQUE_SIZEOF_ALIGNOF =
    "opaque struct cannot be queried by sizeof or alignof directly";

inline constexpr std::string_view SEMA_SIZEOF_ALIGNOF_STORAGE =
    "sizeof and alignof require a valid storage type";

inline constexpr std::string_view SEMA_PTRADDR_POINTER =
    "ptraddr requires a pointer value";

inline constexpr std::string_view SEMA_PTRAT_POINTER =
    "ptrat target type must be a pointer";

inline constexpr std::string_view SEMA_PTRAT_INTEGER =
    "ptrat address must be an integer";

inline constexpr std::string_view SEMA_STRPTR_STR =
    "strptr requires a str value";

inline constexpr std::string_view SEMA_STRBLEN_STR =
    "strblen requires a str value";

inline constexpr std::string_view SEMA_STR_UTF8_SLICE =
    "str UTF-8 builtin requires a []const u8 or []mut u8 byte slice";

inline constexpr std::string_view SEMA_STRRAW_ARITY =
    "strraw requires data and length arguments";

inline constexpr std::string_view SEMA_STRRAW_DATA_POINTER =
    "strraw data must be *const u8";

inline constexpr std::string_view SEMA_STRRAW_LENGTH_INTEGER =
    "strraw length must be an integer";

inline constexpr std::string_view SEMA_TRY_CONST_INITIALIZER =
    "try expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_TRY_RESULT_SHAPE =
    "try expression result-like enum must define ok(payload) and err(payload) cases";

inline constexpr std::string_view SEMA_TRY_RESULT_RETURN =
    "try expression on result-like enum requires enclosing function to return result-like enum with err payload";

inline constexpr std::string_view SEMA_TRY_RESULT_ERR_PAYLOAD =
    "try expression result-like enum err payload type must match enclosing result-like enum err payload type";

inline constexpr std::string_view SEMA_TRY_OPTION_SHAPE =
    "try expression option-like enum must define some(payload) and none cases";

inline constexpr std::string_view SEMA_TRY_OPTION_RETURN =
    "try expression on option-like enum requires enclosing function to return option-like enum with none case";

inline constexpr std::string_view SEMA_TRY_SHAPE =
    "try expression requires result-like ok/err enum or option-like some/none enum";

inline constexpr std::string_view SEMA_IF_EXPR_CONST_INITIALIZER =
    "if expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_IF_EXPR_CONDITION_BOOL =
    "if expression condition must be bool";

inline constexpr std::string_view SEMA_IF_EXPR_BRANCH_TYPE =
    "if expression branches must have the same type";

inline constexpr std::string_view SEMA_IF_EXPR_VOID =
    "if expression branches cannot be void";

inline constexpr std::string_view SEMA_BLOCK_EXPR_CONST_INITIALIZER =
    "block expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_BLOCK_EXPR_FINAL =
    "block expression requires a final expression";

inline constexpr std::string_view SEMA_BLOCK_EXPR_UNREACHABLE =
    "block expression final expression is unreachable";

inline constexpr std::string_view SEMA_BLOCK_EXPR_VOID =
    "block expression result cannot be void";

inline constexpr std::string_view SEMA_INTEGER_DIVISION_BY_ZERO =
    "integer division by zero";

inline constexpr std::string_view SEMA_INTEGER_MODULO_BY_ZERO =
    "integer modulo by zero";

inline constexpr std::string_view SEMA_SIGNED_DIVISION_OVERFLOW =
    "signed integer division overflows";

inline constexpr std::string_view SEMA_SIGNED_MODULO_OVERFLOW =
    "signed integer modulo overflows";

inline constexpr std::string_view SEMA_MATCH_CONST_INITIALIZER =
    "match expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_MATCH_VALUE_TYPE =
    "match expression requires an enum, integer, bool, tuple, struct, array, or slice value";

inline constexpr std::string_view SEMA_MATCH_ARM_REQUIRED =
    "match expression requires at least one arm";

inline constexpr std::string_view SEMA_MATCH_ARM_TYPE =
    "match expression arms must have the same type";

inline constexpr std::string_view SEMA_MATCH_GUARD_BOOL =
    "match guard must be bool";

inline constexpr std::string_view SEMA_MATCH_PAYLOAD_CASE =
    "match arm payload binding requires a payload enum case";

inline constexpr std::string_view SEMA_MATCH_INTEGER_BOOL_WILDCARD =
    "match expression over integer or bool requires a wildcard arm";

inline constexpr std::string_view SEMA_MATCH_RESULT_VOID =
    "match expression result cannot be void";

inline constexpr std::string_view SEMA_OR_PATTERN_BINDING_NAMES =
    "or-pattern alternatives must bind the same names";

inline constexpr std::string_view SEMA_OR_PATTERN_BINDING_TYPES =
    "or-pattern binding types must match across alternatives";

inline constexpr std::string_view SEMA_MATCH_NON_ENUM_IRREFUTABLE =
    "match expression over tuple, struct, array, or slice requires an irrefutable arm";

inline constexpr std::string_view SEMA_MATCH_WILDCARD_UNREACHABLE =
    "match arm is unreachable after wildcard pattern";

inline constexpr std::string_view SEMA_ENUM_MATCH_PATTERN =
    "enum match pattern must be an enum case or wildcard";

inline constexpr std::string_view SEMA_ENUM_PATTERN_TYPE =
    "enum pattern requires an enum value";

inline constexpr std::string_view SEMA_MATCH_CASE_WRONG_ENUM =
    "match arm case does not belong to matched enum";

inline constexpr std::string_view SEMA_INTEGER_BOOL_PATTERN =
    "match pattern for integer or bool value must be a literal or wildcard";

inline constexpr std::string_view SEMA_BOOL_PATTERN =
    "bool match pattern must be true or false";

inline constexpr std::string_view SEMA_INTEGER_PATTERN =
    "integer match pattern must be an integer literal";

inline constexpr std::string_view SEMA_INTEGER_PATTERN_RANGE =
    "integer match pattern literal is out of range for matched type";

inline constexpr std::string_view SEMA_UNSUPPORTED_LITERAL_PATTERN =
    "unsupported literal match pattern";

inline constexpr std::string_view SEMA_STRUCT_PATTERN_TYPE =
    "struct pattern requires a struct value";

inline constexpr std::string_view SEMA_STRUCT_PATTERN_FIELD =
    "unknown struct pattern field";

inline constexpr std::string_view SEMA_STRUCT_PATTERN_DUPLICATE_FIELD =
    "duplicate struct pattern field";

inline constexpr std::string_view SEMA_SLICE_PATTERN_TYPE =
    "slice pattern requires an array or slice value";

inline constexpr std::string_view SEMA_SLICE_PATTERN_LENGTH =
    "slice pattern length does not match array length";

inline constexpr std::string_view SEMA_OPAQUE_POINTER_ONLY =
    "opaque struct can only be used as a pointer target";

inline constexpr std::string_view SEMA_ARRAY_STORAGE_OVERFLOW =
    "array storage size overflows ABI size";

inline constexpr std::string_view SEMA_STRUCT_STORAGE_OVERFLOW =
    "struct storage size overflows ABI size";

inline constexpr std::string_view SEMA_ENUM_STORAGE_OVERFLOW =
    "enum storage size overflows ABI size";

[[nodiscard]] inline std::string sema_duplicate_generic_parameter_message(const std::string_view name) {
    return "duplicate generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_first_generic_parameter_message(const std::string_view name) {
    return "first declaration of generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_did_you_mean_message(const std::string_view name) {
    return "did you mean `" + std::string(name) + "`?";
}

[[nodiscard]] inline std::string sema_expected_type_note_message(const std::string_view name) {
    return "expected type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_actual_type_note_message(const std::string_view name) {
    return "actual type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_previous_declaration_note_message(const std::string_view name) {
    return "previous declaration of `" + std::string(name) + "` is here";
}

[[nodiscard]] inline std::string sema_duplicate_type_definition_message(
    const std::string_view module,
    const std::string_view name
) {
    return "duplicate type definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_struct_definition_message(
    const std::string_view module,
    const std::string_view name
) {
    return "duplicate struct definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_function_definition_message(
    const std::string_view module,
    const std::string_view name
) {
    return "duplicate function definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_signature_mismatch_message(const std::string_view name) {
    return "function prototype and definition signatures do not match: " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_declaration_conflict_message(const std::string_view name) {
    return "function declaration conflicts with existing function: " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_function_prototype_message(const std::string_view name) {
    return "duplicate function prototype: " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_prototype_order_message(const std::string_view name) {
    return "function prototype must appear before definition: " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_prototype_missing_definition_message(const std::string_view name) {
    return "function prototype has no definition: " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_function_definition_simple_message(const std::string_view name) {
    return "duplicate function definition: " + std::string(name);
}

[[nodiscard]] inline std::string sema_extern_c_abi_conflict_message(const std::string_view symbol) {
    return "extern C ABI symbol redeclared with incompatible signature: " + std::string(symbol);
}

[[nodiscard]] inline std::string sema_duplicate_abi_symbol_message(const std::string_view symbol) {
    return "duplicate ABI symbol: " + std::string(symbol);
}

inline constexpr std::string_view SEMA_ORDINARY_MAIN_EXPORTED_C_MAIN =
    "ordinary fn main cannot be combined with an exported C main entry";

[[nodiscard]] inline std::string sema_duplicate_value_definition_message(
    const std::string_view module,
    const std::string_view name
) {
    return "duplicate value definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_value_definition_in_module_message(const std::string_view name) {
    return "duplicate value definition in module: " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_namespace_member_message(
    const std::string_view module,
    const std::string_view name
) {
    return "duplicate module member across namespaces in module " +
           std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_enum_case_message(
    const std::string_view enum_name,
    const std::string_view case_name
) {
    return "duplicate enum case: " + std::string(enum_name) + "." + std::string(case_name);
}

[[nodiscard]] inline std::string sema_duplicate_type_member_message(
    const std::string_view type_name,
    const std::string_view name
) {
    return "duplicate type member: " + std::string(type_name) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_enum_discriminant_message(const std::string_view enum_name) {
    return "duplicate enum discriminant value in " + std::string(enum_name);
}

[[nodiscard]] inline std::string sema_duplicate_struct_field_message(const std::string_view field_name) {
    return "duplicate struct field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_function_name_value_message(const std::string_view name) {
    return std::string(SEMA_FUNCTION_NAME_VALUE) + std::string(name);
}

[[nodiscard]] inline std::string sema_argument_count_message(const std::string_view function_name) {
    return "argument count mismatch in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_argument_type_message(const std::string_view function_name) {
    return "argument type mismatch in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_variadic_argument_type_infer_message(const std::string_view function_name) {
    return "variadic argument type cannot be inferred in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_function_not_generic_message(const std::string_view function_name) {
    return "function " + std::string(function_name) + " is not generic";
}

[[nodiscard]] inline std::string sema_unsafe_function_call_message(const std::string_view function_name) {
    return "call to unsafe function " + std::string(function_name) + " requires unsafe context";
}

[[nodiscard]] inline std::string sema_method_requires_receiver_message(
    const std::string_view owner,
    const std::string_view method
) {
    return "method requires a receiver: " + std::string(owner) + "." + std::string(method);
}

[[nodiscard]] inline std::string sema_unknown_field_message(const std::string_view field_name) {
    return "unknown field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_private_field_message(const std::string_view field_name) {
    return "field is private: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_duplicate_struct_literal_field_message(const std::string_view field_name) {
    return "duplicate struct literal field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_unknown_struct_literal_field_message(const std::string_view field_name) {
    return "unknown field in struct literal: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_struct_literal_missing_field_message(const std::string_view field_name) {
    return "struct literal is missing field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_type_not_generic_message(const std::string_view name) {
    return "type " + std::string(name) + " is not generic";
}

[[nodiscard]] inline std::string sema_generic_type_requires_args_message(const std::string_view name) {
    return "generic type " + std::string(name) + " requires type arguments";
}

[[nodiscard]] inline std::string sema_generic_param_type_args_message(const std::string_view name) {
    return "generic type parameter cannot take type arguments: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_generic_constraint_param_message(const std::string_view name) {
    return "where constraint references unknown generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_unknown_capability_message(const std::string_view name) {
    return "unknown M2 capability `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_duplicate_capability_message(
    const std::string_view param,
    const std::string_view capability
) {
    return "duplicate capability `" + std::string(capability) +
           "` for generic parameter `" + std::string(param) + "`";
}

[[nodiscard]] inline std::string sema_generic_capability_not_satisfied_message(
    const std::string_view type_name,
    const std::string_view capability
) {
    return "type " + std::string(type_name) + " does not satisfy capability `" +
           std::string(capability) + "`";
}

[[nodiscard]] inline std::string sema_cyclic_type_alias_message(const std::string_view name) {
    return "cyclic type alias: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_enum_case_message(const std::string_view name) {
    return "unknown enum case: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_scoped_enum_case_message(
    const std::string_view enum_name,
    const std::string_view case_name
) {
    return "unknown enum case: " + std::string(enum_name) + "." + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_call_message(const std::string_view case_name) {
    return "enum payload constructor requires a call: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_case_message(const std::string_view case_name) {
    return "enum case constructor requires a payload case: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_arity_message(const std::string_view case_name) {
    return "enum payload constructor requires exactly one argument: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_argument_count_message(
    const std::string_view case_name,
    const base::usize count
) {
    return "enum payload constructor requires " + std::to_string(count) +
           " arguments: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_match_missing_enum_case_message(const std::string_view case_name) {
    return "match expression is not exhaustive for enum case: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_unknown_matched_enum_case_message(const std::string_view case_name) {
    return "unknown enum case for matched enum: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_duplicate_match_enum_case_message(const std::string_view case_name) {
    return "duplicate match arm for enum case: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_pattern_binding_count_message(const base::usize count) {
    return "enum payload pattern requires " + std::to_string(count) +
           " binding" + (count == 1 ? "" : "s");
}

[[nodiscard]] inline std::string sema_integer_literal_out_of_range_message(const std::string_view type_name) {
    return "integer literal out of range for " + std::string(type_name);
}

[[nodiscard]] inline std::string sema_invalid_integer_literal_suffix_message(const std::string_view suffix) {
    return "invalid integer literal suffix `" + std::string(suffix) + "`";
}

[[nodiscard]] inline std::string sema_integer_literal_suffix_type_mismatch_message(
    const std::string_view suffix_type,
    const std::string_view expected_type
) {
    return "integer literal suffix type " + std::string(suffix_type) +
           " does not match expected " + std::string(expected_type);
}

[[nodiscard]] inline std::string sema_generic_comparison_operator_message(const std::string_view type_name) {
    return "generic type parameter `" + std::string(type_name) + "` has no known comparison operator";
}

[[nodiscard]] inline std::string sema_generic_equality_operator_message(const std::string_view type_name) {
    return "generic type parameter `" + std::string(type_name) + "` has no known operator `==`";
}

[[nodiscard]] inline std::string sema_generic_integer_operator_message(const std::string_view type_name) {
    return "generic type parameter `" + std::string(type_name) + "` has no known integer operator";
}

[[nodiscard]] inline std::string sema_generic_operator_message(const std::string_view type_name) {
    return "generic type parameter `" + std::string(type_name) + "` has no known operator";
}

[[nodiscard]] inline std::string sema_array_literal_length_mismatch_message(
    const base::u64 expected,
    const base::u64 actual
) {
    return "array literal length mismatch: expected " + std::to_string(expected) +
           ", got " + std::to_string(actual);
}

[[nodiscard]] inline std::string sema_cyclic_const_initializer_message(
    const std::string_view name
) {
    return "cyclic const initializer: " + std::string(name);
}

[[nodiscard]] inline std::string sema_generic_argument_count_message(
    const std::string_view subject,
    const std::string_view name,
    const base::usize actual,
    const base::usize expected
) {
    std::string message = actual < expected ? "too few " : "too many ";
    message += std::string(subject);
    message += " for ";
    message += std::string(name);
    message += ": expected ";
    message += std::to_string(expected);
    message += ", got ";
    message += std::to_string(actual);
    return message;
}

[[nodiscard]] inline std::string sema_ambiguous_generic_type_name_message(
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous generic type name '" + std::string(name) + "' from modules " +
           std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_generic_type_message(const std::string_view name) {
    return "unknown generic type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_generic_type_in_module_message(
    const std::string_view module,
    const std::string_view name
) {
    return "unknown generic type in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_generic_type_message(
    const std::string_view module,
    const std::string_view name
) {
    return "generic type is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_ambiguous_generic_function_name_message(
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous generic function name '" + std::string(name) + "' from modules " +
           std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_generic_function_message(const std::string_view name) {
    return "unknown generic function: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_generic_function_in_module_message(
    const std::string_view module,
    const std::string_view name
) {
    return "unknown generic function in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_generic_function_message(
    const std::string_view module,
    const std::string_view name
) {
    return "generic function is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_generic_call_argument_infer_message(
    const std::string_view parameter,
    const std::string_view function_name
) {
    return "cannot infer generic type argument `" + std::string(parameter) +
           "` for call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_generic_call_argument_unify_message(const std::string_view function_name) {
    return "cannot infer generic type argument for call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_float_literal_out_of_range_message(const std::string_view type_name) {
    return "float literal out of range for " + std::string(type_name);
}

[[nodiscard]] inline std::string sema_invalid_float_literal_suffix_message(const std::string_view suffix) {
    return "invalid float literal suffix `" + std::string(suffix) + "`";
}

[[nodiscard]] inline std::string sema_float_literal_suffix_type_mismatch_message(
    const std::string_view suffix_type,
    const std::string_view expected_type
) {
    return "float literal suffix type " + std::string(suffix_type) +
           " does not match expected " + std::string(expected_type);
}

[[nodiscard]] inline std::string sema_unknown_import_alias_message(const std::string_view alias) {
    return "unknown import alias: " + std::string(alias);
}

[[nodiscard]] inline std::string sema_ambiguous_import_alias_message(const std::string_view alias) {
    return "ambiguous import alias: " + std::string(alias);
}

[[nodiscard]] inline std::string sema_unknown_module_path_message(const std::string_view path) {
    return "unknown module path: " + std::string(path);
}

[[nodiscard]] inline std::string sema_local_shadows_import_alias_message(const std::string_view name) {
    return "local name shadows import alias: " + std::string(name);
}

[[nodiscard]] inline std::string sema_local_shadows_root_module_message(const std::string_view name) {
    return "local name shadows visible root module: " + std::string(name);
}

[[nodiscard]] inline std::string sema_local_shadows_generic_type_parameter_message(const std::string_view name) {
    return "local name shadows generic type parameter: " + std::string(name);
}

[[nodiscard]] inline std::string sema_local_shadows_type_name_message(const std::string_view name) {
    return "local name shadows visible type: " + std::string(name);
}

inline constexpr std::string_view SEMA_MUTABLE_METHOD_RECEIVER_POINTER =
    "mutable method receiver requires mutable pointer";

inline constexpr std::string_view SEMA_METHOD_RECEIVER_PLACE =
    "method receiver must be a place expression";

inline constexpr std::string_view SEMA_MUTABLE_METHOD_RECEIVER_WRITABLE =
    "mutable method receiver requires writable storage";

[[nodiscard]] inline std::string sema_ambiguous_method_message(
    const std::string_view owner,
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous method '" + std::string(owner) + "." + std::string(name) +
           "' from modules " + std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_private_method_message(
    const std::string_view owner,
    const std::string_view name
) {
    return "method is private: " + std::string(owner) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_method_message(
    const std::string_view owner,
    const std::string_view name
) {
    return "unknown method: " + std::string(owner) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_ambiguous_type_name_message(
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous type name '" + std::string(name) + "' from modules " +
           std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_type_message(const std::string_view name) {
    return "unknown type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_type_message(
    const std::string_view module,
    const std::string_view name
) {
    return "type is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_type_in_module_message(
    const std::string_view module,
    const std::string_view name
) {
    return "unknown type in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_ambiguous_function_name_message(
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous function name '" + std::string(name) + "' from modules " +
           std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_function_message(const std::string_view name) {
    return "unknown function: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_function_in_module_message(
    const std::string_view module,
    const std::string_view name
) {
    return "unknown function in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_function_message(
    const std::string_view module,
    const std::string_view name
) {
    return "function is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_ambiguous_enum_case_message(
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous enum case '" + std::string(name) + "' from modules " +
           std::string(first_module) + " and " + std::string(second_module);
}

inline constexpr std::string_view SEMA_ENUM_CASE_SCOPE_TYPE =
    "enum case scope must name an enum type";

[[nodiscard]] inline std::string sema_ambiguous_name_message(
    const std::string_view name,
    const std::string_view first_module,
    const std::string_view second_module
) {
    return "ambiguous name '" + std::string(name) + "' from modules " +
           std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_name_message(const std::string_view name) {
    return "unknown name: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_name_in_module_message(
    const std::string_view module,
    const std::string_view name
) {
    return "unknown name in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_name_message(
    const std::string_view module,
    const std::string_view name
) {
    return "name is private: " + std::string(module) + "." + std::string(name);
}

} // namespace aurex::sema

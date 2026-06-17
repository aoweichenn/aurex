#pragma once

#include <aurex/frontend/syntax/ast/nodes.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <string>
#include <string_view>

namespace aurex::sema {

inline constexpr std::string_view SEMA_ANALYSIS_FAILED = "semantic analysis failed";

inline constexpr std::string_view SEMA_AST_ITEM_MODULE_CONTRACT =
    "semantic AST contract violation: item_modules must contain one module owner per item";

inline constexpr std::string_view SEMA_AST_ITEM_PART_CONTRACT =
    "semantic AST contract violation: item_part_indices must contain one part owner per item";

inline constexpr std::string_view SEMA_AST_ITEM_MODULE_INVALID =
    "semantic AST contract violation: item module owner is invalid";

inline constexpr std::string_view SEMA_AST_ITEM_PART_INVALID =
    "semantic AST contract violation: item module part owner is invalid";

inline constexpr std::string_view SEMA_AST_ITEM_IMPORT_SCOPE_MODULE_INVALID =
    "semantic AST contract violation: item import scope module owner is invalid";

inline constexpr std::string_view SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID =
    "semantic AST contract violation: item import scope part owner is invalid";

inline constexpr std::string_view SEMA_DUPLICATE_SYMBOL = "duplicate symbol";

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

inline constexpr std::string_view SEMA_DEFAULT_PARAMETER_AFTER_REQUIRED =
    "required parameters cannot follow a default parameter";

inline constexpr std::string_view SEMA_DEFAULT_PARAMETER_C_ABI =
    "default parameters are not supported on C ABI functions";

inline constexpr std::string_view SEMA_DEFAULT_PARAMETER_VARIADIC =
    "default parameters are not supported on variadic functions";

inline constexpr std::string_view SEMA_DEFAULT_PARAMETER_TRAIT_UNSUPPORTED =
    "default parameters on trait requirements are not supported yet";

inline constexpr std::string_view SEMA_NAMED_ARGUMENT_NOT_SUPPORTED =
    "named arguments are not supported for this call";

inline constexpr std::string_view SEMA_POSITIONAL_ARGUMENT_AFTER_NAMED =
    "positional arguments cannot follow named arguments";

[[nodiscard]] inline constexpr std::string_view sema_visibility_surface_name(
    const syntax::Visibility visibility) noexcept
{
    switch (visibility) {
        case syntax::Visibility::private_:
            return "private";
        case syntax::Visibility::package_:
            return "package-visible";
        case syntax::Visibility::public_:
            return "public";
    }
    return "private";
}

inline constexpr std::string_view SEMA_VARIADIC_FUNCTION_TYPE_EXTERN_C_ONLY =
    "variadic function types are only supported for extern c fn";

inline constexpr std::string_view SEMA_IMPL_TARGET_NAMED_TYPE = "impl target must be a named type";

inline constexpr std::string_view SEMA_TRAIT_IMPL_TARGET_NAMED_TRAIT = "trait impl target must be a named trait";

inline constexpr std::string_view SEMA_TRAIT_IMPL_GENERIC_UNSUPPORTED =
    "generic trait impl blocks are not supported by M4-WP4 semantic analysis";

inline constexpr std::string_view SEMA_ASSOCIATED_TYPE_GENERIC_UNSUPPORTED =
    "generic associated types are not supported by M4-WP6 semantic analysis";

inline constexpr std::string_view SEMA_DROP_TRAIT_RESERVED =
    "Drop is a reserved destructor trait and cannot be declared by user code";

inline constexpr std::string_view SEMA_DROP_IMPL_TRAIT_SURFACE =
    "Drop impl must use unqualified Drop without type arguments";

inline constexpr std::string_view SEMA_DROP_IMPL_GENERIC_UNSUPPORTED =
    "generic Drop impl blocks are not supported";

inline constexpr std::string_view SEMA_DROP_IMPL_TARGET =
    "Drop impl target must be a named struct, enum, or opaque struct";

inline constexpr std::string_view SEMA_DROP_IMPL_DUPLICATE = "duplicate Drop impl for type";

inline constexpr std::string_view SEMA_DROP_IMPL_METHOD_REQUIRED = "Drop impl must define fn drop";

inline constexpr std::string_view SEMA_DROP_IMPL_SINGLE_METHOD = "Drop impl must define exactly one method";

inline constexpr std::string_view SEMA_DROP_METHOD_NAME = "Drop impl method must be named drop";

inline constexpr std::string_view SEMA_DROP_METHOD_SIGNATURE =
    "Drop method signature must be fn drop(self: deinit T) -> void";

inline constexpr std::string_view SEMA_DROP_METHOD_SELF_DEINIT = "Drop method self parameter must be marked deinit";

inline constexpr std::string_view SEMA_DROP_METHOD_SELF_TYPE =
    "Drop method self parameter must be the impl type by value";

inline constexpr std::string_view SEMA_DROP_METHOD_RETURN_VOID = "Drop method must explicitly return void";

inline constexpr std::string_view SEMA_DROP_METHOD_BODY_REQUIRED = "Drop method must have a body";

inline constexpr std::string_view SEMA_DROP_METHOD_BORROW_CONTRACT_UNSUPPORTED =
    "borrow contracts are not supported on Drop methods";

inline constexpr std::string_view SEMA_DROP_ASSOCIATED_TYPE_UNSUPPORTED =
    "associated types are not supported in Drop impls";

inline constexpr std::string_view SEMA_C_ABI_RETURN_TYPE_EXPLICIT = "C ABI function return type must be explicit";

inline constexpr std::string_view SEMA_PROTOTYPE_RETURN_TYPE_EXPLICIT =
    "function prototype return type must be explicit";

inline constexpr std::string_view SEMA_FUNCTION_PARAMETER_STORAGE = "function parameter type is not valid storage";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_COPY_UNSUPPORTED =
    "capturing a non-Copy value in a closure is not supported yet";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_BORROW_UNSUPPORTED =
    "capturing a borrowed-view value in a closure is not supported yet";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_GENERIC_UNSUPPORTED =
    "capturing a generic-dependent value in a closure is not supported yet";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_NOT_LISTED =
    "closure capture must be listed in the capture list";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_UNUSED =
    "closure capture list contains a name that is not captured";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_DUPLICATE =
    "duplicate closure capture name";

inline constexpr std::string_view SEMA_LAMBDA_CAPTURE_MUTABLE_REQUIRES_MUTABLE_SOURCE =
    "mutable closure capture requires a mutable captured variable";

inline constexpr std::string_view SEMA_FUNCTION_TYPE_PARAMETER_STORAGE =
    "function type parameter type is not valid storage";

inline constexpr std::string_view SEMA_FUNCTION_TYPE_RETURN_STORAGE = "function type return type is not valid storage";

inline constexpr std::string_view SEMA_ARRAY_PARAMETER_UNSUPPORTED =
    "array type cannot be used as a function parameter";

inline constexpr std::string_view SEMA_ARRAY_FUNCTION_TYPE_PARAMETER_UNSUPPORTED =
    "array type cannot be used as a function type parameter";

inline constexpr std::string_view SEMA_ARRAY_FUNCTION_TYPE_RETURN_UNSUPPORTED =
    "array type cannot be used as a function type return";

inline constexpr std::string_view SEMA_ARRAY_STRUCT_PARAMETER_UNSUPPORTED =
    "struct containing array cannot be passed by value";

inline constexpr std::string_view SEMA_METHOD_SELF_FIRST = "method self parameter must be first";

inline constexpr std::string_view SEMA_METHOD_SELF_TYPE =
    "method self parameter must use the impl type or a pointer to it";

inline constexpr std::string_view SEMA_ENUM_BASE_INTEGER = "enum base type must be an integer type";

inline constexpr std::string_view SEMA_ENUM_DISCRIMINANT_OUT_OF_RANGE = "enum discriminant literal is out of range";

inline constexpr std::string_view SEMA_ENUM_DISCRIMINANT_DOES_NOT_FIT = "enum discriminant does not fit enum base type";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_STORAGE = "enum payload type is not valid storage";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_ARRAY_UNSUPPORTED = "enum payload cannot contain array storage";

inline constexpr std::string_view SEMA_ORDINARY_MAIN_ABI_NAME = "ordinary fn main cannot use ABI name 'main'";

inline constexpr std::string_view SEMA_MAIN_PARAMETERS =
    "ordinary fn main must use either no parameters or (argc: i32, argv: *mut *mut u8)";

inline constexpr std::string_view SEMA_MAIN_PARAMETERS_EXACT =
    "ordinary fn main parameters must be (argc: i32, argv: *mut *mut u8)";

inline constexpr std::string_view SEMA_MAIN_RETURN = "ordinary fn main must return i32 or void";

inline constexpr std::string_view SEMA_FIELD_STORAGE = "field type is not valid storage";

inline constexpr std::string_view SEMA_DERIVE_TARGET = "derive attributes are only supported on struct and enum declarations";

inline constexpr std::string_view SEMA_DERIVE_UNSUPPORTED = "unsupported derive capability: ";

inline constexpr std::string_view SEMA_DERIVE_DUPLICATE = "duplicate derive capability: ";

inline constexpr std::string_view SEMA_DERIVE_COMPONENT_PREFIX = "cannot derive ";

inline constexpr std::string_view SEMA_DERIVE_TYPE_INFIX = " because the type does not satisfy ";

inline constexpr std::string_view SEMA_DERIVE_FIELD_INFIX = " because field ";

inline constexpr std::string_view SEMA_DERIVE_PAYLOAD_INFIX = " because enum case ";

inline constexpr std::string_view SEMA_DERIVE_COMPONENT_SUFFIX = " does not satisfy ";

[[nodiscard]] inline std::string sema_unsupported_derive_message(const std::string_view name)
{
    return std::string(SEMA_DERIVE_UNSUPPORTED) + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_derive_message(const std::string_view name)
{
    return std::string(SEMA_DERIVE_DUPLICATE) + std::string(name);
}

[[nodiscard]] inline std::string sema_derive_field_capability_message(
    const std::string_view capability, const std::string_view field_name)
{
    return std::string(SEMA_DERIVE_COMPONENT_PREFIX) + std::string(capability)
        + std::string(SEMA_DERIVE_FIELD_INFIX) + std::string(field_name)
        + std::string(SEMA_DERIVE_COMPONENT_SUFFIX) + std::string(capability);
}

[[nodiscard]] inline std::string sema_derive_type_capability_message(const std::string_view capability)
{
    return std::string(SEMA_DERIVE_COMPONENT_PREFIX) + std::string(capability)
        + std::string(SEMA_DERIVE_TYPE_INFIX) + std::string(capability);
}

[[nodiscard]] inline std::string sema_derive_enum_payload_capability_message(
    const std::string_view capability, const std::string_view case_name)
{
    return std::string(SEMA_DERIVE_COMPONENT_PREFIX) + std::string(capability)
        + std::string(SEMA_DERIVE_PAYLOAD_INFIX) + std::string(case_name)
        + std::string(SEMA_DERIVE_COMPONENT_SUFFIX) + std::string(capability);
}

inline constexpr std::string_view SEMA_CONST_NOT_COMPILE_TIME = "const initializer is not compile-time constant";

inline constexpr std::string_view SEMA_CONST_TYPE_STORAGE = "const type is not valid storage";

inline constexpr std::string_view SEMA_CONST_TYPE_MISMATCH = "const initializer type does not match declared type";

inline constexpr std::string_view SEMA_FUNCTION_NAME_VALUE = "function name cannot be used as a value: ";

inline constexpr std::string_view SEMA_FOR_CONDITION_BOOL = "for condition must be bool";

inline constexpr std::string_view SEMA_FOR_RANGE_ARITY = "range expects 1 to 3 arguments";

inline constexpr std::string_view SEMA_RANGE_BOUNDS_INTEGER = "range bounds must be integer";

inline constexpr std::string_view SEMA_RANGE_STEP_INTEGER = "range step must be integer";

inline constexpr std::string_view SEMA_RANGE_BOUNDS_SAME_TYPE = "range bounds must have the same type";

inline constexpr std::string_view SEMA_RANGE_STEP_SAME_TYPE = "range step must have the same type as bounds";

inline constexpr std::string_view SEMA_LOCAL_TYPE_INFER = "local variable type cannot be inferred";

inline constexpr std::string_view SEMA_LOCAL_STORAGE = "local variable type is not valid storage";

inline constexpr std::string_view SEMA_INITIALIZER_TYPE_MISMATCH = "initializer type does not match declared type";

inline constexpr std::string_view SEMA_ASSIGNMENT_LHS_WRITABLE = "left side of assignment must be writable";

inline constexpr std::string_view SEMA_COMPOUND_ASSIGNMENT_TYPE_MISMATCH = "compound assignment type mismatch";

inline constexpr std::string_view SEMA_ASSIGNMENT_TYPE_MISMATCH = "assignment type mismatch";

inline constexpr std::string_view SEMA_ARRAY_ASSIGNMENT_UNSUPPORTED =
    "array or array-containing type cannot be assigned";

inline constexpr std::string_view SEMA_RESOURCE_PLACE_ASSIGNMENT_UNSUPPORTED =
    "resource field, index, or dereference assignment is not supported yet";

inline constexpr std::string_view SEMA_IF_CONDITION_BOOL = "if condition must be bool";

inline constexpr std::string_view SEMA_WHILE_CONDITION_BOOL = "while condition must be bool";

inline constexpr std::string_view SEMA_RETURN_TYPE_MISMATCH = "return type mismatch";

inline constexpr std::string_view SEMA_EXPR_STMT_CALL_OR_TRY =
    "expression statement must be a function call or try expression";

inline constexpr std::string_view SEMA_BREAK_CONTINUE_IN_LOOP = "break and continue are only valid inside loops";

inline constexpr std::string_view SEMA_DEFER_CALL = "defer statement must be a function call";

inline constexpr std::string_view SEMA_DEFER_EARLY_EXIT = "defer statement cannot contain try expression";

inline constexpr std::string_view SEMA_RETURN_TYPE_INFER = "function return type cannot be inferred";

inline constexpr std::string_view SEMA_INFERRED_RETURN_TYPE_MISMATCH = "inferred function return types do not match";

inline constexpr std::string_view SEMA_ARRAY_RETURN_UNSUPPORTED = "array type cannot be used as a function return type";

inline constexpr std::string_view SEMA_ARRAY_STRUCT_RETURN_UNSUPPORTED =
    "struct containing array cannot be returned by value";

inline constexpr std::string_view SEMA_RECURSIVE_RETURN_INFER =
    "cannot infer recursive function return type without an explicit return type";

inline constexpr std::string_view SEMA_NOT_ALL_PATHS_RETURN = "not all control paths return a value";

inline constexpr std::string_view SEMA_BORROWED_LOCAL_ESCAPE = "borrowed local storage cannot escape the function";

inline constexpr std::string_view SEMA_BORROWED_LOCAL_ORIGIN = "borrowed local storage originates here";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_REDUNDANT =
    "borrow contract requires a return type that can contain a borrow";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_UNKNOWN_SELECTOR =
    "borrow contract return selector does not name a parameter";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_SELF_SELECTOR =
    "borrow contract 'self' selector requires a first self parameter";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_NON_BORROWING_SELECTOR =
    "borrow contract return selector must name a parameter that can carry a borrow";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_DUPLICATE_SELECTOR = "duplicate borrow contract return selector";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_MISMATCH =
    "function body returns a borrow source outside the declared borrow contract";

inline constexpr std::string_view SEMA_BORROW_CONTRACT_DECLARED_HERE = "borrow contract is declared here";

inline constexpr std::string_view SEMA_LIFETIME_UNKNOWN_ORIGIN = "lifetime origin is not declared in this scope";

inline constexpr std::string_view SEMA_LIFETIME_ORIGIN_DECLARED_HERE = "lifetime origin is declared here";

inline constexpr std::string_view SEMA_LIFETIME_ELISION_AMBIGUOUS =
    "borrowed return lifetime is ambiguous; declare @borrow(...) or an explicit return origin";

inline constexpr std::string_view SEMA_LIFETIME_RETURN_OUTSIDE_TYPE =
    "returned borrow source is outside the explicit return origin set";

inline constexpr std::string_view SEMA_LIFETIME_TYPE_OUTLIVES = "borrowed type does not outlive the required origin";

inline constexpr std::string_view SEMA_LIFETIME_CONSTRAINT_NOTE = "lifetime requirement is introduced here";

inline constexpr std::string_view SEMA_ACTIVE_BORROW_CONFLICT =
    "borrowed storage cannot be mutated, moved, or exclusively reborrowed while a loan is active";

inline constexpr std::string_view SEMA_ACTIVE_BORROW_CREATED = "loan is created here";

inline constexpr std::string_view SEMA_ACTIVE_BORROW_INVALIDATING_ACTION = "conflicting access invalidates the loan";

inline constexpr std::string_view SEMA_ACTIVE_BORROW_LATER_CARRIER_USE = "borrow carrier is used later here";

inline constexpr std::string_view SEMA_REBORROW_PARENT_USE_CONFLICT =
    "borrow parent cannot be used while a reborrow is active";

inline constexpr std::string_view SEMA_REBORROW_CHILD_CREATED = "reborrow is created here";

inline constexpr std::string_view SEMA_TWO_PHASE_RECEIVER_CONFLICT =
    "mutable method receiver reservation cannot be invalidated before call activation";

inline constexpr std::string_view SEMA_TWO_PHASE_RECEIVER_RESERVED = "mutable receiver borrow is reserved here";

inline constexpr std::string_view SEMA_TWO_PHASE_RECEIVER_ACTIVATED = "mutable receiver borrow is activated here";

inline constexpr std::string_view SEMA_PLACE_STATE_USE_AFTER_MOVE =
    "place cannot be used after it was moved or dropped";

inline constexpr std::string_view SEMA_PLACE_STATE_MAYBE_UNINITIALIZED_USE =
    "place may be uninitialized on this control-flow path";

inline constexpr std::string_view SEMA_PLACE_STATE_USE_AFTER_PARTIAL_MOVE =
    "whole value cannot be used while one of its tracked parts may be moved";

inline constexpr std::string_view SEMA_PLACE_STATE_DROP_AFTER_MOVE =
    "moved storage cannot be explicitly dropped again";

inline constexpr std::string_view SEMA_PLACE_STATE_DROP_AFTER_PARTIAL_MOVE =
    "whole value cannot be explicitly dropped while one of its tracked parts may be moved";

inline constexpr std::string_view SEMA_PLACE_STATE_DOUBLE_DROP =
    "storage cannot be explicitly dropped twice";

inline constexpr std::string_view SEMA_PLACE_STATE_MOVE_OCCURRED = "tracked move occurs here";

inline constexpr std::string_view SEMA_MOVE_PARTIAL_FIELD_UNSUPPORTED =
    "moving a field out of a move-only value is not supported yet";

inline constexpr std::string_view SEMA_MOVE_INDEXED_ELEMENT_UNSUPPORTED =
    "moving an indexed element out of a move-only value is not supported yet";

inline constexpr std::string_view SEMA_MOVE_PATTERN_PAYLOAD_UNSUPPORTED =
    "consuming pattern payloads are not supported yet";

inline constexpr std::string_view SEMA_MOVE_TRY_PAYLOAD_UNSUPPORTED =
    "try expression transfer of a non-Copy payload is not supported yet";

[[nodiscard]] inline std::string sema_use_of_moved_value_message(const std::string_view name)
{
    return "use of moved value `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_use_of_possibly_moved_value_message(const std::string_view name)
{
    return "use of possibly moved value `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_value_consumed_here_message(const std::string_view name)
{
    return "value `" + std::string(name) + "` was consumed here";
}

inline constexpr std::string_view SEMA_EXPLICIT_GENERIC_CALL_SYNTAX =
    "explicit generic calls use '<...>', for example id<i32>(...)";

inline constexpr std::string_view SEMA_CALLEE_FUNCTION_NAME =
    "callee must be a function value; explicit generic calls use '<...>', for example id<i32>(...)";

inline constexpr std::string_view SEMA_DYNPROJECT_TYPE_ARGUMENT_COUNT =
    "dynproject requires exactly two type arguments";

inline constexpr std::string_view SEMA_DYNPROJECT_ARGUMENT_COUNT =
    "dynproject requires exactly one argument";

inline constexpr std::string_view SEMA_DYNPROJECT_SOURCE_PRINCIPAL =
    "dynproject source must be a dyn trait principal";

inline constexpr std::string_view SEMA_DYNPROJECT_TARGET_SUPERTRAIT =
    "dynproject target must be a dyn trait supertrait";

inline constexpr std::string_view SEMA_DYNPROJECT_ARGUMENT_COMPOSITION =
    "dynproject argument must be a borrowed dyn trait composition";

inline constexpr std::string_view SEMA_DYNPROJECT_SOURCE_NOT_IN_COMPOSITION =
    "dynproject source principal is not in the composition";

inline constexpr std::string_view SEMA_DYNPROJECT_TARGET_NOT_SUPERTRAIT =
    "dynproject target is not a supertrait of the selected source principal";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_ARGUMENT_TYPE_MISMATCH =
    "enum payload constructor argument type mismatch";

inline constexpr std::string_view SEMA_ENUM_PAYLOAD_ARRAY_ARGUMENT_UNSUPPORTED =
    "array-containing type cannot be used as enum payload";

inline constexpr std::string_view SEMA_ARGUMENT_ARRAY_UNSUPPORTED = "array-containing type cannot be passed by value";

inline constexpr std::string_view SEMA_ARRAY_INDEX_INTEGER = "array index must be an integer";

inline constexpr std::string_view SEMA_ARRAY_INDEX_OUT_OF_BOUNDS = "array constant index is out of bounds";

inline constexpr std::string_view SEMA_SLICE_BOUND_INTEGER = "slice bound must be an integer";

inline constexpr std::string_view SEMA_ARRAY_SLICE_BOUND_OUT_OF_BOUNDS = "array constant slice bound is out of bounds";

inline constexpr std::string_view SEMA_ARRAY_SLICE_BOUNDS_ORDER = "array constant slice start exceeds end";

inline constexpr std::string_view SEMA_LOGICAL_NOT_BOOL = "logical not requires bool operand";

inline constexpr std::string_view SEMA_NUMERIC_UNARY_OPERAND =
    "numeric unary operator requires signed integer or float operand";

inline constexpr std::string_view SEMA_BITWISE_NOT_INTEGER = "bitwise not requires integer operand";

inline constexpr std::string_view SEMA_DEREF_POINTER = "dereference requires pointer or reference operand";

inline constexpr std::string_view SEMA_DEREF_STORAGE = "dereference requires pointer or reference to valid storage";

inline constexpr std::string_view SEMA_UNSAFE_DEREF = "raw pointer dereference requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_RAW_POINTER_PROJECTION = "raw pointer projection requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_PTRCAST = "ptrcast requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_BITCAST = "bitcast requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_PTRAT = "ptrat requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_STRRAW = "strraw requires unsafe context";

inline constexpr std::string_view SEMA_UNSAFE_BLOCK_CONST_INITIALIZER =
    "unsafe block cannot be used in const initializer";

inline constexpr std::string_view SEMA_ADDRESS_OF_PLACE = "address-of requires a place expression";

inline constexpr std::string_view SEMA_MUTABLE_REFERENCE_PLACE =
    "mutable reference requires a writable place expression";

inline constexpr std::string_view SEMA_REFERENCE_STORAGE = "reference requires a valid storage type";
inline constexpr std::string_view SEMA_DYN_TRAIT_STORAGE =
    "bare dyn trait type is not valid storage; use &dyn Trait or &mut dyn Trait";

inline constexpr std::string_view SEMA_BINARY_OPERANDS_SAME_TYPE = "binary operands must have the same type";

inline constexpr std::string_view SEMA_SHIFT_NEGATIVE = "shift amount cannot be negative";

inline constexpr std::string_view SEMA_SHIFT_OUT_OF_RANGE = "shift amount is out of range";

inline constexpr std::string_view SEMA_COMPARISON_NUMERIC = "comparison operator requires numeric operands";

inline constexpr std::string_view SEMA_EQUALITY_SCALAR = "equality operator requires scalar operands";

inline constexpr std::string_view SEMA_LOGICAL_BOOL = "logical operator requires bool operands";

inline constexpr std::string_view SEMA_INTEGER_OPERATOR_INTEGER = "integer operator requires integer operands";

inline constexpr std::string_view SEMA_BINARY_NUMERIC = "binary operator requires numeric operands";

inline constexpr std::string_view SEMA_FIELD_STRUCT_VALUE = "field access requires a non-opaque struct value";

inline constexpr std::string_view SEMA_INDEX_POINTER_ARRAY_DEREF =
    "indexing pointer to array requires explicit dereference";

inline constexpr std::string_view SEMA_INDEX_POINTER_STORAGE = "indexing pointer requires pointer to valid storage";

inline constexpr std::string_view SEMA_INDEX_ARRAY_OR_POINTER = "indexing requires array, slice, or pointer value";

inline constexpr std::string_view SEMA_SLICE_ARRAY_OR_SLICE = "slicing requires array, slice, or str value";

inline constexpr std::string_view SEMA_SLICE_ELEMENT_STORAGE = "slice element type is not valid storage";

inline constexpr std::string_view SEMA_ARRAY_LITERAL_EXPECTED_TYPE = "array literal requires an array expected type";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_INTEGER = "array repeat count must be an integer literal";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_OUT_OF_RANGE = "array repeat count literal is out of range";

inline constexpr std::string_view SEMA_EMPTY_ARRAY_CONTEXT = "empty array literal requires an array type context";

inline constexpr std::string_view SEMA_ARRAY_ELEMENT_INFER = "array literal element type cannot be inferred";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_TYPE_MISMATCH = "array repeat value type mismatch";

inline constexpr std::string_view SEMA_ARRAY_REPEAT_COPY_REQUIRED =
    "array repeat value must be Copy when repeated more than once";

inline constexpr std::string_view SEMA_ARRAY_LITERAL_STORAGE = "array literal type is not valid storage";

inline constexpr std::string_view SEMA_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH = "array literal element type mismatch";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_EXPECTED_TYPE = "tuple literal requires a tuple expected type";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_ARITY = "tuple literal arity does not match expected tuple type";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_ELEMENT_TYPE_MISMATCH = "tuple literal element type mismatch";

inline constexpr std::string_view SEMA_TUPLE_LITERAL_STORAGE = "tuple literal type is not valid storage";

inline constexpr std::string_view SEMA_TUPLE_FIELD_ACCESS_NUMERIC = "tuple field access requires a numeric field";
inline constexpr std::string_view SEMA_TUPLE_FIELD_ACCESS_OUT_OF_RANGE = "tuple field index is out of range";

inline constexpr std::string_view SEMA_TUPLE_DESTRUCTURE_TYPE = "tuple destructuring requires a tuple value";

inline constexpr std::string_view SEMA_TUPLE_DESTRUCTURE_ARITY =
    "tuple destructuring pattern arity does not match tuple type";

inline constexpr std::string_view SEMA_LOCAL_PATTERN_REFUTABLE = "local destructuring pattern must be irrefutable";

inline constexpr std::string_view SEMA_LET_ELSE_PATTERN = "let-else requires a pattern";

inline constexpr std::string_view SEMA_LET_ELSE_FALLTHROUGH = "let-else else block must not fall through";

inline constexpr std::string_view SEMA_STRUCT_LITERAL_TYPE = "struct literal requires a non-opaque struct type";

inline constexpr std::string_view SEMA_STRUCT_LITERAL_FIELD_TYPE_MISMATCH = "struct literal field type mismatch";

inline constexpr std::string_view SEMA_INVALID_CONVERSION = "invalid explicit conversion";

inline constexpr std::string_view SEMA_GENERIC_SIZEOF_ALIGNOF =
    "generic type parameter cannot be queried by sizeof or alignof";

inline constexpr std::string_view SEMA_OPAQUE_SIZEOF_ALIGNOF =
    "opaque struct cannot be queried by sizeof or alignof directly";

inline constexpr std::string_view SEMA_SIZEOF_ALIGNOF_STORAGE = "sizeof and alignof require a valid storage type";

inline constexpr std::string_view SEMA_PTRADDR_POINTER = "ptraddr requires a pointer value";

inline constexpr std::string_view SEMA_PTRAT_POINTER = "ptrat target type must be a pointer";

inline constexpr std::string_view SEMA_PTRAT_INTEGER = "ptrat address must be an integer";

inline constexpr std::string_view SEMA_SLICE_PTR_SLICE = "slice.ptr requires a slice value";

inline constexpr std::string_view SEMA_SLICE_LEN_SLICE = "slice.len requires a slice value";

inline constexpr std::string_view SEMA_STR_PTR_STR = "str.ptr requires a str value";

inline constexpr std::string_view SEMA_STR_LEN_STR = "str.len requires a str value";

inline constexpr std::string_view SEMA_STR_UTF8_SLICE =
    "str UTF-8 builtin requires a []u8 or []mut u8 byte slice";

inline constexpr std::string_view SEMA_STRRAW_ARITY = "strraw requires data and length arguments";

inline constexpr std::string_view SEMA_STRRAW_DATA_POINTER = "strraw data must be *const u8";

inline constexpr std::string_view SEMA_STRRAW_LENGTH_INTEGER = "strraw length must be an integer";

inline constexpr std::string_view SEMA_TRY_CONST_INITIALIZER = "try expression cannot be used in const initializer";

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

inline constexpr std::string_view SEMA_IF_EXPR_CONST_INITIALIZER = "if expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_IF_EXPR_CONDITION_BOOL = "if expression condition must be bool";

inline constexpr std::string_view SEMA_IF_EXPR_BRANCH_TYPE = "if expression branches must have the same type";

inline constexpr std::string_view SEMA_IF_EXPR_VOID = "if expression branches cannot be void";

inline constexpr std::string_view SEMA_BLOCK_EXPR_CONST_INITIALIZER =
    "block expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_BLOCK_EXPR_FINAL = "block expression requires a final expression";

inline constexpr std::string_view SEMA_BLOCK_EXPR_UNREACHABLE = "block expression final expression is unreachable";

inline constexpr std::string_view SEMA_BLOCK_EXPR_VOID = "block expression result cannot be void";

inline constexpr std::string_view SEMA_INTEGER_DIVISION_BY_ZERO = "integer division by zero";

inline constexpr std::string_view SEMA_INTEGER_MODULO_BY_ZERO = "integer modulo by zero";

inline constexpr std::string_view SEMA_SIGNED_DIVISION_OVERFLOW = "signed integer division overflows";

inline constexpr std::string_view SEMA_SIGNED_MODULO_OVERFLOW = "signed integer modulo overflows";

inline constexpr std::string_view SEMA_MATCH_CONST_INITIALIZER = "match expression cannot be used in const initializer";

inline constexpr std::string_view SEMA_MATCH_VALUE_TYPE =
    "match expression requires an enum, integer, bool, tuple, struct, array, or slice value";

inline constexpr std::string_view SEMA_MATCH_ARM_REQUIRED = "match expression requires at least one arm";

inline constexpr std::string_view SEMA_MATCH_ARM_TYPE = "match expression arms must have the same type";

inline constexpr std::string_view SEMA_MATCH_GUARD_BOOL = "match guard must be bool";

inline constexpr std::string_view SEMA_MATCH_PAYLOAD_CASE = "match arm payload binding requires a payload enum case";

inline constexpr std::string_view SEMA_MATCH_INTEGER_BOOL_WILDCARD =
    "match expression over integer or bool requires a wildcard arm";

inline constexpr std::string_view SEMA_MATCH_OPEN_INTEGER_WILDCARD =
    "match expression over open integer domain requires a wildcard arm";

inline constexpr std::string_view SEMA_MATCH_RESULT_VOID = "match expression result cannot be void";

inline constexpr std::string_view SEMA_OR_PATTERN_BINDING_NAMES = "or-pattern alternatives must bind the same names";

inline constexpr std::string_view SEMA_OR_PATTERN_BINDING_TYPES =
    "or-pattern binding types must match across alternatives";

inline constexpr std::string_view SEMA_MATCH_NON_ENUM_IRREFUTABLE =
    "match expression over tuple, struct, array, or slice requires an irrefutable arm";

inline constexpr std::string_view SEMA_MATCH_DYNAMIC_SLICE_WITNESS =
    "match expression over dynamic slice is missing length or element coverage";

inline constexpr std::string_view SEMA_MATCH_LARGE_ARRAY_IRREFUTABLE =
    "fixed-array match exhaustiveness for arrays longer than 4096 elements requires an irrefutable arm";

inline constexpr std::string_view SEMA_MATCH_ARM_UNREACHABLE = "match arm is unreachable";

inline constexpr std::string_view SEMA_MATCH_WILDCARD_UNREACHABLE = SEMA_MATCH_ARM_UNREACHABLE;

inline constexpr std::string_view SEMA_ENUM_MATCH_PATTERN = "enum match pattern must be an enum case or wildcard";

inline constexpr std::string_view SEMA_ENUM_PATTERN_TYPE = "enum pattern requires an enum value";

inline constexpr std::string_view SEMA_MATCH_CASE_WRONG_ENUM = "match arm case does not belong to matched enum";

inline constexpr std::string_view SEMA_INTEGER_BOOL_PATTERN =
    "match pattern for integer or bool value must be a literal or wildcard";

inline constexpr std::string_view SEMA_BOOL_PATTERN = "bool match pattern must be true or false";

inline constexpr std::string_view SEMA_INTEGER_PATTERN = "integer match pattern must be an integer literal";

inline constexpr std::string_view SEMA_INTEGER_PATTERN_RANGE =
    "integer match pattern literal is out of range for matched type";

inline constexpr std::string_view SEMA_UNSUPPORTED_LITERAL_PATTERN = "unsupported literal match pattern";

inline constexpr std::string_view SEMA_STRUCT_PATTERN_TYPE = "struct pattern requires a struct value";

inline constexpr std::string_view SEMA_STRUCT_PATTERN_FIELD = "unknown struct pattern field";

inline constexpr std::string_view SEMA_STRUCT_PATTERN_DUPLICATE_FIELD = "duplicate struct pattern field";

inline constexpr std::string_view SEMA_SLICE_PATTERN_TYPE = "slice pattern requires an array or slice value";

inline constexpr std::string_view SEMA_SLICE_PATTERN_LENGTH = "slice pattern length does not match array length";

inline constexpr std::string_view SEMA_OPAQUE_POINTER_ONLY = "opaque struct can only be used as a pointer target";

inline constexpr std::string_view SEMA_ARRAY_STORAGE_OVERFLOW = "array storage size overflows ABI size";

inline constexpr std::string_view SEMA_STRUCT_STORAGE_OVERFLOW = "struct storage size overflows ABI size";

inline constexpr std::string_view SEMA_ENUM_STORAGE_OVERFLOW = "enum storage size overflows ABI size";

[[nodiscard]] inline std::string sema_duplicate_generic_parameter_message(const std::string_view name)
{
    return "duplicate generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_first_generic_parameter_message(const std::string_view name)
{
    return "first declaration of generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_did_you_mean_message(const std::string_view name)
{
    return "did you mean `" + std::string(name) + "`?";
}

[[nodiscard]] inline std::string sema_expected_type_note_message(const std::string_view name)
{
    return "expected type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_actual_type_note_message(const std::string_view name)
{
    return "actual type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_previous_declaration_note_message(const std::string_view name)
{
    return "previous declaration of `" + std::string(name) + "` is here";
}

[[nodiscard]] inline std::string sema_duplicate_type_definition_message(
    const std::string_view module, const std::string_view name)
{
    return "duplicate type definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_trait_definition_message(
    const std::string_view module, const std::string_view name)
{
    return "duplicate trait definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_struct_definition_message(
    const std::string_view module, const std::string_view name)
{
    return "duplicate struct definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_function_definition_message(
    const std::string_view module, const std::string_view name)
{
    return "duplicate function definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_signature_mismatch_message(const std::string_view name)
{
    return "function prototype and definition signatures do not match: " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_declaration_conflict_message(const std::string_view name)
{
    return "function declaration conflicts with existing function: " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_function_prototype_message(const std::string_view name)
{
    return "duplicate function prototype: " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_prototype_order_message(const std::string_view name)
{
    return "function prototype must appear before definition: " + std::string(name);
}

[[nodiscard]] inline std::string sema_function_prototype_missing_definition_message(const std::string_view name)
{
    return "function prototype has no definition: " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_function_definition_simple_message(const std::string_view name)
{
    return "duplicate function definition: " + std::string(name);
}

[[nodiscard]] inline std::string sema_extern_c_abi_conflict_message(const std::string_view symbol)
{
    return "extern C ABI symbol redeclared with incompatible signature: " + std::string(symbol);
}

[[nodiscard]] inline std::string sema_duplicate_abi_symbol_message(const std::string_view symbol)
{
    return "duplicate ABI symbol: " + std::string(symbol);
}

inline constexpr std::string_view SEMA_ORDINARY_MAIN_EXPORTED_C_MAIN =
    "ordinary fn main cannot be combined with an exported C main entry";

[[nodiscard]] inline std::string sema_duplicate_value_definition_message(
    const std::string_view module, const std::string_view name)
{
    return "duplicate value definition in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_value_definition_in_module_message(const std::string_view name)
{
    return "duplicate value definition in module: " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_namespace_member_message(
    const std::string_view module, const std::string_view name)
{
    return "duplicate module member across namespaces in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_enum_case_message(
    const std::string_view enum_name, const std::string_view case_name)
{
    return "duplicate enum case: " + std::string(enum_name) + "." + std::string(case_name);
}

[[nodiscard]] inline std::string sema_duplicate_type_member_message(
    const std::string_view type_name, const std::string_view name)
{
    return "duplicate type member: " + std::string(type_name) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_enum_discriminant_message(const std::string_view enum_name)
{
    return "duplicate enum discriminant value in " + std::string(enum_name);
}

[[nodiscard]] inline std::string sema_duplicate_struct_field_message(const std::string_view field_name)
{
    return "duplicate struct field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_function_name_value_message(const std::string_view name)
{
    return std::string(SEMA_FUNCTION_NAME_VALUE) + std::string(name);
}

[[nodiscard]] inline std::string sema_argument_count_message(const std::string_view function_name)
{
    return "argument count mismatch in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_unknown_named_argument_message(
    const std::string_view function_name, const std::string_view argument_name)
{
    return "unknown named argument `" + std::string(argument_name) + "` in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_duplicate_named_argument_message(const std::string_view argument_name)
{
    return "duplicate named argument `" + std::string(argument_name) + "`";
}

[[nodiscard]] inline std::string sema_missing_required_argument_message(
    const std::string_view function_name, const std::string_view argument_name)
{
    return "missing required argument `" + std::string(argument_name) + "` in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_argument_type_message(const std::string_view function_name)
{
    return "argument type mismatch in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_variadic_argument_type_infer_message(const std::string_view function_name)
{
    return "variadic argument type cannot be inferred in call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_function_not_generic_message(const std::string_view function_name)
{
    return "function " + std::string(function_name) + " is not generic";
}

[[nodiscard]] inline std::string sema_unsafe_function_call_message(const std::string_view function_name)
{
    return "call to unsafe function " + std::string(function_name) + " requires unsafe context";
}

[[nodiscard]] inline std::string sema_method_requires_receiver_message(
    const std::string_view owner, const std::string_view method)
{
    return "method requires a receiver: " + std::string(owner) + "." + std::string(method);
}

[[nodiscard]] inline std::string sema_method_not_generic_message(
    const std::string_view owner, const std::string_view method)
{
    return "method " + std::string(owner) + "." + std::string(method) + " is not generic";
}

[[nodiscard]] inline std::string sema_unknown_field_message(const std::string_view field_name)
{
    return "unknown field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_private_field_message(const std::string_view field_name)
{
    return "field is private: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_duplicate_struct_literal_field_message(const std::string_view field_name)
{
    return "duplicate struct literal field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_unknown_struct_literal_field_message(const std::string_view field_name)
{
    return "unknown field in struct literal: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_struct_literal_missing_field_message(const std::string_view field_name)
{
    return "struct literal is missing field: " + std::string(field_name);
}

[[nodiscard]] inline std::string sema_type_not_generic_message(const std::string_view name)
{
    return "type " + std::string(name) + " is not generic";
}

[[nodiscard]] inline std::string sema_trait_not_generic_message(const std::string_view name)
{
    return "trait " + std::string(name) + " is not generic";
}

[[nodiscard]] inline std::string sema_generic_type_requires_args_message(const std::string_view name)
{
    return "generic type " + std::string(name) + " requires type arguments";
}

[[nodiscard]] inline std::string sema_generic_param_type_args_message(const std::string_view name)
{
    return "generic type parameter cannot take type arguments: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_generic_constraint_param_message(const std::string_view name)
{
    return "where constraint references unknown generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_unknown_capability_message(const std::string_view name)
{
    return "unknown generic capability or trait predicate `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_duplicate_capability_message(
    const std::string_view param, const std::string_view capability)
{
    return "duplicate capability `" + std::string(capability) + "` for generic parameter `" + std::string(param) + "`";
}

[[nodiscard]] inline std::string sema_generic_capability_not_satisfied_message(
    const std::string_view type_name, const std::string_view capability)
{
    return "type " + std::string(type_name) + " does not satisfy capability `" + std::string(capability) + "`";
}

[[nodiscard]] inline std::string sema_const_generic_param_type_message(
    const std::string_view param, const std::string_view type_name)
{
    return "const generic parameter `" + std::string(param)
        + "` must use an integer, bool, or char type, got " + std::string(type_name);
}

[[nodiscard]] inline std::string sema_const_generic_argument_expected_message(const std::string_view param)
{
    return "generic parameter `" + std::string(param) + "` expects a const argument";
}

[[nodiscard]] inline std::string sema_type_generic_argument_expected_message(const std::string_view param)
{
    return "generic parameter `" + std::string(param) + "` expects a type argument";
}

inline constexpr std::string_view SEMA_CONST_GENERIC_ARGUMENT_UNSUPPORTED =
    "const generic argument must be a scalar literal or const generic parameter name";

inline constexpr std::string_view SEMA_CONST_GENERIC_ARGUMENT_TYPE_MISMATCH =
    "const generic argument type mismatch";

inline constexpr std::string_view SEMA_CONST_GENERIC_ARITHMETIC_UNSUPPORTED =
    "generic const expressions are not supported yet";

[[nodiscard]] inline std::string sema_unknown_const_generic_param_message(const std::string_view name)
{
    return "unknown const generic parameter `" + std::string(name) + "`";
}

[[nodiscard]] inline std::string sema_cyclic_type_alias_message(const std::string_view name)
{
    return "cyclic type alias: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_enum_case_message(const std::string_view name)
{
    return "unknown enum case: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_scoped_enum_case_message(
    const std::string_view enum_name, const std::string_view case_name)
{
    return "unknown enum case: " + std::string(enum_name) + "." + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_call_message(const std::string_view case_name)
{
    return "enum payload constructor requires a call: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_case_message(const std::string_view case_name)
{
    return "enum case constructor requires a payload case: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_arity_message(const std::string_view case_name)
{
    return "enum payload constructor requires exactly one argument: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_constructor_argument_count_message(
    const std::string_view case_name, const base::usize count)
{
    return "enum payload constructor requires " + std::to_string(count) + " arguments: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_match_missing_enum_case_message(const std::string_view case_name)
{
    return "match expression is not exhaustive for enum case: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_unknown_matched_enum_case_message(const std::string_view case_name)
{
    return "unknown enum case for matched enum: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_duplicate_match_enum_case_message(const std::string_view case_name)
{
    return "duplicate match arm for enum case: " + std::string(case_name);
}

[[nodiscard]] inline std::string sema_enum_payload_pattern_binding_count_message(const base::usize count)
{
    return "enum payload pattern requires " + std::to_string(count) + " binding" + (count == 1 ? "" : "s");
}

[[nodiscard]] inline std::string sema_integer_literal_out_of_range_message(const std::string_view type_name)
{
    return "integer literal out of range for " + std::string(type_name);
}

[[nodiscard]] inline std::string sema_invalid_integer_literal_suffix_message(const std::string_view suffix)
{
    return "invalid integer literal suffix `" + std::string(suffix) + "`";
}

[[nodiscard]] inline std::string sema_integer_literal_suffix_type_mismatch_message(
    const std::string_view suffix_type, const std::string_view expected_type)
{
    return "integer literal suffix type " + std::string(suffix_type) + " does not match expected "
        + std::string(expected_type);
}

[[nodiscard]] inline std::string sema_generic_comparison_operator_message(const std::string_view type_name)
{
    return "generic type parameter `" + std::string(type_name) + "` has no known comparison operator";
}

[[nodiscard]] inline std::string sema_generic_equality_operator_message(const std::string_view type_name)
{
    return "generic type parameter `" + std::string(type_name) + "` has no known operator `==`";
}

[[nodiscard]] inline std::string sema_generic_integer_operator_message(const std::string_view type_name)
{
    return "generic type parameter `" + std::string(type_name) + "` has no known integer operator";
}

[[nodiscard]] inline std::string sema_generic_operator_message(const std::string_view type_name)
{
    return "generic type parameter `" + std::string(type_name) + "` has no known operator";
}

[[nodiscard]] inline std::string sema_array_literal_length_mismatch_message(
    const base::u64 expected, const base::u64 actual)
{
    return "array literal length mismatch: expected " + std::to_string(expected) + ", got " + std::to_string(actual);
}

[[nodiscard]] inline std::string sema_cyclic_const_initializer_message(const std::string_view name)
{
    return "cyclic const initializer: " + std::string(name);
}

[[nodiscard]] inline std::string sema_generic_argument_count_message(
    const std::string_view subject, const std::string_view name, const base::usize actual, const base::usize expected)
{
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
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous generic type name '" + std::string(name) + "' from modules " + std::string(first_module) + " and "
        + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_generic_type_message(const std::string_view name)
{
    return "unknown generic type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_generic_type_in_module_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown generic type in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_generic_type_message(
    const std::string_view module, const std::string_view name)
{
    return "generic type is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_ambiguous_generic_function_name_message(
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous generic function name '" + std::string(name) + "' from modules " + std::string(first_module)
        + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_generic_function_message(const std::string_view name)
{
    return "unknown generic function: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_generic_function_in_module_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown generic function in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_generic_function_message(
    const std::string_view module, const std::string_view name)
{
    return "generic function is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_generic_call_argument_infer_message(
    const std::string_view parameter, const std::string_view function_name)
{
    return "cannot infer generic type argument `" + std::string(parameter) + "` for call to "
        + std::string(function_name);
}

[[nodiscard]] inline std::string sema_generic_call_argument_unify_message(const std::string_view function_name)
{
    return "cannot infer generic type argument for call to " + std::string(function_name);
}

[[nodiscard]] inline std::string sema_float_literal_out_of_range_message(const std::string_view type_name)
{
    return "float literal out of range for " + std::string(type_name);
}

[[nodiscard]] inline std::string sema_invalid_float_literal_suffix_message(const std::string_view suffix)
{
    return "invalid float literal suffix `" + std::string(suffix) + "`";
}

[[nodiscard]] inline std::string sema_float_literal_suffix_type_mismatch_message(
    const std::string_view suffix_type, const std::string_view expected_type)
{
    return "float literal suffix type " + std::string(suffix_type) + " does not match expected "
        + std::string(expected_type);
}

[[nodiscard]] inline std::string sema_unknown_import_alias_message(const std::string_view alias)
{
    return "unknown import alias: " + std::string(alias);
}

inline constexpr std::string_view SEMA_IMPORTS_ARE_PART_LOCAL_HELP =
    "imports are part-local; add this import to the current module part";

[[nodiscard]] inline std::string sema_ambiguous_import_alias_message(const std::string_view alias)
{
    return "ambiguous import alias: " + std::string(alias);
}

[[nodiscard]] inline std::string sema_unknown_reexport_target_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown selective re-export target in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_reexport_target_message(
    const std::string_view module, const std::string_view name)
{
    return "selective re-export target is not visible enough in module " + std::string(module) + ": "
        + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_module_path_message(const std::string_view path)
{
    return "unknown module path: " + std::string(path);
}

[[nodiscard]] inline std::string sema_local_shadows_import_alias_message(const std::string_view name)
{
    return "local name shadows import alias: " + std::string(name);
}

[[nodiscard]] inline std::string sema_local_shadows_root_module_message(const std::string_view name)
{
    return "local name shadows visible root module: " + std::string(name);
}

[[nodiscard]] inline std::string sema_local_shadows_generic_type_parameter_message(const std::string_view name)
{
    return "local name shadows generic type parameter: " + std::string(name);
}

[[nodiscard]] inline std::string sema_local_shadows_type_name_message(const std::string_view name)
{
    return "local name shadows visible type: " + std::string(name);
}

inline constexpr std::string_view SEMA_MUTABLE_METHOD_RECEIVER_POINTER =
    "mutable method receiver requires mutable pointer";

inline constexpr std::string_view SEMA_METHOD_RECEIVER_TYPE_MISMATCH = "method receiver type mismatch";

inline constexpr std::string_view SEMA_METHOD_RECEIVER_PLACE = "method receiver must be a place expression";

inline constexpr std::string_view SEMA_MUTABLE_METHOD_RECEIVER_WRITABLE =
    "mutable method receiver requires writable storage";

[[nodiscard]] inline std::string sema_ambiguous_method_message(const std::string_view owner,
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous method '" + std::string(owner) + "." + std::string(name) + "' from modules "
        + std::string(first_module) + " and " + std::string(second_module);
}

[[nodiscard]] inline std::string sema_private_method_message(const std::string_view owner, const std::string_view name)
{
    return "method is private: " + std::string(owner) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_method_message(const std::string_view owner, const std::string_view name)
{
    return "unknown method: " + std::string(owner) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_ambiguous_type_name_message(
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous type name '" + std::string(name) + "' from modules " + std::string(first_module) + " and "
        + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_type_message(const std::string_view name)
{
    return "unknown type: " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_type_message(const std::string_view module, const std::string_view name)
{
    return "type is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_type_in_module_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown type in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_trait_message(const std::string_view name)
{
    return "unknown trait: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_trait_in_module_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown trait in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_trait_message(const std::string_view module, const std::string_view name)
{
    return "trait is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_duplicate_trait_requirement_message(
    const std::string_view trait_name, const std::string_view method)
{
    return "duplicate trait requirement: " + std::string(trait_name) + "." + std::string(method);
}

[[nodiscard]] inline std::string sema_duplicate_trait_associated_type_message(
    const std::string_view trait_name, const std::string_view associated_type)
{
    return "duplicate trait associated type: " + std::string(trait_name) + "." + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_duplicate_trait_associated_item_message(
    const std::string_view trait_name, const std::string_view associated_item)
{
    return "duplicate trait associated item: " + std::string(trait_name) + "." + std::string(associated_item);
}

[[nodiscard]] inline std::string sema_duplicate_trait_impl_associated_type_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view associated_type)
{
    return "duplicate trait impl associated type: " + std::string(trait_name) + " for " + std::string(self_type) + "."
        + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_trait_impl_missing_associated_type_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view associated_type)
{
    return "trait impl missing associated type: " + std::string(trait_name) + " for " + std::string(self_type) + "."
        + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_trait_impl_unknown_associated_type_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view associated_type)
{
    return "trait impl associated type is not required: " + std::string(trait_name) + " for " + std::string(self_type)
        + "." + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_unknown_associated_type_constraint_message(
    const std::string_view trait_name, const std::string_view associated_type)
{
    return "trait " + std::string(trait_name) + " has no associated type `" + std::string(associated_type) + "`";
}

[[nodiscard]] inline std::string sema_duplicate_associated_type_constraint_message(
    const std::string_view trait_name, const std::string_view associated_type)
{
    return "duplicate associated type equality for " + std::string(trait_name) + "." + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_associated_type_constraint_on_builtin_message(
    const std::string_view capability_name, const std::string_view associated_type)
{
    return "builtin capability `" + std::string(capability_name) + "` has no associated type `"
        + std::string(associated_type) + "`";
}

[[nodiscard]] inline std::string sema_unknown_associated_type_projection_message(
    const std::string_view base_type, const std::string_view associated_type)
{
    return "unknown associated type projection " + std::string(base_type) + "." + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_associated_type_projection_missing_bound_message(
    const std::string_view base_type, const std::string_view associated_type)
{
    return "associated type projection " + std::string(base_type) + "." + std::string(associated_type)
        + " requires a trait bound";
}

[[nodiscard]] inline std::string sema_ambiguous_associated_type_projection_message(const std::string_view base_type,
    const std::string_view associated_type, const std::string_view first_trait, const std::string_view second_trait)
{
    return "ambiguous associated type projection " + std::string(base_type) + "." + std::string(associated_type)
        + ": candidates from " + std::string(first_trait) + " and " + std::string(second_trait);
}

[[nodiscard]] inline std::string sema_associated_type_projection_cycle_message(
    const std::string_view trait_name, const std::string_view associated_type)
{
    return "associated type equality forms a projection cycle: " + std::string(trait_name) + "."
        + std::string(associated_type);
}

[[nodiscard]] inline std::string sema_trait_associated_type_equality_not_satisfied_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view associated_type,
    const std::string_view expected_type, const std::string_view actual_type)
{
    return "trait associated type equality is not satisfied: " + std::string(trait_name) + " for "
        + std::string(self_type) + "." + std::string(associated_type) + " expected " + std::string(expected_type)
        + ", got " + std::string(actual_type);
}

[[nodiscard]] inline std::string sema_duplicate_trait_impl_method_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view method)
{
    return "duplicate trait impl method: " + std::string(trait_name) + " for " + std::string(self_type) + "."
        + std::string(method);
}

[[nodiscard]] inline std::string sema_trait_impl_missing_method_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view method)
{
    return "trait impl missing method: " + std::string(trait_name) + " for " + std::string(self_type) + "."
        + std::string(method);
}

[[nodiscard]] inline std::string sema_trait_impl_unknown_method_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view method)
{
    return "trait impl method is not required: " + std::string(trait_name) + " for " + std::string(self_type) + "."
        + std::string(method);
}

[[nodiscard]] inline std::string sema_trait_impl_method_signature_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view method)
{
    return "trait impl method signature does not match requirement: " + std::string(trait_name) + " for "
        + std::string(self_type) + "." + std::string(method);
}

[[nodiscard]] inline std::string sema_duplicate_trait_impl_message(
    const std::string_view trait_name, const std::string_view self_type)
{
    return "duplicate trait impl: " + std::string(trait_name) + " for " + std::string(self_type);
}

[[nodiscard]] inline std::string sema_overlapping_trait_impl_message(
    const std::string_view trait_name, const std::string_view self_type)
{
    return "overlapping trait impl: " + std::string(trait_name) + " for " + std::string(self_type);
}

[[nodiscard]] inline std::string sema_previous_trait_impl_note_message(
    const std::string_view trait_name, const std::string_view self_type)
{
    return "previous trait impl: " + std::string(trait_name) + " for " + std::string(self_type);
}

[[nodiscard]] inline std::string sema_trait_impl_orphan_rule_message(
    const std::string_view trait_name, const std::string_view self_type, const std::string_view impl_module)
{
    return "orphan trait impl is not allowed in module " + std::string(impl_module) + ": neither trait "
        + std::string(trait_name) + " nor type " + std::string(self_type) + " is local";
}

[[nodiscard]] inline std::string sema_candidate_trait_impl_note_message(
    const std::string_view trait_name, const std::string_view self_type)
{
    return "candidate trait impl: " + std::string(trait_name) + " for " + std::string(self_type);
}

[[nodiscard]] inline std::string sema_rejected_trait_impl_note_message(const std::string_view trait_name,
    const std::string_view self_type, const std::string_view required_type, const std::string_view reason)
{
    return "rejected trait impl candidate: " + std::string(trait_name) + " for " + std::string(self_type)
        + " while checking " + std::string(required_type) + " (" + std::string(reason) + ")";
}

[[nodiscard]] inline std::string sema_trait_impl_associated_type_note_message(
    const std::string_view associated_type, const std::string_view actual_type)
{
    return "candidate associated type `" + std::string(associated_type) + "` resolves to " + std::string(actual_type);
}

[[nodiscard]] inline std::string sema_trait_impl_orphan_rule_note_message(
    const std::string_view trait_name, const std::string_view self_type)
{
    return "orphan check location for " + std::string(trait_name) + " for " + std::string(self_type);
}

[[nodiscard]] inline std::string sema_trait_predicate_not_satisfied_message(
    const std::string_view type_name, const std::string_view trait_name)
{
    return "type " + std::string(type_name) + " does not satisfy trait predicate `" + std::string(trait_name) + "`";
}

[[nodiscard]] inline std::string sema_ambiguous_trait_method_message(const std::string_view type_name,
    const std::string_view method_name, const std::string_view first_trait, const std::string_view second_trait)
{
    return "ambiguous trait method `" + std::string(method_name) + "` for type " + std::string(type_name)
        + ": candidates from " + std::string(first_trait) + " and " + std::string(second_trait);
}

[[nodiscard]] inline std::string sema_trait_method_missing_bound_message(
    const std::string_view type_name, const std::string_view method_name)
{
    return "trait method `" + std::string(method_name) + "` requires a trait bound for type " + std::string(type_name);
}

[[nodiscard]] inline std::string sema_trait_method_impl_missing_message(
    const std::string_view type_name, const std::string_view method_name)
{
    return "type " + std::string(type_name) + " has no visible impl for trait method `" + std::string(method_name)
        + "`";
}

[[nodiscard]] inline std::string sema_dyn_trait_missing_associated_type_message(
    const std::string_view trait_name, const std::string_view associated_type)
{
    return "dyn trait `" + std::string(trait_name) + "` requires associated type equality `"
        + std::string(associated_type) + " = ...`";
}

[[nodiscard]] inline std::string sema_dyn_trait_method_requires_self_message(
    const std::string_view trait_name, const std::string_view method_name)
{
    return "dyn trait `" + std::string(trait_name) + "` method `" + std::string(method_name)
        + "` must have a self receiver";
}

[[nodiscard]] inline std::string sema_dyn_trait_receiver_message(
    const std::string_view trait_name, const std::string_view method_name)
{
    return "dyn trait `" + std::string(trait_name) + "` method `" + std::string(method_name)
        + "` receiver must be &Self or &mut Self";
}

[[nodiscard]] inline std::string sema_dyn_trait_self_usage_message(
    const std::string_view trait_name, const std::string_view method_name)
{
    return "dyn trait `" + std::string(trait_name) + "` method `" + std::string(method_name)
        + "` can only use Self in the receiver";
}

[[nodiscard]] inline std::string sema_dyn_trait_impl_missing_message(
    const std::string_view type_name, const std::string_view trait_name)
{
    return "type " + std::string(type_name) + " cannot be coerced to dyn trait `" + std::string(trait_name)
        + "` because no matching trait impl is visible";
}

[[nodiscard]] inline std::string sema_dyn_trait_composition_min_principals_message()
{
    return "dyn trait composition requires at least two principal traits";
}

[[nodiscard]] inline std::string sema_dyn_trait_composition_principal_message()
{
    return "dyn trait composition principal must be a dyn trait";
}

[[nodiscard]] inline std::string sema_dyn_trait_composition_duplicate_principal_message(
    const std::string_view principal_name)
{
    return "duplicate dyn trait composition principal `" + std::string(principal_name) + "`";
}

[[nodiscard]] inline std::string sema_dyn_trait_composition_method_ambiguous_message(
    const std::string_view method_name)
{
    return "dyn trait composition method `" + std::string(method_name)
        + "` is ambiguous across multiple principal traits";
}

[[nodiscard]] inline std::string sema_dyn_trait_composition_associated_conflict_message(
    const std::string_view associated_name)
{
    return "dyn trait composition associated type `" + std::string(associated_name)
        + "` has conflicting equality constraints";
}

[[nodiscard]] inline std::string sema_ambiguous_function_name_message(
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous function name '" + std::string(name) + "' from modules " + std::string(first_module) + " and "
        + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_function_message(const std::string_view name)
{
    return "unknown function: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_function_in_module_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown function in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_function_message(
    const std::string_view module, const std::string_view name)
{
    return "function is private: " + std::string(module) + "." + std::string(name);
}

[[nodiscard]] inline std::string sema_export_surface_exposes_restricted_type_message(
    const syntax::Visibility surface_visibility, const std::string_view surface, const std::string_view name,
    const syntax::Visibility type_visibility, const std::string_view type)
{
    return std::string(sema_visibility_surface_name(surface_visibility)) + " " + std::string(surface) + " `"
        + std::string(name) + "` exposes " + std::string(sema_visibility_surface_name(type_visibility)) + " type `"
        + std::string(type) + "`";
}

[[nodiscard]] inline std::string sema_ambiguous_enum_case_message(
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous enum case '" + std::string(name) + "' from modules " + std::string(first_module) + " and "
        + std::string(second_module);
}

inline constexpr std::string_view SEMA_ENUM_CASE_SCOPE_TYPE = "enum case scope must name an enum type";

[[nodiscard]] inline std::string sema_ambiguous_name_message(
    const std::string_view name, const std::string_view first_module, const std::string_view second_module)
{
    return "ambiguous name '" + std::string(name) + "' from modules " + std::string(first_module) + " and "
        + std::string(second_module);
}

[[nodiscard]] inline std::string sema_unknown_name_message(const std::string_view name)
{
    return "unknown name: " + std::string(name);
}

[[nodiscard]] inline std::string sema_unknown_name_in_module_message(
    const std::string_view module, const std::string_view name)
{
    return "unknown name in module " + std::string(module) + ": " + std::string(name);
}

[[nodiscard]] inline std::string sema_private_name_message(const std::string_view module, const std::string_view name)
{
    return "name is private: " + std::string(module) + "." + std::string(name);
}

} // namespace aurex::sema

#pragma once

#include <string_view>

namespace aurex::parse {

inline constexpr std::string_view PARSER_PARSE_FAILED =
    "parsing failed";

inline constexpr std::string_view PARSER_EXPECT_MODULE_TERMINATOR =
    "expected ';' after module declaration";

inline constexpr std::string_view PARSER_EXPECT_IMPORT_KEYWORD =
    "expected 'import'";

inline constexpr std::string_view PARSER_EXPECT_IMPORT_TERMINATOR =
    "expected ';' after import declaration";

inline constexpr std::string_view PARSER_EXPECT_PATH_IDENTIFIER =
    "expected identifier in path";

inline constexpr std::string_view PARSER_EXPECT_PATH_IDENTIFIER_AFTER_DOT =
    "expected identifier after '.'";

inline constexpr std::string_view PARSER_EXPECT_IMPORT_ALIAS =
    "expected import alias after 'as'";

inline constexpr std::string_view PARSER_EXPECT_ITEM_DECLARATION =
    "expected item declaration";

inline constexpr std::string_view PARSER_EXPECT_CONST_KEYWORD =
    "expected 'const'";

inline constexpr std::string_view PARSER_EXPECT_CONST_NAME =
    "expected const name";

inline constexpr std::string_view PARSER_EXPECT_CONST_TYPE_COLON =
    "expected ':' after const name";

inline constexpr std::string_view PARSER_EXPECT_CONST_INITIALIZER_EQUAL =
    "expected '=' in const declaration";

inline constexpr std::string_view PARSER_EXPECT_CONST_TERMINATOR =
    "expected ';' after const declaration";

inline constexpr std::string_view PARSER_EXPECT_TYPE_KEYWORD =
    "expected 'type'";

inline constexpr std::string_view PARSER_EXPECT_TYPE_ALIAS_NAME =
    "expected type alias name";

inline constexpr std::string_view PARSER_EXPECT_TYPE_ALIAS_INITIALIZER_EQUAL =
    "expected '=' in type alias declaration";

inline constexpr std::string_view PARSER_EXPECT_TYPE_ALIAS_TERMINATOR =
    "expected ';' after type alias declaration";

inline constexpr std::string_view PARSER_IMPL_PRIVATE_UNSUPPORTED =
    "impl block cannot be private";

inline constexpr std::string_view PARSER_EXTERN_PRIVATE_UNSUPPORTED =
    "extern block cannot be private";

inline constexpr std::string_view PARSER_EXPORT_C_PRIVATE_UNSUPPORTED =
    "exported C function cannot be private";

inline constexpr std::string_view PARSER_EXPECT_EXPORT_C_KEYWORD =
    "expected 'c' after 'export'";

inline constexpr std::string_view PARSER_EXPECT_EXPORT_C_FN =
    "expected function declaration after 'export c'";

inline constexpr std::string_view PARSER_EXPECT_FN_AFTER_UNSAFE =
    "expected 'fn' after 'unsafe'";

inline constexpr std::string_view PARSER_EXPECT_UNSAFE_KEYWORD =
    "expected 'unsafe'";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_KEYWORD =
    "expected 'struct'";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_NAME =
    "expected struct name";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_BODY =
    "expected '{' after struct name";

inline constexpr std::string_view PARSER_EXPECT_FIELD_NAME =
    "expected field name";

inline constexpr std::string_view PARSER_EXPECT_FIELD_TYPE_COLON =
    "expected ':' after field name";

inline constexpr std::string_view PARSER_EXPECT_FIELD_SEPARATOR =
    "expected ';' or '}' after field declaration";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_END =
    "expected '}' after struct declaration";

inline constexpr std::string_view PARSER_EXPECT_ENUM_KEYWORD =
    "expected 'enum'";

inline constexpr std::string_view PARSER_EXPECT_ENUM_NAME =
    "expected enum name";

inline constexpr std::string_view PARSER_EXPECT_ENUM_BODY_AFTER_NAME =
    "expected '{' after enum name";

inline constexpr std::string_view PARSER_EXPECT_ENUM_BODY_AFTER_BASE =
    "expected '{' after enum base type";

inline constexpr std::string_view PARSER_EXPECT_ENUM_CASE_NAME =
    "expected enum case name";

inline constexpr std::string_view PARSER_EXPECT_ENUM_CASE_PAYLOAD_TYPE =
    "expected enum case payload type";

inline constexpr std::string_view PARSER_EXPECT_ENUM_CASE_PAYLOAD_END =
    "expected ')' after enum case payload type";

inline constexpr std::string_view PARSER_EXPECT_ENUM_VALUE =
    "expected integer literal enum value";

inline constexpr std::string_view PARSER_EXPECT_ENUM_CASE_SEPARATOR =
    "expected ',' or '}' after enum case";

inline constexpr std::string_view PARSER_EXPECT_ENUM_END =
    "expected '}' after enum declaration";

inline constexpr std::string_view PARSER_EXPECT_OPAQUE_KEYWORD =
    "expected 'opaque'";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_AFTER_OPAQUE =
    "expected 'struct' after 'opaque'";

inline constexpr std::string_view PARSER_EXPECT_OPAQUE_STRUCT_NAME =
    "expected opaque struct name";

inline constexpr std::string_view PARSER_EXPECT_OPAQUE_STRUCT_TERMINATOR =
    "expected ';' after opaque struct declaration";

inline constexpr std::string_view PARSER_EXPECT_IMPL_KEYWORD =
    "expected 'impl'";

inline constexpr std::string_view PARSER_EXPECT_IMPL_BODY =
    "expected '{' after impl type";

inline constexpr std::string_view PARSER_EXPECT_IMPL_FN =
    "expected function declaration in impl block";

inline constexpr std::string_view PARSER_EXPECT_IMPL_END =
    "expected '}' after impl block";

inline constexpr std::string_view PARSER_EXPECT_EXTERN_KEYWORD =
    "expected 'extern'";

inline constexpr std::string_view PARSER_EXPECT_C_AFTER_EXTERN =
    "expected 'c' after 'extern'";

inline constexpr std::string_view PARSER_EXPECT_EXTERN_BODY =
    "expected '{' after 'extern c'";

inline constexpr std::string_view PARSER_EXPECT_EXTERN_ITEM =
    "expected extern item";

inline constexpr std::string_view PARSER_EXPECT_EXTERN_END =
    "expected '}' after extern block";

inline constexpr std::string_view PARSER_EXPECT_FN_KEYWORD =
    "expected 'fn'";

inline constexpr std::string_view PARSER_EXPECT_FN_NAME =
    "expected function name";

inline constexpr std::string_view PARSER_EXPECT_FN_PARAM_LIST =
    "expected '(' after function name";

inline constexpr std::string_view PARSER_EXPECT_FN_PARAM_LIST_END =
    "expected ')' after parameter list";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_TYPE_PARAMETER =
    "expected generic type parameter";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_TYPE_PARAMETER_NAME =
    "expected generic type parameter name";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_PARAM_LIST_END =
    "expected ']' after generic parameter list";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_PARAM_SEPARATOR =
    "expected ',' or ']' after generic parameter";

inline constexpr std::string_view PARSER_EXPECT_LEGACY_GENERIC_BEGIN =
    "expected '<'";

inline constexpr std::string_view PARSER_M2_GENERIC_BOUNDS_UNSUPPORTED =
    "generic bounds are not part of M2 syntax";

inline constexpr std::string_view PARSER_EXPECT_WHERE_GENERIC_PARAM =
    "expected generic parameter name in where clause";

inline constexpr std::string_view PARSER_EXPECT_WHERE_GENERIC_PARAM_COLON =
    "expected ':' after generic parameter name in where clause";

inline constexpr std::string_view PARSER_EXPECT_WHERE_CAPABILITY =
    "expected capability name in where clause";

inline constexpr std::string_view PARSER_EXPECT_WHERE_SEPARATOR =
    "expected ',' or declaration body after where constraint";

inline constexpr std::string_view PARSER_M2_LEGACY_ANGLE_GENERIC_UNSUPPORTED =
    "Aurex generics use '[' and ']'; '<' and '>' are not generic delimiters";

inline constexpr std::string_view PARSER_M2_RANGE_FOR_ONLY_RANGE =
    "M2 range-for only supports range(...); generic iteration is not part of M2 syntax";

inline constexpr std::string_view PARSER_M2_EXPLICIT_GENERIC_CALL_SYNTAX =
    "explicit generic calls use '[...]', for example id[i32](...)";

inline constexpr std::string_view PARSER_DOT_ONLY_SELECTOR =
    "Aurex selectors use '.', not '::'";

inline constexpr std::string_view PARSER_M2_IF_EXPR_REQUIRES_ELSE =
    "if expression requires an else branch";

inline constexpr std::string_view PARSER_M2_BLOCK_RESULT_ASSIGNMENT =
    "assignment cannot be used as block result";

inline constexpr std::string_view PARSER_EXPECT_PARAMETER_NAME =
    "expected parameter name";

inline constexpr std::string_view PARSER_EXPECT_PARAMETER_TYPE_COLON =
    "expected ':' after parameter name";

inline constexpr std::string_view PARSER_EXPECT_PARAMETER_SEPARATOR =
    "expected ',' or ')' after parameter";

inline constexpr std::string_view PARSER_VARIADIC_MARKER_MUST_BE_LAST =
    "variadic marker must be last in parameter list";

inline constexpr std::string_view PARSER_EXPECT_EXTERN_FN_TERMINATOR =
    "expected ';' after extern function declaration";

inline constexpr std::string_view PARSER_EXPECT_ABI_NAME_ATTRIBUTE =
    "expected ABI attribute 'name'";

inline constexpr std::string_view PARSER_EXPECT_ABI_ATTRIBUTE_START =
    "expected '(' after ABI attribute";

inline constexpr std::string_view PARSER_EXPECT_ABI_NAME_STRING =
    "expected string literal in ABI name";

inline constexpr std::string_view PARSER_EXPECT_ABI_ATTRIBUTE_END =
    "expected ')' after ABI attribute";

inline constexpr std::string_view PARSER_EXPECT_BLOCK =
    "expected block";

inline constexpr std::string_view PARSER_EXPECT_BLOCK_END =
    "expected '}' after block";

inline constexpr std::string_view PARSER_EXPECT_BLOCK_EXPR =
    "expected block expression";

inline constexpr std::string_view PARSER_EXPECT_BLOCK_EXPR_END =
    "expected '}' after block expression";

inline constexpr std::string_view PARSER_EXPECT_UNSAFE_BLOCK =
    "expected block after 'unsafe'";

inline constexpr std::string_view PARSER_EXPECT_IF =
    "expected 'if'";

inline constexpr std::string_view PARSER_EXPECT_WHILE =
    "expected 'while'";

inline constexpr std::string_view PARSER_EXPECT_FOR =
    "expected 'for'";

inline constexpr std::string_view PARSER_EXPECT_FOR_CONDITION_TERMINATOR =
    "expected ';' after for condition";

inline constexpr std::string_view PARSER_EXPECT_FOR_RANGE_VARIABLE =
    "expected loop variable after 'for'";

inline constexpr std::string_view PARSER_EXPECT_IN_AFTER_LOOP_VARIABLE =
    "expected 'in' after loop variable";

inline constexpr std::string_view PARSER_EXPECT_RANGE_AFTER_IN =
    "expected range after 'in'";

inline constexpr std::string_view PARSER_EXPECT_RANGE_CALL_START =
    "expected '(' after range";

inline constexpr std::string_view PARSER_EXPECT_RANGE_ARGUMENTS_END =
    "expected ')' after range arguments";

inline constexpr std::string_view PARSER_FOR_RANGE_ARITY =
    "range expects 1 to 3 arguments";

inline constexpr std::string_view PARSER_EXPECT_RANGE_ARGUMENT_SEPARATOR =
    "expected ',' or ')' after range argument";

inline constexpr std::string_view PARSER_EXPECT_BREAK =
    "expected 'break'";

inline constexpr std::string_view PARSER_EXPECT_BREAK_TERMINATOR =
    "expected ';' after break";

inline constexpr std::string_view PARSER_EXPECT_CONTINUE =
    "expected 'continue'";

inline constexpr std::string_view PARSER_EXPECT_CONTINUE_TERMINATOR =
    "expected ';' after continue";

inline constexpr std::string_view PARSER_EXPECT_DEFER =
    "expected 'defer'";

inline constexpr std::string_view PARSER_EXPECT_DEFER_TERMINATOR =
    "expected ';' after defer statement";

inline constexpr std::string_view PARSER_EXPECT_RETURN =
    "expected 'return'";

inline constexpr std::string_view PARSER_EXPECT_RETURN_TERMINATOR =
    "expected ';' after return";

inline constexpr std::string_view PARSER_EXPECT_MATCH =
    "expected 'match'";

inline constexpr std::string_view PARSER_EXPECT_MATCH_BODY =
    "expected '{' after match value";

inline constexpr std::string_view PARSER_EXPECT_MATCH_END =
    "expected '}' after match expression";

inline constexpr std::string_view PARSER_EXPECT_MATCH_ARM_ARROW =
    "expected '=>' after match case";

inline constexpr std::string_view PARSER_EXPECT_MATCH_ARM_SEPARATOR =
    "expected ',' or '}' after match arm";

inline constexpr std::string_view PARSER_INCREMENT_UNSUPPORTED =
    "increment operator is not supported; use '+= 1'";

inline constexpr std::string_view PARSER_DECREMENT_UNSUPPORTED =
    "decrement operator is not supported; use '-= 1'";

inline constexpr std::string_view PARSER_EXPECT_EXPRESSION_NAME =
    "expected expression name";

inline constexpr std::string_view PARSER_EXPECT_ITEM_NAME_AFTER_SCOPE =
    "expected item name after '.'";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_LITERAL_END =
    "expected '}' after struct literal";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_TYPE_ARGUMENT =
    "expected generic type argument";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_TYPE_ARGUMENT_SEPARATOR =
    "expected ',' or ']' after generic type argument";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_LITERAL_FIELD =
    "expected field name in struct literal";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_LITERAL_FIELD_SEPARATOR =
    "expected ',' or '}' after struct literal field";

inline constexpr std::string_view PARSER_EXPECT_EXPRESSION =
    "expected expression";

inline constexpr std::string_view PARSER_EXPECT_ARRAY_LITERAL_START =
    "expected '['";

inline constexpr std::string_view PARSER_EXPECT_ARRAY_REPEAT_COUNT =
    "expected array repeat count";

inline constexpr std::string_view PARSER_EXPECT_ARRAY_ELEMENT_SEPARATOR =
    "expected ',' or ']' after array element";

inline constexpr std::string_view PARSER_EXPECT_ARRAY_LITERAL_END =
    "expected ']' after array literal";

inline constexpr std::string_view PARSER_EMPTY_TUPLE_LITERAL_UNSUPPORTED =
    "empty tuple literal is not part of M2 syntax";

inline constexpr std::string_view PARSER_EXPECT_TUPLE_ELEMENT_SEPARATOR =
    "expected ',' or ')' after tuple element";

inline constexpr std::string_view PARSER_EXPECT_TUPLE_LITERAL_END =
    "expected ')' after tuple literal";

inline constexpr std::string_view PARSER_EXPECT_GROUPED_EXPR_END =
    "expected ')' after expression";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_APPLY_SCOPE =
    "expected callee before generic type arguments";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_APPLY_START =
    "expected '[' before generic type arguments";

inline constexpr std::string_view PARSER_EXPECT_GENERIC_TYPE_ARGS_END =
    "expected ']' after generic type arguments";

inline constexpr std::string_view PARSER_EXPECT_FIELD_AFTER_DOT =
    "expected field name after '.'";

inline constexpr std::string_view PARSER_TUPLE_FIELD_ACCESS_UNSUPPORTED =
    "tuple fields are not directly accessible; destructure the tuple or use a named struct";

inline constexpr std::string_view PARSER_EXPECT_INDEX_END =
    "expected ']' after index";

inline constexpr std::string_view PARSER_EXPECT_SLICE_END =
    "expected ']' after slice expression";

inline constexpr std::string_view PARSER_EXPECT_CALL_ARGUMENTS_END =
    "expected ')' after argument list";

inline constexpr std::string_view PARSER_EXPECT_CALL_ARGUMENT_SEPARATOR =
    "expected ',' or ')' after argument";

inline constexpr std::string_view PARSER_EXPECT_UNSUPPORTED_UPDATE =
    "expected unsupported update operator";

inline constexpr std::string_view PARSER_EXPECT_TYPE_POINTER_MUTABILITY =
    "expected 'mut' or 'const' after '*'";

inline constexpr std::string_view PARSER_EXPECT_TYPE_SLICE_MUTABILITY =
    "expected 'mut' or 'const' after '[]'";

inline constexpr std::string_view PARSER_EXPECT_C_AFTER_EXTERN_FUNCTION_TYPE =
    "expected 'c' after 'extern' in function type";

inline constexpr std::string_view PARSER_EXPECT_FN_AFTER_EXTERN_C_FUNCTION_TYPE =
    "expected 'fn' after 'extern c' in function type";

inline constexpr std::string_view PARSER_EXPECT_FUNCTION_TYPE_PARAM_LIST =
    "expected '(' after function type";

inline constexpr std::string_view PARSER_EXPECT_FUNCTION_TYPE_PARAM_LIST_END =
    "expected ')' after function type parameter list";

inline constexpr std::string_view PARSER_EXPECT_FUNCTION_TYPE_RETURN_ARROW =
    "expected '->' after function type parameter list";

inline constexpr std::string_view PARSER_EXPECT_FUNCTION_TYPE_PARAM_SEPARATOR =
    "expected ',' or ')' after function type parameter";

inline constexpr std::string_view PARSER_EMPTY_TUPLE_TYPE_UNSUPPORTED =
    "empty tuple type is not part of M2 syntax";

inline constexpr std::string_view PARSER_EXPECT_TUPLE_TYPE_SEPARATOR =
    "expected ',' or ')' after tuple type element";

inline constexpr std::string_view PARSER_EXPECT_TUPLE_TYPE_END =
    "expected ')' after tuple type";

inline constexpr std::string_view PARSER_EXPECT_ARRAY_LENGTH =
    "expected array length";

inline constexpr std::string_view PARSER_ARRAY_LENGTH_OUT_OF_RANGE =
    "array length literal is out of range";

inline constexpr std::string_view PARSER_EXPECT_TYPE =
    "expected type";

inline constexpr std::string_view PARSER_EXPECT_TYPE_NAME_AFTER_SCOPE =
    "expected type name after '.'";

inline constexpr std::string_view PARSER_EXPECT_ARRAY_LENGTH_END =
    "expected ']' after array length";

inline constexpr std::string_view PARSER_EXPECT_LOCAL_NAME =
    "expected local name";

inline constexpr std::string_view PARSER_EXPECT_INITIALIZER =
    "expected initializer";

inline constexpr std::string_view PARSER_EXPECT_LOCAL_DECL_TERMINATOR =
    "expected ';' after local declaration";

inline constexpr std::string_view PARSER_EXPECT_LET_ELSE_BLOCK =
    "expected block after let-else";

inline constexpr std::string_view PARSER_EXPECT_LET_ELSE_TERMINATOR =
    "expected ';' after let-else declaration";

inline constexpr std::string_view PARSER_LET_ELSE_REQUIRES_PATTERN =
    "let-else requires a destructuring or refutable pattern";

inline constexpr std::string_view PARSER_EXPECT_EXPR_STMT_TERMINATOR =
    "expected ';' after expression statement";

inline constexpr std::string_view PARSER_EXPECT_ASSIGNMENT_TERMINATOR =
    "expected ';' after assignment";

inline constexpr std::string_view PARSER_EXPECT_MATCH_PATTERN =
    "expected match pattern";

inline constexpr std::string_view PARSER_BARE_ENUM_CASE_PATTERN_UNSUPPORTED =
    "bare enum case patterns are not supported; use '.case' or explicit 'Type.case' / 'Type[Args].case'";

inline constexpr std::string_view PARSER_EXPECT_ENUM_CASE_PATTERN_DOT =
    "expected '.' before enum case pattern name";

inline constexpr std::string_view PARSER_EMPTY_TUPLE_PATTERN_UNSUPPORTED =
    "empty tuple pattern is not part of M2 syntax";

inline constexpr std::string_view PARSER_EXPECT_TUPLE_PATTERN_SEPARATOR =
    "expected ',' or ')' after tuple pattern element";

inline constexpr std::string_view PARSER_EXPECT_TUPLE_PATTERN_END =
    "expected ')' after tuple pattern";

inline constexpr std::string_view PARSER_EXPECT_SLICE_PATTERN_SEPARATOR =
    "expected ',' or ']' after slice pattern element";

inline constexpr std::string_view PARSER_EXPECT_SLICE_PATTERN_END =
    "expected ']' after slice pattern";

inline constexpr std::string_view PARSER_DUPLICATE_SLICE_PATTERN_REST =
    "slice pattern can contain at most one '..' rest marker";

inline constexpr std::string_view PARSER_EXPECT_SLICE_PATTERN_REST =
    "slice pattern rest marker is '..'";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_PATTERN_FIELD =
    "expected field name in struct pattern";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_PATTERN_FIELD_SEPARATOR =
    "expected ',' or '}' after struct pattern field";

inline constexpr std::string_view PARSER_EXPECT_STRUCT_PATTERN_END =
    "expected '}' after struct pattern";

inline constexpr std::string_view PARSER_EXPECT_ENUM_CASE_AFTER_DOT =
    "expected enum case name after '.'";

inline constexpr std::string_view PARSER_EXPECT_PAYLOAD_BINDING =
    "expected payload binding name";

inline constexpr std::string_view PARSER_EXPECT_PAYLOAD_BINDING_END =
    "expected ')' after payload binding";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_PTRADDR_START =
    "expected '(' after ptraddr";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_PTRADDR_END =
    "expected ')' after ptraddr argument";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_PTRAT_TYPE_START =
    "expected '[' after ptrat";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_PTRAT_TYPE_END =
    "expected ']' after ptrat type";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_PTRAT_START =
    "expected '(' after ptrat address";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_PTRAT_END =
    "expected ')' after ptrat address";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_STRRAW_START =
    "expected '(' after strraw";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_STRRAW_DATA_SEPARATOR =
    "expected ',' after strraw data";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_STRRAW_END =
    "expected ')' after strraw length";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_TYPE_START_PREFIX =
    "expected '[' after ";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_TYPE_START_SUFFIX =
    " builtin";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_TYPE_END_PREFIX =
    "expected ']' after ";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_TYPE_END_SUFFIX =
    " type";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_EXPR_START_PREFIX =
    "expected '(' after ";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_EXPR_START_SUFFIX =
    " expression";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_EXPR_END_PREFIX =
    "expected ')' after ";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_EXPR_END_SUFFIX =
    " expression";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_ARG_START_PREFIX =
    "expected '(' after ";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_ARG_END_PREFIX =
    "expected ')' after ";

inline constexpr std::string_view PARSER_EXPECT_BUILTIN_ARG_END_SUFFIX =
    " argument";

} // namespace aurex::parse

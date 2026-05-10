#pragma once

#include <aurex/base/config.hpp>
#include <aurex/base/source.hpp>
#include <aurex/syntax/ast_ids.hpp>

#include <string_view>
#include <vector>

namespace aurex::syntax {

enum class PrimitiveTypeKind {
    void_,
    bool_,
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    isize,
    usize,
    f32,
    f64,
    str,
};

enum class PointerMutability {
    mut,
    const_,
};

enum class Visibility {
    public_,
    private_,
};

enum class TypeKind {
    primitive,
    named,
    pointer,
    array,
};

struct TypeNode {
    TypeKind kind = TypeKind::named;
    base::SourceRange range {};
    PrimitiveTypeKind primitive = PrimitiveTypeKind::void_;
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::string_view name;
    std::vector<TypeId> type_args;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeId pointee = INVALID_TYPE_ID;
    base::u64 array_count = 0;
    TypeId array_element = INVALID_TYPE_ID;
};

enum class ExprKind {
    invalid,
    integer_literal,
    float_literal,
    bool_literal,
    null_literal,
    string_literal,
    c_string_literal,
    byte_literal,
    name,
    unary,
    binary,
    call,
    try_expr,
    if_expr,
    block_expr,
    match_expr,
    field,
    index,
    struct_literal,
    cast,
    ptr_cast,
    bit_cast,
    size_of,
    align_of,
    ptr_addr,
    ptr_from_addr,
    str_data,
    str_byte_len,
    str_from_bytes_unchecked,
};

enum class UnaryOp {
    logical_not,
    numeric_negate,
    bitwise_not,
    address_of,
    dereference,
};

enum class BinaryOp {
    add,
    sub,
    mul,
    div,
    mod,
    shl,
    shr,
    less,
    less_equal,
    greater,
    greater_equal,
    equal,
    not_equal,
    bit_and,
    bit_xor,
    bit_or,
    logical_and,
    logical_or,
};

enum class AssignOp {
    assign,
    add,
    sub,
    mul,
    div,
    mod,
    shl,
    shr,
    bit_and,
    bit_xor,
    bit_or,
};

enum class PatternKind {
    wildcard,
    enum_case,
    literal,
    or_pattern,
};

struct PatternNode {
    PatternKind kind = PatternKind::wildcard;
    base::SourceRange range {};
    std::string_view enum_name;
    std::string_view case_name;
    std::string_view binding_name;
    std::vector<PatternId> alternatives;
    bool scoped = false;
};

struct FieldInit {
    std::string_view name;
    ExprId value = INVALID_EXPR_ID;
    base::SourceRange range {};
};

struct MatchArm {
    PatternId pattern = INVALID_PATTERN_ID;
    ExprId guard = INVALID_EXPR_ID;
    ExprId value = INVALID_EXPR_ID;
    base::SourceRange range {};
};

struct ExprNode {
    ExprKind kind = ExprKind::invalid;
    base::SourceRange range {};
    std::string_view scope_name;
    base::SourceRange scope_range {};
    std::string_view text;
    UnaryOp unary_op = UnaryOp::logical_not;
    ExprId unary_operand = INVALID_EXPR_ID;
    BinaryOp binary_op = BinaryOp::add;
    ExprId binary_lhs = INVALID_EXPR_ID;
    ExprId binary_rhs = INVALID_EXPR_ID;
    ExprId callee = INVALID_EXPR_ID;
    std::vector<ExprId> args;
    ExprId condition = INVALID_EXPR_ID;
    ExprId then_expr = INVALID_EXPR_ID;
    ExprId else_expr = INVALID_EXPR_ID;
    StmtId block = INVALID_STMT_ID;
    ExprId block_result = INVALID_EXPR_ID;
    ExprId match_value = INVALID_EXPR_ID;
    std::vector<MatchArm> match_arms;
    ExprId object = INVALID_EXPR_ID;
    std::string_view field_name;
    ExprId index = INVALID_EXPR_ID;
    std::string_view struct_name;
    std::vector<TypeId> struct_type_args;
    std::vector<FieldInit> field_inits;
    TypeId cast_type = INVALID_TYPE_ID;
    ExprId cast_expr = INVALID_EXPR_ID;
    std::vector<TypeId> type_args;
};

enum class StmtKind {
    let,
    var,
    assign,
    if_,
    for_,
    while_,
    break_,
    continue_,
    defer,
    return_,
    expr,
    block,
};

struct StmtNode {
    StmtKind kind = StmtKind::expr;
    base::SourceRange range {};
    std::string_view name;
    TypeId declared_type = INVALID_TYPE_ID;
    ExprId init = INVALID_EXPR_ID;
    AssignOp assign_op = AssignOp::assign;
    ExprId lhs = INVALID_EXPR_ID;
    ExprId rhs = INVALID_EXPR_ID;
    ExprId condition = INVALID_EXPR_ID;
    StmtId then_block = INVALID_STMT_ID;
    StmtId else_block = INVALID_STMT_ID;
    StmtId else_if = INVALID_STMT_ID;
    StmtId body = INVALID_STMT_ID;
    StmtId for_init = INVALID_STMT_ID;
    StmtId for_update = INVALID_STMT_ID;
    ExprId return_value = INVALID_EXPR_ID;
    std::vector<StmtId> statements;
};

struct ParamDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
};

struct FieldDecl {
    std::string_view name;
    TypeId type = INVALID_TYPE_ID;
    base::SourceRange range {};
    Visibility visibility = Visibility::public_;
};

struct EnumCaseDecl {
    std::string_view name;
    TypeId payload_type = INVALID_TYPE_ID;
    std::string_view value_text;
    base::SourceRange range {};
};

enum class ItemKind {
    const_decl,
    type_alias,
    struct_decl,
    enum_decl,
    opaque_struct_decl,
    fn_decl,
    extern_block,
    impl_block,
};

struct ItemNode {
    ItemKind kind = ItemKind::fn_decl;
    base::SourceRange range {};
    std::string_view name;
    Visibility visibility = Visibility::public_;
    std::vector<std::string_view> generic_params;
    base::usize impl_generic_param_count = 0;
    TypeId const_type = INVALID_TYPE_ID;
    ExprId const_value = INVALID_EXPR_ID;
    TypeId alias_type = INVALID_TYPE_ID;
    std::vector<FieldDecl> fields;
    TypeId enum_base_type = INVALID_TYPE_ID;
    std::vector<EnumCaseDecl> enum_cases;
    std::vector<ParamDecl> params;
    TypeId return_type = INVALID_TYPE_ID;
    StmtId body = INVALID_STMT_ID;
    TypeId impl_type = INVALID_TYPE_ID;
    bool is_export_c = false;
    bool is_extern_c = false;
    bool is_variadic = false;
    bool is_prototype = false;
    std::string_view abi_name;
    std::vector<ItemId> extern_items;
    std::vector<ItemId> impl_items;
};

struct ModulePath {
    std::vector<std::string_view> parts;
    base::SourceRange range {};
};

struct ImportDecl {
    ModulePath path;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
    bool explicit_visibility = false;
};

struct ResolvedImport {
    ModuleId module = INVALID_MODULE_ID;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
};

struct ModuleInfo {
    ModulePath path;
    std::vector<ResolvedImport> imports;
};

struct AstModule {
    // The AST is intentionally stored as parallel vectors addressed by small
    // IDs. This keeps nodes compact, avoids virtual dispatch, and lets later
    // compiler stages attach side tables without changing syntax nodes.
    ModulePath module_path;
    std::vector<ImportDecl> imports;
    std::vector<ModuleInfo> modules;
    std::vector<TypeNode> types;
    std::vector<ExprNode> exprs;
    std::vector<PatternNode> patterns;
    std::vector<StmtNode> stmts;
    std::vector<ItemNode> items;
    std::vector<ModuleId> item_modules;

    AstModule() {
        types.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        exprs.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        patterns.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        stmts.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
        items.reserve(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    }

    [[nodiscard]] TypeId push_type(TypeNode node) {
        const TypeId id {static_cast<base::u32>(types.size())};
        types.push_back(std::move(node));
        return id;
    }

    [[nodiscard]] ExprId push_expr(ExprNode node) {
        const ExprId id {static_cast<base::u32>(exprs.size())};
        exprs.push_back(std::move(node));
        return id;
    }

    [[nodiscard]] PatternId push_pattern(PatternNode node) {
        const PatternId id {static_cast<base::u32>(patterns.size())};
        patterns.push_back(std::move(node));
        return id;
    }

    [[nodiscard]] StmtId push_stmt(StmtNode node) {
        const StmtId id {static_cast<base::u32>(stmts.size())};
        stmts.push_back(std::move(node));
        return id;
    }

    [[nodiscard]] ItemId push_item(ItemNode node) {
        const ItemId id {static_cast<base::u32>(items.size())};
        items.push_back(std::move(node));
        item_modules.push_back(INVALID_MODULE_ID);
        return id;
    }
};

} // namespace aurex::syntax

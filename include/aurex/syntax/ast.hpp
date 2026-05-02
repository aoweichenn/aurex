#pragma once

#include "aurex/base/config.hpp"
#include "aurex/base/source.hpp"
#include "aurex/syntax/ast_ids.hpp"

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
    std::string_view name;
    PointerMutability pointer_mutability = PointerMutability::const_;
    TypeId pointee = invalid_type_id;
    base::u64 array_count = 0;
    TypeId array_element = invalid_type_id;
};

enum class ExprKind {
    invalid,
    integer_literal,
    bool_literal,
    null_literal,
    string_literal,
    c_string_literal,
    byte_literal,
    name,
    unary,
    binary,
    call,
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

struct FieldInit {
    std::string_view name;
    ExprId value = invalid_expr_id;
    base::SourceRange range {};
};

struct ExprNode {
    ExprKind kind = ExprKind::invalid;
    base::SourceRange range {};
    std::string_view text;
    UnaryOp unary_op = UnaryOp::logical_not;
    ExprId unary_operand = invalid_expr_id;
    BinaryOp binary_op = BinaryOp::add;
    ExprId binary_lhs = invalid_expr_id;
    ExprId binary_rhs = invalid_expr_id;
    ExprId callee = invalid_expr_id;
    std::vector<ExprId> args;
    ExprId object = invalid_expr_id;
    std::string_view field_name;
    ExprId index = invalid_expr_id;
    std::string_view struct_name;
    std::vector<FieldInit> field_inits;
    TypeId cast_type = invalid_type_id;
    ExprId cast_expr = invalid_expr_id;
};

enum class StmtKind {
    let,
    var,
    assign,
    if_,
    while_,
    break_,
    continue_,
    return_,
    expr,
    block,
};

struct StmtNode {
    StmtKind kind = StmtKind::expr;
    base::SourceRange range {};
    std::string_view name;
    TypeId declared_type = invalid_type_id;
    ExprId init = invalid_expr_id;
    ExprId lhs = invalid_expr_id;
    ExprId rhs = invalid_expr_id;
    ExprId condition = invalid_expr_id;
    StmtId then_block = invalid_stmt_id;
    StmtId else_block = invalid_stmt_id;
    StmtId body = invalid_stmt_id;
    ExprId return_value = invalid_expr_id;
    std::vector<StmtId> statements;
};

struct ParamDecl {
    std::string_view name;
    TypeId type = invalid_type_id;
    base::SourceRange range {};
};

struct FieldDecl {
    std::string_view name;
    TypeId type = invalid_type_id;
    base::SourceRange range {};
};

struct EnumCaseDecl {
    std::string_view name;
    std::string_view value_text;
    base::SourceRange range {};
};

enum class ItemKind {
    const_decl,
    struct_decl,
    enum_decl,
    opaque_struct_decl,
    fn_decl,
    extern_block,
};

struct ItemNode {
    ItemKind kind = ItemKind::fn_decl;
    base::SourceRange range {};
    std::string_view name;
    TypeId const_type = invalid_type_id;
    ExprId const_value = invalid_expr_id;
    std::vector<FieldDecl> fields;
    TypeId enum_base_type = invalid_type_id;
    std::vector<EnumCaseDecl> enum_cases;
    std::vector<ParamDecl> params;
    TypeId return_type = invalid_type_id;
    StmtId body = invalid_stmt_id;
    bool is_export_c = false;
    bool is_extern_c = false;
    std::string_view abi_name;
    std::vector<ItemId> extern_items;
};

struct ModulePath {
    std::vector<std::string_view> parts;
    base::SourceRange range {};
};

struct ModuleInfo {
    ModulePath path;
    std::vector<ModuleId> imports;
};

struct AstModule {
    // The AST is intentionally stored as parallel vectors addressed by small
    // IDs. This keeps nodes compact, avoids virtual dispatch, and lets later
    // compiler stages attach side tables without changing syntax nodes.
    ModulePath module_path;
    std::vector<ModulePath> imports;
    std::vector<ModuleInfo> modules;
    std::vector<TypeNode> types;
    std::vector<ExprNode> exprs;
    std::vector<StmtNode> stmts;
    std::vector<ItemNode> items;
    std::vector<ModuleId> item_modules;

    AstModule() {
        types.reserve(base::config::initial_ast_node_capacity);
        exprs.reserve(base::config::initial_ast_node_capacity);
        stmts.reserve(base::config::initial_ast_node_capacity);
        items.reserve(base::config::initial_ast_node_capacity);
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

    [[nodiscard]] StmtId push_stmt(StmtNode node) {
        const StmtId id {static_cast<base::u32>(stmts.size())};
        stmts.push_back(std::move(node));
        return id;
    }

    [[nodiscard]] ItemId push_item(ItemNode node) {
        const ItemId id {static_cast<base::u32>(items.size())};
        items.push_back(std::move(node));
        item_modules.push_back(invalid_module_id);
        return id;
    }
};

} // namespace aurex::syntax

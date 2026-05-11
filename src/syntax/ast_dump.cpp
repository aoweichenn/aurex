#include <aurex/syntax/ast_dump.hpp>

#include <sstream>
#include <unordered_set>

namespace aurex::syntax {

std::string_view token_kind_name(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::eof: return "eof";
    case TokenKind::invalid: return "invalid";
    case TokenKind::identifier: return "identifier";
    case TokenKind::integer_literal: return "integer_literal";
    case TokenKind::float_literal: return "float_literal";
    case TokenKind::string_literal: return "string_literal";
    case TokenKind::c_string_literal: return "c_string_literal";
    case TokenKind::byte_literal: return "byte_literal";
    case TokenKind::kw_module: return "kw_module";
    case TokenKind::kw_import: return "kw_import";
    case TokenKind::kw_as: return "kw_as";
    case TokenKind::kw_pub: return "kw_pub";
    case TokenKind::kw_priv: return "kw_priv";
    case TokenKind::kw_extern: return "kw_extern";
    case TokenKind::kw_export: return "kw_export";
    case TokenKind::kw_c: return "kw_c";
    case TokenKind::kw_fn: return "kw_fn";
    case TokenKind::kw_struct: return "kw_struct";
    case TokenKind::kw_opaque: return "kw_opaque";
    case TokenKind::kw_enum: return "kw_enum";
    case TokenKind::kw_const: return "kw_const";
    case TokenKind::kw_type: return "kw_type";
    case TokenKind::kw_impl: return "kw_impl";
    case TokenKind::kw_match: return "kw_match";
    case TokenKind::kw_let: return "kw_let";
    case TokenKind::kw_var: return "kw_var";
    case TokenKind::kw_if: return "kw_if";
    case TokenKind::kw_else: return "kw_else";
    case TokenKind::kw_for: return "kw_for";
    case TokenKind::kw_in: return "kw_in";
    case TokenKind::kw_while: return "kw_while";
    case TokenKind::kw_break: return "kw_break";
    case TokenKind::kw_continue: return "kw_continue";
    case TokenKind::kw_defer: return "kw_defer";
    case TokenKind::kw_return: return "kw_return";
    case TokenKind::kw_true: return "kw_true";
    case TokenKind::kw_false: return "kw_false";
    case TokenKind::kw_null: return "kw_null";
    case TokenKind::kw_void: return "kw_void";
    case TokenKind::kw_bool: return "kw_bool";
    case TokenKind::kw_i8: return "kw_i8";
    case TokenKind::kw_u8: return "kw_u8";
    case TokenKind::kw_i16: return "kw_i16";
    case TokenKind::kw_u16: return "kw_u16";
    case TokenKind::kw_i32: return "kw_i32";
    case TokenKind::kw_u32: return "kw_u32";
    case TokenKind::kw_i64: return "kw_i64";
    case TokenKind::kw_u64: return "kw_u64";
    case TokenKind::kw_isize: return "kw_isize";
    case TokenKind::kw_usize: return "kw_usize";
    case TokenKind::kw_f32: return "kw_f32";
    case TokenKind::kw_f64: return "kw_f64";
    case TokenKind::kw_str: return "kw_str";
    case TokenKind::kw_mut: return "kw_mut";
    case TokenKind::kw_cast: return "kw_cast";
    case TokenKind::kw_pcast: return "kw_pcast";
    case TokenKind::kw_bcast: return "kw_bcast";
    case TokenKind::kw_size_of: return "kw_size_of";
    case TokenKind::kw_align_of: return "kw_align_of";
    case TokenKind::kw_ptr_addr: return "kw_ptr_addr";
    case TokenKind::kw_paddr: return "kw_paddr";
    case TokenKind::kw_str_data: return "kw_str_data";
    case TokenKind::kw_str_byte_len: return "kw_str_byte_len";
    case TokenKind::kw_str_from_bytes_unchecked: return "kw_str_from_bytes_unchecked";
    case TokenKind::l_paren: return "l_paren";
    case TokenKind::r_paren: return "r_paren";
    case TokenKind::l_brace: return "l_brace";
    case TokenKind::r_brace: return "r_brace";
    case TokenKind::l_bracket: return "l_bracket";
    case TokenKind::r_bracket: return "r_bracket";
    case TokenKind::comma: return "comma";
    case TokenKind::dot: return "dot";
    case TokenKind::ellipsis: return "ellipsis";
    case TokenKind::semicolon: return "semicolon";
    case TokenKind::colon: return "colon";
    case TokenKind::colon_colon: return "colon_colon";
    case TokenKind::arrow: return "arrow";
    case TokenKind::fat_arrow: return "fat_arrow";
    case TokenKind::plus: return "plus";
    case TokenKind::plus_plus: return "plus_plus";
    case TokenKind::plus_equal: return "plus_equal";
    case TokenKind::minus: return "minus";
    case TokenKind::minus_minus: return "minus_minus";
    case TokenKind::minus_equal: return "minus_equal";
    case TokenKind::star: return "star";
    case TokenKind::star_equal: return "star_equal";
    case TokenKind::slash: return "slash";
    case TokenKind::slash_equal: return "slash_equal";
    case TokenKind::percent: return "percent";
    case TokenKind::percent_equal: return "percent_equal";
    case TokenKind::amp: return "amp";
    case TokenKind::amp_equal: return "amp_equal";
    case TokenKind::pipe: return "pipe";
    case TokenKind::pipe_equal: return "pipe_equal";
    case TokenKind::caret: return "caret";
    case TokenKind::caret_equal: return "caret_equal";
    case TokenKind::tilde: return "tilde";
    case TokenKind::bang: return "bang";
    case TokenKind::equal: return "equal";
    case TokenKind::equal_equal: return "equal_equal";
    case TokenKind::bang_equal: return "bang_equal";
    case TokenKind::less: return "less";
    case TokenKind::less_equal: return "less_equal";
    case TokenKind::greater: return "greater";
    case TokenKind::greater_equal: return "greater_equal";
    case TokenKind::less_less: return "less_less";
    case TokenKind::less_less_equal: return "less_less_equal";
    case TokenKind::greater_greater: return "greater_greater";
    case TokenKind::greater_greater_equal: return "greater_greater_equal";
    case TokenKind::amp_amp: return "amp_amp";
    case TokenKind::pipe_pipe: return "pipe_pipe";
    case TokenKind::question: return "question";
    case TokenKind::at: return "at";
    }
    return "unknown";
}

namespace {

void indent(std::ostringstream& out, const int depth) {
    for (int i = 0; i < depth; ++i) {
        out << "  ";
    }
}

std::string_view primitive_name(const PrimitiveTypeKind kind) {
    switch (kind) {
    case PrimitiveTypeKind::void_: return "void";
    case PrimitiveTypeKind::bool_: return "bool";
    case PrimitiveTypeKind::i8: return "i8";
    case PrimitiveTypeKind::u8: return "u8";
    case PrimitiveTypeKind::i16: return "i16";
    case PrimitiveTypeKind::u16: return "u16";
    case PrimitiveTypeKind::i32: return "i32";
    case PrimitiveTypeKind::u32: return "u32";
    case PrimitiveTypeKind::i64: return "i64";
    case PrimitiveTypeKind::u64: return "u64";
    case PrimitiveTypeKind::isize: return "isize";
    case PrimitiveTypeKind::usize: return "usize";
    case PrimitiveTypeKind::f32: return "f32";
    case PrimitiveTypeKind::f64: return "f64";
    case PrimitiveTypeKind::str: return "str";
    }
    return "unknown";
}

std::string type_label(const AstModule& module, const TypeId id) {
    if (!is_valid(id) || id.value >= module.types.size()) {
        return "<invalid-type>";
    }
    const TypeNode& type = module.types[id.value];
    std::ostringstream out;
    switch (type.kind) {
    case TypeKind::primitive:
        out << primitive_name(type.primitive);
        break;
    case TypeKind::named:
        if (!type.scope_name.empty()) {
            out << type.scope_name << "::";
        }
        out << type.name;
        if (!type.type_args.empty()) {
            out << "<";
            for (base::usize i = 0; i < type.type_args.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << type_label(module, type.type_args[i]);
            }
            out << ">";
        }
        break;
    case TypeKind::pointer:
        out << "*" << (type.pointer_mutability == PointerMutability::mut ? "mut " : "const ");
        out << type_label(module, type.pointee);
        break;
    case TypeKind::array:
        out << "[" << type.array_count << "]" << type_label(module, type.array_element);
        break;
    }
    return out.str();
}

std::string pattern_label(const AstModule& module, const PatternId id) {
    if (!is_valid(id) || id.value >= module.patterns.size()) {
        return "<invalid-pattern>";
    }
    const PatternNode& pattern = module.patterns[id.value];
    if (pattern.kind == PatternKind::wildcard) {
        return "_";
    }
    if (pattern.kind == PatternKind::literal) {
        return std::string(pattern.case_name);
    }
    if (pattern.kind == PatternKind::or_pattern) {
        std::string label;
        for (base::usize i = 0; i < pattern.alternatives.size(); ++i) {
            if (i != 0) {
                label += " | ";
            }
            label += pattern_label(module, pattern.alternatives[i]);
        }
        return label;
    }
    std::string label;
    if (pattern.scoped) {
        if (!pattern.enum_name.empty()) {
            label += std::string(pattern.enum_name);
        }
        label += ".";
    }
    label += std::string(pattern.case_name);
    if (!pattern.binding_name.empty()) {
        label += "(" + std::string(pattern.binding_name) + ")";
    }
    return label;
}

std::string_view item_kind_name(const ItemKind kind) {
    switch (kind) {
    case ItemKind::const_decl: return "const";
    case ItemKind::type_alias: return "type_alias";
    case ItemKind::struct_decl: return "struct";
    case ItemKind::enum_decl: return "enum";
    case ItemKind::opaque_struct_decl: return "opaque_struct";
    case ItemKind::fn_decl: return "fn";
    case ItemKind::extern_block: return "extern_block";
    case ItemKind::impl_block: return "impl";
    }
    return "unknown";
}

void dump_visibility(std::ostringstream& out, const Visibility visibility) {
    if (visibility == Visibility::public_) {
        out << "pub ";
        return;
    }
    out << "priv ";
}

std::string_view stmt_kind_name(const StmtKind kind) {
    switch (kind) {
    case StmtKind::let: return "let";
    case StmtKind::var: return "var";
    case StmtKind::assign: return "assign";
    case StmtKind::if_: return "if";
    case StmtKind::for_: return "for";
    case StmtKind::for_range: return "for_range";
    case StmtKind::while_: return "while";
    case StmtKind::break_: return "break";
    case StmtKind::continue_: return "continue";
    case StmtKind::defer: return "defer";
    case StmtKind::return_: return "return";
    case StmtKind::expr: return "expr";
    case StmtKind::block: return "block";
    }
    return "unknown";
}

std::string_view expr_kind_name(const ExprKind kind) {
    switch (kind) {
    case ExprKind::invalid: return "invalid";
    case ExprKind::integer_literal: return "integer_literal";
    case ExprKind::float_literal: return "float_literal";
    case ExprKind::bool_literal: return "bool_literal";
    case ExprKind::null_literal: return "null_literal";
    case ExprKind::string_literal: return "string_literal";
    case ExprKind::c_string_literal: return "c_string_literal";
    case ExprKind::byte_literal: return "byte_literal";
    case ExprKind::name: return "name";
    case ExprKind::unary: return "unary";
    case ExprKind::binary: return "binary";
    case ExprKind::call: return "call";
    case ExprKind::try_expr: return "try_expr";
    case ExprKind::if_expr: return "if_expr";
    case ExprKind::block_expr: return "block_expr";
    case ExprKind::match_expr: return "match_expr";
    case ExprKind::field: return "field";
    case ExprKind::index: return "index";
    case ExprKind::struct_literal: return "struct_literal";
    case ExprKind::cast: return "cast";
    case ExprKind::pcast: return "pcast";
    case ExprKind::bcast: return "bcast";
    case ExprKind::size_of: return "size_of";
    case ExprKind::align_of: return "align_of";
    case ExprKind::ptr_addr: return "ptr_addr";
    case ExprKind::paddr: return "paddr";
    case ExprKind::str_data: return "str_data";
    case ExprKind::str_byte_len: return "str_byte_len";
    case ExprKind::str_from_bytes_unchecked: return "str_from_bytes_unchecked";
    }
    return "unknown";
}

void dump_expr(std::ostringstream& out, const AstModule& module, ExprId id, int depth);

void dump_stmt(std::ostringstream& out, const AstModule& module, const StmtId id, const int depth) {
    if (!is_valid(id) || id.value >= module.stmts.size()) {
        indent(out, depth);
        out << "stmt <invalid>\n";
        return;
    }
    const StmtNode& stmt = module.stmts[id.value];
    indent(out, depth);
    out << "stmt #" << id.value << " " << stmt_kind_name(stmt.kind);
    if (!stmt.name.empty()) {
        out << " " << stmt.name;
    }
    if (is_valid(stmt.declared_type)) {
        out << " : " << type_label(module, stmt.declared_type);
    }
    out << "\n";
    if (is_valid(stmt.init)) {
        dump_expr(out, module, stmt.init, depth + 1);
    }
    if (is_valid(stmt.lhs)) {
        dump_expr(out, module, stmt.lhs, depth + 1);
    }
    if (is_valid(stmt.rhs)) {
        dump_expr(out, module, stmt.rhs, depth + 1);
    }
    if (is_valid(stmt.condition)) {
        dump_expr(out, module, stmt.condition, depth + 1);
    }
    if (is_valid(stmt.range_start)) {
        dump_expr(out, module, stmt.range_start, depth + 1);
    }
    if (is_valid(stmt.range_end)) {
        dump_expr(out, module, stmt.range_end, depth + 1);
    }
    if (is_valid(stmt.range_step)) {
        dump_expr(out, module, stmt.range_step, depth + 1);
    }
    if (is_valid(stmt.then_block)) {
        dump_stmt(out, module, stmt.then_block, depth + 1);
    }
    if (is_valid(stmt.else_block)) {
        dump_stmt(out, module, stmt.else_block, depth + 1);
    }
    if (is_valid(stmt.else_if)) {
        dump_stmt(out, module, stmt.else_if, depth + 1);
    }
    if (is_valid(stmt.body)) {
        dump_stmt(out, module, stmt.body, depth + 1);
    }
    if (is_valid(stmt.for_init)) {
        dump_stmt(out, module, stmt.for_init, depth + 1);
    }
    if (is_valid(stmt.for_update)) {
        dump_stmt(out, module, stmt.for_update, depth + 1);
    }
    if (is_valid(stmt.return_value)) {
        dump_expr(out, module, stmt.return_value, depth + 1);
    }
    for (StmtId child : stmt.statements) {
        dump_stmt(out, module, child, depth + 1);
    }
}

void dump_expr(std::ostringstream& out, const AstModule& module, const ExprId id, const int depth) {
    if (!is_valid(id) || id.value >= module.exprs.size()) {
        indent(out, depth);
        out << "expr <invalid>\n";
        return;
    }
    const ExprNode& expr = module.exprs[id.value];
    indent(out, depth);
    out << "expr #" << id.value << " " << expr_kind_name(expr.kind);
    if (!expr.text.empty()) {
        out << " `";
        if (!expr.scope_name.empty()) {
            out << expr.scope_name << "::";
        }
        out << expr.text << "`";
        if (!expr.type_args.empty()) {
            out << "<";
            for (base::usize i = 0; i < expr.type_args.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << type_label(module, expr.type_args[i]);
            }
            out << ">";
        }
    }
    if (!expr.field_name.empty()) {
        out << " ." << expr.field_name;
        if (!expr.type_args.empty()) {
            out << "<";
            for (base::usize i = 0; i < expr.type_args.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << type_label(module, expr.type_args[i]);
            }
            out << ">";
        }
    }
    if (!expr.struct_name.empty()) {
        out << " ";
        if (!expr.scope_name.empty()) {
            out << expr.scope_name << "::";
        }
        out << expr.struct_name;
        if (!expr.struct_type_args.empty()) {
            out << "<";
            for (base::usize i = 0; i < expr.struct_type_args.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << type_label(module, expr.struct_type_args[i]);
            }
            out << ">";
        }
    }
    if (is_valid(expr.cast_type)) {
        out << " to " << type_label(module, expr.cast_type);
    }
    out << "\n";
    if (is_valid(expr.unary_operand)) {
        dump_expr(out, module, expr.unary_operand, depth + 1);
    }
    if (is_valid(expr.binary_lhs)) {
        dump_expr(out, module, expr.binary_lhs, depth + 1);
    }
    if (is_valid(expr.binary_rhs)) {
        dump_expr(out, module, expr.binary_rhs, depth + 1);
    }
    if (is_valid(expr.callee)) {
        dump_expr(out, module, expr.callee, depth + 1);
    }
    for (ExprId arg : expr.args) {
        dump_expr(out, module, arg, depth + 1);
    }
    if (is_valid(expr.condition)) {
        dump_expr(out, module, expr.condition, depth + 1);
    }
    if (is_valid(expr.then_expr)) {
        dump_expr(out, module, expr.then_expr, depth + 1);
    }
    if (is_valid(expr.else_expr)) {
        dump_expr(out, module, expr.else_expr, depth + 1);
    }
    if (is_valid(expr.block)) {
        dump_stmt(out, module, expr.block, depth + 1);
    }
    if (is_valid(expr.block_result)) {
        dump_expr(out, module, expr.block_result, depth + 1);
    }
    if (is_valid(expr.match_value)) {
        dump_expr(out, module, expr.match_value, depth + 1);
    }
    for (const MatchArm& arm : expr.match_arms) {
        indent(out, depth + 1);
        out << "match_arm " << pattern_label(module, arm.pattern) << "\n";
        if (is_valid(arm.guard)) {
            indent(out, depth + 2);
            out << "guard\n";
            dump_expr(out, module, arm.guard, depth + 3);
        }
        dump_expr(out, module, arm.value, depth + 2);
    }
    if (is_valid(expr.object)) {
        dump_expr(out, module, expr.object, depth + 1);
    }
    if (is_valid(expr.index)) {
        dump_expr(out, module, expr.index, depth + 1);
    }
    for (const FieldInit& init : expr.field_inits) {
        indent(out, depth + 1);
        out << "field_init " << init.name << "\n";
        dump_expr(out, module, init.value, depth + 2);
    }
    if (is_valid(expr.cast_expr)) {
        dump_expr(out, module, expr.cast_expr, depth + 1);
    }
}

void dump_item(std::ostringstream& out, const AstModule& module, const ItemId id, const int depth) {
    if (!is_valid(id) || id.value >= module.items.size()) {
        indent(out, depth);
        out << "item <invalid>\n";
        return;
    }
    const ItemNode& item = module.items[id.value];
    indent(out, depth);
    out << "item #" << id.value << " ";
    dump_visibility(out, item.visibility);
    out << item_kind_name(item.kind);
    if (!item.name.empty()) {
        out << " " << item.name;
        if (!item.generic_params.empty()) {
            out << "<";
            for (base::usize i = 0; i < item.generic_params.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << item.generic_params[i];
            }
            out << ">";
        }
    } else if (item.kind == ItemKind::impl_block && !item.generic_params.empty()) {
        out << "<";
        for (base::usize i = 0; i < item.generic_params.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << item.generic_params[i];
        }
        out << ">";
    }
    if (is_valid(item.impl_type)) {
        out << " for " << type_label(module, item.impl_type);
    }
    if (item.is_export_c) {
        out << " export_c";
    }
    if (item.is_extern_c) {
        out << " extern_c";
    }
    if (item.is_variadic) {
        out << " variadic";
    }
    if (item.is_prototype) {
        out << " prototype";
    }
    if (!item.abi_name.empty()) {
        out << " @name=" << item.abi_name;
    }
    out << "\n";
    for (const FieldDecl& field : item.fields) {
        indent(out, depth + 1);
        out << "field ";
        dump_visibility(out, field.visibility);
        out << field.name << " : " << type_label(module, field.type) << "\n";
    }
    for (const EnumCaseDecl& enum_case : item.enum_cases) {
        indent(out, depth + 1);
        out << "case " << enum_case.name;
        if (is_valid(enum_case.payload_type)) {
            out << "(" << type_label(module, enum_case.payload_type) << ")";
        }
        out << " = " << enum_case.value_text << "\n";
    }
    for (const ParamDecl& param : item.params) {
        indent(out, depth + 1);
        out << "param " << param.name << " : " << type_label(module, param.type) << "\n";
    }
    if (is_valid(item.return_type)) {
        indent(out, depth + 1);
        out << "return " << type_label(module, item.return_type) << "\n";
    }
    if (is_valid(item.const_value)) {
        dump_expr(out, module, item.const_value, depth + 1);
    }
    if (is_valid(item.alias_type)) {
        indent(out, depth + 1);
        out << "alias " << type_label(module, item.alias_type) << "\n";
    }
    if (is_valid(item.body)) {
        dump_stmt(out, module, item.body, depth + 1);
    }
    for (ItemId extern_item : item.extern_items) {
        dump_item(out, module, extern_item, depth + 1);
    }
    for (ItemId impl_item : item.impl_items) {
        dump_item(out, module, impl_item, depth + 1);
    }
}

[[nodiscard]] std::unordered_set<base::u32> collect_nested_items(const AstModule& module) {
    std::unordered_set<base::u32> nested;
    for (const ItemNode& item : module.items) {
        if (item.kind == ItemKind::extern_block) {
            for (ItemId id : item.extern_items) {
                if (is_valid(id)) {
                    nested.insert(id.value);
                }
            }
        } else if (item.kind == ItemKind::impl_block) {
            for (ItemId id : item.impl_items) {
                if (is_valid(id)) {
                    nested.insert(id.value);
                }
            }
        }
    }
    return nested;
}

} // namespace

std::string dump_tokens(const std::span<const Token> tokens) {
    std::ostringstream out;
    for (const Token& token : tokens) {
        out << token.range.begin << ".." << token.range.end << " "
            << token_kind_name(token.kind);
        if (!token.text.empty()) {
            out << " `" << token.text << "`";
        }
        out << "\n";
    }
    return out.str();
}

std::string dump_ast(const AstModule& module) {
    std::ostringstream out;
    out << "module";
    for (base::usize i = 0; i < module.module_path.parts.size(); ++i) {
        out << (i == 0 ? " " : ".") << module.module_path.parts[i];
    }
    out << "\n";
    for (const ImportDecl& import : module.imports) {
        dump_visibility(out, import.visibility);
        out << "import";
        for (base::usize i = 0; i < import.path.parts.size(); ++i) {
            out << (i == 0 ? " " : ".") << import.path.parts[i];
        }
        if (!import.alias.empty()) {
            out << " as " << import.alias;
        }
        out << "\n";
    }
    const std::unordered_set<base::u32> nested = collect_nested_items(module);
    for (base::u32 i = 0; i < module.items.size(); ++i) {
        if (nested.contains(i)) {
            continue;
        }
        dump_item(out, module, ItemId {i}, 1);
    }
    return out.str();
}

} // namespace aurex::syntax

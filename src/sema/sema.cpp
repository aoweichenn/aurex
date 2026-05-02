#include "aurex/sema/sema.hpp"

#include "aurex/syntax/module.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] BuiltinType map_builtin(const syntax::PrimitiveTypeKind kind) noexcept {
    switch (kind) {
    case syntax::PrimitiveTypeKind::void_: return BuiltinType::void_;
    case syntax::PrimitiveTypeKind::bool_: return BuiltinType::bool_;
    case syntax::PrimitiveTypeKind::i8: return BuiltinType::i8;
    case syntax::PrimitiveTypeKind::u8: return BuiltinType::u8;
    case syntax::PrimitiveTypeKind::i16: return BuiltinType::i16;
    case syntax::PrimitiveTypeKind::u16: return BuiltinType::u16;
    case syntax::PrimitiveTypeKind::i32: return BuiltinType::i32;
    case syntax::PrimitiveTypeKind::u32: return BuiltinType::u32;
    case syntax::PrimitiveTypeKind::i64: return BuiltinType::i64;
    case syntax::PrimitiveTypeKind::u64: return BuiltinType::u64;
    case syntax::PrimitiveTypeKind::isize: return BuiltinType::isize;
    case syntax::PrimitiveTypeKind::usize: return BuiltinType::usize;
    case syntax::PrimitiveTypeKind::f32: return BuiltinType::f32;
    case syntax::PrimitiveTypeKind::f64: return BuiltinType::f64;
    case syntax::PrimitiveTypeKind::str: return BuiltinType::str;
    }
    return BuiltinType::void_;
}

[[nodiscard]] PointerMutability map_mutability(const syntax::PointerMutability mutability) noexcept {
    return mutability == syntax::PointerMutability::mut ? PointerMutability::mut : PointerMutability::const_;
}

[[nodiscard]] bool is_function_item(const syntax::ItemNode& item) noexcept {
    return item.kind == syntax::ItemKind::fn_decl;
}

[[nodiscard]] std::string abi_or_c_name(const syntax::ItemNode& item, const std::string& c_name) {
    if (!item.abi_name.empty()) {
        return std::string(item.abi_name);
    }
    return c_name;
}

[[nodiscard]] base::u64 align_forward(const base::u64 offset, const base::u64 alignment) noexcept {
    if (alignment == 0) {
        return offset;
    }
    const base::u64 mask = alignment - 1;
    return (offset + mask) & ~mask;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept
    : module_(module), diagnostics_(diagnostics) {}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    checked_.expr_types.assign(module_.exprs.size(), invalid_type_handle);
    checked_.expr_c_names.assign(module_.exprs.size(), {});
    checked_.syntax_type_handles.assign(module_.types.size(), invalid_type_handle);
    checked_.item_c_names.assign(module_.items.size(), {});
    register_type_names();
    analyze_struct_properties();
    register_value_names();
    analyze_const_decls();

    for (const syntax::ItemNode& item : module_.items) {
        if (is_function_item(item) && !item.is_extern_c && syntax::is_valid(item.body)) {
            analyze_function_body(item);
        }
    }

    if (diagnostics_.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, "semantic analysis failed"});
    }
    return base::Result<CheckedModule>::ok(std::move(checked_));
}

void SemanticAnalyzer::register_type_names() {
    for (const syntax::ItemNode& item : module_.items) {
        const syntax::ModuleId owner = item_module(item);
        const std::string key = module_key(owner, item.name);
        const std::string qualified = qualified_name(owner, item.name);
        const std::string c_name = c_symbol_name(owner, item.name);
        TypeHandle handle = invalid_type_handle;
        if (item.kind == syntax::ItemKind::struct_decl) {
            handle = checked_.types.named_struct(qualified, c_name, false);
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            handle = checked_.types.named_enum(qualified, c_name);
        } else if (item.kind == syntax::ItemKind::opaque_struct_decl) {
            handle = checked_.types.opaque_struct(qualified, c_name);
        }

        if (!is_valid(handle)) {
            continue;
        }
        const auto* const begin = module_.items.data();
        const base::usize item_index = static_cast<base::usize>(&item - begin);
        if (item_index < checked_.item_c_names.size()) {
            checked_.item_c_names[item_index] = c_name;
        }
        auto inserted = named_types_.emplace(key, handle);
        if (!inserted.second) {
            report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            continue;
        }

        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::opaque_struct_decl) {
            StructInfo info;
            info.name = std::string(item.name);
            info.c_name = c_name;
            info.module = owner;
            info.type = handle;
            info.is_opaque = item.kind == syntax::ItemKind::opaque_struct_decl;
            auto struct_inserted = checked_.structs.emplace(key, std::move(info));
            if (!struct_inserted.second) {
                report(item.range, "duplicate struct definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
        }
    }
}

void SemanticAnalyzer::register_value_names() {
    for (const syntax::ItemNode& item : module_.items) {
        current_module_ = item_module(item);
        const std::string key = module_key(current_module_, item.name);
        const std::string c_name = c_symbol_name(current_module_, item.name);
        if (item.kind == syntax::ItemKind::fn_decl) {
            FunctionSignature signature;
            signature.name = std::string(item.name);
            signature.c_name = abi_or_c_name(item, c_name);
            signature.module = current_module_;
            signature.return_type = resolve_type(item.return_type);
            signature.range = item.range;
            signature.is_extern_c = item.is_extern_c;
            signature.is_export_c = item.is_export_c;
            for (const syntax::ParamDecl& param : item.params) {
                TypeHandle param_type = resolve_type(param.type);
                if (!is_valid_storage_type(param_type)) {
                    report(param.range, "function parameter type is not valid storage");
                }
                if (checked_.types.is_array(param_type)) {
                    report(param.range, "array type cannot be used as a function parameter");
                }
                if (checked_.types.contains_array(param_type)) {
                    report(param.range, "struct containing array cannot be passed by value");
                }
                signature.param_types.push_back(param_type);
            }
            if (checked_.types.is_array(signature.return_type)) {
                report(item.range, "array type cannot be used as a function return type");
            }
            if (checked_.types.contains_array(signature.return_type)) {
                report(item.range, "struct containing array cannot be returned by value");
            }
            const auto* const begin = module_.items.data();
            const base::usize item_index = static_cast<base::usize>(&item - begin);
            if (item_index < checked_.item_c_names.size()) {
                checked_.item_c_names[item_index] = signature.c_name;
            }
            auto inserted = checked_.functions.emplace(key, std::move(signature));
            if (!inserted.second) {
                report(item.range, "function overloading is not allowed in module " + module_name(current_module_) + ": " + std::string(item.name));
            }
            const auto value_inserted = global_values_.emplace(key, Symbol {
                SymbolKind::function,
                std::string(item.name),
                inserted.first->second.c_name,
                current_module_,
                inserted.first->second.return_type,
                item.range,
                false,
            });
            if (!value_inserted.second) {
                report(item.range, "duplicate value definition in module " + module_name(current_module_) + ": " + std::string(item.name));
            }
        } else if (item.kind == syntax::ItemKind::const_decl) {
            TypeHandle type = resolve_type(item.const_type);
            const auto* const begin = module_.items.data();
            const base::usize item_index = static_cast<base::usize>(&item - begin);
            if (item_index < checked_.item_c_names.size()) {
                checked_.item_c_names[item_index] = c_name;
            }
            const auto inserted = global_values_.emplace(key, Symbol {
                SymbolKind::const_,
                std::string(item.name),
                c_name,
                current_module_,
                type,
                item.range,
                false,
            });
            if (!inserted.second) {
                report(item.range, "duplicate value definition in module " + module_name(current_module_) + ": " + std::string(item.name));
            }
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            const TypeHandle enum_type = resolve_type(item.enum_base_type);
            if (!checked_.types.is_integer(enum_type)) {
                report(item.range, "enum base type must be an integer type");
            }
            const auto type_found = named_types_.find(key);
            const TypeHandle named_enum_type = type_found == named_types_.end() ? enum_type : type_found->second;
            if (is_valid(named_enum_type)) {
                checked_.types.set_enum_underlying(named_enum_type, enum_type);
            }
            for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
                const std::string full_name = std::string(item.name) + "_" + std::string(enum_case.name);
                const std::string enum_case_key = module_key(current_module_, full_name);
                checked_.enum_cases.emplace(enum_case_key, EnumCaseInfo {
                    full_name,
                    c_symbol_name(current_module_, full_name),
                    current_module_,
                    named_enum_type,
                    std::string(enum_case.value_text),
                    enum_case.range,
                });
                const auto value_inserted = global_values_.emplace(enum_case_key, Symbol {
                    SymbolKind::enum_case,
                    full_name,
                    c_symbol_name(current_module_, full_name),
                    current_module_,
                    named_enum_type,
                    enum_case.range,
                    false,
                });
                if (!value_inserted.second) {
                    report(enum_case.range, "duplicate value definition in module " + module_name(current_module_) + ": " + full_name);
                }
            }
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_struct_properties() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::struct_decl) {
            continue;
        }
        current_module_ = item_module(item);
        const std::string key = module_key(current_module_, item.name);
        bool contains_array = false;
        bool copyable = true;
        for (const syntax::FieldDecl& field : item.fields) {
            const TypeHandle field_type = resolve_type(field.type);
            if (!is_valid_storage_type(field_type)) {
                report(field.range, "field type is not valid storage");
            }
            if (const auto struct_found = checked_.structs.find(key); struct_found != checked_.structs.end()) {
                struct_found->second.fields.push_back(StructFieldInfo {
                    std::string(field.name),
                    {},
                    syntax::invalid_module_id,
                    field_type,
                    field.range,
                });
            }
            if (checked_.types.contains_array(field_type)) {
                contains_array = true;
            }
            if (!checked_.types.is_copyable(field_type)) {
                copyable = false;
            }
        }
        const auto found = named_types_.find(key);
        if (found != named_types_.end()) {
            checked_.types.set_record_properties(found->second, contains_array, copyable && !contains_array);
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_const_decls() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::const_decl) {
            continue;
        }
        current_module_ = item_module(item);
        const TypeHandle declared = resolve_type(item.const_type);
        const TypeHandle actual = analyze_expr(item.const_value);
        if (!is_valid_storage_type(declared)) {
            report(item.range, "const type is not valid storage");
        }
        if (!can_assign(declared, actual, item.const_value)) {
            report(item.range, "const initializer type does not match declared type");
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_function_body(const syntax::ItemNode& function) {
    current_module_ = item_module(function);
    const auto found = checked_.functions.find(module_key(current_module_, function.name));
    const TypeHandle expected_return = found == checked_.functions.end()
        ? checked_.types.builtin(BuiltinType::void_)
        : found->second.return_type;

    symbols_.push_scope();
    for (const syntax::ParamDecl& param : function.params) {
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::parameter,
            std::string(param.name),
            {},
            syntax::invalid_module_id,
            resolve_type(param.type),
            param.range,
            false,
        }, diagnostics_));
    }
    analyze_block(function.body, expected_return);
    symbols_.pop_scope();
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_block(const syntax::StmtId block, const TypeHandle expected_return) {
    if (!syntax::is_valid(block) || block.value >= module_.stmts.size()) {
        return;
    }
    symbols_.push_scope();
    const syntax::StmtNode& stmt = module_.stmts[block.value];
    if (stmt.kind != syntax::StmtKind::block) {
        return;
    }
    for (syntax::StmtId child : stmt.statements) {
        analyze_stmt(child, expected_return);
    }
    symbols_.pop_scope();
}

void SemanticAnalyzer::analyze_stmt(const syntax::StmtId stmt_id, const TypeHandle expected_return) {
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= module_.stmts.size()) {
        return;
    }
    const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
    switch (stmt.kind) {
    case syntax::StmtKind::let:
    case syntax::StmtKind::var: {
        const TypeHandle declared = resolve_type(stmt.declared_type);
        const TypeHandle init = analyze_expr(stmt.init);
        if (!is_valid_storage_type(declared)) {
            report(stmt.range, "local variable type is not valid storage");
        }
        if (!can_assign(declared, init, stmt.init)) {
            report(stmt.range, "initializer type does not match declared type");
        }
        if (!checked_.types.is_copyable(declared)) {
            report(stmt.range, "non-copyable storage type cannot be implicitly copied");
        }
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::local,
            std::string(stmt.name),
            {},
            syntax::invalid_module_id,
            declared,
            stmt.range,
            stmt.kind == syntax::StmtKind::var,
        }, diagnostics_));
        break;
    }
    case syntax::StmtKind::assign: {
        if (!is_writable_place(stmt.lhs)) {
            report(module_.exprs[stmt.lhs.value].range, "left side of assignment must be writable");
        }
        const TypeHandle lhs = analyze_expr(stmt.lhs);
        const TypeHandle rhs = analyze_expr(stmt.rhs);
        if (!can_assign(lhs, rhs, stmt.rhs)) {
            report(stmt.range, "assignment type mismatch");
        }
        if (!checked_.types.is_copyable(lhs)) {
            report(stmt.range, "array or array-containing type cannot be assigned");
        }
        break;
    }
    case syntax::StmtKind::if_: {
        const TypeHandle condition = analyze_expr(stmt.condition);
        if (!checked_.types.is_bool(condition)) {
            report(module_.exprs[stmt.condition.value].range, "if condition must be bool");
        }
        analyze_block(stmt.then_block, expected_return);
        if (syntax::is_valid(stmt.else_block)) {
            analyze_block(stmt.else_block, expected_return);
        }
        break;
    }
    case syntax::StmtKind::while_: {
        const TypeHandle condition = analyze_expr(stmt.condition);
        if (!checked_.types.is_bool(condition)) {
            report(module_.exprs[stmt.condition.value].range, "while condition must be bool");
        }
        ++loop_depth_;
        analyze_block(stmt.body, expected_return);
        --loop_depth_;
        break;
    }
    case syntax::StmtKind::return_: {
        const TypeHandle actual = syntax::is_valid(stmt.return_value)
            ? analyze_expr(stmt.return_value)
            : checked_.types.builtin(BuiltinType::void_);
        if (!can_assign(expected_return, actual, stmt.return_value)) {
            report(stmt.range, "return type mismatch");
        }
        break;
    }
    case syntax::StmtKind::expr:
        if (syntax::is_valid(stmt.init) &&
            stmt.init.value < module_.exprs.size() &&
            module_.exprs[stmt.init.value].kind != syntax::ExprKind::call) {
            report(module_.exprs[stmt.init.value].range, "expression statement must be a function call");
        }
        static_cast<void>(analyze_expr(stmt.init));
        break;
    case syntax::StmtKind::block:
        analyze_block(stmt_id, expected_return);
        break;
    case syntax::StmtKind::break_:
    case syntax::StmtKind::continue_:
        if (loop_depth_ == 0) {
            report(stmt.range, "break and continue are only valid inside while loops");
        }
        break;
    }
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return invalid_type_handle;
    }

    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::i32));
    case syntax::ExprKind::bool_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
    case syntax::ExprKind::null_literal:
        return record_expr_type(expr_id, invalid_type_handle);
    case syntax::ExprKind::string_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::str));
    case syntax::ExprKind::c_string_literal:
        return record_expr_type(expr_id, checked_.types.pointer(PointerMutability::const_, checked_.types.builtin(BuiltinType::u8)));
    case syntax::ExprKind::byte_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::u8));
    case syntax::ExprKind::name: {
        const Symbol* symbol = find_symbol(expr.text, expr.range);
        if (symbol == nullptr) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (!symbol->c_name.empty() && expr_id.value < checked_.expr_c_names.size()) {
            checked_.expr_c_names[expr_id.value] = symbol->c_name;
        }
        return record_expr_type(expr_id, symbol->type);
    }
    case syntax::ExprKind::call: {
        if (!syntax::is_valid(expr.callee) || module_.exprs[expr.callee.value].kind != syntax::ExprKind::name) {
            report(expr.range, "callee must be a function name");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const std::string name(module_.exprs[expr.callee.value].text);
        const FunctionSignature* signature = find_function_in_visible_modules(name, module_.exprs[expr.callee.value].range);
        if (signature == nullptr) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (expr.callee.value < checked_.expr_c_names.size()) {
            checked_.expr_c_names[expr.callee.value] = signature->c_name;
        }
        if (signature->param_types.size() != expr.args.size()) {
            report(expr.range, "argument count mismatch in call to " + name);
        }
        const base::usize count = expr.args.size() < signature->param_types.size() ? expr.args.size() : signature->param_types.size();
        for (base::usize i = 0; i < count; ++i) {
            const TypeHandle actual = analyze_expr(expr.args[i]);
            if (!can_assign(signature->param_types[i], actual, expr.args[i])) {
                report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
            }
            if (is_copy_forbidden_value(signature->param_types[i])) {
                report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
            }
        }
        return record_expr_type(expr_id, signature->return_type);
    }
    case syntax::ExprKind::unary: {
        const TypeHandle operand = analyze_expr(expr.unary_operand);
        if (expr.unary_op == syntax::UnaryOp::logical_not && !checked_.types.is_bool(operand)) {
            report(expr.range, "logical not requires bool operand");
        }
        if ((expr.unary_op == syntax::UnaryOp::numeric_negate || expr.unary_op == syntax::UnaryOp::bitwise_not) &&
            !checked_.types.is_integer(operand) && !checked_.types.is_float(operand)) {
            report(expr.range, "numeric unary operator requires numeric operand");
        }
        if (expr.unary_op == syntax::UnaryOp::dereference) {
            if (!checked_.types.is_pointer(operand)) {
                report(expr.range, "dereference requires pointer operand");
                return record_expr_type(expr_id, invalid_type_handle);
            }
            return record_expr_type(expr_id, checked_.types.get(operand).pointee);
        }
        if (expr.unary_op == syntax::UnaryOp::address_of) {
            if (!is_place_expr(expr.unary_operand)) {
                report(expr.range, "address-of requires a place expression");
            }
            const PointerMutability mutability = is_writable_place(expr.unary_operand)
                ? PointerMutability::mut
                : PointerMutability::const_;
            return record_expr_type(expr_id, checked_.types.pointer(mutability, operand));
        }
        return record_expr_type(expr_id, operand);
    }
    case syntax::ExprKind::binary: {
        const TypeHandle lhs = analyze_expr(expr.binary_lhs);
        const TypeHandle rhs = analyze_expr(expr.binary_rhs);
        const bool is_equality =
            expr.binary_op == syntax::BinaryOp::equal ||
            expr.binary_op == syntax::BinaryOp::not_equal;
        const bool is_null_pointer_comparison =
            is_equality &&
            ((checked_.types.is_pointer(lhs) && is_null_literal(expr.binary_rhs)) ||
             (checked_.types.is_pointer(rhs) && is_null_literal(expr.binary_lhs)));
        if (!checked_.types.same(lhs, rhs) && !is_null_pointer_comparison) {
            report(expr.range, "binary operands must have the same type");
        }
        switch (expr.binary_op) {
        case syntax::BinaryOp::less:
        case syntax::BinaryOp::less_equal:
        case syntax::BinaryOp::greater:
        case syntax::BinaryOp::greater_equal:
            if (!checked_.types.is_integer(lhs) && !checked_.types.is_float(lhs)) {
                report(expr.range, "comparison operator requires numeric operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        case syntax::BinaryOp::equal:
        case syntax::BinaryOp::not_equal: {
            const bool scalar =
                checked_.types.is_bool(lhs) ||
                checked_.types.is_integer(lhs) ||
                checked_.types.is_float(lhs) ||
                checked_.types.is_pointer(lhs) ||
                (is_valid(lhs) && checked_.types.get(lhs).kind == TypeKind::enum_);
            if (!scalar && !is_null_pointer_comparison) {
                report(expr.range, "equality operator requires scalar operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        }
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            if (!checked_.types.is_bool(lhs) || !checked_.types.is_bool(rhs)) {
                report(expr.range, "logical operator requires bool operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        case syntax::BinaryOp::bit_and:
        case syntax::BinaryOp::bit_xor:
        case syntax::BinaryOp::bit_or:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::mod:
            if (!checked_.types.is_integer(lhs)) {
                report(expr.range, "integer operator requires integer operands");
            }
            return record_expr_type(expr_id, lhs);
        default:
            if (!checked_.types.is_integer(lhs) && !checked_.types.is_float(lhs)) {
                report(expr.range, "binary operator requires numeric operands");
            }
            return record_expr_type(expr_id, lhs);
        }
    }
    case syntax::ExprKind::field: {
        TypeHandle object = analyze_expr(expr.object);
        if (checked_.types.is_pointer(object)) {
            object = checked_.types.get(object).pointee;
        }
        const StructInfo* info = find_struct(object);
        if (info == nullptr || info->is_opaque) {
            report(expr.range, "field access requires a non-opaque struct value");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        for (const StructFieldInfo& field : info->fields) {
            if (field.name == expr.field_name) {
                return record_expr_type(expr_id, field.type);
            }
        }
        report(expr.range, "unknown field: " + std::string(expr.field_name));
        return record_expr_type(expr_id, invalid_type_handle);
    }
    case syntax::ExprKind::index: {
        const TypeHandle object = analyze_expr(expr.object);
        const TypeHandle index = analyze_expr(expr.index);
        if (!checked_.types.is_integer(index)) {
            report(module_.exprs[expr.index.value].range, "array index must be an integer");
        }
        if (checked_.types.is_array(object)) {
            return record_expr_type(expr_id, checked_.types.get(object).array_element);
        }
        if (checked_.types.is_pointer(object)) {
            return record_expr_type(expr_id, checked_.types.get(object).pointee);
        }
        report(expr.range, "indexing requires array or pointer value");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    case syntax::ExprKind::struct_literal: {
        const TypeHandle struct_type = find_type_in_visible_modules(expr.struct_name, expr.range);
        if (!is_valid(struct_type)) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const StructInfo* info = find_struct(struct_type);
        if (info == nullptr || info->is_opaque) {
            report(expr.range, "struct literal requires a non-opaque struct type");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        for (const syntax::FieldInit& init : expr.field_inits) {
            const StructFieldInfo* field_info = nullptr;
            for (const StructFieldInfo& field : info->fields) {
                if (field.name == init.name) {
                    field_info = &field;
                    break;
                }
            }
            if (field_info == nullptr) {
                report(init.range, "unknown field in struct literal: " + std::string(init.name));
                continue;
            }
            const TypeHandle actual = analyze_expr(init.value);
            if (!can_assign(field_info->type, actual, init.value)) {
                report(init.range, "struct literal field type mismatch");
            }
        }
        return record_expr_type(expr_id, struct_type);
    }
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast: {
        const TypeHandle source = analyze_expr(expr.cast_expr);
        const TypeHandle target = resolve_type(expr.cast_type);
        if (!is_valid_cast(expr.kind, target, source)) {
            report(expr.range, "invalid explicit conversion");
        }
        return record_expr_type(expr_id, target);
    }
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of: {
        const TypeHandle queried = resolve_type(expr.cast_type);
        if (is_valid(queried) && checked_.types.get(queried).kind == TypeKind::opaque_struct) {
            report(expr.range, "opaque struct cannot be queried by size_of or align_of directly");
        }
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::usize));
    }
    case syntax::ExprKind::ptr_addr: {
        const TypeHandle value = analyze_expr(expr.cast_expr);
        if (!checked_.types.is_pointer(value)) {
            report(expr.range, "ptr_addr requires a pointer value");
        }
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::usize));
    }
    case syntax::ExprKind::ptr_from_addr: {
        const TypeHandle target = resolve_type(expr.cast_type);
        const TypeHandle address = analyze_expr(expr.cast_expr);
        if (!checked_.types.is_pointer(target)) {
            report(expr.range, "ptr_from_addr target type must be a pointer");
        }
        if (!checked_.types.is_integer(address)) {
            report(module_.exprs[expr.cast_expr.value].range, "ptr_from_addr address must be an integer");
        }
        return record_expr_type(expr_id, target);
    }
    case syntax::ExprKind::invalid:
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id) {
    return resolve_type(type_id, false);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee) {
    if (!syntax::is_valid(type_id) || type_id.value >= module_.types.size()) {
        return invalid_type_handle;
    }

    if (type_id.value < checked_.syntax_type_handles.size() && is_valid(checked_.syntax_type_handles[type_id.value])) {
        return checked_.syntax_type_handles[type_id.value];
    }

    const syntax::TypeNode& type = module_.types[type_id.value];
    TypeHandle resolved = invalid_type_handle;
    switch (type.kind) {
    case syntax::TypeKind::primitive:
        resolved = checked_.types.builtin(map_builtin(type.primitive));
        break;
    case syntax::TypeKind::pointer:
        resolved = checked_.types.pointer(map_mutability(type.pointer_mutability), resolve_type(type.pointee, true));
        break;
    case syntax::TypeKind::array:
        resolved = checked_.types.array(type.array_count, resolve_type(type.array_element));
        break;
    case syntax::TypeKind::named: {
        resolved = find_type_in_visible_modules(type.name, type.range);
        if (!is_valid(resolved)) {
            break;
        }
        if (checked_.types.get(resolved).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
            report(type.range, "opaque struct can only be used as a pointer target");
        }
        break;
    }
    }
    if (type_id.value < checked_.syntax_type_handles.size()) {
        checked_.syntax_type_handles[type_id.value] = resolved;
    }
    return resolved;
}

bool SemanticAnalyzer::can_assign(const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return is_valid(dst) && is_null_literal(value) && checked_.types.is_pointer(dst);
    }
    if (checked_.types.is_integer(dst) && checked_.types.is_integer(src) && is_integer_literal(value)) {
        return true;
    }
    return checked_.types.same(dst, src);
}

bool SemanticAnalyzer::is_valid_storage_type(const TypeHandle type) const noexcept {
    return is_valid(type) && !checked_.types.is_void(type) && checked_.types.get(type).kind != TypeKind::opaque_struct;
}

bool SemanticAnalyzer::is_valid_cast(const syntax::ExprKind kind, const TypeHandle dst, const TypeHandle src) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return false;
    }

    if (kind == syntax::ExprKind::cast) {
        return (checked_.types.is_integer(dst) || checked_.types.is_float(dst) || checked_.types.is_bool(dst)) &&
               (checked_.types.is_integer(src) || checked_.types.is_float(src) || checked_.types.is_bool(src));
    }
    if (kind == syntax::ExprKind::ptr_cast) {
        return checked_.types.is_pointer(dst) && checked_.types.is_pointer(src);
    }
    if (kind == syntax::ExprKind::bit_cast) {
        return checked_.types.is_copyable(dst) && checked_.types.is_copyable(src) && abi_size(dst) == abi_size(src);
    }
    return false;
}

base::u64 SemanticAnalyzer::abi_size(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return 0;
    }
    const TypeInfo& info = checked_.types.get(type);
    switch (info.kind) {
    case TypeKind::builtin:
        switch (info.builtin) {
        case BuiltinType::void_: return 0;
        case BuiltinType::bool_: return sizeof(bool);
        case BuiltinType::i8:
        case BuiltinType::u8: return sizeof(std::uint8_t);
        case BuiltinType::i16:
        case BuiltinType::u16: return sizeof(std::uint16_t);
        case BuiltinType::i32:
        case BuiltinType::u32: return sizeof(std::uint32_t);
        case BuiltinType::i64:
        case BuiltinType::u64: return sizeof(std::uint64_t);
        case BuiltinType::isize: return sizeof(std::ptrdiff_t);
        case BuiltinType::usize: return sizeof(std::size_t);
        case BuiltinType::f32: return sizeof(float);
        case BuiltinType::f64: return sizeof(double);
        case BuiltinType::str: return sizeof(void*) + sizeof(std::size_t);
        }
        return 0;
    case TypeKind::pointer:
        return sizeof(void*);
    case TypeKind::array:
        return info.array_count * abi_size(info.array_element);
    case TypeKind::enum_:
        return abi_size(info.enum_underlying);
    case TypeKind::struct_: {
        const StructInfo* struct_info = find_struct(type);
        if (struct_info == nullptr) {
            return 0;
        }
        base::u64 offset = 0;
        base::u64 max_align = 1;
        for (const StructFieldInfo& field : struct_info->fields) {
            const base::u64 field_align = abi_align(field.type);
            max_align = std::max(max_align, field_align);
            offset = align_forward(offset, field_align);
            offset += abi_size(field.type);
        }
        return align_forward(offset, max_align);
    }
    case TypeKind::opaque_struct:
        return 0;
    }
    return 0;
}

base::u64 SemanticAnalyzer::abi_align(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return 1;
    }
    const TypeInfo& info = checked_.types.get(type);
    switch (info.kind) {
    case TypeKind::builtin:
        switch (info.builtin) {
        case BuiltinType::void_: return 1;
        case BuiltinType::bool_: return alignof(bool);
        case BuiltinType::i8:
        case BuiltinType::u8: return alignof(std::uint8_t);
        case BuiltinType::i16:
        case BuiltinType::u16: return alignof(std::uint16_t);
        case BuiltinType::i32:
        case BuiltinType::u32: return alignof(std::uint32_t);
        case BuiltinType::i64:
        case BuiltinType::u64: return alignof(std::uint64_t);
        case BuiltinType::isize: return alignof(std::ptrdiff_t);
        case BuiltinType::usize: return alignof(std::size_t);
        case BuiltinType::f32: return alignof(float);
        case BuiltinType::f64: return alignof(double);
        case BuiltinType::str: return alignof(void*);
        }
        return 1;
    case TypeKind::pointer:
        return alignof(void*);
    case TypeKind::array:
        return abi_align(info.array_element);
    case TypeKind::enum_:
        return abi_align(info.enum_underlying);
    case TypeKind::struct_: {
        const StructInfo* struct_info = find_struct(type);
        if (struct_info == nullptr) {
            return 1;
        }
        base::u64 max_align = 1;
        for (const StructFieldInfo& field : struct_info->fields) {
            max_align = std::max(max_align, abi_align(field.type));
        }
        return max_align;
    }
    case TypeKind::opaque_struct:
        return 1;
    }
    return 1;
}

bool SemanticAnalyzer::is_integer_literal(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::integer_literal;
}

bool SemanticAnalyzer::is_null_literal(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::null_literal;
}

bool SemanticAnalyzer::is_place_expr(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::name:
        return find_symbol(expr.text, expr.range) != nullptr;
    case syntax::ExprKind::field:
    case syntax::ExprKind::index:
        return is_place_expr(expr.object);
    case syntax::ExprKind::unary:
        return expr.unary_op == syntax::UnaryOp::dereference;
    default:
        return false;
    }
}

bool SemanticAnalyzer::is_writable_place(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::name: {
        const Symbol* symbol = find_symbol(expr.text, expr.range);
        return symbol != nullptr && symbol->is_mutable;
    }
    case syntax::ExprKind::field:
    case syntax::ExprKind::index:
        return is_writable_place(expr.object);
    case syntax::ExprKind::unary: {
        if (expr.unary_op != syntax::UnaryOp::dereference) {
            return false;
        }
        const TypeHandle pointer = analyze_expr(expr.unary_operand);
        return checked_.types.is_pointer(pointer) &&
               checked_.types.get(pointer).pointer_mutability == PointerMutability::mut;
    }
    default:
        return false;
    }
}

bool SemanticAnalyzer::is_copy_forbidden_value(const TypeHandle type) const noexcept {
    return is_valid(type) && (!checked_.types.is_copyable(type) || checked_.types.contains_array(type));
}

const StructInfo* SemanticAnalyzer::find_struct(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    for (const auto& entry : checked_.structs) {
        if (checked_.types.same(entry.second.type, type)) {
            return &entry.second;
        }
    }
    return nullptr;
}

syntax::ModuleId SemanticAnalyzer::item_module(const syntax::ItemNode& item) const noexcept {
    const auto* const begin = module_.items.data();
    const auto* const end = begin + module_.items.size();
    if (&item < begin || &item >= end) {
        return syntax::invalid_module_id;
    }
    const base::usize index = static_cast<base::usize>(&item - begin);
    if (index >= module_.item_modules.size()) {
        return syntax::invalid_module_id;
    }
    return module_.item_modules[index];
}

std::vector<syntax::ModuleId> SemanticAnalyzer::visible_modules(const syntax::ModuleId module) const {
    std::vector<syntax::ModuleId> result;
    if (!syntax::is_valid(module)) {
        return result;
    }
    result.push_back(module);
    if (module.value >= module_.modules.size()) {
        return result;
    }
    for (syntax::ModuleId import : module_.modules[module.value].imports) {
        if (syntax::is_valid(import)) {
            result.push_back(import);
        }
    }
    return result;
}

std::string SemanticAnalyzer::module_name(const syntax::ModuleId module) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return "<unknown>";
    }
    return syntax::module_path_to_string(module_.modules[module.value].path);
}

std::string SemanticAnalyzer::qualified_name(const syntax::ModuleId module, const std::string_view name) const {
    const std::string module_text = module_name(module);
    if (module_text.empty() || module_text == "<unknown>") {
        return std::string(name);
    }
    return module_text + "." + std::string(name);
}

std::string SemanticAnalyzer::c_symbol_name(const syntax::ModuleId module, const std::string_view name) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return std::string(name);
    }
    return syntax::mangle_c_symbol(module_.modules[module.value].path, name);
}

std::string SemanticAnalyzer::module_key(const syntax::ModuleId module, const std::string_view name) const {
    return std::to_string(module.value) + ":" + std::string(name);
}

TypeHandle SemanticAnalyzer::find_type_in_visible_modules(const std::string_view name, const base::SourceRange range) {
    if (const auto found = named_types_.find(module_key(current_module_, name)); found != named_types_.end()) {
        return found->second;
    }

    TypeHandle imported_result = invalid_type_handle;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    if (syntax::is_valid(current_module_) && current_module_.value < module_.modules.size()) {
        for (syntax::ModuleId module : module_.modules[current_module_.value].imports) {
            const auto found = named_types_.find(module_key(module, name));
            if (found == named_types_.end()) {
                continue;
            }
            if (is_valid(imported_result)) {
                report(range, "ambiguous type name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return invalid_type_handle;
            }
            imported_result = found->second;
            result_module = module;
        }
    }
    if (!is_valid(imported_result)) {
        report(range, "unknown type: " + std::string(name));
    }
    return imported_result;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_visible_modules(const std::string_view name, const base::SourceRange range) {
    if (const auto found = checked_.functions.find(module_key(current_module_, name)); found != checked_.functions.end()) {
        return &found->second;
    }

    const FunctionSignature* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    if (syntax::is_valid(current_module_) && current_module_.value < module_.modules.size()) {
        for (syntax::ModuleId module : module_.modules[current_module_.value].imports) {
            const auto found = checked_.functions.find(module_key(module, name));
            if (found == checked_.functions.end()) {
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous function name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = &found->second;
            result_module = module;
        }
    }
    if (imported_result == nullptr) {
        report(range, "unknown function: " + std::string(name));
    }
    return imported_result;
}

const Symbol* SemanticAnalyzer::find_symbol(const std::string_view name, const base::SourceRange range) {
    if (const Symbol* local = symbols_.find(name); local != nullptr) {
        return local;
    }

    if (const auto found = global_values_.find(module_key(current_module_, name)); found != global_values_.end()) {
        return &found->second;
    }

    const Symbol* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    if (syntax::is_valid(current_module_) && current_module_.value < module_.modules.size()) {
        for (syntax::ModuleId module : module_.modules[current_module_.value].imports) {
            const auto found = global_values_.find(module_key(module, name));
            if (found == global_values_.end()) {
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = &found->second;
            result_module = module;
        }
    }
    if (imported_result == nullptr) {
        report(range, "unknown name: " + std::string(name));
    }
    return imported_result;
}

TypeHandle SemanticAnalyzer::record_expr_type(const syntax::ExprId expr, const TypeHandle type) noexcept {
    if (syntax::is_valid(expr) && expr.value < checked_.expr_types.size()) {
        checked_.expr_types[expr.value] = type;
    }
    return type;
}

void SemanticAnalyzer::report(base::SourceRange range, std::string message) {
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        range,
        std::move(message),
    });
}

std::string dump_checked_module(const CheckedModule& checked) {
    std::ostringstream out;
    out << "checked_module\n";
    out << "  expr_types " << checked.expr_types.size() << "\n";

    std::vector<std::string> function_names;
    function_names.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        function_names.push_back(entry.first);
    }
    std::sort(function_names.begin(), function_names.end());
    out << "  functions " << function_names.size() << "\n";
    for (const std::string& name : function_names) {
        const FunctionSignature& fn = checked.functions.at(name);
        out << "    fn " << fn.name << " -> " << checked.types.display_name(fn.return_type);
        if (fn.c_name != fn.name) {
            out << " @c_name=" << fn.c_name;
        }
        if (fn.is_extern_c) {
            out << " extern_c";
        }
        if (fn.is_export_c) {
            out << " export_c";
        }
        out << "\n";
    }

    std::vector<std::string> struct_names;
    struct_names.reserve(checked.structs.size());
    for (const auto& entry : checked.structs) {
        struct_names.push_back(entry.first);
    }
    std::sort(struct_names.begin(), struct_names.end());
    out << "  structs " << struct_names.size() << "\n";
    for (const std::string& name : struct_names) {
        const StructInfo& info = checked.structs.at(name);
        out << "    struct " << info.name;
        if (info.is_opaque) {
            out << " opaque";
        }
        out << " fields=" << info.fields.size() << "\n";
    }

    out << "  enum_cases " << checked.enum_cases.size() << "\n";
    return out.str();
}

} // namespace aurex::sema

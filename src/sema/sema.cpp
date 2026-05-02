#include "aurex/sema/sema.hpp"

#include <algorithm>
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

[[nodiscard]] std::string abi_or_m0_name(const syntax::ItemNode& item) {
    if (!item.abi_name.empty()) {
        return std::string(item.abi_name);
    }
    return std::string(item.name);
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept
    : module_(module), diagnostics_(diagnostics) {}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    checked_.expr_types.assign(module_.exprs.size(), invalid_type_handle);
    register_type_names();
    register_value_names();
    analyze_struct_properties();
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
        TypeHandle handle = invalid_type_handle;
        if (item.kind == syntax::ItemKind::struct_decl) {
            handle = checked_.types.named_struct(std::string(item.name), false);
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            handle = checked_.types.named_enum(std::string(item.name));
        } else if (item.kind == syntax::ItemKind::opaque_struct_decl) {
            handle = checked_.types.opaque_struct(std::string(item.name));
        }

        if (!is_valid(handle)) {
            continue;
        }
        auto inserted = named_types_.emplace(std::string(item.name), handle);
        if (!inserted.second) {
            report(item.range, "duplicate type definition: " + std::string(item.name));
            continue;
        }
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::type,
            std::string(item.name),
            handle,
            item.range,
            false,
        }, diagnostics_));

        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::opaque_struct_decl) {
            StructInfo info;
            info.name = std::string(item.name);
            info.type = handle;
            info.is_opaque = item.kind == syntax::ItemKind::opaque_struct_decl;
            auto struct_inserted = checked_.structs.emplace(info.name, std::move(info));
            if (!struct_inserted.second) {
                report(item.range, "duplicate struct definition: " + std::string(item.name));
            }
        }
    }
}

void SemanticAnalyzer::register_value_names() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind == syntax::ItemKind::fn_decl) {
            FunctionSignature signature;
            signature.name = std::string(item.name);
            signature.c_name = abi_or_m0_name(item);
            signature.return_type = resolve_type(item.return_type);
            signature.range = item.range;
            signature.is_extern_c = item.is_extern_c;
            signature.is_export_c = item.is_export_c;
            for (const syntax::ParamDecl& param : item.params) {
                TypeHandle param_type = resolve_type(param.type);
                if (checked_.types.is_array(param_type)) {
                    report(param.range, "array type cannot be used as a function parameter");
                }
                signature.param_types.push_back(param_type);
            }
            if (checked_.types.is_array(signature.return_type)) {
                report(item.range, "array type cannot be used as a function return type");
            }
            auto inserted = checked_.functions.emplace(signature.name, std::move(signature));
            if (!inserted.second) {
                report(item.range, "function overloading is not allowed: " + std::string(item.name));
            }
            static_cast<void>(symbols_.insert(Symbol {
                SymbolKind::function,
                std::string(item.name),
                inserted.first->second.return_type,
                item.range,
                false,
            }, diagnostics_));
        } else if (item.kind == syntax::ItemKind::const_decl) {
            TypeHandle type = resolve_type(item.const_type);
            static_cast<void>(symbols_.insert(Symbol {
                SymbolKind::const_,
                std::string(item.name),
                type,
                item.range,
                false,
            }, diagnostics_));
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            const TypeHandle enum_type = resolve_type(item.enum_base_type);
            const auto type_found = named_types_.find(std::string(item.name));
            const TypeHandle named_enum_type = type_found == named_types_.end() ? enum_type : type_found->second;
            for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
                const std::string full_name = std::string(item.name) + "_" + std::string(enum_case.name);
                checked_.enum_cases.emplace(full_name, EnumCaseInfo {
                    full_name,
                    named_enum_type,
                    std::string(enum_case.value_text),
                    enum_case.range,
                });
                static_cast<void>(symbols_.insert(Symbol {
                    SymbolKind::enum_case,
                    full_name,
                    named_enum_type,
                    enum_case.range,
                    false,
                }, diagnostics_));
            }
        }
    }
}

void SemanticAnalyzer::analyze_struct_properties() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::struct_decl) {
            continue;
        }
        bool contains_array = false;
        bool copyable = true;
        for (const syntax::FieldDecl& field : item.fields) {
            const TypeHandle field_type = resolve_type(field.type);
            if (const auto struct_found = checked_.structs.find(std::string(item.name)); struct_found != checked_.structs.end()) {
                struct_found->second.fields.push_back(StructFieldInfo {
                    std::string(field.name),
                    field_type,
                    field.range,
                });
            }
            if (checked_.types.is_array(field_type)) {
                contains_array = true;
            }
            if (!checked_.types.is_copyable(field_type)) {
                copyable = false;
            }
        }
        const auto found = named_types_.find(std::string(item.name));
        if (found != named_types_.end()) {
            checked_.types.set_record_properties(found->second, contains_array, copyable && !contains_array);
        }
    }
}

void SemanticAnalyzer::analyze_const_decls() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::const_decl) {
            continue;
        }
        const TypeHandle declared = resolve_type(item.const_type);
        const TypeHandle actual = analyze_expr(item.const_value);
        if (!can_assign(declared, actual, item.const_value)) {
            report(item.range, "const initializer type does not match declared type");
        }
    }
}

void SemanticAnalyzer::analyze_function_body(const syntax::ItemNode& function) {
    const auto found = checked_.functions.find(std::string(function.name));
    const TypeHandle expected_return = found == checked_.functions.end()
        ? checked_.types.builtin(BuiltinType::void_)
        : found->second.return_type;

    symbols_.push_scope();
    for (const syntax::ParamDecl& param : function.params) {
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::parameter,
            std::string(param.name),
            resolve_type(param.type),
            param.range,
            false,
        }, diagnostics_));
    }
    analyze_block(function.body, expected_return);
    symbols_.pop_scope();
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
        if (!can_assign(declared, init, stmt.init)) {
            report(stmt.range, "initializer type does not match declared type");
        }
        if (!checked_.types.is_copyable(declared)) {
            report(stmt.range, "non-copyable storage type cannot be implicitly copied");
        }
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::local,
            std::string(stmt.name),
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
        const Symbol* symbol = symbols_.find(expr.text);
        if (symbol == nullptr) {
            report(expr.range, "unknown name: " + std::string(expr.text));
            return record_expr_type(expr_id, invalid_type_handle);
        }
        return record_expr_type(expr_id, symbol->type);
    }
    case syntax::ExprKind::call: {
        if (!syntax::is_valid(expr.callee) || module_.exprs[expr.callee.value].kind != syntax::ExprKind::name) {
            report(expr.range, "callee must be a function name");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const std::string name(module_.exprs[expr.callee.value].text);
        const auto found = checked_.functions.find(name);
        if (found == checked_.functions.end()) {
            report(expr.range, "unknown function: " + name);
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const FunctionSignature& signature = found->second;
        if (signature.param_types.size() != expr.args.size()) {
            report(expr.range, "argument count mismatch in call to " + name);
        }
        const base::usize count = expr.args.size() < signature.param_types.size() ? expr.args.size() : signature.param_types.size();
        for (base::usize i = 0; i < count; ++i) {
            const TypeHandle actual = analyze_expr(expr.args[i]);
            if (!can_assign(signature.param_types[i], actual, expr.args[i])) {
                report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
            }
            if (is_copy_forbidden_value(signature.param_types[i])) {
                report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
            }
        }
        return record_expr_type(expr_id, signature.return_type);
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
            if (!is_writable_place(expr.unary_operand)) {
                report(expr.range, "address-of requires a place expression");
            }
            return record_expr_type(expr_id, checked_.types.pointer(PointerMutability::mut, operand));
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
        if (!checked_.types.same(lhs, rhs) && !is_integer_literal(expr.binary_rhs) && !is_null_pointer_comparison) {
            report(expr.range, "binary operands must have the same type");
        }
        switch (expr.binary_op) {
        case syntax::BinaryOp::less:
        case syntax::BinaryOp::less_equal:
        case syntax::BinaryOp::greater:
        case syntax::BinaryOp::greater_equal:
        case syntax::BinaryOp::equal:
        case syntax::BinaryOp::not_equal:
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        default:
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
        const auto type_found = named_types_.find(std::string(expr.struct_name));
        if (type_found == named_types_.end()) {
            report(expr.range, "unknown struct type: " + std::string(expr.struct_name));
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const StructInfo* info = find_struct(type_found->second);
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
        return record_expr_type(expr_id, type_found->second);
    }
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast:
        static_cast<void>(analyze_expr(expr.cast_expr));
        return record_expr_type(expr_id, resolve_type(expr.cast_type));
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

    const syntax::TypeNode& type = module_.types[type_id.value];
    switch (type.kind) {
    case syntax::TypeKind::primitive:
        return checked_.types.builtin(map_builtin(type.primitive));
    case syntax::TypeKind::pointer:
        return checked_.types.pointer(map_mutability(type.pointer_mutability), resolve_type(type.pointee, true));
    case syntax::TypeKind::array:
        return checked_.types.array(type.array_count, resolve_type(type.array_element));
    case syntax::TypeKind::named: {
        const auto found = named_types_.find(std::string(type.name));
        if (found == named_types_.end()) {
            report(type.range, "unknown type: " + std::string(type.name));
            return invalid_type_handle;
        }
        if (checked_.types.get(found->second).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
            report(type.range, "opaque struct can only be used as a pointer target");
        }
        return found->second;
    }
    }
    return invalid_type_handle;
}

bool SemanticAnalyzer::can_assign(const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return is_valid(dst) && is_null_literal(value) && checked_.types.is_pointer(dst);
    }
    return checked_.types.same(dst, src) || (checked_.types.is_integer(dst) && is_integer_literal(value));
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

bool SemanticAnalyzer::is_writable_place(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::name: {
        const Symbol* symbol = symbols_.find(expr.text);
        return symbol != nullptr && symbol->is_mutable;
    }
    case syntax::ExprKind::field:
    case syntax::ExprKind::index:
        return is_writable_place(expr.object);
    case syntax::ExprKind::unary:
        return expr.unary_op == syntax::UnaryOp::dereference;
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

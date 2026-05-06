#include "aurex/sema/sema.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

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

[[nodiscard]] bool builtin_is_unsigned(const BuiltinType type) noexcept {
    switch (type) {
    case BuiltinType::u8:
    case BuiltinType::u16:
    case BuiltinType::u32:
    case BuiltinType::u64:
    case BuiltinType::usize:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] base::u32 builtin_integer_bits(const BuiltinType type) noexcept {
    switch (type) {
    case BuiltinType::i8:
    case BuiltinType::u8: return 8;
    case BuiltinType::i16:
    case BuiltinType::u16: return 16;
    case BuiltinType::i32:
    case BuiltinType::u32: return 32;
    case BuiltinType::i64:
    case BuiltinType::u64: return 64;
    case BuiltinType::isize: return static_cast<base::u32>(sizeof(std::ptrdiff_t) * 8);
    case BuiltinType::usize: return static_cast<base::u32>(sizeof(std::size_t) * 8);
    default: return 0;
    }
}

[[nodiscard]] bool parse_u64_literal_text(const std::string_view text, base::u64& value) noexcept {
    int base = 10;
    base::usize index = 0;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        index = 2;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
        index = 2;
    }

    value = 0;
    bool saw_digit = false;
    for (; index < text.size(); ++index) {
        const char c = text[index];
        if (c == '_') {
            continue;
        }

        base::u64 digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<base::u64>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<base::u64>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<base::u64>(10 + c - 'A');
        } else {
            return false;
        }
        if (digit >= static_cast<base::u64>(base)) {
            return false;
        }
        saw_digit = true;
        if (value > (std::numeric_limits<base::u64>::max() - digit) / static_cast<base::u64>(base)) {
            return false;
        }
        value = value * static_cast<base::u64>(base) + digit;
    }
    return saw_digit;
}

[[nodiscard]] bool integer_literal_fits_type(
    const TypeTable& types,
    const TypeHandle destination,
    const std::string_view text
) noexcept {
    if (!types.is_integer(destination)) {
        return false;
    }
    const TypeInfo& info = types.get(destination);
    const base::u32 bits = builtin_integer_bits(info.builtin);
    if (bits == 0) {
        return false;
    }

    base::u64 value = 0;
    if (!parse_u64_literal_text(text, value)) {
        return false;
    }
    if (builtin_is_unsigned(info.builtin)) {
        if (bits >= 64) {
            return true;
        }
        return value <= ((base::u64 {1} << bits) - 1);
    }
    if (bits >= 64) {
        return value <= static_cast<base::u64>(std::numeric_limits<std::int64_t>::max());
    }
    return value <= ((base::u64 {1} << (bits - 1)) - 1);
}

[[nodiscard]] base::u64 align_forward(const base::u64 offset, const base::u64 alignment) noexcept {
    if (alignment == 0) {
        return offset;
    }
    const base::u64 mask = alignment - 1;
    return (offset + mask) & ~mask;
}

} // namespace

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id) {
    return resolve_type(type_id, false);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee) {
    return resolve_type_with_substitution(type_id, current_type_substitution_, opaque_allowed_as_pointee);
}

TypeHandle SemanticAnalyzer::resolve_type_with_substitution(
    const syntax::TypeId type_id,
    const GenericTypeSubstitution* const substitution,
    const bool opaque_allowed_as_pointee
) {
    if (!syntax::is_valid(type_id) || type_id.value >= module_.types.size()) {
        return invalid_type_handle;
    }

    if (substitution != nullptr &&
        current_generic_syntax_type_handles_ != nullptr) {
        if (const auto found = current_generic_syntax_type_handles_->find(type_id.value);
            found != current_generic_syntax_type_handles_->end() &&
            is_valid(found->second)) {
            if (checked_.types.get(found->second).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
                report(module_.types[type_id.value].range, "opaque struct can only be used as a pointer target");
            }
            return found->second;
        }
    }

    if (substitution == nullptr &&
        type_id.value < checked_.syntax_type_handles.size() &&
        is_valid(checked_.syntax_type_handles[type_id.value])) {
        const TypeHandle cached = checked_.syntax_type_handles[type_id.value];
        if (checked_.types.get(cached).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
            report(module_.types[type_id.value].range, "opaque struct can only be used as a pointer target");
        }
        return cached;
    }

    const syntax::TypeNode& type = module_.types[type_id.value];
    TypeHandle resolved = invalid_type_handle;
    switch (type.kind) {
    case syntax::TypeKind::primitive:
        resolved = checked_.types.builtin(map_builtin(type.primitive));
        break;
    case syntax::TypeKind::pointer:
        resolved = checked_.types.pointer(
            map_mutability(type.pointer_mutability),
            resolve_type_with_substitution(type.pointee, substitution, true)
        );
        break;
    case syntax::TypeKind::array:
        resolved = checked_.types.array(type.array_count, resolve_type_with_substitution(type.array_element, substitution, false));
        break;
    case syntax::TypeKind::named: {
        const bool qualified = !type.scope_name.empty();
        syntax::ModuleId scope_module = syntax::invalid_module_id;
        if (qualified) {
            scope_module = resolve_import_alias(type.scope_name, type.scope_range);
            if (!syntax::is_valid(scope_module)) {
                break;
            }
        }

        if (substitution != nullptr && type.type_args.empty() && !qualified) {
            if (const auto found = substitution->types.find(std::string(type.name)); found != substitution->types.end()) {
                resolved = found->second;
                break;
            }
        }
        if (!type.type_args.empty()) {
            if (qualified) {
                if (const GenericStructTemplateInfo* info =
                        find_generic_struct_template_in_module(scope_module, type.name, type.range, false);
                    info != nullptr) {
                    if (substitution == nullptr) {
                        resolved = instantiate_generic_struct_from_syntax(*info, type.type_args, type.range, opaque_allowed_as_pointee);
                    } else {
                        if (type.type_args.size() != info->params.size()) {
                            report(type.range, "generic struct type argument count mismatch for " + info->name);
                            break;
                        }
                        std::vector<TypeHandle> resolved_args;
                        resolved_args.reserve(type.type_args.size());
                        for (syntax::TypeId arg : type.type_args) {
                            resolved_args.push_back(resolve_type_with_substitution(arg, substitution, opaque_allowed_as_pointee));
                        }
                        resolved = instantiate_generic_struct(*info, resolved_args, type.range);
                    }
                    break;
                }
                if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_module(scope_module, type.name, type.range, false);
                    info != nullptr) {
                    if (substitution == nullptr) {
                        resolved = instantiate_generic_enum_from_syntax(*info, type.type_args, type.range, opaque_allowed_as_pointee);
                    } else {
                        if (type.type_args.size() != info->params.size()) {
                            report(type.range, "generic enum type argument count mismatch for " + info->name);
                            break;
                        }
                        std::vector<TypeHandle> resolved_args;
                        resolved_args.reserve(type.type_args.size());
                        for (syntax::TypeId arg : type.type_args) {
                            resolved_args.push_back(resolve_type_with_substitution(arg, substitution, opaque_allowed_as_pointee));
                        }
                        resolved = instantiate_generic_enum(*info, resolved_args, type.range);
                    }
                    break;
                }
                report(type.range, "type arguments require a generic type: " + std::string(type.scope_name) + "::" + std::string(type.name));
                break;
            }
            if (const GenericStructTemplateInfo* info =
                    find_generic_struct_template_in_visible_modules(type.name, type.range, false);
                info != nullptr) {
                if (substitution == nullptr) {
                    resolved = instantiate_generic_struct_from_syntax(*info, type.type_args, type.range, opaque_allowed_as_pointee);
                } else {
                    if (type.type_args.size() != info->params.size()) {
                        report(type.range, "generic struct type argument count mismatch for " + info->name);
                        break;
                    }
                    std::vector<TypeHandle> resolved_args;
                    resolved_args.reserve(type.type_args.size());
                    for (syntax::TypeId arg : type.type_args) {
                        resolved_args.push_back(resolve_type_with_substitution(arg, substitution, opaque_allowed_as_pointee));
                    }
                    resolved = instantiate_generic_struct(*info, resolved_args, type.range);
                }
                break;
            }
            if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_visible_modules(type.name, type.range);
                info != nullptr) {
                if (substitution == nullptr) {
                    resolved = instantiate_generic_enum_from_syntax(*info, type.type_args, type.range, opaque_allowed_as_pointee);
                } else {
                    if (type.type_args.size() != info->params.size()) {
                        report(type.range, "generic enum type argument count mismatch for " + info->name);
                        break;
                    }
                    std::vector<TypeHandle> resolved_args;
                    resolved_args.reserve(type.type_args.size());
                    for (syntax::TypeId arg : type.type_args) {
                        resolved_args.push_back(resolve_type_with_substitution(arg, substitution, opaque_allowed_as_pointee));
                    }
                    resolved = instantiate_generic_enum(*info, resolved_args, type.range);
                }
                break;
            }
            report(type.range, "type arguments require a generic type: " + std::string(type.name));
            break;
        }
        if (qualified) {
            if (const GenericStructTemplateInfo* info = find_generic_struct_template_in_module(scope_module, type.name, type.range, false);
                info != nullptr) {
                report(type.range, "generic struct type requires type arguments: " + std::string(type.scope_name) + "::" + info->name);
                break;
            }
            if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_module(scope_module, type.name, type.range, false);
                info != nullptr) {
                report(type.range, "generic enum type requires type arguments: " + std::string(type.scope_name) + "::" + info->name);
                break;
            }
            resolved = find_type_in_module(scope_module, type.name, type.range, opaque_allowed_as_pointee);
            if (is_valid(resolved) && checked_.types.get(resolved).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
                report(type.range, "opaque struct can only be used as a pointer target");
            }
            break;
        }
        if (const GenericStructTemplateInfo* info = find_generic_struct_template_in_visible_modules(type.name, type.range, false);
            info != nullptr) {
            report(type.range, "generic struct type requires type arguments: " + info->name);
            break;
        }
        if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_visible_modules(type.name, type.range, false);
            info != nullptr) {
            report(type.range, "generic enum type requires type arguments: " + info->name);
            break;
        }
        resolved = find_type_in_visible_modules(type.name, type.range, opaque_allowed_as_pointee);
        if (!is_valid(resolved)) {
            break;
        }
        if (checked_.types.get(resolved).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
            report(type.range, "opaque struct can only be used as a pointer target");
        }
        break;
    }
    }
    record_syntax_type_handle(type_id, resolved);
    return resolved;
}

TypeHandle SemanticAnalyzer::resolve_type_alias(const TypeAliasInfo& alias, const bool opaque_allowed_as_pointee) {
    const std::string key = module_key(alias.module, alias.name);
    if (const auto found = resolved_type_aliases_.find(key); found != resolved_type_aliases_.end()) {
        return found->second;
    }
    if (std::find(resolving_type_aliases_.begin(), resolving_type_aliases_.end(), key) != resolving_type_aliases_.end()) {
        report(alias.range, "cyclic type alias: " + alias.name);
        resolved_type_aliases_[key] = invalid_type_handle;
        return invalid_type_handle;
    }
    resolving_type_aliases_.push_back(key);
    const syntax::ModuleId previous_module = current_module_;
    current_module_ = alias.module;
    const TypeHandle resolved = resolve_type(alias.target, opaque_allowed_as_pointee);
    current_module_ = previous_module;
    resolving_type_aliases_.pop_back();
    resolved_type_aliases_[key] = resolved;
    return resolved;
}

bool SemanticAnalyzer::can_assign(const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return is_valid(dst) && is_null_literal(value) && checked_.types.is_pointer(dst);
    }
    if (checked_.types.is_integer(dst) && checked_.types.is_integer(src) && is_integer_literal(value)) {
        return syntax::is_valid(value) &&
               value.value < module_.exprs.size() &&
               integer_literal_fits_type(checked_.types, dst, module_.exprs[value.value].text);
    }
    if (checked_.types.is_pointer(dst) && checked_.types.is_pointer(src)) {
        const TypeInfo& dst_info = checked_.types.get(dst);
        const TypeInfo& src_info = checked_.types.get(src);
        if (dst_info.pointer_mutability == PointerMutability::const_ &&
            checked_.types.same(dst_info.pointee, src_info.pointee)) {
            return true;
        }
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
    case TypeKind::enum_: {
        if (!is_valid(info.enum_payload_storage)) {
            return abi_size(info.enum_underlying);
        }
        const base::u64 tag_align = abi_align(info.enum_underlying);
        const base::u64 storage_align = info.enum_payload_align;
        const base::u64 max_align = std::max(tag_align, storage_align);
        const base::u64 storage_offset = align_forward(abi_size(info.enum_underlying), storage_align);
        return align_forward(storage_offset + abi_size(info.enum_payload_storage), max_align);
    }
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
        if (!is_valid(info.enum_payload_storage)) {
            return abi_align(info.enum_underlying);
        }
        return std::max(abi_align(info.enum_underlying), info.enum_payload_align);
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
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module = resolve_import_alias(expr.scope_name, expr.scope_range, false);
            return syntax::is_valid(module) && find_symbol_in_module(module, expr.text, expr.range, false) != nullptr;
        }
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
        const Symbol* symbol = nullptr;
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module = resolve_import_alias(expr.scope_name, expr.scope_range, false);
            symbol = syntax::is_valid(module) ? find_symbol_in_module(module, expr.text, expr.range, false) : nullptr;
        } else {
            symbol = find_symbol(expr.text, expr.range);
        }
        return symbol != nullptr && symbol->is_mutable;
    }
    case syntax::ExprKind::field: {
        const TypeHandle object = analyze_expr(expr.object);
        if (checked_.types.is_pointer(object)) {
            return checked_.types.get(object).pointer_mutability == PointerMutability::mut;
        }
        return is_writable_place(expr.object);
    }
    case syntax::ExprKind::index: {
        const TypeHandle object = analyze_expr(expr.object);
        if (checked_.types.is_pointer(object)) {
            return checked_.types.get(object).pointer_mutability == PointerMutability::mut;
        }
        return is_writable_place(expr.object);
    }
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
    if (const auto found = struct_infos_by_type_.find(type.value); found != struct_infos_by_type_.end()) {
        return found->second;
    }
    for (const auto& entry : checked_.structs) {
        if (checked_.types.same(entry.second.type, type)) {
            return &entry.second;
        }
    }
    return nullptr;
}

} // namespace aurex::sema

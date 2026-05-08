#include "aurex/sema/sema.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>

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

[[nodiscard]] bool parse_u64_literal_checked(const std::string_view text, base::u64& value) noexcept {
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

[[nodiscard]] bool literal_fits_integer_type(
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
    if (!parse_u64_literal_checked(text, value)) {
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

[[nodiscard]] bool negative_literal_fits_integer_type(
    const TypeTable& types,
    const TypeHandle destination,
    const std::string_view text
) noexcept {
    if (!types.is_integer(destination)) {
        return false;
    }
    const TypeInfo& info = types.get(destination);
    const base::u32 bits = builtin_integer_bits(info.builtin);
    if (bits == 0 || builtin_is_unsigned(info.builtin)) {
        return false;
    }

    base::u64 value = 0;
    if (!parse_u64_literal_checked(text, value)) {
        return false;
    }
    if (bits >= 64) {
        return value <= (base::u64 {1} << 63);
    }
    return value <= (base::u64 {1} << (bits - 1));
}

[[nodiscard]] base::u64 align_forward(const base::u64 offset, const base::u64 alignment) noexcept {
    if (alignment == 0) {
        return offset;
    }
    const base::u64 mask = alignment - 1;
    if (offset > std::numeric_limits<base::u64>::max() - mask) {
        return std::numeric_limits<base::u64>::max();
    }
    return (offset + mask) & ~mask;
}

[[nodiscard]] bool checked_add_u64(const base::u64 lhs, const base::u64 rhs, base::u64& result) noexcept {
    if (lhs > std::numeric_limits<base::u64>::max() - rhs) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = lhs + rhs;
    return true;
}

[[nodiscard]] bool checked_mul_u64(const base::u64 lhs, const base::u64 rhs, base::u64& result) noexcept {
    if (lhs != 0 && rhs > std::numeric_limits<base::u64>::max() / lhs) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = lhs * rhs;
    return true;
}

[[nodiscard]] bool checked_align_forward(
    const base::u64 offset,
    const base::u64 alignment,
    base::u64& result
) noexcept {
    if (alignment == 0) {
        result = offset;
        return true;
    }
    const base::u64 mask = alignment - 1;
    if (offset > std::numeric_limits<base::u64>::max() - mask) {
        result = std::numeric_limits<base::u64>::max();
        return false;
    }
    result = (offset + mask) & ~mask;
    return true;
}

[[nodiscard]] base::u64 add_saturating(const base::u64 lhs, const base::u64 rhs) noexcept {
    base::u64 result = 0;
    static_cast<void>(checked_add_u64(lhs, rhs, result));
    return result;
}

[[nodiscard]] base::u64 mul_saturating(const base::u64 lhs, const base::u64 rhs) noexcept {
    base::u64 result = 0;
    static_cast<void>(checked_mul_u64(lhs, rhs, result));
    return result;
}

[[nodiscard]] bool is_builtin_scalar_bitcast_type(const TypeTable& types, const TypeHandle type) noexcept {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    return info.kind == TypeKind::builtin &&
           info.builtin != BuiltinType::void_ &&
           info.builtin != BuiltinType::bool_ &&
           info.builtin != BuiltinType::str;
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
            if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_visible_modules(type.name, type.range, false);
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
               integer_literal_fits_type(dst, module_.exprs[value.value].text);
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
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = checked_.types.get(type);
    if (checked_.types.is_void(type) || info.kind == TypeKind::opaque_struct) {
        return false;
    }
    if (info.kind != TypeKind::array) {
        return true;
    }
    if (!is_valid_storage_type(info.array_element)) {
        return false;
    }
    const base::u64 element_size = abi_size(info.array_element);
    return element_size == 0 || info.array_count <= std::numeric_limits<base::u64>::max() / element_size;
}

void SemanticAnalyzer::validate_type_layouts() {
    struct LayoutResult {
        base::u64 size = 0;
        base::u64 align = 1;
        bool ok = false;
    };

    std::unordered_map<base::u32, base::u8> states;
    std::unordered_map<base::u32, LayoutResult> results;

    const auto compute = [&](const auto& self, const TypeHandle type, const base::SourceRange range) -> LayoutResult {
        if (!is_valid(type)) {
            return {};
        }

        const TypeInfo& info = checked_.types.get(type);
        switch (info.kind) {
        case TypeKind::builtin:
            if (info.builtin == BuiltinType::void_) {
                return {};
            }
            return LayoutResult {abi_size(type), abi_align(type), true};
        case TypeKind::pointer:
            return LayoutResult {abi_size(type), abi_align(type), true};
        case TypeKind::opaque_struct:
            return {};
        case TypeKind::array: {
            const LayoutResult element = self(self, info.array_element, range);
            if (!element.ok) {
                return LayoutResult {0, std::max<base::u64>(1, element.align), false};
            }
            base::u64 size = 0;
            if (!checked_mul_u64(info.array_count, element.size, size)) {
                report(range, "array storage size overflows ABI size");
                return LayoutResult {size, element.align, false};
            }
            return LayoutResult {size, element.align, true};
        }
        case TypeKind::struct_:
        case TypeKind::enum_:
            break;
        }

        if (const auto cached = results.find(type.value); cached != results.end()) {
            return cached->second;
        }

        if (states[type.value] == 1) {
            report(range, "recursive value type is not valid storage: " + checked_.types.display_name(type));
            return {};
        }
        states[type.value] = 1;

        LayoutResult result;
        result.ok = true;
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = find_struct(type);
            if (struct_info == nullptr || struct_info->is_opaque) {
                result.ok = false;
            } else {
                base::u64 offset = 0;
                base::u64 max_align = 1;
                for (const StructFieldInfo& field : struct_info->fields) {
                    const LayoutResult field_layout = self(self, field.type, field.range);
                    if (!field_layout.ok) {
                        result.ok = false;
                        continue;
                    }
                    max_align = std::max(max_align, field_layout.align);
                    base::u64 aligned_offset = 0;
                    if (!checked_align_forward(offset, field_layout.align, aligned_offset)) {
                        report(field.range, "struct storage size overflows ABI size");
                        result.ok = false;
                    }
                    base::u64 next_offset = 0;
                    if (!checked_add_u64(aligned_offset, field_layout.size, next_offset)) {
                        report(field.range, "struct storage size overflows ABI size");
                        result.ok = false;
                    }
                    offset = next_offset;
                }
                base::u64 size = 0;
                if (!checked_align_forward(offset, max_align, size)) {
                    report(range, "struct storage size overflows ABI size");
                    result.ok = false;
                }
                result.size = size;
                result.align = max_align;
            }
        } else {
            result.size = is_valid(info.enum_underlying) ? abi_size(info.enum_underlying) : 0;
            result.align = is_valid(info.enum_underlying) ? abi_align(info.enum_underlying) : 1;
            base::u64 payload_size = 0;
            base::u64 payload_align = 1;
            bool has_payload = false;
            for (const auto& entry : checked_.enum_cases) {
                const EnumCaseInfo& enum_case = entry.second;
                if (!checked_.types.same(enum_case.type, type) || !is_valid(enum_case.payload_type)) {
                    continue;
                }
                has_payload = true;
                const LayoutResult payload = self(self, enum_case.payload_type, enum_case.range);
                if (!payload.ok) {
                    result.ok = false;
                    continue;
                }
                if (payload.size > payload_size ||
                    (payload.size == payload_size && payload.align > payload_align)) {
                    payload_size = payload.size;
                    payload_align = payload.align;
                }
            }
            if (has_payload) {
                const base::u64 max_align = std::max(result.align, payload_align);
                base::u64 storage_offset = 0;
                if (!checked_align_forward(result.size, payload_align, storage_offset)) {
                    report(range, "enum storage size overflows ABI size");
                    result.ok = false;
                }
                base::u64 total = 0;
                if (!checked_add_u64(storage_offset, payload_size, total)) {
                    report(range, "enum storage size overflows ABI size");
                    result.ok = false;
                }
                base::u64 size = 0;
                if (!checked_align_forward(total, max_align, size)) {
                    report(range, "enum storage size overflows ABI size");
                    result.ok = false;
                }
                result.size = size;
                result.align = max_align;
            }
        }

        states[type.value] = 2;
        results[type.value] = result;
        return result;
    };

    for (const auto& entry : checked_.structs) {
        const StructInfo& info = entry.second;
        if (!info.is_opaque) {
            const base::SourceRange range = info.fields.empty() ? base::SourceRange {} : info.fields.front().range;
            static_cast<void>(compute(compute, info.type, range));
        }
    }

    std::unordered_set<base::u32> seen_enums;
    for (const auto& entry : named_types_) {
        const TypeHandle type = entry.second;
        if (is_valid(type) &&
            checked_.types.get(type).kind == TypeKind::enum_ &&
            seen_enums.insert(type.value).second) {
            static_cast<void>(compute(compute, type, {}));
        }
    }
    for (const auto& entry : checked_.enum_cases) {
        const TypeHandle type = entry.second.type;
        if (is_valid(type) && seen_enums.insert(type.value).second) {
            static_cast<void>(compute(compute, type, entry.second.range));
        }
    }
}

bool SemanticAnalyzer::parse_integer_literal_text(const std::string_view text, base::u64& value) const noexcept {
    return parse_u64_literal_checked(text, value);
}

bool SemanticAnalyzer::integer_literal_fits_type(const TypeHandle destination, const std::string_view text) const noexcept {
    return literal_fits_integer_type(checked_.types, destination, text);
}

bool SemanticAnalyzer::negative_integer_literal_fits_type(
    const TypeHandle destination,
    const std::string_view text
) const noexcept {
    return negative_literal_fits_integer_type(checked_.types, destination, text);
}

TypeHandle SemanticAnalyzer::analyze_integer_literal(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    const TypeHandle literal_type = checked_.types.is_integer(expected_type)
        ? expected_type
        : checked_.types.builtin(BuiltinType::i32);
    if (!integer_literal_fits_type(literal_type, expr.text)) {
        report(
            expr.range,
            "integer literal out of range for " + checked_.types.display_name(literal_type)
        );
    }
    return record_expr_type(expr_id, literal_type);
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
        if (checked_.types.same(dst, src)) {
            return checked_.types.is_copyable(dst);
        }
        if (!checked_.types.is_copyable(dst) || !checked_.types.is_copyable(src) || abi_size(dst) != abi_size(src)) {
            return false;
        }
        if (is_builtin_scalar_bitcast_type(checked_.types, dst) &&
            is_builtin_scalar_bitcast_type(checked_.types, src)) {
            return true;
        }
        return checked_.types.is_pointer(dst) && checked_.types.is_pointer(src);
    }
    return false;
}

base::u64 SemanticAnalyzer::abi_size(const TypeHandle type) const noexcept {
    std::unordered_set<base::u32> visiting;
    const auto compute = [&](const auto& self, const TypeHandle current) -> base::u64 {
        if (!is_valid(current)) {
            return 0;
        }
        const TypeInfo& info = checked_.types.get(current);
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
            return mul_saturating(info.array_count, self(self, info.array_element));
        case TypeKind::enum_: {
            if (!visiting.insert(current.value).second) {
                return 0;
            }
            if (!is_valid(info.enum_payload_storage)) {
                const base::u64 result = self(self, info.enum_underlying);
                visiting.erase(current.value);
                return result;
            }
            const base::u64 tag_align = abi_align(info.enum_underlying);
            const base::u64 storage_align = info.enum_payload_align;
            const base::u64 max_align = std::max(tag_align, storage_align);
            const base::u64 storage_offset = align_forward(self(self, info.enum_underlying), storage_align);
            const base::u64 result = align_forward(
                add_saturating(storage_offset, self(self, info.enum_payload_storage)),
                max_align
            );
            visiting.erase(current.value);
            return result;
        }
        case TypeKind::struct_: {
            if (!visiting.insert(current.value).second) {
                return 0;
            }
            const StructInfo* struct_info = find_struct(current);
            if (struct_info == nullptr) {
                visiting.erase(current.value);
                return 0;
            }
            base::u64 offset = 0;
            base::u64 max_align = 1;
            for (const StructFieldInfo& field : struct_info->fields) {
                const base::u64 field_align = abi_align(field.type);
                max_align = std::max(max_align, field_align);
                offset = align_forward(offset, field_align);
                offset = add_saturating(offset, self(self, field.type));
            }
            const base::u64 result = align_forward(offset, max_align);
            visiting.erase(current.value);
            return result;
        }
        case TypeKind::opaque_struct:
            return 0;
        }
        return 0;
    };
    return compute(compute, type);
}

base::u64 SemanticAnalyzer::abi_align(const TypeHandle type) const noexcept {
    std::unordered_set<base::u32> visiting;
    const auto compute = [&](const auto& self, const TypeHandle current) -> base::u64 {
        if (!is_valid(current)) {
            return 1;
        }
        const TypeInfo& info = checked_.types.get(current);
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
            return self(self, info.array_element);
        case TypeKind::enum_:
            if (!visiting.insert(current.value).second) {
                return 1;
            }
            if (!is_valid(info.enum_payload_storage)) {
                const base::u64 result = self(self, info.enum_underlying);
                visiting.erase(current.value);
                return result;
            }
            {
                const base::u64 result = std::max(self(self, info.enum_underlying), info.enum_payload_align);
                visiting.erase(current.value);
                return result;
            }
        case TypeKind::struct_: {
            if (!visiting.insert(current.value).second) {
                return 1;
            }
            const StructInfo* struct_info = find_struct(current);
            if (struct_info == nullptr) {
                visiting.erase(current.value);
                return 1;
            }
            base::u64 max_align = 1;
            for (const StructFieldInfo& field : struct_info->fields) {
                max_align = std::max(max_align, self(self, field.type));
            }
            visiting.erase(current.value);
            return max_align;
        }
        case TypeKind::opaque_struct:
            return 1;
        }
        return 1;
    };
    return compute(compute, type);
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
    case syntax::ExprKind::name: {
        const Symbol* symbol = nullptr;
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module = resolve_import_alias(expr.scope_name, expr.scope_range, false);
            symbol = syntax::is_valid(module) ? find_symbol_in_module(module, expr.text, expr.range, false) : nullptr;
        } else {
            symbol = find_symbol(expr.text, expr.range);
        }
        return symbol != nullptr && (symbol->kind == SymbolKind::local || symbol->kind == SymbolKind::parameter);
    }
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

bool SemanticAnalyzer::is_move_only_value(const TypeHandle type) const noexcept {
    return is_valid(type) && !checked_.types.is_copyable(type) && !checked_.types.contains_array(type);
}

bool SemanticAnalyzer::is_explicit_move_expr(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::move_expr;
}

bool SemanticAnalyzer::is_fresh_owned_expr(const syntax::ExprId expr_id) const noexcept {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::call:
    case syntax::ExprKind::try_expr:
    case syntax::ExprKind::if_expr:
    case syntax::ExprKind::block_expr:
    case syntax::ExprKind::match_expr:
    case syntax::ExprKind::struct_literal:
        return true;
    default:
        return false;
    }
}

std::string SemanticAnalyzer::move_source_name(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return {};
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    if (expr.kind != syntax::ExprKind::name || !expr.scope_name.empty()) {
        return {};
    }
    const Symbol* symbol = find_symbol(expr.text, expr.range);
    if (symbol == nullptr || (symbol->kind != SymbolKind::local && symbol->kind != SymbolKind::parameter)) {
        return {};
    }
    return symbol->name;
}

void SemanticAnalyzer::push_ownership_scope() {
    ownership_scopes_.push_back({});
}

void SemanticAnalyzer::pop_ownership_scope() {
    if (ownership_scopes_.empty()) {
        return;
    }
    for (const std::string& name : ownership_scopes_.back()) {
        moved_bindings_.erase(name);
    }
    ownership_scopes_.pop_back();
}

void SemanticAnalyzer::register_ownership_binding(const std::string_view name) {
    if (ownership_scopes_.empty()) {
        push_ownership_scope();
    }
    ownership_scopes_.back().push_back(std::string(name));
    moved_bindings_.erase(std::string(name));
}

void SemanticAnalyzer::mark_ownership_initialized(const std::string_view name) {
    moved_bindings_.erase(std::string(name));
}

void SemanticAnalyzer::mark_ownership_moved(const std::string_view name) {
    moved_bindings_.insert(std::string(name));
}

bool SemanticAnalyzer::is_ownership_moved(const std::string_view name) const {
    return moved_bindings_.contains(std::string(name));
}

void SemanticAnalyzer::report_moved_value_use(const std::string_view name, const base::SourceRange range) {
    report(range, "use of moved value: " + std::string(name));
}

void SemanticAnalyzer::consume_ownership_transfer(
    const syntax::ExprId expr_id,
    const TypeHandle type,
    const std::string_view context
) {
    if (!is_move_only_value(type)) {
        return;
    }
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    if (expr.kind == syntax::ExprKind::move_expr) {
        const std::string source = move_source_name(expr.unary_operand);
        if (!source.empty()) {
            mark_ownership_moved(source);
        }
        return;
    }
    if (is_fresh_owned_expr(expr_id)) {
        return;
    }
    report(module_.exprs[expr_id.value].range, "non-copyable value must be moved explicitly in " + std::string(context));
}

void SemanticAnalyzer::merge_ownership_states(
    const std::unordered_set<std::string>& lhs,
    const std::unordered_set<std::string>& rhs
) {
    moved_bindings_ = lhs;
    moved_bindings_.insert(rhs.begin(), rhs.end());
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

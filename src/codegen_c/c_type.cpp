#include "aurex/codegen_c/c_type.hpp"

#include <utility>

namespace aurex::codegen_c {

std::string c_primitive_name(const syntax::PrimitiveTypeKind primitive) {
    switch (primitive) {
    case syntax::PrimitiveTypeKind::void_: return "void";
    case syntax::PrimitiveTypeKind::bool_: return "bool";
    case syntax::PrimitiveTypeKind::i8: return "int8_t";
    case syntax::PrimitiveTypeKind::u8: return "uint8_t";
    case syntax::PrimitiveTypeKind::i16: return "int16_t";
    case syntax::PrimitiveTypeKind::u16: return "uint16_t";
    case syntax::PrimitiveTypeKind::i32: return "int32_t";
    case syntax::PrimitiveTypeKind::u32: return "uint32_t";
    case syntax::PrimitiveTypeKind::i64: return "int64_t";
    case syntax::PrimitiveTypeKind::u64: return "uint64_t";
    case syntax::PrimitiveTypeKind::isize: return "ptrdiff_t";
    case syntax::PrimitiveTypeKind::usize: return "size_t";
    case syntax::PrimitiveTypeKind::f32: return "float";
    case syntax::PrimitiveTypeKind::f64: return "double";
    case syntax::PrimitiveTypeKind::str: return "aurex_m0_str";
    }
    return "void";
}

sema::TypeHandle type_handle_for_syntax_type(const sema::CheckedModule& checked, const syntax::TypeId type) noexcept {
    if (!syntax::is_valid(type) || type.value >= checked.syntax_type_handles.size()) {
        return sema::invalid_type_handle;
    }
    return checked.syntax_type_handles[type.value];
}

std::string format_c_type(const sema::TypeTable& types, const sema::TypeHandle type, std::string declarator) {
    if (!sema::is_valid(type)) {
        return "void" + (declarator.empty() ? "" : " " + declarator);
    }

    const sema::TypeInfo& info = types.get(type);
    switch (info.kind) {
    case sema::TypeKind::builtin: {
        const std::string primitive = [&info]() {
            switch (info.builtin) {
            case sema::BuiltinType::void_: return std::string("void");
            case sema::BuiltinType::bool_: return std::string("bool");
            case sema::BuiltinType::i8: return std::string("int8_t");
            case sema::BuiltinType::u8: return std::string("uint8_t");
            case sema::BuiltinType::i16: return std::string("int16_t");
            case sema::BuiltinType::u16: return std::string("uint16_t");
            case sema::BuiltinType::i32: return std::string("int32_t");
            case sema::BuiltinType::u32: return std::string("uint32_t");
            case sema::BuiltinType::i64: return std::string("int64_t");
            case sema::BuiltinType::u64: return std::string("uint64_t");
            case sema::BuiltinType::isize: return std::string("ptrdiff_t");
            case sema::BuiltinType::usize: return std::string("size_t");
            case sema::BuiltinType::f32: return std::string("float");
            case sema::BuiltinType::f64: return std::string("double");
            case sema::BuiltinType::str: return std::string("aurex_m0_str");
            }
            return std::string("void");
        }();
        return declarator.empty() ? primitive : primitive + " " + declarator;
    }
    case sema::TypeKind::pointer: {
        if (!sema::is_valid(info.pointee)) {
            return declarator.empty() ? "void *" : "void *" + declarator;
        }
        const sema::TypeInfo& pointee = types.get(info.pointee);
        if (pointee.kind == sema::TypeKind::array) {
            std::string inner = "(*" + declarator + ")";
            if (info.pointer_mutability == sema::PointerMutability::const_) {
                inner = "const " + inner;
            }
            return format_c_type(types, info.pointee, std::move(inner));
        }
        const std::string pointer_decl = "*" + declarator;
        const std::string emitted = format_c_type(types, info.pointee, pointer_decl);
        if (info.pointer_mutability == sema::PointerMutability::const_) {
            if (pointee.kind == sema::TypeKind::builtin && pointee.builtin == sema::BuiltinType::void_) {
                return declarator.empty() ? "const void *" : "const void *" + declarator;
            }
            return "const " + emitted;
        }
        return emitted;
    }
    case sema::TypeKind::array:
        return format_c_type(types, info.array_element, declarator + "[" + std::to_string(info.array_count) + "]");
    case sema::TypeKind::struct_:
    case sema::TypeKind::enum_:
    case sema::TypeKind::opaque_struct:
        return declarator.empty() ? info.c_name : info.c_name + " " + declarator;
    }
    return "void";
}

std::string format_c_type(
    const syntax::AstModule& module,
    const sema::CheckedModule& checked,
    const syntax::TypeId type,
    std::string declarator
) {
    if (!syntax::is_valid(type) || type.value >= module.types.size()) {
        return "void" + (declarator.empty() ? "" : " " + declarator);
    }
    return format_c_type(checked.types, type_handle_for_syntax_type(checked, type), std::move(declarator));
}

} // namespace aurex::codegen_c

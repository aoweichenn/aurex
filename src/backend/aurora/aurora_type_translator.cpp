#include "aurora_backend_internal.hpp"

#include "Aurora/Air/Type.h"

#include <cstdint>

namespace aurex::backend {

TypeTranslator::TypeTranslator(const ir::Module& src_module)
    : src_module_(src_module) {}

aurora::Type* TypeTranslator::translate_type(const sema::TypeHandle handle) {
    if (!sema::is_valid(handle)) {
        return aurora::Type::getVoidTy();
    }

    auto it = type_map_.find(handle.value);
    if (it != type_map_.end()) {
        return it->second;
    }

    return build_type(handle);
}

aurora::Type* TypeTranslator::build_type(const sema::TypeHandle handle) {
    const sema::TypeInfo& info = src_module_.types.get(handle);

    aurora::Type* result = nullptr;

    switch (info.kind) {
    case sema::TypeKind::builtin: {
        switch (info.builtin) {
        case sema::BuiltinType::void_:
            result = aurora::Type::getVoidTy();
            break;
        case sema::BuiltinType::bool_:
            result = aurora::Type::getInt1Ty();
            break;
        case sema::BuiltinType::i8:
        case sema::BuiltinType::u8:
            result = aurora::Type::getInt8Ty();
            break;
        case sema::BuiltinType::i16:
        case sema::BuiltinType::u16:
            result = aurora::Type::getInt16Ty();
            break;
        case sema::BuiltinType::i32:
        case sema::BuiltinType::u32:
            result = aurora::Type::getInt32Ty();
            break;
        case sema::BuiltinType::i64:
        case sema::BuiltinType::u64:
        case sema::BuiltinType::isize:
        case sema::BuiltinType::usize:
            result = aurora::Type::getInt64Ty();
            break;
        case sema::BuiltinType::f32:
            result = aurora::Type::getFloatTy();
            break;
        case sema::BuiltinType::f64:
            result = aurora::Type::getDoubleTy();
            break;
        case sema::BuiltinType::str:
            result = aurora::Type::getPointerTy(aurora::Type::getInt8Ty());
            break;
        }
        break;
    }
    case sema::TypeKind::pointer: {
        aurora::Type* pointee = translate_type(info.pointee);
        result = aurora::Type::getPointerTy(pointee);
        break;
    }
    case sema::TypeKind::array: {
        aurora::Type* element = translate_type(info.array_element);
        result = aurora::Type::getArrayTy(element, static_cast<unsigned>(info.array_count));
        break;
    }
    case sema::TypeKind::struct_:
    case sema::TypeKind::opaque_struct: {
        auto sit = struct_map_.find(handle.value);
        if (sit != struct_map_.end()) {
            return sit->second;
        }
        const ir::RecordLayout* record = ir::find_record(src_module_, handle);
        if (record != nullptr && !record->is_opaque) {
            result = translate_record_layout(*record);
        } else {
            aurora::SmallVector<aurora::Type*, 8> empty_members;
            result = aurora::Type::getStructTy(std::move(empty_members));
        }
        struct_map_[handle.value] = result;
        break;
    }
    case sema::TypeKind::enum_: {
        sema::TypeHandle underlying = info.enum_underlying;
        if (!sema::is_valid(underlying)) {
            result = aurora::Type::getInt32Ty();
        } else {
            result = translate_type(underlying);
        }
        break;
    }
    }

    type_map_[handle.value] = result;
    return result;
}

aurora::FunctionType* TypeTranslator::translate_function_type(
    const sema::TypeHandle return_type,
    const std::vector<ir::FunctionParam>& params)
{
    aurora::SmallVector<aurora::Type*, 8> param_types;
    for (const ir::FunctionParam& param : params) {
        param_types.push_back(translate_type(param.type));
    }
    aurora::Type* ret_ty = translate_type(return_type);
    return new aurora::FunctionType(ret_ty, param_types);
}

aurora::Type* TypeTranslator::translate_record_layout(const ir::RecordLayout& record) {
    aurora::SmallVector<aurora::Type*, 8> field_types;
    for (const ir::RecordField& field : record.fields) {
        field_types.push_back(translate_type(field.type));
    }
    return aurora::Type::getStructTy(std::move(field_types));
}

void TypeTranslator::translate_all_records() {
    for (const ir::RecordLayout& record : src_module_.records) {
        translate_type(record.type);
    }
}

} // namespace aurex::backend

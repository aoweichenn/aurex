#include "llvm_emit_internal.hpp"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/Target/TargetMachine.h>

namespace aurex::ir {

void LlvmEmitter::declare_records() {
    for (const RecordLayout& record : source_.records) {
        if (!sema::is_valid(record.type)) {
            continue;
        }
        llvm::StructType* type = llvm::StructType::create(context_, record.symbol.empty() ? record.name : record.symbol);
        records_[record.type.value] = type;
    }
    for (const RecordLayout& record : source_.records) {
        auto found = records_.find(record.type.value);
        if (found == records_.end() || record.is_opaque) {
            continue;
        }
        std::vector<llvm::Type*> fields;
        fields.reserve(record.fields.size());
        for (const RecordField& field : record.fields) {
            fields.push_back(llvm_type(field.type));
        }
        found->second->setBody(fields, false);
    }
}

llvm::Type* LlvmEmitter::llvm_type(const sema::TypeHandle type) {
    if (!sema::is_valid(type)) {
        return llvm::Type::getVoidTy(context_);
    }
    const sema::TypeInfo& info = source_.types.get(type);
    switch (info.kind) {
    case sema::TypeKind::builtin:
        switch (info.builtin) {
        case sema::BuiltinType::void_: return llvm::Type::getVoidTy(context_);
        case sema::BuiltinType::bool_: return llvm::Type::getInt1Ty(context_);
        case sema::BuiltinType::i8:
        case sema::BuiltinType::u8: return llvm::Type::getInt8Ty(context_);
        case sema::BuiltinType::i16:
        case sema::BuiltinType::u16: return llvm::Type::getInt16Ty(context_);
        case sema::BuiltinType::i32:
        case sema::BuiltinType::u32: return llvm::Type::getInt32Ty(context_);
        case sema::BuiltinType::i64:
        case sema::BuiltinType::u64: return llvm::Type::getInt64Ty(context_);
        case sema::BuiltinType::isize:
        case sema::BuiltinType::usize: return data_layout().getIntPtrType(context_);
        case sema::BuiltinType::f32: return llvm::Type::getFloatTy(context_);
        case sema::BuiltinType::f64: return llvm::Type::getDoubleTy(context_);
        case sema::BuiltinType::str:
            return llvm::StructType::get(
                llvm::PointerType::get(context_, 0),
                data_layout().getIntPtrType(context_)
            );
        }
        return llvm::Type::getVoidTy(context_);
    case sema::TypeKind::pointer:
        return llvm::PointerType::get(context_, 0);
    case sema::TypeKind::array:
        return llvm::ArrayType::get(llvm_type(info.array_element), info.array_count);
    case sema::TypeKind::struct_:
    case sema::TypeKind::opaque_struct:
        return records_.at(type.value);
    case sema::TypeKind::enum_:
        return llvm_type(info.enum_underlying);
    }
    return llvm::Type::getVoidTy(context_);
}

llvm::Type* LlvmEmitter::pointee_llvm_type(const sema::TypeHandle pointer_type) {
    if (!source_.types.is_pointer(pointer_type)) {
        return llvm::Type::getVoidTy(context_);
    }
    return llvm_type(source_.types.get(pointer_type).pointee);
}

sema::TypeHandle LlvmEmitter::pointee_type(const ValueId value) const noexcept {
    const sema::TypeHandle type = source_.values[value.value].type;
    if (!source_.types.is_pointer(type)) {
        return sema::invalid_type_handle;
    }
    return source_.types.get(type).pointee;
}

bool LlvmEmitter::is_unsigned_integer(const sema::TypeHandle type) const noexcept {
    if (!sema::is_valid(type)) {
        return false;
    }
    const sema::TypeInfo& info = source_.types.get(type);
    if (info.kind == sema::TypeKind::enum_) {
        return is_unsigned_integer(info.enum_underlying);
    }
    if (info.kind != sema::TypeKind::builtin) {
        return false;
    }
    switch (info.builtin) {
    case sema::BuiltinType::u8:
    case sema::BuiltinType::u16:
    case sema::BuiltinType::u32:
    case sema::BuiltinType::u64:
    case sema::BuiltinType::usize:
        return true;
    default:
        return false;
    }
}

const llvm::DataLayout& LlvmEmitter::data_layout() const {
    return module_->getDataLayout();
}

} // namespace aurex::ir

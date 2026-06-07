#include <backend/llvm/internal/llvm_backend_internal.hpp>

#include <vector>

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/Target/TargetMachine.h>

namespace aurex::backend {

namespace {

constexpr unsigned LLVM_DEFAULT_ADDRESS_SPACE = 0U;

llvm::Type* llvm_fat_pointer_type(llvm::LLVMContext& context, const llvm::DataLayout& data_layout)
{
    return llvm::StructType::get(
        llvm::PointerType::get(context, LLVM_DEFAULT_ADDRESS_SPACE), data_layout.getIntPtrType(context));
}

[[nodiscard]] bool is_trait_object_view_type(const sema::TypeTable& types, const sema::TypeHandle type) noexcept
{
    if (!types.is_pointer(type) && !types.is_reference(type)) {
        return false;
    }
    const sema::TypeHandle pointee = types.get(type).pointee;
    return sema::is_valid(pointee) && pointee.value < types.size()
        && types.get(pointee).kind == sema::TypeKind::trait_object;
}

} // namespace

void LlvmEmitter::declare_records()
{
    for (const RecordLayout& record : this->source_.records) {
        if (!sema::is_valid(record.type)) {
            continue;
        }
        const std::string_view symbol = this->text(record.symbol);
        llvm::StructType* type =
            llvm::StructType::create(this->context_, symbol.empty() ? this->text(record.name) : symbol);
        this->records_[record.type.value] = type;
    }
    for (const RecordLayout& record : this->source_.records) {
        auto found = this->records_.find(record.type.value);
        if (found == this->records_.end() || record.is_opaque) {
            continue;
        }
        std::vector<llvm::Type*> fields;
        fields.reserve(record.fields.size());
        for (const RecordField& field : record.fields) {
            fields.push_back(this->llvm_type(field.type));
        }
        found->second->setBody(fields, false);
    }
}

llvm::Type* LlvmEmitter::llvm_type(const sema::TypeHandle type)
{
    if (!sema::is_valid(type)) {
        return llvm::Type::getVoidTy(this->context_);
    }

    std::vector<base::u64> array_counts;
    sema::TypeHandle current = type;
    llvm::Type* result = nullptr;
    while (sema::is_valid(current)) {
        const sema::TypeInfo& info = this->source_.types.get(current);
        switch (info.kind) {
            case sema::TypeKind::builtin:
                switch (info.builtin) {
                    case sema::BuiltinType::void_:
                        result = llvm::Type::getVoidTy(this->context_);
                        break;
                    case sema::BuiltinType::bool_:
                        result = llvm::Type::getInt1Ty(this->context_);
                        break;
                    case sema::BuiltinType::i8:
                    case sema::BuiltinType::u8:
                        result = llvm::Type::getInt8Ty(this->context_);
                        break;
                    case sema::BuiltinType::i16:
                    case sema::BuiltinType::u16:
                        result = llvm::Type::getInt16Ty(this->context_);
                        break;
                    case sema::BuiltinType::i32:
                    case sema::BuiltinType::u32:
                        result = llvm::Type::getInt32Ty(this->context_);
                        break;
                    case sema::BuiltinType::char_:
                        result = llvm::Type::getInt32Ty(this->context_);
                        break;
                    case sema::BuiltinType::i64:
                    case sema::BuiltinType::u64:
                        result = llvm::Type::getInt64Ty(this->context_);
                        break;
                    case sema::BuiltinType::isize:
                    case sema::BuiltinType::usize:
                        result = this->data_layout().getIntPtrType(this->context_);
                        break;
                    case sema::BuiltinType::f32:
                        result = llvm::Type::getFloatTy(this->context_);
                        break;
                    case sema::BuiltinType::f64:
                        result = llvm::Type::getDoubleTy(this->context_);
                        break;
                    case sema::BuiltinType::str:
                        result = llvm_fat_pointer_type(this->context_, this->data_layout());
                        break;
                }
                break;
            case sema::TypeKind::pointer:
            case sema::TypeKind::reference:
                if (is_trait_object_view_type(this->source_.types, current)) {
                    result = this->llvm_trait_object_view_type();
                    break;
                }
                [[fallthrough]];
            case sema::TypeKind::function:
                result = llvm::PointerType::get(this->context_, LLVM_DEFAULT_ADDRESS_SPACE);
                break;
            case sema::TypeKind::slice:
                result = llvm_fat_pointer_type(this->context_, this->data_layout());
                break;
            case sema::TypeKind::array:
                array_counts.push_back(info.array_count);
                current = info.array_element;
                continue;
            case sema::TypeKind::struct_:
            case sema::TypeKind::tuple:
            case sema::TypeKind::opaque_struct:
                result = this->records_.at(current.value);
                break;
            case sema::TypeKind::enum_:
                if (sema::is_valid(info.enum_payload_storage)) {
                    result = this->records_.at(current.value);
                    break;
                }
                current = info.enum_underlying;
                continue;
            case sema::TypeKind::generic_param:
            case sema::TypeKind::associated_projection:
            case sema::TypeKind::trait_object:
                result = llvm::Type::getVoidTy(this->context_);
                break;
        }
        break;
    }

    if (result == nullptr) {
        result = llvm::Type::getVoidTy(this->context_);
    }
    for (auto count = array_counts.rbegin(); count != array_counts.rend(); ++count) {
        result = llvm::ArrayType::get(result, *count);
    }
    return result;
}

llvm::FunctionType* LlvmEmitter::llvm_function_type(const Function& function)
{
    std::vector<llvm::Type*> params;
    params.reserve(function.signature_params.size());
    for (const FunctionParam& param : function.signature_params) {
        params.push_back(this->llvm_type(param.type));
    }
    return llvm::FunctionType::get(this->llvm_type(function.return_type), params, function.is_variadic);
}

llvm::FunctionType* LlvmEmitter::llvm_function_type(const sema::TypeHandle function_type)
{
    if (!this->source_.types.is_function(function_type)) {
        return llvm::FunctionType::get(llvm::Type::getVoidTy(this->context_), false);
    }
    const sema::TypeInfo& function = this->source_.types.get(function_type);
    std::vector<llvm::Type*> params;
    params.reserve(function.function_params.size());
    for (const sema::TypeHandle param : function.function_params) {
        params.push_back(this->llvm_type(param));
    }
    return llvm::FunctionType::get(this->llvm_type(function.function_return), params, function.function_is_variadic);
}

llvm::Type* LlvmEmitter::llvm_trait_object_view_type()
{
    return llvm::StructType::get(llvm::PointerType::get(this->context_, LLVM_DEFAULT_ADDRESS_SPACE),
        llvm::PointerType::get(this->context_, LLVM_DEFAULT_ADDRESS_SPACE));
}

llvm::ArrayType* LlvmEmitter::llvm_vtable_array_type(const TraitObjectVTableLayout& layout)
{
    return llvm::ArrayType::get(
        llvm::PointerType::get(this->context_, LLVM_DEFAULT_ADDRESS_SPACE), layout.method_slots.size());
}

llvm::Type* LlvmEmitter::pointee_llvm_type(const sema::TypeHandle pointer_type)
{
    if (!this->source_.types.is_pointer(pointer_type) && !this->source_.types.is_reference(pointer_type)) {
        return llvm::Type::getVoidTy(this->context_);
    }
    return this->llvm_type(this->source_.types.get(pointer_type).pointee);
}

sema::TypeHandle LlvmEmitter::pointee_type(const ValueId value) const noexcept
{
    const sema::TypeHandle type = this->source_.values[value.value].type;
    if (!this->source_.types.is_pointer(type) && !this->source_.types.is_reference(type)) {
        return sema::INVALID_TYPE_HANDLE;
    }
    return this->source_.types.get(type).pointee;
}

bool LlvmEmitter::is_unsigned_integer(const sema::TypeHandle type) const noexcept
{
    sema::TypeHandle current = type;
    while (sema::is_valid(current)) {
        const sema::TypeInfo& info = this->source_.types.get(current);
        if (info.kind != sema::TypeKind::enum_) {
            if (info.kind != sema::TypeKind::builtin) {
                return false;
            }
            switch (info.builtin) {
                case sema::BuiltinType::bool_:
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
        current = info.enum_underlying;
    }
    return false;
}

const llvm::DataLayout& LlvmEmitter::data_layout() const
{
    return this->module_->getDataLayout();
}

} // namespace aurex::backend

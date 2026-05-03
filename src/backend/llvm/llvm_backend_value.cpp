#include "llvm_backend_internal.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/ConstantFold.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

#include <vector>

namespace aurex::backend {

llvm::Value* LlvmEmitter::emit_value(const ValueId id) {
    const Value& value = source_.values[id.value];
    switch (value.kind) {
    case ValueKind::param:
    case ValueKind::phi:
        return values_.at(id.value);
    case ValueKind::integer_literal:
    case ValueKind::bool_literal:
    case ValueKind::byte_literal:
    case ValueKind::null_literal:
    case ValueKind::string_literal:
    case ValueKind::c_string_literal:
    case ValueKind::constant_ref:
    case ValueKind::alloca:
    case ValueKind::load:
    case ValueKind::store:
    case ValueKind::unary:
    case ValueKind::binary:
    case ValueKind::call:
    case ValueKind::field_addr:
    case ValueKind::index_addr:
    case ValueKind::aggregate:
    case ValueKind::cast:
    case ValueKind::size_of:
    case ValueKind::align_of:
        return emit_runtime_value(value);
    }
    return llvm::UndefValue::get(llvm_type(value.type));
}

llvm::Value* LlvmEmitter::emit_runtime_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::param:
    case ValueKind::phi:
        break;
    case ValueKind::integer_literal:
        return integer_constant(value.type, value.text);
    case ValueKind::bool_literal:
        return llvm::ConstantInt::get(llvm_type(value.type), value.text == "true" ? 1 : 0, false);
    case ValueKind::byte_literal:
        return llvm::ConstantInt::get(llvm_type(value.type), parse_byte_literal(value.text), false);
    case ValueKind::null_literal:
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm_type(value.type)));
    case ValueKind::string_literal:
        return emit_string_literal(value.text, false);
    case ValueKind::c_string_literal:
        return emit_string_literal(value.text, true);
    case ValueKind::constant_ref:
        return emit_constant_ref(value);
    case ValueKind::alloca:
        return builder_.CreateAlloca(pointee_llvm_type(value.type), nullptr, value.name);
    case ValueKind::load:
        return builder_.CreateLoad(llvm_type(value.type), get(value.object), value.name);
    case ValueKind::store:
        return builder_.CreateStore(get(value.lhs), get(value.object));
    case ValueKind::unary:
        return emit_unary(value);
    case ValueKind::binary:
        return emit_binary(value);
    case ValueKind::call:
        return emit_call(value);
    case ValueKind::field_addr:
        return emit_field_addr(value);
    case ValueKind::index_addr:
        return emit_index_addr(value);
    case ValueKind::aggregate:
        return emit_aggregate(value);
    case ValueKind::cast:
        return emit_cast(value);
    case ValueKind::size_of:
        return emit_size_of(value.target_type);
    case ValueKind::align_of:
        return emit_align_of(value.target_type);
    }
    return llvm::UndefValue::get(llvm_type(value.type));
}

llvm::Value* LlvmEmitter::emit_constant_ref(const Value& value) {
    const GlobalConstant* constant = find_global_constant(source_, value.constant);
    if (constant == nullptr) {
        return llvm::UndefValue::get(llvm_type(value.type));
    }
    auto found = constants_.find(value.constant.value);
    if (found == constants_.end()) {
        return emit_runtime_value(source_.values[constant->initializer.value]);
    }
    return builder_.CreateLoad(llvm_type(constant->type), found->second, constant->symbol + ".value");
}

llvm::Constant* LlvmEmitter::emit_constant_initializer(const Value& value) {
    switch (value.kind) {
    case ValueKind::integer_literal:
        return llvm::cast<llvm::Constant>(integer_constant(value.type, value.text));
    case ValueKind::bool_literal:
        return llvm::ConstantInt::get(llvm_type(value.type), value.text == "true" ? 1 : 0, false);
    case ValueKind::byte_literal:
        return llvm::ConstantInt::get(llvm_type(value.type), parse_byte_literal(value.text), false);
    case ValueKind::null_literal:
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(llvm_type(value.type)));
    case ValueKind::string_literal:
        return emit_constant_string(value.text, false);
    case ValueKind::c_string_literal:
        return emit_constant_string(value.text, true);
    case ValueKind::constant_ref: {
        const GlobalConstant* constant = find_global_constant(source_, value.constant);
        if (constant == nullptr) {
            return llvm::UndefValue::get(llvm_type(value.type));
        }
        return emit_constant_initializer(source_.values[constant->initializer.value]);
    }
    case ValueKind::aggregate:
        return emit_constant_aggregate(value);
    case ValueKind::cast:
        return emit_constant_cast(value);
    case ValueKind::size_of:
        return llvm::cast<llvm::Constant>(emit_size_of(value.target_type));
    case ValueKind::align_of:
        return llvm::cast<llvm::Constant>(emit_align_of(value.target_type));
    default:
        return llvm::UndefValue::get(llvm_type(value.type));
    }
}

llvm::Constant* LlvmEmitter::emit_constant_cast(const Value& value) {
    llvm::Constant* operand = emit_constant_initializer(source_.values[value.lhs.value]);
    llvm::Type* target = llvm_type(value.target_type);
    if (operand->getType() == target) {
        return operand;
    }
    const bool source_unsigned = is_unsigned_integer(source_.values[value.lhs.value].type);
    unsigned opcode = llvm::Instruction::BitCast;
    switch (value.cast_kind) {
    case CastKind::numeric:
        if (operand->getType()->isIntegerTy() && target->isIntegerTy()) {
            const unsigned source_bits = operand->getType()->getIntegerBitWidth();
            const unsigned target_bits = target->getIntegerBitWidth();
            if (source_bits == target_bits) {
                return operand;
            }
            opcode = target_bits < source_bits
                ? llvm::Instruction::Trunc
                : (source_unsigned ? llvm::Instruction::ZExt : llvm::Instruction::SExt);
            break;
        }
        if (operand->getType()->isIntegerTy() && target->isFloatingPointTy()) {
            opcode = source_unsigned ? llvm::Instruction::UIToFP : llvm::Instruction::SIToFP;
            break;
        }
        if (operand->getType()->isFloatingPointTy() && target->isIntegerTy()) {
            opcode = is_unsigned_integer(value.target_type) ? llvm::Instruction::FPToUI : llvm::Instruction::FPToSI;
            break;
        }
        if (operand->getType()->isFloatingPointTy() && target->isFloatingPointTy()) {
            const unsigned source_bits = operand->getType()->getScalarSizeInBits();
            const unsigned target_bits = target->getScalarSizeInBits();
            opcode = target_bits < source_bits ? llvm::Instruction::FPTrunc : llvm::Instruction::FPExt;
            break;
        }
        return operand;
    case CastKind::pointer:
        opcode = llvm::Instruction::BitCast;
        break;
    case CastKind::bitcast:
        opcode = llvm::Instruction::BitCast;
        break;
    case CastKind::ptr_addr:
        opcode = llvm::Instruction::PtrToInt;
        break;
    case CastKind::ptr_from_addr:
        opcode = llvm::Instruction::IntToPtr;
        break;
    }
    llvm::Constant* folded = llvm::ConstantFoldCastInstruction(opcode, operand, target);
    return folded == nullptr ? llvm::UndefValue::get(target) : folded;
}

llvm::Constant* LlvmEmitter::emit_constant_aggregate(const Value& value) {
    llvm::StructType* type = llvm::cast<llvm::StructType>(llvm_type(value.type));
    const RecordLayout* record = find_record(source_, value.type);
    if (record == nullptr) {
        return llvm::UndefValue::get(type);
    }

    std::vector<llvm::Constant*> fields(record->fields.size());
    for (const FieldValue& field : value.fields) {
        const base::usize index = record_field_index(*record, field.name);
        if (index < fields.size()) {
            fields[index] = emit_constant_initializer(source_.values[field.value.value]);
        }
    }
    for (base::usize i = 0; i < fields.size(); ++i) {
        if (fields[i] == nullptr) {
            fields[i] = llvm::UndefValue::get(llvm_type(record->fields[i].type));
        }
    }
    return llvm::ConstantStruct::get(type, fields);
}

llvm::Constant* LlvmEmitter::emit_constant_string(const std::string& literal, const bool c_string) {
    if (c_string) {
        std::string decoded = decode_string_literal(literal, true);
        decoded.push_back('\0');
        return llvm::cast<llvm::Constant>(global_string_pointer(decoded, "const.cstr", false));
    }
    return llvm::cast<llvm::Constant>(emit_string_literal(literal, false));
}

llvm::Value* LlvmEmitter::emit_unary(const Value& value) {
    llvm::Value* operand = get(value.lhs);
    switch (value.unary_op) {
    case UnaryOp::logical_not:
        return builder_.CreateNot(operand);
    case UnaryOp::numeric_negate:
        return source_.types.is_float(value.type) ? builder_.CreateFNeg(operand) : builder_.CreateNeg(operand);
    case UnaryOp::bitwise_not:
        return builder_.CreateNot(operand);
    case UnaryOp::address_of:
    case UnaryOp::dereference:
        return operand;
    }
    return operand;
}

llvm::Value* LlvmEmitter::emit_binary(const Value& value) {
    llvm::Value* lhs = get(value.lhs);
    llvm::Value* rhs = get(value.rhs);
    const sema::TypeHandle operand_type = source_.values[value.lhs.value].type;
    const bool is_float = source_.types.is_float(operand_type);
    const bool is_unsigned = is_unsigned_integer(operand_type);
    switch (value.binary_op) {
    case BinaryOp::add: return is_float ? builder_.CreateFAdd(lhs, rhs) : builder_.CreateAdd(lhs, rhs);
    case BinaryOp::sub: return is_float ? builder_.CreateFSub(lhs, rhs) : builder_.CreateSub(lhs, rhs);
    case BinaryOp::mul: return is_float ? builder_.CreateFMul(lhs, rhs) : builder_.CreateMul(lhs, rhs);
    case BinaryOp::div: return is_float ? builder_.CreateFDiv(lhs, rhs) : (is_unsigned ? builder_.CreateUDiv(lhs, rhs) : builder_.CreateSDiv(lhs, rhs));
    case BinaryOp::mod: return is_unsigned ? builder_.CreateURem(lhs, rhs) : builder_.CreateSRem(lhs, rhs);
    case BinaryOp::shl: return builder_.CreateShl(lhs, rhs);
    case BinaryOp::shr: return is_unsigned ? builder_.CreateLShr(lhs, rhs) : builder_.CreateAShr(lhs, rhs);
    case BinaryOp::less: return is_float ? builder_.CreateFCmpOLT(lhs, rhs) : (is_unsigned ? builder_.CreateICmpULT(lhs, rhs) : builder_.CreateICmpSLT(lhs, rhs));
    case BinaryOp::less_equal: return is_float ? builder_.CreateFCmpOLE(lhs, rhs) : (is_unsigned ? builder_.CreateICmpULE(lhs, rhs) : builder_.CreateICmpSLE(lhs, rhs));
    case BinaryOp::greater: return is_float ? builder_.CreateFCmpOGT(lhs, rhs) : (is_unsigned ? builder_.CreateICmpUGT(lhs, rhs) : builder_.CreateICmpSGT(lhs, rhs));
    case BinaryOp::greater_equal: return is_float ? builder_.CreateFCmpOGE(lhs, rhs) : (is_unsigned ? builder_.CreateICmpUGE(lhs, rhs) : builder_.CreateICmpSGE(lhs, rhs));
    case BinaryOp::equal: return is_float ? builder_.CreateFCmpOEQ(lhs, rhs) : builder_.CreateICmpEQ(lhs, rhs);
    case BinaryOp::not_equal: return is_float ? builder_.CreateFCmpONE(lhs, rhs) : builder_.CreateICmpNE(lhs, rhs);
    case BinaryOp::bit_and:
    case BinaryOp::logical_and: return builder_.CreateAnd(lhs, rhs);
    case BinaryOp::bit_xor: return builder_.CreateXor(lhs, rhs);
    case BinaryOp::bit_or:
    case BinaryOp::logical_or: return builder_.CreateOr(lhs, rhs);
    }
    return lhs;
}

llvm::Value* LlvmEmitter::emit_call(const Value& value) {
    llvm::Function* target = nullptr;
    if (is_valid(value.call_target)) {
        target = functions_.at(value.call_target.value);
    } else {
        target = module_->getFunction(value.name);
    }
    if (target == nullptr) {
        return llvm::UndefValue::get(llvm_type(value.type));
    }
    std::vector<llvm::Value*> args;
    args.reserve(value.args.size());
    for (const ValueId arg : value.args) {
        args.push_back(get(arg));
    }
    if (source_.types.is_void(value.type)) {
        return builder_.CreateCall(target, args);
    }
    return builder_.CreateCall(target, args, value.name.empty() ? "" : value.name + ".result");
}

llvm::Value* LlvmEmitter::emit_field_addr(const Value& value) {
    const sema::TypeHandle object_pointee = pointee_type(value.object);
    const sema::TypeHandle record_type = source_.types.is_pointer(object_pointee)
        ? source_.types.get(object_pointee).pointee
        : object_pointee;
    const RecordLayout* record = find_record(source_, record_type);
    const base::usize index = record == nullptr ? 0 : record_field_index(*record, value.name);
    return builder_.CreateStructGEP(llvm_type(record_type), get(value.object), static_cast<unsigned>(index), value.name + ".addr");
}

llvm::Value* LlvmEmitter::emit_index_addr(const Value& value) {
    const sema::TypeHandle object_pointee = pointee_type(value.object);
    llvm::Value* object = get(value.object);
    llvm::Value* index = get(value.index);
    if (source_.types.is_array(object_pointee)) {
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0);
        return builder_.CreateGEP(llvm_type(object_pointee), object, {zero, index}, "index.addr");
    }
    return builder_.CreateGEP(llvm_type(object_pointee), object, index, "index.addr");
}

llvm::Value* LlvmEmitter::emit_aggregate(const Value& value) {
    llvm::Value* aggregate = llvm::UndefValue::get(llvm_type(value.type));
    const RecordLayout* record = find_record(source_, value.type);
    if (record == nullptr) {
        return aggregate;
    }
    for (const FieldValue& field : value.fields) {
        const base::usize index = record_field_index(*record, field.name);
        aggregate = builder_.CreateInsertValue(aggregate, get(field.value), {static_cast<unsigned>(index)});
    }
    return aggregate;
}

llvm::Value* LlvmEmitter::emit_cast(const Value& value) {
    llvm::Value* operand = get(value.lhs);
    llvm::Type* target = llvm_type(value.target_type);
    const bool source_unsigned = is_unsigned_integer(source_.values[value.lhs.value].type);
    switch (value.cast_kind) {
    case CastKind::numeric:
        if (operand->getType()->isIntegerTy() && target->isIntegerTy()) {
            return builder_.CreateIntCast(operand, target, !source_unsigned);
        }
        if (operand->getType()->isIntegerTy() && target->isFloatingPointTy()) {
            return source_unsigned ? builder_.CreateUIToFP(operand, target) : builder_.CreateSIToFP(operand, target);
        }
        if (operand->getType()->isFloatingPointTy() && target->isIntegerTy()) {
            return is_unsigned_integer(value.target_type) ? builder_.CreateFPToUI(operand, target) : builder_.CreateFPToSI(operand, target);
        }
        if (operand->getType()->isFloatingPointTy() && target->isFloatingPointTy()) {
            return builder_.CreateFPCast(operand, target);
        }
        return operand;
    case CastKind::pointer:
    case CastKind::bitcast:
        return builder_.CreateBitCast(operand, target);
    case CastKind::ptr_addr:
        return builder_.CreatePtrToInt(operand, target);
    case CastKind::ptr_from_addr:
        return builder_.CreateIntToPtr(operand, target);
    }
    return operand;
}

llvm::Value* LlvmEmitter::emit_size_of(const sema::TypeHandle type) {
    llvm::TypeSize size = data_layout().getTypeAllocSize(llvm_type(type));
    return llvm::ConstantInt::get(llvm_type(source_.types.builtin(sema::BuiltinType::usize)), size.getFixedValue());
}

llvm::Value* LlvmEmitter::emit_align_of(const sema::TypeHandle type) {
    llvm::Align align = data_layout().getABITypeAlign(llvm_type(type));
    return llvm::ConstantInt::get(llvm_type(source_.types.builtin(sema::BuiltinType::usize)), align.value());
}

llvm::Value* LlvmEmitter::integer_constant(const sema::TypeHandle type, const std::string& text) {
    std::uint64_t value = 0;
    static_cast<void>(parse_u64(text, value));
    return llvm::ConstantInt::get(llvm_type(type), value, !is_unsigned_integer(type));
}

llvm::Value* LlvmEmitter::emit_string_literal(const std::string& literal, const bool c_string) {
    std::string decoded = decode_string_literal(literal, c_string);
    if (c_string) {
        decoded.push_back('\0');
        return global_string_pointer(decoded, "cstr", false);
    }

    llvm::Value* data = global_string_pointer(decoded, "str.data", false);
    const sema::TypeHandle str_type = source_.types.builtin(sema::BuiltinType::str);
    llvm::Value* result = llvm::UndefValue::get(llvm_type(str_type));
    result = builder_.CreateInsertValue(result, data, {0});
    result = builder_.CreateInsertValue(
        result,
        llvm::ConstantInt::get(llvm_type(source_.types.builtin(sema::BuiltinType::usize)), decoded.size()),
        {1}
    );
    return result;
}

llvm::Value* LlvmEmitter::global_string_pointer(const std::string& text, const std::string& name, const bool add_null) {
    llvm::GlobalVariable* global = builder_.CreateGlobalString(text, name, 0, module_.get(), add_null);
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
    return builder_.CreateInBoundsGEP(global->getValueType(), global, {zero, zero}, name + ".ptr");
}

} // namespace aurex::backend

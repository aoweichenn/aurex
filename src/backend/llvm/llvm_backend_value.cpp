#include <backend/llvm/llvm_backend_internal.hpp>

#include <llvm/IR/Constants.h>
#include <llvm/IR/ConstantFold.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

#include <cstdint>
#include <string>
#include <vector>

namespace aurex::backend {
namespace {

constexpr char LLVM_BACKEND_VALUE_BOOL_TRUE_TEXT[] = "true";
constexpr char LLVM_BACKEND_VALUE_CONSTANT_VALUE_SUFFIX[] = ".value";
constexpr char LLVM_BACKEND_VALUE_CONSTANT_C_STRING_NAME[] = "const.cstr";
constexpr char LLVM_BACKEND_VALUE_C_STRING_NAME[] = "cstr";
constexpr char LLVM_BACKEND_VALUE_STRING_DATA_NAME[] = "str.data";
constexpr char LLVM_BACKEND_VALUE_STRING_LENGTH_NAME[] = "str.len";
constexpr char LLVM_BACKEND_VALUE_SLICE_DATA_NAME[] = "slice.data";
constexpr char LLVM_BACKEND_VALUE_SLICE_LENGTH_NAME[] = "slice.len";
constexpr char LLVM_BACKEND_VALUE_CALL_RESULT_SUFFIX[] = ".result";
constexpr char LLVM_BACKEND_VALUE_FIELD_ADDRESS_SUFFIX[] = ".addr";
constexpr char LLVM_BACKEND_VALUE_INDEX_ADDRESS_NAME[] = "index.addr";
constexpr char LLVM_BACKEND_VALUE_GLOBAL_POINTER_SUFFIX[] = ".ptr";
constexpr unsigned LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX = 0U;
constexpr unsigned LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX = 1U;
constexpr unsigned LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX = 0U;
constexpr unsigned LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX = 1U;
constexpr unsigned LLVM_BACKEND_VALUE_GLOBAL_STRING_ADDRESS_SPACE = 0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_ZERO_INTEGER = 0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_BOOL_TRUE_INTEGER = 1U;
constexpr double LLVM_BACKEND_VALUE_ZERO_FLOAT = 0.0;
constexpr char LLVM_BACKEND_VALUE_NULL_TERMINATOR = '\0';

} // namespace

llvm::Value* LlvmEmitter::emit_value(const ValueId id) {
    return this->emit_runtime_value(this->source_.values[id.value]);
}

llvm::Value* LlvmEmitter::emit_runtime_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::integer_literal:
        return this->integer_constant(value.type, value.text);
    case ValueKind::float_literal:
        return this->float_constant(value.type, value.text);
    case ValueKind::bool_literal:
        return llvm::ConstantInt::get(
            this->llvm_type(value.type),
            value.text == LLVM_BACKEND_VALUE_BOOL_TRUE_TEXT
                ? LLVM_BACKEND_VALUE_BOOL_TRUE_INTEGER
                : LLVM_BACKEND_VALUE_ZERO_INTEGER,
            false
        );
    case ValueKind::byte_literal:
        return llvm::ConstantInt::get(this->llvm_type(value.type), parse_byte_literal(value.text), false);
    case ValueKind::undef:
        return llvm::UndefValue::get(this->llvm_type(value.type));
    case ValueKind::null_literal:
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(this->llvm_type(value.type)));
    case ValueKind::string_literal:
        return this->emit_string_literal(value.text, false);
    case ValueKind::c_string_literal:
        return this->emit_string_literal(value.text, true);
    case ValueKind::constant_ref:
        return this->emit_constant_ref(value);
    case ValueKind::function_ref:
        return this->emit_function_ref(value);
    case ValueKind::alloca:
        return this->builder_.CreateAlloca(this->pointee_llvm_type(value.type), nullptr, value.name);
    case ValueKind::load:
        return this->builder_.CreateLoad(this->llvm_type(value.type), this->get(value.object), value.name);
    case ValueKind::store:
        return this->builder_.CreateStore(this->get(value.lhs), this->get(value.object));
    case ValueKind::unary:
        return this->emit_unary(value);
    case ValueKind::binary:
        return this->emit_binary(value);
    case ValueKind::call:
        return this->emit_call(value);
    case ValueKind::field_addr:
        return this->emit_field_addr(value);
    case ValueKind::index_addr:
        return this->emit_index_addr(value);
    case ValueKind::aggregate:
        return this->emit_aggregate(value);
    case ValueKind::slice: {
        llvm::Value* result = llvm::UndefValue::get(this->llvm_type(value.type));
        result = this->builder_.CreateInsertValue(
            result,
            this->get(value.lhs),
            {LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX}
        );
        result = this->builder_.CreateInsertValue(
            result,
            this->get(value.rhs),
            {LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX}
        );
        return result;
    }
    case ValueKind::slice_data:
        return this->builder_.CreateExtractValue(
            this->get(value.object),
            {LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX},
            LLVM_BACKEND_VALUE_SLICE_DATA_NAME
        );
    case ValueKind::slice_len:
        return this->builder_.CreateExtractValue(
            this->get(value.object),
            {LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX},
            LLVM_BACKEND_VALUE_SLICE_LENGTH_NAME
        );
    case ValueKind::cast:
        return this->emit_cast(value);
    case ValueKind::size_of:
        return this->emit_size_of(value.target_type);
    case ValueKind::align_of:
        return this->emit_align_of(value.target_type);
    case ValueKind::str_data:
        return this->builder_.CreateExtractValue(
            this->get(value.object),
            {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX},
            LLVM_BACKEND_VALUE_STRING_DATA_NAME
        );
    case ValueKind::str_byte_len:
        return this->builder_.CreateExtractValue(
            this->get(value.object),
            {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX},
            LLVM_BACKEND_VALUE_STRING_LENGTH_NAME
        );
    case ValueKind::str_from_bytes_unchecked: {
        llvm::Value* result = llvm::UndefValue::get(this->llvm_type(value.type));
        result = this->builder_.CreateInsertValue(
            result,
            this->get(value.args[LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX]),
            {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX}
        );
        result = this->builder_.CreateInsertValue(
            result,
            this->get(value.args[LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX]),
            {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX}
        );
        return result;
    }
    default:
        break;
    }
    return llvm::UndefValue::get(this->llvm_type(value.type));
}

llvm::Value* LlvmEmitter::emit_constant_ref(const Value& value) {
    const GlobalConstant* constant = find_global_constant(this->source_, value.constant);
    auto found = this->constants_.find(value.constant.value);
    return this->builder_.CreateLoad(
        this->llvm_type(constant->type),
        found->second,
        constant->symbol + LLVM_BACKEND_VALUE_CONSTANT_VALUE_SUFFIX
    );
}

llvm::Value* LlvmEmitter::emit_function_ref(const Value& value) {
    return this->functions_.at(value.call_target.value);
}

llvm::Constant* LlvmEmitter::emit_constant_initializer(const Value& value) {
    switch (value.kind) {
    case ValueKind::integer_literal:
        return llvm::cast<llvm::Constant>(this->integer_constant(value.type, value.text));
    case ValueKind::float_literal:
        return llvm::cast<llvm::Constant>(this->float_constant(value.type, value.text));
    case ValueKind::bool_literal:
        return llvm::ConstantInt::get(
            this->llvm_type(value.type),
            value.text == LLVM_BACKEND_VALUE_BOOL_TRUE_TEXT
                ? LLVM_BACKEND_VALUE_BOOL_TRUE_INTEGER
                : LLVM_BACKEND_VALUE_ZERO_INTEGER,
            false
        );
    case ValueKind::byte_literal:
        return llvm::ConstantInt::get(this->llvm_type(value.type), parse_byte_literal(value.text), false);
    case ValueKind::undef:
        return llvm::UndefValue::get(this->llvm_type(value.type));
    case ValueKind::null_literal:
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(this->llvm_type(value.type)));
    case ValueKind::string_literal:
        return this->emit_constant_string(value.text, false);
    case ValueKind::c_string_literal:
        return this->emit_constant_string(value.text, true);
    case ValueKind::constant_ref: {
        const GlobalConstant* constant = find_global_constant(this->source_, value.constant);
        return this->emit_constant_initializer(this->source_.values[constant->initializer.value]);
    }
    case ValueKind::function_ref:
        return llvm::cast<llvm::Constant>(this->emit_function_ref(value));
    case ValueKind::unary:
        return this->emit_constant_unary(value);
    case ValueKind::binary:
        return this->emit_constant_binary(value);
    case ValueKind::aggregate:
        return this->emit_constant_aggregate(value);
    case ValueKind::cast:
        return this->emit_constant_cast(value);
    case ValueKind::size_of:
        return llvm::cast<llvm::Constant>(this->emit_size_of(value.target_type));
    case ValueKind::align_of:
        return llvm::cast<llvm::Constant>(this->emit_align_of(value.target_type));
    default:
        break;
    }
    return llvm::UndefValue::get(this->llvm_type(value.type));
}

llvm::Constant* LlvmEmitter::emit_constant_binary(const Value& value) {
    llvm::Constant* lhs = this->emit_constant_initializer(this->source_.values[value.lhs.value]);
    llvm::Constant* rhs = this->emit_constant_initializer(this->source_.values[value.rhs.value]);
    const sema::TypeHandle operand_type = this->source_.values[value.lhs.value].type;
    const bool is_float = this->source_.types.is_float(operand_type);
    const bool is_unsigned = this->is_unsigned_integer(operand_type);

    const auto fold_binary = [&](const unsigned opcode) -> llvm::Constant* {
        if (llvm::Constant* folded = llvm::ConstantFoldBinaryInstruction(opcode, lhs, rhs);
            folded != nullptr) {
            return folded;
        }
        return llvm::UndefValue::get(this->llvm_type(value.type));
    };
    const auto fold_compare = [&](const llvm::CmpInst::Predicate predicate) -> llvm::Constant* {
        if (llvm::Constant* folded = llvm::ConstantFoldCompareInstruction(predicate, lhs, rhs);
            folded != nullptr) {
            return folded;
        }
        return llvm::UndefValue::get(this->llvm_type(value.type));
    };

    switch (value.binary_op) {
    case BinaryOp::add:
        return fold_binary(is_float ? llvm::Instruction::FAdd : llvm::Instruction::Add);
    case BinaryOp::sub:
        return fold_binary(is_float ? llvm::Instruction::FSub : llvm::Instruction::Sub);
    case BinaryOp::mul:
        return fold_binary(is_float ? llvm::Instruction::FMul : llvm::Instruction::Mul);
    case BinaryOp::div:
        return fold_binary(is_float
            ? llvm::Instruction::FDiv
            : (is_unsigned ? llvm::Instruction::UDiv : llvm::Instruction::SDiv));
    case BinaryOp::mod:
        return fold_binary(is_unsigned ? llvm::Instruction::URem : llvm::Instruction::SRem);
    case BinaryOp::shl:
        return fold_binary(llvm::Instruction::Shl);
    case BinaryOp::shr:
        return fold_binary(is_unsigned ? llvm::Instruction::LShr : llvm::Instruction::AShr);
    case BinaryOp::less:
        return fold_compare(is_float
            ? llvm::CmpInst::FCMP_OLT
            : (is_unsigned ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT));
    case BinaryOp::less_equal:
        return fold_compare(is_float
            ? llvm::CmpInst::FCMP_OLE
            : (is_unsigned ? llvm::CmpInst::ICMP_ULE : llvm::CmpInst::ICMP_SLE));
    case BinaryOp::greater:
        return fold_compare(is_float
            ? llvm::CmpInst::FCMP_OGT
            : (is_unsigned ? llvm::CmpInst::ICMP_UGT : llvm::CmpInst::ICMP_SGT));
    case BinaryOp::greater_equal:
        return fold_compare(is_float
            ? llvm::CmpInst::FCMP_OGE
            : (is_unsigned ? llvm::CmpInst::ICMP_UGE : llvm::CmpInst::ICMP_SGE));
    case BinaryOp::equal:
        return fold_compare(is_float ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ);
    case BinaryOp::not_equal:
        return fold_compare(is_float ? llvm::CmpInst::FCMP_UNE : llvm::CmpInst::ICMP_NE);
    case BinaryOp::bit_and:
    case BinaryOp::logical_and:
        return fold_binary(llvm::Instruction::And);
    case BinaryOp::bit_xor:
        return fold_binary(llvm::Instruction::Xor);
    case BinaryOp::bit_or:
    case BinaryOp::logical_or:
        return fold_binary(llvm::Instruction::Or);
    }
}

llvm::Constant* LlvmEmitter::emit_constant_unary(const Value& value) {
    llvm::Constant* operand = this->emit_constant_initializer(this->source_.values[value.lhs.value]);
    switch (value.unary_op) {
    case UnaryOp::logical_not:
    case UnaryOp::bitwise_not:
        return llvm::ConstantExpr::getNot(operand);
    case UnaryOp::numeric_negate:
        if (this->source_.types.is_float(value.type)) {
            if (llvm::Constant* folded = llvm::ConstantFoldUnaryInstruction(llvm::Instruction::FNeg, operand);
                folded != nullptr) {
                return folded;
            }
            return llvm::UndefValue::get(this->llvm_type(value.type));
        }
        return llvm::ConstantExpr::getNeg(operand);
    case UnaryOp::address_of:
    case UnaryOp::dereference:
        break;
    }
    return llvm::UndefValue::get(this->llvm_type(value.type));
}

llvm::Constant* LlvmEmitter::emit_constant_cast(const Value& value) {
    llvm::Constant* operand = this->emit_constant_initializer(this->source_.values[value.lhs.value]);
    llvm::Type* target = this->llvm_type(value.target_type);
    if (value.cast_kind != CastKind::pointer && operand->getType() == target) {
        return operand;
    }
    const bool source_unsigned = this->is_unsigned_integer(this->source_.values[value.lhs.value].type);
    unsigned opcode = llvm::Instruction::BitCast;
    switch (value.cast_kind) {
    case CastKind::numeric:
        if (this->source_.types.is_bool(value.target_type)) {
            if (operand->getType()->isIntegerTy()) {
                if (llvm::Constant* folded = llvm::ConstantFoldCompareInstruction(
                        llvm::CmpInst::ICMP_NE,
                        operand,
                        llvm::ConstantInt::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER)
                    );
                    folded != nullptr) {
                    return folded;
                }
                return llvm::UndefValue::get(target);
            }
            if (operand->getType()->isFloatingPointTy()) {
                if (llvm::Constant* folded = llvm::ConstantFoldCompareInstruction(
                        llvm::CmpInst::FCMP_UNE,
                        operand,
                        llvm::ConstantFP::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_FLOAT)
                    );
                    folded != nullptr) {
                    return folded;
                }
                return llvm::UndefValue::get(target);
            }
        }
        if (operand->getType()->isIntegerTy() && target->isIntegerTy()) {
            const unsigned source_bits = operand->getType()->getIntegerBitWidth();
            const unsigned target_bits = target->getIntegerBitWidth();
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
            opcode = this->is_unsigned_integer(value.target_type) ? llvm::Instruction::FPToUI : llvm::Instruction::FPToSI;
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
        return operand;
    case CastKind::bcast:
        opcode = llvm::Instruction::BitCast;
        break;
    case CastKind::ptr_addr:
        opcode = llvm::Instruction::PtrToInt;
        break;
    case CastKind::paddr:
        opcode = llvm::Instruction::IntToPtr;
        break;
    }
    llvm::Constant* folded = llvm::ConstantFoldCastInstruction(opcode, operand, target);
    return folded == nullptr ? llvm::UndefValue::get(target) : folded;
}

llvm::Constant* LlvmEmitter::emit_constant_aggregate(const Value& value) {
    if (this->source_.types.is_array(value.type)) {
        llvm::ArrayType* type = llvm::cast<llvm::ArrayType>(this->llvm_type(value.type));
        std::vector<llvm::Constant*> elements;
        elements.reserve(value.elements.size());
        for (const ValueId element : value.elements) {
            elements.push_back(this->emit_constant_initializer(this->source_.values[element.value]));
        }
        return llvm::ConstantArray::get(type, elements);
    }

    llvm::StructType* type = llvm::cast<llvm::StructType>(this->llvm_type(value.type));
    const RecordLayout* record = find_record(this->source_, value.type);
    std::vector<llvm::Constant*> fields(record->fields.size());
    for (const FieldValue& field : value.fields) {
        const base::usize index = record_field_index(*record, field.name);
        fields[index] = this->emit_constant_initializer(this->source_.values[field.value.value]);
    }
    return llvm::ConstantStruct::get(type, fields);
}

llvm::Constant* LlvmEmitter::emit_constant_string(const std::string& literal, const bool c_string) {
    if (c_string) {
        std::string decoded = decode_string_literal(literal, true);
        decoded.push_back(LLVM_BACKEND_VALUE_NULL_TERMINATOR);
        return llvm::cast<llvm::Constant>(
            this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_CONSTANT_C_STRING_NAME, false)
        );
    }
    return llvm::cast<llvm::Constant>(this->emit_string_literal(literal, false));
}

llvm::Value* LlvmEmitter::emit_unary(const Value& value) {
    llvm::Value* operand = this->get(value.lhs);
    switch (value.unary_op) {
    case UnaryOp::logical_not:
        return this->builder_.CreateNot(operand);
    case UnaryOp::numeric_negate:
        return this->source_.types.is_float(value.type) ? this->builder_.CreateFNeg(operand) : this->builder_.CreateNeg(operand);
    case UnaryOp::bitwise_not:
        return this->builder_.CreateNot(operand);
    case UnaryOp::address_of:
    case UnaryOp::dereference:
        return operand;
    }
}

llvm::Value* LlvmEmitter::emit_binary(const Value& value) {
    llvm::Value* lhs = this->get(value.lhs);
    llvm::Value* rhs = this->get(value.rhs);
    const sema::TypeHandle operand_type = this->source_.values[value.lhs.value].type;
    const bool is_float = this->source_.types.is_float(operand_type);
    const bool is_unsigned = this->is_unsigned_integer(operand_type);
    switch (value.binary_op) {
    case BinaryOp::add:
        return is_float ? this->builder_.CreateFAdd(lhs, rhs) : this->builder_.CreateAdd(lhs, rhs);
    case BinaryOp::sub:
        return is_float ? this->builder_.CreateFSub(lhs, rhs) : this->builder_.CreateSub(lhs, rhs);
    case BinaryOp::mul:
        return is_float ? this->builder_.CreateFMul(lhs, rhs) : this->builder_.CreateMul(lhs, rhs);
    case BinaryOp::div:
        return is_float
            ? this->builder_.CreateFDiv(lhs, rhs)
            : (is_unsigned ? this->builder_.CreateUDiv(lhs, rhs) : this->builder_.CreateSDiv(lhs, rhs));
    case BinaryOp::mod:
        return is_unsigned ? this->builder_.CreateURem(lhs, rhs) : this->builder_.CreateSRem(lhs, rhs);
    case BinaryOp::shl:
        return this->builder_.CreateShl(lhs, rhs);
    case BinaryOp::shr:
        return is_unsigned ? this->builder_.CreateLShr(lhs, rhs) : this->builder_.CreateAShr(lhs, rhs);
    case BinaryOp::less:
        return is_float
            ? this->builder_.CreateFCmpOLT(lhs, rhs)
            : (is_unsigned ? this->builder_.CreateICmpULT(lhs, rhs) : this->builder_.CreateICmpSLT(lhs, rhs));
    case BinaryOp::less_equal:
        return is_float
            ? this->builder_.CreateFCmpOLE(lhs, rhs)
            : (is_unsigned ? this->builder_.CreateICmpULE(lhs, rhs) : this->builder_.CreateICmpSLE(lhs, rhs));
    case BinaryOp::greater:
        return is_float
            ? this->builder_.CreateFCmpOGT(lhs, rhs)
            : (is_unsigned ? this->builder_.CreateICmpUGT(lhs, rhs) : this->builder_.CreateICmpSGT(lhs, rhs));
    case BinaryOp::greater_equal:
        return is_float
            ? this->builder_.CreateFCmpOGE(lhs, rhs)
            : (is_unsigned ? this->builder_.CreateICmpUGE(lhs, rhs) : this->builder_.CreateICmpSGE(lhs, rhs));
    case BinaryOp::equal:
        return is_float ? this->builder_.CreateFCmpOEQ(lhs, rhs) : this->builder_.CreateICmpEQ(lhs, rhs);
    case BinaryOp::not_equal:
        return is_float ? this->builder_.CreateFCmpUNE(lhs, rhs) : this->builder_.CreateICmpNE(lhs, rhs);
    case BinaryOp::bit_and:
    case BinaryOp::logical_and:
        return this->builder_.CreateAnd(lhs, rhs);
    case BinaryOp::bit_xor:
        return this->builder_.CreateXor(lhs, rhs);
    case BinaryOp::bit_or:
    case BinaryOp::logical_or:
        return this->builder_.CreateOr(lhs, rhs);
    }
}

llvm::Value* LlvmEmitter::emit_call(const Value& value) {
    std::vector<llvm::Value*> args;
    args.reserve(value.args.size());
    for (const ValueId arg : value.args) {
        args.push_back(this->get(arg));
    }
    if (!is_valid(value.call_target)) {
        llvm::Value* callee = this->get(value.object);
        llvm::FunctionType* type = this->llvm_function_type(this->source_.values[value.object.value].type);
        if (this->source_.types.is_void(value.type)) {
            return this->builder_.CreateCall(type, callee, args);
        }
        return this->builder_.CreateCall(
            type,
            callee,
            args,
            value.name.empty() ? "" : value.name + LLVM_BACKEND_VALUE_CALL_RESULT_SUFFIX
        );
    }

    llvm::Function* target = this->functions_.at(value.call_target.value);
    if (this->source_.types.is_void(value.type)) {
        return this->builder_.CreateCall(target, args);
    }
    return this->builder_.CreateCall(
        target,
        args,
        value.name.empty() ? "" : value.name + LLVM_BACKEND_VALUE_CALL_RESULT_SUFFIX
    );
}

llvm::Value* LlvmEmitter::emit_field_addr(const Value& value) {
    const sema::TypeHandle object_pointee = this->pointee_type(value.object);
    const sema::TypeHandle record_type = this->source_.types.is_pointer(object_pointee)
        ? this->source_.types.get(object_pointee).pointee
        : object_pointee;
    const RecordLayout* record = find_record(this->source_, record_type);
    const base::usize index = record_field_index(*record, value.name);
    return this->builder_.CreateStructGEP(
        this->llvm_type(record_type),
        this->get(value.object),
        static_cast<unsigned>(index),
        value.name + LLVM_BACKEND_VALUE_FIELD_ADDRESS_SUFFIX
    );
}

llvm::Value* LlvmEmitter::emit_index_addr(const Value& value) {
    const sema::TypeHandle object_pointee = this->pointee_type(value.object);
    llvm::Value* object = this->get(value.object);
    llvm::Value* index = this->get(value.index);
    if (this->source_.types.is_array(object_pointee)) {
        llvm::Value* zero = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(this->context_),
            LLVM_BACKEND_VALUE_ZERO_INTEGER
        );
        return this->builder_.CreateGEP(
            this->llvm_type(object_pointee),
            object,
            {zero, index},
            LLVM_BACKEND_VALUE_INDEX_ADDRESS_NAME
        );
    }
    return this->builder_.CreateGEP(this->llvm_type(object_pointee), object, index, LLVM_BACKEND_VALUE_INDEX_ADDRESS_NAME);
}

llvm::Value* LlvmEmitter::emit_aggregate(const Value& value) {
    llvm::Value* aggregate = llvm::UndefValue::get(this->llvm_type(value.type));
    if (this->source_.types.is_array(value.type)) {
        for (base::usize index = 0; index < value.elements.size(); ++index) {
            aggregate = this->builder_.CreateInsertValue(
                aggregate,
                this->get(value.elements[index]),
                {static_cast<unsigned>(index)}
            );
        }
        return aggregate;
    }

    const RecordLayout* record = find_record(this->source_, value.type);
    for (const FieldValue& field : value.fields) {
        const base::usize index = record_field_index(*record, field.name);
        aggregate = this->builder_.CreateInsertValue(aggregate, this->get(field.value), {static_cast<unsigned>(index)});
    }
    return aggregate;
}

llvm::Value* LlvmEmitter::emit_cast(const Value& value) {
    llvm::Value* operand = this->get(value.lhs);
    llvm::Type* target = this->llvm_type(value.target_type);
    const bool source_unsigned = this->is_unsigned_integer(this->source_.values[value.lhs.value].type);
    switch (value.cast_kind) {
    case CastKind::numeric:
        if (this->source_.types.is_bool(value.target_type)) {
            if (operand->getType()->isIntegerTy()) {
                return this->builder_.CreateICmpNE(
                    operand,
                    llvm::ConstantInt::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER)
                );
            }
            if (operand->getType()->isFloatingPointTy()) {
                return this->builder_.CreateFCmpUNE(
                    operand,
                    llvm::ConstantFP::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_FLOAT)
                );
            }
        }
        if (operand->getType()->isIntegerTy() && target->isIntegerTy()) {
            return this->builder_.CreateIntCast(operand, target, !source_unsigned);
        }
        if (operand->getType()->isIntegerTy() && target->isFloatingPointTy()) {
            return source_unsigned ? this->builder_.CreateUIToFP(operand, target) : this->builder_.CreateSIToFP(operand, target);
        }
        if (operand->getType()->isFloatingPointTy() && target->isIntegerTy()) {
            return this->is_unsigned_integer(value.target_type)
                ? this->builder_.CreateFPToUI(operand, target)
                : this->builder_.CreateFPToSI(operand, target);
        }
        if (operand->getType()->isFloatingPointTy() && target->isFloatingPointTy()) {
            return this->builder_.CreateFPCast(operand, target);
        }
        return operand;
    case CastKind::pointer:
    case CastKind::bcast:
        return this->builder_.CreateBitCast(operand, target);
    case CastKind::ptr_addr:
        return this->builder_.CreatePtrToInt(operand, target);
    case CastKind::paddr:
        return this->builder_.CreateIntToPtr(operand, target);
    }
}

llvm::Value* LlvmEmitter::emit_size_of(const sema::TypeHandle type) {
    llvm::TypeSize size = this->data_layout().getTypeAllocSize(this->llvm_type(type));
    return llvm::ConstantInt::get(
        this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)),
        size.getFixedValue()
    );
}

llvm::Value* LlvmEmitter::emit_align_of(const sema::TypeHandle type) {
    llvm::Align align = this->data_layout().getABITypeAlign(this->llvm_type(type));
    return llvm::ConstantInt::get(
        this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)),
        align.value()
    );
}

llvm::Value* LlvmEmitter::integer_constant(const sema::TypeHandle type, const std::string& text) {
    std::uint64_t value = LLVM_BACKEND_VALUE_ZERO_INTEGER;
    static_cast<void>(parse_u64(text, value));
    return llvm::ConstantInt::get(this->llvm_type(type), value, !this->is_unsigned_integer(type));
}

llvm::Value* LlvmEmitter::float_constant(const sema::TypeHandle type, const std::string& text) {
    double value = LLVM_BACKEND_VALUE_ZERO_FLOAT;
    static_cast<void>(parse_f64(text, value));
    return llvm::ConstantFP::get(this->llvm_type(type), value);
}

llvm::Value* LlvmEmitter::emit_string_literal(const std::string& literal, const bool c_string) {
    std::string decoded = decode_string_literal(literal, c_string);
    if (c_string) {
        decoded.push_back(LLVM_BACKEND_VALUE_NULL_TERMINATOR);
        return this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_C_STRING_NAME, false);
    }

    llvm::Value* data = this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_STRING_DATA_NAME, false);
    const sema::TypeHandle str_type = this->source_.types.builtin(sema::BuiltinType::str);
    llvm::Value* result = llvm::UndefValue::get(this->llvm_type(str_type));
    result = this->builder_.CreateInsertValue(result, data, {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX});
    result = this->builder_.CreateInsertValue(
        result,
        llvm::ConstantInt::get(this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)), decoded.size()),
        {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX}
    );
    return result;
}

llvm::Value* LlvmEmitter::global_string_pointer(const std::string& text, const std::string& name, const bool add_null) {
    llvm::GlobalVariable* global = this->builder_.CreateGlobalString(
        text,
        name,
        LLVM_BACKEND_VALUE_GLOBAL_STRING_ADDRESS_SPACE,
        this->module_.get(),
        add_null
    );
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context_), LLVM_BACKEND_VALUE_ZERO_INTEGER);
    return this->builder_.CreateInBoundsGEP(
        global->getValueType(),
        global,
        {zero, zero},
        name + LLVM_BACKEND_VALUE_GLOBAL_POINTER_SUFFIX
    );
}

} // namespace aurex::backend

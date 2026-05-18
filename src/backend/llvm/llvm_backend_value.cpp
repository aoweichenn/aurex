#include <backend/llvm/llvm_backend_internal.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/ConstantFold.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/ErrorHandling.h>

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
constexpr char LLVM_BACKEND_VALUE_UTF8_VALID_NAME[] = "str.utf8.valid";
constexpr char LLVM_BACKEND_VALUE_STR_SLICE_NAME[] = "str.slice";
constexpr char LLVM_BACKEND_VALUE_STR_SLICE_DATA_NAME[] = "str.slice.data";
constexpr char LLVM_BACKEND_VALUE_STR_SLICE_LENGTH_NAME[] = "str.slice.len";
constexpr char LLVM_BACKEND_VALUE_STR_SLICE_SUCCESS_NAME[] = "str.slice.ok";
constexpr char LLVM_BACKEND_VALUE_STR_SLICE_START_BOUNDARY_SUFFIX[] = ".start";
constexpr char LLVM_BACKEND_VALUE_STR_SLICE_END_BOUNDARY_SUFFIX[] = ".end";
constexpr char LLVM_BACKEND_VALUE_UTF8_HELPER_NAME[] = "__aurex_utf8_validate";
constexpr char LLVM_BACKEND_VALUE_UTF8_BOUNDARY_HELPER_NAME[] = "__aurex_utf8_boundary";
constexpr char LLVM_BACKEND_VALUE_UTF8_BOUNDARY_EDGE_BLOCK[] = "edge";
constexpr char LLVM_BACKEND_VALUE_UTF8_BOUNDARY_PROBE_BLOCK[] = "probe";
constexpr char LLVM_BACKEND_VALUE_UTF8_BOUNDARY_LOAD_BLOCK[] = "load";
constexpr char LLVM_BACKEND_VALUE_UTF8_ENTRY_BLOCK[] = "entry";
constexpr char LLVM_BACKEND_VALUE_UTF8_NULL_CHECK_BLOCK[] = "null.check";
constexpr char LLVM_BACKEND_VALUE_UTF8_LOOP_BLOCK[] = "loop";
constexpr char LLVM_BACKEND_VALUE_UTF8_SCAN_BLOCK[] = "scan";
constexpr char LLVM_BACKEND_VALUE_UTF8_VALID_BLOCK[] = "valid";
constexpr char LLVM_BACKEND_VALUE_UTF8_INVALID_BLOCK[] = "invalid";
constexpr char LLVM_BACKEND_VALUE_UTF8_CASE_MATCH_SUFFIX[] = ".match";
constexpr char LLVM_BACKEND_VALUE_UTF8_CASE_BYTES_SUFFIX[] = ".bytes";
constexpr char LLVM_BACKEND_VALUE_UTF8_CASE_SUCCESS_SUFFIX[] = ".success";
constexpr char LLVM_BACKEND_VALUE_UTF8_CASE_MISS_SUFFIX[] = ".miss";
constexpr char LLVM_BACKEND_VALUE_UTF8_INDEX_NAME[] = "i";
constexpr char LLVM_BACKEND_VALUE_UTF8_FIRST_NAME[] = "first";
constexpr char LLVM_BACKEND_VALUE_UTF8_BYTE_NAME[] = "byte";
constexpr char LLVM_BACKEND_VALUE_UTF8_STR_NAME[] = "str.checked";
constexpr char LLVM_BACKEND_VALUE_CALL_RESULT_SUFFIX[] = ".result";
constexpr char LLVM_BACKEND_VALUE_FIELD_ADDRESS_SUFFIX[] = ".addr";
constexpr char LLVM_BACKEND_VALUE_INDEX_ADDRESS_NAME[] = "index.addr";
constexpr char LLVM_BACKEND_VALUE_GLOBAL_POINTER_SUFFIX[] = ".ptr";
[[maybe_unused]] constexpr char LLVM_BACKEND_VALUE_UNHANDLED_BINARY_OP[] = "unhandled IR binary operation";
[[maybe_unused]] constexpr char LLVM_BACKEND_VALUE_UNHANDLED_CAST_KIND[] = "unhandled IR cast kind";
[[maybe_unused]] constexpr char LLVM_BACKEND_VALUE_UNHANDLED_UNARY_OP[] = "unhandled IR unary operation";
constexpr unsigned LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX = 0U;
constexpr unsigned LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX = 1U;
constexpr unsigned LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX = 0U;
constexpr unsigned LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX = 1U;
constexpr unsigned LLVM_BACKEND_VALUE_GLOBAL_STRING_ADDRESS_SPACE = 0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_ZERO_INTEGER = 0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_BOOL_TRUE_INTEGER = 1U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_WIDTH_ONE = 1U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_WIDTH_TWO = 2U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_WIDTH_THREE = 3U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_WIDTH_FOUR = 4U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_ASCII_MAX = 0x7FU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_CONT_MIN = 0x80U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_CONT_MAX = 0xBFU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_TWO_MIN = 0xC2U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_TWO_MAX = 0xDFU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_E0 = 0xE0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_E0_SECOND_MIN = 0xA0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_E1_MIN = 0xE1U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_EC_MAX = 0xECU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_ED = 0xEDU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_ED_SECOND_MAX = 0x9FU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_EE_MIN = 0xEEU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_THREE_EF_MAX = 0xEFU;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_FOUR_F0 = 0xF0U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_FOUR_F0_SECOND_MIN = 0x90U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_FOUR_F1_MIN = 0xF1U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_FOUR_F3_MAX = 0xF3U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_FOUR_F4 = 0xF4U;
constexpr std::uint64_t LLVM_BACKEND_VALUE_UTF8_FOUR_F4_SECOND_MAX = 0x8FU;
constexpr double LLVM_BACKEND_VALUE_ZERO_FLOAT = 0.0;
constexpr char LLVM_BACKEND_VALUE_NULL_TERMINATOR = '\0';

struct Utf8ByteRange {
    std::uint64_t min = LLVM_BACKEND_VALUE_ZERO_INTEGER;
    std::uint64_t max = LLVM_BACKEND_VALUE_ZERO_INTEGER;
};

} // namespace

llvm::Value* LlvmEmitter::emit_value(const ValueId id)
{
    return this->emit_runtime_value(this->source_.values[id.value]);
}

llvm::Value* LlvmEmitter::emit_runtime_value(const Value& value)
{
    switch (value.kind) {
        case ValueKind::integer_literal:
            return this->integer_constant(value.type, this->text(value.text));
        case ValueKind::float_literal:
            return this->float_constant(value.type, this->text(value.text));
        case ValueKind::bool_literal:
            return llvm::ConstantInt::get(this->llvm_type(value.type),
                this->text(value.text) == LLVM_BACKEND_VALUE_BOOL_TRUE_TEXT ? LLVM_BACKEND_VALUE_BOOL_TRUE_INTEGER
                                                                            : LLVM_BACKEND_VALUE_ZERO_INTEGER,
                false);
        case ValueKind::char_literal:
            return llvm::ConstantInt::get(
                this->llvm_type(value.type), parse_char_literal(this->text(value.text)), false);
        case ValueKind::byte_literal:
            return llvm::ConstantInt::get(
                this->llvm_type(value.type), parse_byte_literal(this->text(value.text)), false);
        case ValueKind::undef:
            return llvm::UndefValue::get(this->llvm_type(value.type));
        case ValueKind::null_literal:
            return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(this->llvm_type(value.type)));
        case ValueKind::string_literal:
            return this->emit_string_literal(this->text(value.text), false);
        case ValueKind::raw_string_literal:
            return this->emit_raw_string_literal(this->text(value.text));
        case ValueKind::c_string_literal:
            return this->emit_string_literal(this->text(value.text), true);
        case ValueKind::constant_ref:
            return this->emit_constant_ref(value);
        case ValueKind::function_ref:
            return this->emit_function_ref(value);
        case ValueKind::alloca:
            return this->builder_.CreateAlloca(this->pointee_llvm_type(value.type), nullptr, this->text(value.name));
        case ValueKind::load:
            return this->builder_.CreateLoad(
                this->llvm_type(value.type), this->get(value.object), this->text(value.name));
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
                result, this->get(value.lhs), {LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX});
            result = this->builder_.CreateInsertValue(
                result, this->get(value.rhs), {LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX});
            return result;
        }
        case ValueKind::slice_data:
            return this->builder_.CreateExtractValue(this->get(value.object),
                {LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_SLICE_DATA_NAME);
        case ValueKind::slice_len:
            return this->builder_.CreateExtractValue(this->get(value.object),
                {LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_SLICE_LENGTH_NAME);
        case ValueKind::cast:
            return this->emit_cast(value);
        case ValueKind::size_of:
            return this->emit_size_of(value.target_type);
        case ValueKind::align_of:
            return this->emit_align_of(value.target_type);
        case ValueKind::str_data:
            return this->builder_.CreateExtractValue(this->get(value.object),
                {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_STRING_DATA_NAME);
        case ValueKind::str_byte_len:
            return this->builder_.CreateExtractValue(this->get(value.object),
                {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_STRING_LENGTH_NAME);
        case ValueKind::str_is_valid_utf8:
            return this->emit_str_is_valid_utf8(value);
        case ValueKind::str_from_utf8_checked:
            return this->emit_str_from_utf8_checked(value);
        case ValueKind::str_slice_checked:
            return this->emit_str_slice_checked(value);
        case ValueKind::str_from_bytes_unchecked: {
            llvm::Value* result = llvm::UndefValue::get(this->llvm_type(value.type));
            result = this->builder_.CreateInsertValue(result,
                this->get(value.args[LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX]),
                {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX});
            result = this->builder_.CreateInsertValue(result,
                this->get(value.args[LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX]),
                {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX});
            return result;
        }
        default:
            break;
    }
    return llvm::UndefValue::get(this->llvm_type(value.type));
}

llvm::Value* LlvmEmitter::emit_constant_ref(const Value& value)
{
    const GlobalConstant* constant = find_global_constant(this->source_, value.constant);
    const auto found = this->constants_.find(value.constant.value);
    return this->builder_.CreateLoad(this->llvm_type(constant->type), found->second,
        this->suffixed_name(constant->symbol, LLVM_BACKEND_VALUE_CONSTANT_VALUE_SUFFIX));
}

llvm::Value* LlvmEmitter::emit_function_ref(const Value& value) const
{
    return this->functions_.at(value.call_target.value);
}

llvm::Constant* LlvmEmitter::emit_constant_initializer(const Value& value)
{
    switch (value.kind) {
        case ValueKind::integer_literal:
            return llvm::cast<llvm::Constant>(this->integer_constant(value.type, this->text(value.text)));
        case ValueKind::float_literal:
            return llvm::cast<llvm::Constant>(this->float_constant(value.type, this->text(value.text)));
        case ValueKind::bool_literal:
            return llvm::ConstantInt::get(this->llvm_type(value.type),
                this->text(value.text) == LLVM_BACKEND_VALUE_BOOL_TRUE_TEXT ? LLVM_BACKEND_VALUE_BOOL_TRUE_INTEGER
                                                                            : LLVM_BACKEND_VALUE_ZERO_INTEGER,
                false);
        case ValueKind::char_literal:
            return llvm::ConstantInt::get(
                this->llvm_type(value.type), parse_char_literal(this->text(value.text)), false);
        case ValueKind::byte_literal:
            return llvm::ConstantInt::get(
                this->llvm_type(value.type), parse_byte_literal(this->text(value.text)), false);
        case ValueKind::undef:
            return llvm::UndefValue::get(this->llvm_type(value.type));
        case ValueKind::null_literal:
            return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(this->llvm_type(value.type)));
        case ValueKind::string_literal:
            return this->emit_constant_string(this->text(value.text), false);
        case ValueKind::raw_string_literal:
            return this->emit_constant_raw_string(this->text(value.text));
        case ValueKind::c_string_literal:
            return this->emit_constant_string(this->text(value.text), true);
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

llvm::Constant* LlvmEmitter::emit_constant_binary(const Value& value)
{
    llvm::Constant* lhs = this->emit_constant_initializer(this->source_.values[value.lhs.value]);
    llvm::Constant* rhs = this->emit_constant_initializer(this->source_.values[value.rhs.value]);
    const sema::TypeHandle operand_type = this->source_.values[value.lhs.value].type;
    const bool is_float = this->source_.types.is_float(operand_type);
    const bool is_unsigned = this->is_unsigned_integer(operand_type);

    const auto fold_binary = [&](const unsigned opcode) -> llvm::Constant* {
        if (llvm::Constant* folded = llvm::ConstantFoldBinaryInstruction(opcode, lhs, rhs); folded != nullptr) {
            return folded;
        }
        return llvm::UndefValue::get(this->llvm_type(value.type));
    };
    const auto fold_compare = [&](const llvm::CmpInst::Predicate predicate) -> llvm::Constant* {
        if (llvm::Constant* folded = llvm::ConstantFoldCompareInstruction(predicate, lhs, rhs); folded != nullptr) {
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
            return fold_binary(
                is_float ? llvm::Instruction::FDiv : (is_unsigned ? llvm::Instruction::UDiv : llvm::Instruction::SDiv));
        case BinaryOp::mod:
            return fold_binary(is_unsigned ? llvm::Instruction::URem : llvm::Instruction::SRem);
        case BinaryOp::shl:
            return fold_binary(llvm::Instruction::Shl);
        case BinaryOp::shr:
            return fold_binary(is_unsigned ? llvm::Instruction::LShr : llvm::Instruction::AShr);
        case BinaryOp::less:
            return fold_compare(
                is_float ? llvm::CmpInst::FCMP_OLT : (is_unsigned ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT));
        case BinaryOp::less_equal:
            return fold_compare(
                is_float ? llvm::CmpInst::FCMP_OLE : (is_unsigned ? llvm::CmpInst::ICMP_ULE : llvm::CmpInst::ICMP_SLE));
        case BinaryOp::greater:
            return fold_compare(
                is_float ? llvm::CmpInst::FCMP_OGT : (is_unsigned ? llvm::CmpInst::ICMP_UGT : llvm::CmpInst::ICMP_SGT));
        case BinaryOp::greater_equal:
            return fold_compare(
                is_float ? llvm::CmpInst::FCMP_OGE : (is_unsigned ? llvm::CmpInst::ICMP_UGE : llvm::CmpInst::ICMP_SGE));
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
    llvm_unreachable(LLVM_BACKEND_VALUE_UNHANDLED_BINARY_OP);
}

llvm::Constant* LlvmEmitter::emit_constant_unary(const Value& value)
{
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

llvm::Constant* LlvmEmitter::emit_constant_cast(const Value& value)
{
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
                    if (llvm::Constant* folded = llvm::ConstantFoldCompareInstruction(llvm::CmpInst::ICMP_NE, operand,
                            llvm::ConstantInt::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER));
                        folded != nullptr) {
                        return folded;
                    }
                    return llvm::UndefValue::get(target);
                }
                if (operand->getType()->isFloatingPointTy()) {
                    if (llvm::Constant* folded = llvm::ConstantFoldCompareInstruction(llvm::CmpInst::FCMP_UNE, operand,
                            llvm::ConstantFP::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_FLOAT));
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
                opcode = this->is_unsigned_integer(value.target_type) ? llvm::Instruction::FPToUI
                                                                      : llvm::Instruction::FPToSI;
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

llvm::Constant* LlvmEmitter::emit_constant_aggregate(const Value& value)
{
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

llvm::Constant* LlvmEmitter::emit_constant_string(const std::string_view literal, const bool c_string)
{
    if (c_string) {
        std::string decoded = decode_string_literal(literal, true);
        decoded.push_back(LLVM_BACKEND_VALUE_NULL_TERMINATOR);
        return llvm::cast<llvm::Constant>(
            this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_CONSTANT_C_STRING_NAME, false));
    }
    return llvm::cast<llvm::Constant>(this->emit_string_literal(literal, false));
}

llvm::Constant* LlvmEmitter::emit_constant_raw_string(const std::string_view literal)
{
    return llvm::cast<llvm::Constant>(this->emit_raw_string_literal(literal));
}

llvm::Value* LlvmEmitter::emit_unary(const Value& value)
{
    llvm::Value* operand = this->get(value.lhs);
    switch (value.unary_op) {
        case UnaryOp::logical_not:
            return this->builder_.CreateNot(operand);
        case UnaryOp::numeric_negate:
            return this->source_.types.is_float(value.type) ? this->builder_.CreateFNeg(operand)
                                                            : this->builder_.CreateNeg(operand);
        case UnaryOp::bitwise_not:
            return this->builder_.CreateNot(operand);
        case UnaryOp::address_of:
        case UnaryOp::dereference:
            return operand;
    }
    llvm_unreachable(LLVM_BACKEND_VALUE_UNHANDLED_UNARY_OP);
}

llvm::Value* LlvmEmitter::emit_binary(const Value& value)
{
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
            return is_float ? this->builder_.CreateFDiv(lhs, rhs)
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
    llvm_unreachable(LLVM_BACKEND_VALUE_UNHANDLED_BINARY_OP);
}

llvm::Value* LlvmEmitter::emit_call(const Value& value)
{
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
        const std::string result_name = this->text(value.name).empty()
            ? std::string{}
            : this->suffixed_name(value.name, LLVM_BACKEND_VALUE_CALL_RESULT_SUFFIX);
        return this->builder_.CreateCall(type, callee, args, result_name);
    }

    llvm::Function* target = this->functions_.at(value.call_target.value);
    if (this->source_.types.is_void(value.type)) {
        return this->builder_.CreateCall(target, args);
    }
    const std::string result_name = this->text(value.name).empty()
        ? std::string{}
        : this->suffixed_name(value.name, LLVM_BACKEND_VALUE_CALL_RESULT_SUFFIX);
    return this->builder_.CreateCall(target, args, result_name);
}

llvm::Value* LlvmEmitter::emit_field_addr(const Value& value)
{
    const sema::TypeHandle object_pointee = this->pointee_type(value.object);
    const sema::TypeHandle record_type =
        (this->source_.types.is_pointer(object_pointee) || this->source_.types.is_reference(object_pointee))
        ? this->source_.types.get(object_pointee).pointee
        : object_pointee;
    const RecordLayout* record = find_record(this->source_, record_type);
    const base::usize index = record_field_index(*record, value.name);
    return this->builder_.CreateStructGEP(this->llvm_type(record_type), this->get(value.object),
        static_cast<unsigned>(index), this->suffixed_name(value.name, LLVM_BACKEND_VALUE_FIELD_ADDRESS_SUFFIX));
}

llvm::Value* LlvmEmitter::emit_index_addr(const Value& value)
{
    const sema::TypeHandle object_pointee = this->pointee_type(value.object);
    llvm::Value* object = this->get(value.object);
    llvm::Value* index = this->get(value.index);
    if (this->source_.types.is_array(object_pointee)) {
        llvm::Value* zero =
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(this->context_), LLVM_BACKEND_VALUE_ZERO_INTEGER);
        return this->builder_.CreateGEP(
            this->llvm_type(object_pointee), object, {zero, index}, LLVM_BACKEND_VALUE_INDEX_ADDRESS_NAME);
    }
    return this->builder_.CreateGEP(
        this->llvm_type(object_pointee), object, index, LLVM_BACKEND_VALUE_INDEX_ADDRESS_NAME);
}

llvm::Value* LlvmEmitter::emit_aggregate(const Value& value)
{
    llvm::Value* aggregate = llvm::UndefValue::get(this->llvm_type(value.type));
    if (this->source_.types.is_array(value.type)) {
        for (base::usize index = 0; index < value.elements.size(); ++index) {
            aggregate = this->builder_.CreateInsertValue(
                aggregate, this->get(value.elements[index]), {static_cast<unsigned>(index)});
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

llvm::Value* LlvmEmitter::emit_str_is_valid_utf8(const Value& value)
{
    llvm::Value* slice = this->get(value.object);
    llvm::Value* data = this->builder_.CreateExtractValue(
        slice, {LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_SLICE_DATA_NAME);
    llvm::Value* length = this->builder_.CreateExtractValue(
        slice, {LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_SLICE_LENGTH_NAME);
    return this->emit_utf8_validation_call(data, length);
}

llvm::Value* LlvmEmitter::emit_str_from_utf8_checked(const Value& value)
{
    llvm::Value* slice = this->get(value.object);
    llvm::Value* data = this->builder_.CreateExtractValue(
        slice, {LLVM_BACKEND_VALUE_SLICE_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_SLICE_DATA_NAME);
    llvm::Value* length = this->builder_.CreateExtractValue(
        slice, {LLVM_BACKEND_VALUE_SLICE_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_SLICE_LENGTH_NAME);
    llvm::Value* valid = this->emit_utf8_validation_call(data, length);

    llvm::Value* empty_data = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(data->getType()));
    llvm::Value* empty_length = llvm::ConstantInt::get(length->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER);
    llvm::Value* checked_data = this->builder_.CreateSelect(valid, data, empty_data);
    llvm::Value* checked_length = this->builder_.CreateSelect(valid, length, empty_length);

    llvm::Value* text = llvm::UndefValue::get(this->llvm_type(value.type));
    text = this->builder_.CreateInsertValue(
        text, checked_data, {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_UTF8_STR_NAME);
    text = this->builder_.CreateInsertValue(
        text, checked_length, {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_UTF8_STR_NAME);
    return text;
}

llvm::Value* LlvmEmitter::emit_str_slice_checked(const Value& value)
{
    llvm::Value* source = this->get(value.object);
    llvm::Value* data = this->builder_.CreateExtractValue(
        source, {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_STRING_DATA_NAME);
    llvm::Value* length = this->builder_.CreateExtractValue(
        source, {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_STRING_LENGTH_NAME);
    llvm::Value* start = this->get(value.lhs);
    llvm::Value* end = this->get(value.rhs);

    llvm::Type* byte_type = llvm::Type::getInt8Ty(this->context_);
    llvm::Value* zero = llvm::ConstantInt::get(length->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER);
    llvm::Value* empty_source = this->builder_.CreateICmpEQ(length, zero);
    llvm::Value* null_data = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(data->getType()));
    llvm::Value* data_present = this->builder_.CreateICmpNE(data, null_data);
    llvm::Value* source_storage_ok = this->builder_.CreateOr(empty_source, data_present);
    llvm::Value* ordered_bounds = this->builder_.CreateICmpULE(start, end);
    llvm::Value* end_in_bounds = this->builder_.CreateICmpULE(end, length);
    llvm::Value* start_boundary = this->emit_utf8_boundary_check(data, length, start,
        std::string(LLVM_BACKEND_VALUE_STR_SLICE_NAME) + LLVM_BACKEND_VALUE_STR_SLICE_START_BOUNDARY_SUFFIX);
    llvm::Value* end_boundary = this->emit_utf8_boundary_check(data, length, end,
        std::string(LLVM_BACKEND_VALUE_STR_SLICE_NAME) + LLVM_BACKEND_VALUE_STR_SLICE_END_BOUNDARY_SUFFIX);

    llvm::Value* success = this->builder_.CreateAnd(source_storage_ok, ordered_bounds);
    success = this->builder_.CreateAnd(success, end_in_bounds);
    success = this->builder_.CreateAnd(success, start_boundary);
    success = this->builder_.CreateAnd(success, end_boundary, LLVM_BACKEND_VALUE_STR_SLICE_SUCCESS_NAME);

    llvm::Value* slice_data = this->builder_.CreateGEP(byte_type, data, start, LLVM_BACKEND_VALUE_STR_SLICE_DATA_NAME);
    llvm::Value* slice_length = this->builder_.CreateSub(end, start, LLVM_BACKEND_VALUE_STR_SLICE_LENGTH_NAME);
    llvm::Value* empty_length = llvm::ConstantInt::get(length->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER);
    llvm::Value* checked_data = this->builder_.CreateSelect(success, slice_data, null_data);
    llvm::Value* checked_length = this->builder_.CreateSelect(success, slice_length, empty_length);

    llvm::Value* result = llvm::UndefValue::get(this->llvm_type(value.type));
    result = this->builder_.CreateInsertValue(
        result, checked_data, {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX}, LLVM_BACKEND_VALUE_STR_SLICE_NAME);
    result = this->builder_.CreateInsertValue(
        result, checked_length, {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX}, LLVM_BACKEND_VALUE_STR_SLICE_NAME);
    return result;
}

llvm::Value* LlvmEmitter::emit_utf8_validation_call(llvm::Value* data, llvm::Value* length)
{
    return this->builder_.CreateCall(
        this->utf8_validator_function(), {data, length}, LLVM_BACKEND_VALUE_UTF8_VALID_NAME);
}

llvm::Value* LlvmEmitter::emit_utf8_boundary_check(
    llvm::Value* data, llvm::Value* length, llvm::Value* index, const std::string& name)
{
    return this->builder_.CreateCall(this->utf8_boundary_function(), {data, length, index}, name);
}

llvm::Function* LlvmEmitter::utf8_boundary_function()
{
    if (llvm::Function* existing = this->module_->getFunction(LLVM_BACKEND_VALUE_UTF8_BOUNDARY_HELPER_NAME);
        existing != nullptr) {
        return existing;
    }

    llvm::Type* byte_type = llvm::Type::getInt8Ty(this->context_);
    llvm::Type* bool_type = llvm::Type::getInt1Ty(this->context_);
    llvm::Type* size_type = this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize));
    llvm::PointerType* byte_pointer_type =
        llvm::PointerType::get(this->context_, LLVM_BACKEND_VALUE_GLOBAL_STRING_ADDRESS_SPACE);
    llvm::FunctionType* function_type =
        llvm::FunctionType::get(bool_type, {byte_pointer_type, size_type, size_type}, false);
    llvm::Function* function = llvm::Function::Create(function_type, llvm::GlobalValue::InternalLinkage,
        LLVM_BACKEND_VALUE_UTF8_BOUNDARY_HELPER_NAME, this->module_.get());

    auto argument = function->arg_begin();
    llvm::Value* data = &*argument++;
    llvm::Value* length = &*argument++;
    llvm::Value* index = &*argument;

    const llvm::IRBuilderBase::InsertPoint saved_insert_point = this->builder_.saveIP();
    llvm::BasicBlock* entry_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_ENTRY_BLOCK, function);
    llvm::BasicBlock* edge_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_BOUNDARY_EDGE_BLOCK, function);
    llvm::BasicBlock* probe_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_BOUNDARY_PROBE_BLOCK, function);
    llvm::BasicBlock* load_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_BOUNDARY_LOAD_BLOCK, function);
    llvm::BasicBlock* valid_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_VALID_BLOCK, function);
    llvm::BasicBlock* invalid_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_INVALID_BLOCK, function);

    const auto size_constant = [&](const std::uint64_t value) {
        return llvm::ConstantInt::get(size_type, value);
    };
    const auto byte_constant = [&](const std::uint64_t value) {
        return llvm::ConstantInt::get(byte_type, value);
    };

    this->builder_.SetInsertPoint(entry_block);
    llvm::Value* zero = size_constant(LLVM_BACKEND_VALUE_ZERO_INTEGER);
    llvm::Value* at_start = this->builder_.CreateICmpEQ(index, zero);
    this->builder_.CreateCondBr(at_start, valid_block, edge_block);

    this->builder_.SetInsertPoint(edge_block);
    llvm::Value* at_end = this->builder_.CreateICmpEQ(index, length);
    this->builder_.CreateCondBr(at_end, valid_block, probe_block);

    this->builder_.SetInsertPoint(probe_block);
    llvm::Value* in_bounds = this->builder_.CreateICmpULT(index, length);
    llvm::Value* null_data = llvm::ConstantPointerNull::get(byte_pointer_type);
    llvm::Value* data_present = this->builder_.CreateICmpNE(data, null_data);
    llvm::Value* can_load = this->builder_.CreateAnd(in_bounds, data_present);
    this->builder_.CreateCondBr(can_load, load_block, invalid_block);

    this->builder_.SetInsertPoint(load_block);
    llvm::Value* pointer = this->builder_.CreateGEP(byte_type, data, index);
    llvm::Value* byte = this->builder_.CreateLoad(byte_type, pointer, LLVM_BACKEND_VALUE_UTF8_BYTE_NAME);
    llvm::Value* above_cont_min = this->builder_.CreateICmpUGE(byte, byte_constant(LLVM_BACKEND_VALUE_UTF8_CONT_MIN));
    llvm::Value* below_cont_max = this->builder_.CreateICmpULE(byte, byte_constant(LLVM_BACKEND_VALUE_UTF8_CONT_MAX));
    llvm::Value* continuation = this->builder_.CreateAnd(above_cont_min, below_cont_max);
    llvm::Value* boundary = this->builder_.CreateNot(continuation);
    this->builder_.CreateCondBr(boundary, valid_block, invalid_block);

    this->builder_.SetInsertPoint(valid_block);
    this->builder_.CreateRet(llvm::ConstantInt::get(bool_type, true));

    this->builder_.SetInsertPoint(invalid_block);
    this->builder_.CreateRet(llvm::ConstantInt::get(bool_type, false));

    this->builder_.restoreIP(saved_insert_point);
    return function;
}

llvm::Function* LlvmEmitter::utf8_validator_function()
{
    if (llvm::Function* existing = this->module_->getFunction(LLVM_BACKEND_VALUE_UTF8_HELPER_NAME);
        existing != nullptr) {
        return existing;
    }

    llvm::Type* byte_type = llvm::Type::getInt8Ty(this->context_);
    llvm::Type* bool_type = llvm::Type::getInt1Ty(this->context_);
    llvm::Type* size_type = this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize));
    llvm::PointerType* byte_pointer_type =
        llvm::PointerType::get(this->context_, LLVM_BACKEND_VALUE_GLOBAL_STRING_ADDRESS_SPACE);
    llvm::FunctionType* function_type = llvm::FunctionType::get(bool_type, {byte_pointer_type, size_type}, false);
    llvm::Function* function = llvm::Function::Create(
        function_type, llvm::GlobalValue::InternalLinkage, LLVM_BACKEND_VALUE_UTF8_HELPER_NAME, this->module_.get());

    auto argument = function->arg_begin();
    llvm::Value* data = &*argument++;
    llvm::Value* length = &*argument;

    const llvm::IRBuilderBase::InsertPoint saved_insert_point = this->builder_.saveIP();
    llvm::BasicBlock* entry_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_ENTRY_BLOCK, function);
    llvm::BasicBlock* null_check_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_NULL_CHECK_BLOCK, function);
    llvm::BasicBlock* loop_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_LOOP_BLOCK, function);
    llvm::BasicBlock* scan_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_SCAN_BLOCK, function);
    llvm::BasicBlock* valid_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_VALID_BLOCK, function);
    llvm::BasicBlock* invalid_block =
        llvm::BasicBlock::Create(this->context_, LLVM_BACKEND_VALUE_UTF8_INVALID_BLOCK, function);

    const auto size_constant = [&](const std::uint64_t value) {
        return llvm::ConstantInt::get(size_type, value);
    };
    const auto byte_constant = [&](const std::uint64_t value) {
        return llvm::ConstantInt::get(byte_type, value);
    };
    const auto byte_in_range = [&](llvm::Value* byte, const Utf8ByteRange range) {
        llvm::Value* above_min = this->builder_.CreateICmpUGE(byte, byte_constant(range.min));
        llvm::Value* below_max = this->builder_.CreateICmpULE(byte, byte_constant(range.max));
        return this->builder_.CreateAnd(above_min, below_max);
    };
    const auto byte_at = [&](llvm::Value* index) {
        llvm::Value* pointer = this->builder_.CreateInBoundsGEP(byte_type, data, index);
        return this->builder_.CreateLoad(byte_type, pointer, LLVM_BACKEND_VALUE_UTF8_BYTE_NAME);
    };

    this->builder_.SetInsertPoint(entry_block);
    llvm::Value* zero = size_constant(LLVM_BACKEND_VALUE_ZERO_INTEGER);
    llvm::Value* empty = this->builder_.CreateICmpEQ(length, zero);
    this->builder_.CreateCondBr(empty, valid_block, null_check_block);

    this->builder_.SetInsertPoint(null_check_block);
    llvm::Value* null_data = llvm::ConstantPointerNull::get(byte_pointer_type);
    llvm::Value* data_is_null = this->builder_.CreateICmpEQ(data, null_data);
    this->builder_.CreateCondBr(data_is_null, invalid_block, loop_block);

    this->builder_.SetInsertPoint(loop_block);
    llvm::PHINode* index = this->builder_.CreatePHI(size_type, 0, LLVM_BACKEND_VALUE_UTF8_INDEX_NAME);
    index->addIncoming(zero, null_check_block);
    llvm::Value* done = this->builder_.CreateICmpEQ(index, length);
    this->builder_.CreateCondBr(done, valid_block, scan_block);

    this->builder_.SetInsertPoint(scan_block);
    llvm::Value* first = byte_at(index);
    first->setName(LLVM_BACKEND_VALUE_UTF8_FIRST_NAME);

    const auto append_case = [&](const std::string& name, llvm::Value* first_matches, const std::uint64_t width,
                                 const std::initializer_list<Utf8ByteRange> continuation_ranges) {
        llvm::BasicBlock* match_block =
            llvm::BasicBlock::Create(this->context_, name + LLVM_BACKEND_VALUE_UTF8_CASE_MATCH_SUFFIX, function);
        llvm::BasicBlock* bytes_block =
            llvm::BasicBlock::Create(this->context_, name + LLVM_BACKEND_VALUE_UTF8_CASE_BYTES_SUFFIX, function);
        llvm::BasicBlock* success_block =
            llvm::BasicBlock::Create(this->context_, name + LLVM_BACKEND_VALUE_UTF8_CASE_SUCCESS_SUFFIX, function);
        llvm::BasicBlock* miss_block =
            llvm::BasicBlock::Create(this->context_, name + LLVM_BACKEND_VALUE_UTF8_CASE_MISS_SUFFIX, function);

        this->builder_.CreateCondBr(first_matches, match_block, miss_block);

        this->builder_.SetInsertPoint(match_block);
        llvm::Value* remaining = this->builder_.CreateSub(length, index);
        llvm::Value* has_width = this->builder_.CreateICmpUGE(remaining, size_constant(width));
        this->builder_.CreateCondBr(has_width, bytes_block, invalid_block);

        this->builder_.SetInsertPoint(bytes_block);
        llvm::Value* bytes_ok = llvm::ConstantInt::get(bool_type, true);
        std::uint64_t offset = LLVM_BACKEND_VALUE_UTF8_WIDTH_ONE;
        for (const Utf8ByteRange range : continuation_ranges) {
            llvm::Value* byte_index = this->builder_.CreateAdd(index, size_constant(offset));
            llvm::Value* byte = byte_at(byte_index);
            bytes_ok = this->builder_.CreateAnd(bytes_ok, byte_in_range(byte, range));
            ++offset;
        }
        this->builder_.CreateCondBr(bytes_ok, success_block, invalid_block);

        this->builder_.SetInsertPoint(success_block);
        llvm::Value* next_index = this->builder_.CreateAdd(index, size_constant(width));
        index->addIncoming(next_index, success_block);
        this->builder_.CreateBr(loop_block);

        this->builder_.SetInsertPoint(miss_block);
    };

    append_case("utf8.ascii", this->builder_.CreateICmpULE(first, byte_constant(LLVM_BACKEND_VALUE_UTF8_ASCII_MAX)),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_ONE, {});
    append_case("utf8.two", byte_in_range(first, {LLVM_BACKEND_VALUE_UTF8_TWO_MIN, LLVM_BACKEND_VALUE_UTF8_TWO_MAX}),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_TWO, {{LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX}});
    append_case("utf8.three.e0", this->builder_.CreateICmpEQ(first, byte_constant(LLVM_BACKEND_VALUE_UTF8_THREE_E0)),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_THREE,
        {
            {LLVM_BACKEND_VALUE_UTF8_THREE_E0_SECOND_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    append_case("utf8.three.mid",
        byte_in_range(first, {LLVM_BACKEND_VALUE_UTF8_THREE_E1_MIN, LLVM_BACKEND_VALUE_UTF8_THREE_EC_MAX}),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_THREE,
        {
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    append_case("utf8.three.ed", this->builder_.CreateICmpEQ(first, byte_constant(LLVM_BACKEND_VALUE_UTF8_THREE_ED)),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_THREE,
        {
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_THREE_ED_SECOND_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    append_case("utf8.three.high",
        byte_in_range(first, {LLVM_BACKEND_VALUE_UTF8_THREE_EE_MIN, LLVM_BACKEND_VALUE_UTF8_THREE_EF_MAX}),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_THREE,
        {
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    append_case("utf8.four.f0", this->builder_.CreateICmpEQ(first, byte_constant(LLVM_BACKEND_VALUE_UTF8_FOUR_F0)),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_FOUR,
        {
            {LLVM_BACKEND_VALUE_UTF8_FOUR_F0_SECOND_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    append_case("utf8.four.mid",
        byte_in_range(first, {LLVM_BACKEND_VALUE_UTF8_FOUR_F1_MIN, LLVM_BACKEND_VALUE_UTF8_FOUR_F3_MAX}),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_FOUR,
        {
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    append_case("utf8.four.f4", this->builder_.CreateICmpEQ(first, byte_constant(LLVM_BACKEND_VALUE_UTF8_FOUR_F4)),
        LLVM_BACKEND_VALUE_UTF8_WIDTH_FOUR,
        {
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_FOUR_F4_SECOND_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
            {LLVM_BACKEND_VALUE_UTF8_CONT_MIN, LLVM_BACKEND_VALUE_UTF8_CONT_MAX},
        });
    this->builder_.CreateBr(invalid_block);

    this->builder_.SetInsertPoint(valid_block);
    this->builder_.CreateRet(llvm::ConstantInt::get(bool_type, true));

    this->builder_.SetInsertPoint(invalid_block);
    this->builder_.CreateRet(llvm::ConstantInt::get(bool_type, false));

    this->builder_.restoreIP(saved_insert_point);
    return function;
}

llvm::Value* LlvmEmitter::emit_cast(const Value& value)
{
    llvm::Value* operand = this->get(value.lhs);
    llvm::Type* target = this->llvm_type(value.target_type);
    const bool source_unsigned = this->is_unsigned_integer(this->source_.values[value.lhs.value].type);
    switch (value.cast_kind) {
        case CastKind::numeric:
            if (this->source_.types.is_bool(value.target_type)) {
                if (operand->getType()->isIntegerTy()) {
                    return this->builder_.CreateICmpNE(
                        operand, llvm::ConstantInt::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_INTEGER));
                }
                if (operand->getType()->isFloatingPointTy()) {
                    return this->builder_.CreateFCmpUNE(
                        operand, llvm::ConstantFP::get(operand->getType(), LLVM_BACKEND_VALUE_ZERO_FLOAT));
                }
            }
            if (operand->getType()->isIntegerTy() && target->isIntegerTy()) {
                return this->builder_.CreateIntCast(operand, target, !source_unsigned);
            }
            if (operand->getType()->isIntegerTy() && target->isFloatingPointTy()) {
                return source_unsigned ? this->builder_.CreateUIToFP(operand, target)
                                       : this->builder_.CreateSIToFP(operand, target);
            }
            if (operand->getType()->isFloatingPointTy() && target->isIntegerTy()) {
                return this->is_unsigned_integer(value.target_type) ? this->builder_.CreateFPToUI(operand, target)
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
    llvm_unreachable(LLVM_BACKEND_VALUE_UNHANDLED_CAST_KIND);
}

llvm::Value* LlvmEmitter::emit_size_of(const sema::TypeHandle type)
{
    const llvm::TypeSize size = this->data_layout().getTypeAllocSize(this->llvm_type(type));
    return llvm::ConstantInt::get(
        this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)), size.getFixedValue());
}

llvm::Value* LlvmEmitter::emit_align_of(const sema::TypeHandle type)
{
    const llvm::Align align = this->data_layout().getABITypeAlign(this->llvm_type(type));
    return llvm::ConstantInt::get(
        this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)), align.value());
}

llvm::Value* LlvmEmitter::integer_constant(const sema::TypeHandle type, const std::string_view text)
{
    std::uint64_t value = LLVM_BACKEND_VALUE_ZERO_INTEGER;
    static_cast<void>(parse_u64(text, value));
    return llvm::ConstantInt::get(this->llvm_type(type), value, !this->is_unsigned_integer(type));
}

llvm::Value* LlvmEmitter::float_constant(const sema::TypeHandle type, const std::string_view text)
{
    double value = LLVM_BACKEND_VALUE_ZERO_FLOAT;
    static_cast<void>(parse_f64(text, value));
    return llvm::ConstantFP::get(this->llvm_type(type), value);
}

llvm::Value* LlvmEmitter::emit_string_literal(const std::string_view literal, const bool c_string)
{
    std::string decoded = decode_string_literal(literal, c_string);
    if (c_string) {
        decoded.push_back(LLVM_BACKEND_VALUE_NULL_TERMINATOR);
        return this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_C_STRING_NAME, false);
    }

    llvm::Value* data = this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_STRING_DATA_NAME, false);
    const sema::TypeHandle str_type = this->source_.types.builtin(sema::BuiltinType::str);
    llvm::Value* result = llvm::UndefValue::get(this->llvm_type(str_type));
    result = this->builder_.CreateInsertValue(result, data, {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX});
    result = this->builder_.CreateInsertValue(result,
        llvm::ConstantInt::get(this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)), decoded.size()),
        {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX});
    return result;
}

llvm::Value* LlvmEmitter::emit_raw_string_literal(const std::string_view literal)
{
    const std::string decoded = decode_raw_string_literal(literal);
    llvm::Value* data = this->global_string_pointer(decoded, LLVM_BACKEND_VALUE_STRING_DATA_NAME, false);
    const sema::TypeHandle str_type = this->source_.types.builtin(sema::BuiltinType::str);
    llvm::Value* result = llvm::UndefValue::get(this->llvm_type(str_type));
    result = this->builder_.CreateInsertValue(result, data, {LLVM_BACKEND_VALUE_STRING_DATA_FIELD_INDEX});
    result = this->builder_.CreateInsertValue(result,
        llvm::ConstantInt::get(this->llvm_type(this->source_.types.builtin(sema::BuiltinType::usize)), decoded.size()),
        {LLVM_BACKEND_VALUE_STRING_LENGTH_FIELD_INDEX});
    return result;
}

llvm::Value* LlvmEmitter::global_string_pointer(const std::string& text, const std::string& name, const bool add_null)
{
    llvm::GlobalVariable* global = this->builder_.CreateGlobalString(
        text, name, LLVM_BACKEND_VALUE_GLOBAL_STRING_ADDRESS_SPACE, this->module_.get(), add_null);
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(this->context_), LLVM_BACKEND_VALUE_ZERO_INTEGER);
    return this->builder_.CreateInBoundsGEP(
        global->getValueType(), global, {zero, zero}, name + LLVM_BACKEND_VALUE_GLOBAL_POINTER_SUFFIX);
}

} // namespace aurex::backend

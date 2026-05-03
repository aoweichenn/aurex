#include "aurex/ir/llvm_emit.hpp"

#include "aurex/ir/verify.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <charconv>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace aurex::ir {

namespace {

[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& out) noexcept {
    int base = 10;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        begin += 2;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
        begin += 2;
    }
    const auto result = std::from_chars(begin, end, out, base);
    return result.ec == std::errc {};
}

[[nodiscard]] std::string decode_string_literal(const std::string& literal, const bool has_c_prefix) {
    std::string text = has_c_prefix && !literal.empty() && literal.front() == 'c'
        ? literal.substr(1)
        : literal;
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }

    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            decoded.push_back(text[i]);
            continue;
        }
        const char escaped = text[++i];
        switch (escaped) {
        case '0': decoded.push_back('\0'); break;
        case 'n': decoded.push_back('\n'); break;
        case 'r': decoded.push_back('\r'); break;
        case 't': decoded.push_back('\t'); break;
        case '\\': decoded.push_back('\\'); break;
        case '"': decoded.push_back('"'); break;
        default: decoded.push_back(escaped); break;
        }
    }
    return decoded;
}

[[nodiscard]] std::uint64_t parse_byte_literal(const std::string& literal) {
    std::string text = literal;
    if (!text.empty() && text.front() == 'b') {
        text.erase(text.begin());
    }
    if (text.size() >= 2 && text.front() == '\'' && text.back() == '\'') {
        text = text.substr(1, text.size() - 2);
    }
    if (text.size() >= 2 && text.front() == '\\') {
        switch (text[1]) {
        case '0': return 0;
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        case '\'': return '\'';
        default: return static_cast<unsigned char>(text[1]);
        }
    }
    return text.empty() ? 0 : static_cast<unsigned char>(text.front());
}

class LlvmEmitter final {
public:
    LlvmEmitter(const Module& module, std::string module_name)
        : source_(module),
          context_(),
          module_(std::make_unique<llvm::Module>(std::move(module_name), context_)),
          builder_(context_) {}

    [[nodiscard]] base::Result<LlvmIrOutput> run() {
        if (auto verified = verify_module(source_); !verified) {
            return base::Result<LlvmIrOutput>::fail(verified.error());
        }

        module_->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
        declare_records();
        declare_functions();
        for (base::u32 i = 0; i < source_.functions.size(); ++i) {
            const Function& function = source_.functions[i];
            if (function.linkage != Linkage::extern_c) {
                emit_function(FunctionId {i}, function);
            }
        }

        std::string verifier_error;
        llvm::raw_string_ostream verifier_stream(verifier_error);
        if (llvm::verifyModule(*module_, &verifier_stream)) {
            verifier_stream.flush();
            return base::Result<LlvmIrOutput>::fail({base::ErrorCode::codegen_error, verifier_error});
        }

        std::string text;
        llvm::raw_string_ostream out(text);
        module_->print(out, nullptr);
        out.flush();
        return base::Result<LlvmIrOutput>::ok(LlvmIrOutput {std::move(text)});
    }

private:
    void declare_records() {
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

    void declare_functions() {
        for (base::u32 i = 0; i < source_.functions.size(); ++i) {
            const Function& function = source_.functions[i];
            std::vector<llvm::Type*> params;
            params.reserve(function.signature_params.size());
            for (const FunctionParam& param : function.signature_params) {
                params.push_back(llvm_type(param.type));
            }
            llvm::FunctionType* function_type = llvm::FunctionType::get(llvm_type(function.return_type), params, false);
            llvm::Function* llvm_function = llvm::Function::Create(
                function_type,
                function.linkage == Linkage::internal ? llvm::GlobalValue::InternalLinkage : llvm::GlobalValue::ExternalLinkage,
                function.symbol,
                module_.get()
            );
            functions_[i] = llvm_function;
        }
        declare_main_wrapper();
    }

    void declare_main_wrapper() {
        for (base::u32 i = 0; i < source_.functions.size(); ++i) {
            const Function& function = source_.functions[i];
            if (function.linkage != Linkage::export_c || function.name != "main") {
                continue;
            }
            llvm::FunctionType* main_type = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context_),
                {llvm::Type::getInt32Ty(context_), llvm::PointerType::get(context_, 0)},
                false
            );
            llvm::Function* wrapper = llvm::Function::Create(
                main_type,
                llvm::GlobalValue::ExternalLinkage,
                "main",
                module_.get()
            );
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", wrapper);
            builder_.SetInsertPoint(entry);
            std::vector<llvm::Value*> args;
            auto arg_it = wrapper->arg_begin();
            args.push_back(&*arg_it++);
            args.push_back(&*arg_it);
            llvm::Value* result = builder_.CreateCall(functions_.at(i), args, "aurex.main.result");
            builder_.CreateRet(result);
            break;
        }
    }

    void emit_function(const FunctionId function_id, const Function& function) {
        llvm::Function* llvm_function = functions_.at(function_id.value);
        current_function_ = &function;
        values_.clear();
        blocks_.clear();

        for (base::u32 i = 0; i < function.blocks.size(); ++i) {
            blocks_[i] = llvm::BasicBlock::Create(context_, function.blocks[i].name, llvm_function);
        }

        base::usize param_index = 0;
        for (llvm::Argument& arg : llvm_function->args()) {
            if (param_index < function.param_values.size()) {
                values_[function.param_values[param_index].value] = &arg;
            }
            ++param_index;
        }

        for (base::u32 i = 0; i < function.blocks.size(); ++i) {
            builder_.SetInsertPoint(blocks_.at(i));
            for (const ValueId value_id : function.blocks[i].values) {
                const Value& value = source_.values[value_id.value];
                if (value.kind == ValueKind::param) {
                    continue;
                }
                values_[value_id.value] = emit_value(value);
            }
            emit_terminator(function.blocks[i].terminator);
        }

        current_function_ = nullptr;
    }

    [[nodiscard]] llvm::Value* emit_value(const Value& value) {
        switch (value.kind) {
        case ValueKind::param:
            return values_.at(value_id_for_param(value));
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
        case ValueKind::phi:
            return emit_phi(value);
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

    [[nodiscard]] llvm::Value* emit_unary(const Value& value) {
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

    [[nodiscard]] llvm::Value* emit_binary(const Value& value) {
        llvm::Value* lhs = get(value.lhs);
        llvm::Value* rhs = get(value.rhs);
        const bool is_float = source_.types.is_float(source_.values[value.lhs.value].type);
        switch (value.binary_op) {
        case BinaryOp::add: return is_float ? builder_.CreateFAdd(lhs, rhs) : builder_.CreateAdd(lhs, rhs);
        case BinaryOp::sub: return is_float ? builder_.CreateFSub(lhs, rhs) : builder_.CreateSub(lhs, rhs);
        case BinaryOp::mul: return is_float ? builder_.CreateFMul(lhs, rhs) : builder_.CreateMul(lhs, rhs);
        case BinaryOp::div: return is_float ? builder_.CreateFDiv(lhs, rhs) : builder_.CreateSDiv(lhs, rhs);
        case BinaryOp::mod: return builder_.CreateSRem(lhs, rhs);
        case BinaryOp::shl: return builder_.CreateShl(lhs, rhs);
        case BinaryOp::shr: return builder_.CreateAShr(lhs, rhs);
        case BinaryOp::less: return is_float ? builder_.CreateFCmpOLT(lhs, rhs) : builder_.CreateICmpSLT(lhs, rhs);
        case BinaryOp::less_equal: return is_float ? builder_.CreateFCmpOLE(lhs, rhs) : builder_.CreateICmpSLE(lhs, rhs);
        case BinaryOp::greater: return is_float ? builder_.CreateFCmpOGT(lhs, rhs) : builder_.CreateICmpSGT(lhs, rhs);
        case BinaryOp::greater_equal: return is_float ? builder_.CreateFCmpOGE(lhs, rhs) : builder_.CreateICmpSGE(lhs, rhs);
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

    [[nodiscard]] llvm::Value* emit_phi(const Value& value) {
        llvm::PHINode* phi = builder_.CreatePHI(llvm_type(value.type), static_cast<unsigned>(value.incoming.size()));
        for (const PhiInput& incoming : value.incoming) {
            phi->addIncoming(get(incoming.value), blocks_.at(incoming.predecessor.value));
        }
        return phi;
    }

    [[nodiscard]] llvm::Value* emit_call(const Value& value) {
        llvm::Function* target = nullptr;
        if (is_valid(value.call_target)) {
            target = functions_.at(value.call_target.value);
        } else {
            target = module_->getFunction(value.name);
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

    [[nodiscard]] llvm::Value* emit_field_addr(const Value& value) {
        const sema::TypeHandle object_pointee = pointee_type(value.object);
        const sema::TypeHandle record_type = source_.types.is_pointer(object_pointee)
            ? source_.types.get(object_pointee).pointee
            : object_pointee;
        const RecordLayout* record = find_record(source_, record_type);
        const base::usize index = record == nullptr ? 0 : record_field_index(*record, value.name);
        return builder_.CreateStructGEP(llvm_type(record_type), get(value.object), static_cast<unsigned>(index), value.name + ".addr");
    }

    [[nodiscard]] llvm::Value* emit_index_addr(const Value& value) {
        const sema::TypeHandle object_pointee = pointee_type(value.object);
        llvm::Value* object = get(value.object);
        llvm::Value* index = get(value.index);
        if (source_.types.is_array(object_pointee)) {
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0);
            return builder_.CreateGEP(llvm_type(object_pointee), object, {zero, index}, "index.addr");
        }
        return builder_.CreateGEP(llvm_type(object_pointee), object, index, "index.addr");
    }

    [[nodiscard]] llvm::Value* emit_aggregate(const Value& value) {
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

    [[nodiscard]] llvm::Value* emit_cast(const Value& value) {
        llvm::Value* operand = get(value.lhs);
        llvm::Type* target = llvm_type(value.target_type);
        switch (value.cast_kind) {
        case CastKind::numeric:
            if (operand->getType()->isIntegerTy() && target->isIntegerTy()) {
                return builder_.CreateIntCast(operand, target, true);
            }
            if (operand->getType()->isIntegerTy() && target->isFloatingPointTy()) {
                return builder_.CreateSIToFP(operand, target);
            }
            if (operand->getType()->isFloatingPointTy() && target->isIntegerTy()) {
                return builder_.CreateFPToSI(operand, target);
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

    [[nodiscard]] llvm::Value* emit_size_of(const sema::TypeHandle type) {
        llvm::TypeSize size = module_->getDataLayout().getTypeAllocSize(llvm_type(type));
        return llvm::ConstantInt::get(llvm_type(source_.types.builtin(sema::BuiltinType::usize)), size.getFixedValue());
    }

    [[nodiscard]] llvm::Value* emit_align_of(const sema::TypeHandle type) {
        llvm::Align align = module_->getDataLayout().getABITypeAlign(llvm_type(type));
        return llvm::ConstantInt::get(llvm_type(source_.types.builtin(sema::BuiltinType::usize)), align.value());
    }

    void emit_terminator(const Terminator& terminator) {
        if (builder_.GetInsertBlock()->getTerminator() != nullptr) {
            return;
        }
        switch (terminator.kind) {
        case TerminatorKind::none:
            builder_.CreateUnreachable();
            break;
        case TerminatorKind::branch:
            builder_.CreateBr(blocks_.at(terminator.target.value));
            break;
        case TerminatorKind::cond_branch:
            builder_.CreateCondBr(get(terminator.condition), blocks_.at(terminator.then_target.value), blocks_.at(terminator.else_target.value));
            break;
        case TerminatorKind::return_:
            if (is_valid(terminator.value)) {
                builder_.CreateRet(get(terminator.value));
            } else {
                builder_.CreateRetVoid();
            }
            break;
        }
    }

    [[nodiscard]] llvm::Value* integer_constant(const sema::TypeHandle type, const std::string& text) {
        std::uint64_t value = 0;
        static_cast<void>(parse_u64(text, value));
        return llvm::ConstantInt::get(llvm_type(type), value, true);
    }

    [[nodiscard]] llvm::Value* emit_string_literal(const std::string& literal, const bool c_string) {
        std::string decoded = decode_string_literal(literal, c_string);
        if (c_string) {
            decoded.push_back('\0');
            return global_string_pointer(decoded, "cstr", false);
        }

        llvm::Value* data = global_string_pointer(decoded, "str.data", false);
        const sema::TypeHandle str_type = source_.types.builtin(sema::BuiltinType::str);
        llvm::Value* result = llvm::UndefValue::get(llvm_type(str_type));
        result = builder_.CreateInsertValue(result, data, {0});
        result = builder_.CreateInsertValue(result, llvm::ConstantInt::get(llvm_type(source_.types.builtin(sema::BuiltinType::usize)), decoded.size()), {1});
        return result;
    }

    [[nodiscard]] llvm::Value* global_string_pointer(const std::string& text, const std::string& name, const bool add_null) {
        llvm::GlobalVariable* global = builder_.CreateGlobalString(text, name, 0, module_.get(), add_null);
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
        return builder_.CreateInBoundsGEP(global->getValueType(), global, {zero, zero}, name + ".ptr");
    }

    [[nodiscard]] llvm::Type* llvm_type(const sema::TypeHandle type) {
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
            case sema::BuiltinType::usize: return module_->getDataLayout().getIntPtrType(context_);
            case sema::BuiltinType::f32: return llvm::Type::getFloatTy(context_);
            case sema::BuiltinType::f64: return llvm::Type::getDoubleTy(context_);
            case sema::BuiltinType::str:
                return llvm::StructType::get(
                    llvm::PointerType::get(context_, 0),
                    module_->getDataLayout().getIntPtrType(context_)
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

    [[nodiscard]] llvm::Type* pointee_llvm_type(const sema::TypeHandle pointer_type) {
        if (!source_.types.is_pointer(pointer_type)) {
            return llvm::Type::getVoidTy(context_);
        }
        return llvm_type(source_.types.get(pointer_type).pointee);
    }

    [[nodiscard]] sema::TypeHandle pointee_type(const ValueId value) const noexcept {
        const sema::TypeHandle type = source_.values[value.value].type;
        if (!source_.types.is_pointer(type)) {
            return sema::invalid_type_handle;
        }
        return source_.types.get(type).pointee;
    }

    [[nodiscard]] llvm::Value* get(const ValueId id) const {
        return values_.at(id.value);
    }

    [[nodiscard]] base::u32 value_id_for_param(const Value& value) const {
        for (const auto& entry : values_) {
            if (entry.second != nullptr && entry.second->getName() == value.name) {
                return entry.first;
            }
        }
        return 0;
    }

    const Module& source_;
    llvm::LLVMContext context_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;
    const Function* current_function_ = nullptr;
    std::unordered_map<base::u32, llvm::StructType*> records_;
    std::unordered_map<base::u32, llvm::Function*> functions_;
    std::unordered_map<base::u32, llvm::BasicBlock*> blocks_;
    std::unordered_map<base::u32, llvm::Value*> values_;
};

} // namespace

base::Result<LlvmIrOutput> emit_llvm_ir(const Module& module, std::string module_name) {
    LlvmEmitter emitter(module, std::move(module_name));
    return emitter.run();
}

} // namespace aurex::ir

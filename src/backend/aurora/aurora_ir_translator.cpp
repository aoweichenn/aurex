#include "aurora_backend_internal.hpp"

#include "Aurora/Air/Builder.h"
#include "Aurora/Air/Constant.h"
#include "Aurora/Air/Function.h"
#include "Aurora/Air/Instruction.h"
#include "Aurora/Air/Module.h"
#include "Aurora/Air/Type.h"
#include "Aurora/ADT/SmallVector.h"

#include <limits>
#include <stdexcept>

namespace aurex::backend {

IrTranslator::IrTranslator(const ir::Module& src_module, base::DiagnosticSink& diagnostics)
    : src_module_(src_module)
    , diag_(diagnostics)
    , type_translator_(src_module) {}

bool IrTranslator::is_signed_integer(const sema::TypeHandle type) const {
    if (!sema::is_valid(type)) {
        return false;
    }
    const sema::TypeInfo& info = src_module_.types.get(type);
    if (info.kind != sema::TypeKind::builtin) {
        return false;
    }
    switch (info.builtin) {
    case sema::BuiltinType::i8:
    case sema::BuiltinType::i16:
    case sema::BuiltinType::i32:
    case sema::BuiltinType::i64:
    case sema::BuiltinType::isize:
        return true;
    default:
        return false;
    }
}

aurora::ICmpCond IrTranslator::map_compare(const aurex::ir::BinaryOp op, const bool is_signed) const {
    switch (op) {
    case ir::BinaryOp::equal:
        return aurora::ICmpCond::EQ;
    case ir::BinaryOp::not_equal:
        return aurora::ICmpCond::NE;
    case ir::BinaryOp::less:
        return is_signed ? aurora::ICmpCond::SLT : aurora::ICmpCond::ULT;
    case ir::BinaryOp::less_equal:
        return is_signed ? aurora::ICmpCond::SLE : aurora::ICmpCond::ULE;
    case ir::BinaryOp::greater:
        return is_signed ? aurora::ICmpCond::SGT : aurora::ICmpCond::UGT;
    case ir::BinaryOp::greater_equal:
        return is_signed ? aurora::ICmpCond::SGE : aurora::ICmpCond::UGE;
    default:
        return aurora::ICmpCond::EQ;
    }
}

std::unique_ptr<aurora::Module> IrTranslator::translate() {
    dst_module_ = std::make_unique<aurora::Module>("aurex");

    type_translator_.translate_all_records();

    for (base::u32 i = 0; i < static_cast<base::u32>(src_module_.constants.size()); ++i) {
        translate_global_constant(src_module_.constants[i], i);
    }

    for (base::u32 i = 0; i < static_cast<base::u32>(src_module_.functions.size()); ++i) {
        translate_function(src_module_.functions[i], i);
    }

    return std::move(dst_module_);
}

void IrTranslator::translate_global_constant(const ir::GlobalConstant& constant, const base::u32 idx) {
    aurora::Type* type = type_translator_.translate_type(constant.type);
    std::string symbol = constant.symbol.empty() ? constant.name : constant.symbol;

    aurora::Constant* init = nullptr;
    if (ir::is_valid(constant.initializer)) {
        const ir::Value& init_val = src_module_.values[constant.initializer.value];
        if (init_val.kind == ir::ValueKind::integer_literal ||
            init_val.kind == ir::ValueKind::bool_literal ||
            init_val.kind == ir::ValueKind::byte_literal ||
            init_val.kind == ir::ValueKind::null_literal) {
            int64_t val = 0;
            if (init_val.kind != ir::ValueKind::null_literal) {
                try { val = std::stoll(init_val.text, nullptr, 0); } catch (...) {}
            }
            init = aurora::ConstantInt::getInt(type, static_cast<uint64_t>(val));
        }
    }

    aurora::GlobalVariable* gv = dst_module_->createGlobal(type, symbol);
    if (init != nullptr) {
        gv->setInitializer(init);
    }
    const_map_[idx] = gv;
}

void IrTranslator::translate_function(const ir::Function& src_fn, const base::u32 fn_idx) {
    aurora::FunctionType* fn_type = type_translator_.translate_function_type(
        src_fn.return_type, src_fn.signature_params);

    std::string symbol = src_fn.symbol.empty() ? src_fn.name : src_fn.symbol;

    if (src_fn.linkage == ir::Linkage::extern_c) {
        func_map_[fn_idx] = nullptr;
        return;
    }

    aurora::Function* dst_fn = dst_module_->createFunction(fn_type, symbol);

    func_map_[fn_idx] = dst_fn;

    if (src_fn.blocks.empty()) {
        return;
    }

    value_map_.clear();
    block_map_.clear();

    for (base::u32 i = 0; i < static_cast<base::u32>(src_fn.param_values.size()); ++i) {
        const ir::ValueId param_id = src_fn.param_values[i];
        if (ir::is_valid(param_id)) {
            value_map_[param_id.value] = static_cast<unsigned>(i);
        }
    }

    for (base::u32 i = 0; i < static_cast<base::u32>(src_fn.blocks.size()); ++i) {
        const ir::BasicBlock& src_block = src_fn.blocks[i];
        translate_block(i, src_block, dst_fn);
    }
}

void IrTranslator::translate_block(const base::u32 block_idx, const ir::BasicBlock& src_block, aurora::Function* dst_fn) {
    aurora::BasicBlock* dst_bb = nullptr;
    if (block_idx == 0) {
        dst_bb = dst_fn->getEntryBlock();
    } else {
        dst_bb = dst_fn->createBasicBlock(src_block.name);
    }
    block_map_[block_idx] = dst_bb;

    aurora::AIRBuilder builder(dst_bb);

    for (base::u32 i = 0; i < static_cast<base::u32>(src_block.values.size()); ++i) {
        const ir::Value& value = src_module_.values[src_block.values[i].value];
        unsigned result_vreg = translate_value(value, builder);
        value_map_[src_block.values[i].value] = result_vreg;
    }

    translate_terminator(src_block.terminator, builder);
}

unsigned IrTranslator::translate_value(const ir::Value& value, aurora::AIRBuilder& builder) {
    switch (value.kind) {
    case ir::ValueKind::integer_literal: {
        int64_t int_val = 0;
        try {
            int_val = std::stoll(value.text, nullptr, 0);
        } catch (...) {
            int_val = 0;
        }
        return builder.createConstantInt(int_val);
    }
    case ir::ValueKind::bool_literal: {
        return builder.createConstantInt((value.text == "true") ? 1 : 0);
    }
    case ir::ValueKind::null_literal: {
        return builder.createConstantInt(0);
    }
    case ir::ValueKind::byte_literal: {
        int64_t byte_val = 0;
        try {
            byte_val = std::stoll(value.text, nullptr, 0);
        } catch (...) {
            byte_val = 0;
        }
        return builder.createConstantInt(byte_val & 0xFF);
    }
    case ir::ValueKind::param: {
        return 0;
    }
    case ir::ValueKind::string_literal:
    case ir::ValueKind::c_string_literal: {
        auto it = string_map_.find(value.text);
        if (it != string_map_.end()) {
            return builder.createConstantInt(0);
        }
        aurora::Type* elem_ty = aurora::Type::getInt8Ty();
        base::u32 len = static_cast<base::u32>(value.text.size());
        if (value.kind == ir::ValueKind::c_string_literal) {
            len = len + 1;
        }
        aurora::Type* arr_ty = aurora::Type::getArrayTy(elem_ty, len);
        std::string gv_name = ".str." + std::to_string(string_counter_++);
        aurora::GlobalVariable* gv = dst_module_->createGlobal(arr_ty, gv_name);
        string_map_[value.text] = gv;
        return builder.createConstantInt(0);
    }
    case ir::ValueKind::constant_ref: {
        auto it = const_map_.find(value.constant.value);
        if (it != const_map_.end()) {
            return builder.createConstantInt(0);
        }
        return builder.createConstantInt(0);
    }
    case ir::ValueKind::alloca: {
        aurora::Type* ty = type_translator_.translate_type(value.type);
        return builder.createAlloca(ty);
    }
    case ir::ValueKind::load: {
        aurora::Type* ty = type_translator_.translate_type(value.type);
        auto it = value_map_.find(value.object.value);
        unsigned ptr = (it != value_map_.end()) ? it->second : 0;
        return builder.createLoad(ty, ptr);
    }
    case ir::ValueKind::store: {
        auto val_it = value_map_.find(value.lhs.value);
        auto ptr_it = value_map_.find(value.object.value);
        unsigned val_vreg = (val_it != value_map_.end()) ? val_it->second : 0;
        unsigned ptr_vreg = (ptr_it != value_map_.end()) ? ptr_it->second : 0;
        builder.createStore(val_vreg, ptr_vreg);
        return 0;
    }
    case ir::ValueKind::unary: {
        auto it = value_map_.find(value.lhs.value);
        unsigned op = (it != value_map_.end()) ? it->second : 0;
        aurora::Type* ty = type_translator_.translate_type(value.type);

        switch (value.unary_op) {
        case ir::UnaryOp::numeric_negate: {
            unsigned zero = builder.createConstantInt(0);
            return builder.createSub(ty, zero, op);
        }
        case ir::UnaryOp::bitwise_not: {
            unsigned neg_one = builder.createConstantInt(-1);
            return builder.createXor(ty, op, neg_one);
        }
        case ir::UnaryOp::logical_not: {
            unsigned zero = builder.createConstantInt(0);
            return builder.createICmp(aurora::ICmpCond::EQ, op, zero);
        }
        default:
            return op;
        }
    }
    case ir::ValueKind::binary: {
        auto lhs_it = value_map_.find(value.lhs.value);
        auto rhs_it = value_map_.find(value.rhs.value);
        unsigned lhs = (lhs_it != value_map_.end()) ? lhs_it->second : 0;
        unsigned rhs = (rhs_it != value_map_.end()) ? rhs_it->second : 0;
        aurora::Type* ty = type_translator_.translate_type(value.type);
        bool signed_ty = is_signed_integer(value.type);

        switch (value.binary_op) {
        case ir::BinaryOp::add:
            return builder.createAdd(ty, lhs, rhs);
        case ir::BinaryOp::sub:
            return builder.createSub(ty, lhs, rhs);
        case ir::BinaryOp::mul:
            return builder.createMul(ty, lhs, rhs);
        case ir::BinaryOp::div:
            if (signed_ty) {
                return builder.createSDiv(ty, lhs, rhs);
            } else {
                return builder.createUDiv(ty, lhs, rhs);
            }
        case ir::BinaryOp::mod: {
            aurora::AIRInstruction* inst = signed_ty
                ? aurora::AIRInstruction::createSRem(ty, lhs, rhs)
                : aurora::AIRInstruction::createURem(ty, lhs, rhs);
            unsigned vreg = builder.getInsertBlock()->getParent()->nextVReg();
            builder.getInsertBlock()->getParent()->recordVRegType(vreg, ty);
            builder.getInsertBlock()->pushBack(inst);
            builder.setDestVReg(inst, vreg);
            return vreg;
        }
        case ir::BinaryOp::shl:
            return builder.createShl(ty, lhs, rhs);
        case ir::BinaryOp::shr:
            if (signed_ty) {
                return builder.createAShr(ty, lhs, rhs);
            } else {
                return builder.createLShr(ty, lhs, rhs);
            }
        case ir::BinaryOp::equal:
        case ir::BinaryOp::not_equal:
        case ir::BinaryOp::less:
        case ir::BinaryOp::less_equal:
        case ir::BinaryOp::greater:
        case ir::BinaryOp::greater_equal: {
            auto cond = map_compare(value.binary_op, signed_ty);
            return builder.createICmp(cond, lhs, rhs);
        }
        case ir::BinaryOp::bit_and:
            return builder.createAnd(ty, lhs, rhs);
        case ir::BinaryOp::bit_xor:
            return builder.createXor(ty, lhs, rhs);
        case ir::BinaryOp::bit_or:
            return builder.createOr(ty, lhs, rhs);
        case ir::BinaryOp::logical_and:
        case ir::BinaryOp::logical_or:
            return builder.createConstantInt(0);
        }
        return 0;
    }
    case ir::ValueKind::phi: {
        aurora::Type* ty = type_translator_.translate_type(value.type);
        aurora::SmallVector<std::pair<aurora::BasicBlock*, unsigned>, 4> incomings;
        for (const ir::PhiInput& input : value.incoming) {
            auto bb_it = block_map_.find(input.predecessor.value);
            auto val_it = value_map_.find(input.value.value);
            if (bb_it != block_map_.end() && val_it != value_map_.end()) {
                incomings.push_back({bb_it->second, val_it->second});
            }
        }
        return builder.createPhi(ty, incomings);
    }
    case ir::ValueKind::call: {
        aurora::Function* callee_fn = nullptr;
        auto fit = func_map_.find(value.call_target.value);
        if (fit != func_map_.end()) {
            callee_fn = fit->second;
        }

        if (callee_fn == nullptr && value.call_target.value < src_module_.functions.size()) {
            const ir::Function& target_fn = src_module_.functions[value.call_target.value];
            std::string extern_sym = target_fn.symbol.empty() ? target_fn.name : target_fn.symbol;
            auto eit = extern_func_map_.find(extern_sym);
            if (eit != extern_func_map_.end()) {
                callee_fn = eit->second;
            } else {
                aurora::FunctionType* extern_ty = type_translator_.translate_function_type(
                    target_fn.return_type, target_fn.signature_params);
                callee_fn = dst_module_->createFunction(extern_ty, extern_sym);
                extern_func_map_[extern_sym] = callee_fn;
                func_map_[value.call_target.value] = callee_fn;
            }
        }

        aurora::SmallVector<unsigned, 8> args;
        for (const ir::ValueId& arg_id : value.args) {
            auto ait = value_map_.find(arg_id.value);
            args.push_back((ait != value_map_.end()) ? ait->second : 0);
        }
        if (callee_fn != nullptr) {
            return builder.createCall(callee_fn, args);
        }
        return builder.createConstantInt(0);
    }
    case ir::ValueKind::field_addr:
    case ir::ValueKind::index_addr: {
        aurora::Type* ty = type_translator_.translate_type(value.type);
        auto ptr_it = value_map_.find(value.object.value);
        unsigned ptr = (ptr_it != value_map_.end()) ? ptr_it->second : 0;

        aurora::SmallVector<unsigned, 4> indices;
        if (value.kind == ir::ValueKind::field_addr) {
            indices.push_back(0);
            auto idx_it = value_map_.find(value.index.value);
            indices.push_back((idx_it != value_map_.end()) ? idx_it->second : 0);
        } else {
            auto idx_it = value_map_.find(value.index.value);
            indices.push_back((idx_it != value_map_.end()) ? idx_it->second : 0);
        }
        return builder.createGEP(ty, ptr, indices);
    }
    case ir::ValueKind::aggregate: {
        aurora::Type* struct_ty = type_translator_.translate_type(value.type);
        aurora::Function* fn = builder.getInsertBlock()->getParent();
        aurora::BasicBlock* bb = builder.getInsertBlock();

        unsigned alloca_vreg = fn->nextVReg();
        aurora::Type* ptr_ty = aurora::Type::getPointerTy(struct_ty);
        fn->recordVRegType(alloca_vreg, ptr_ty);
        aurora::AIRInstruction* alloca_inst = aurora::AIRInstruction::createAlloca(struct_ty);
        bb->pushBack(alloca_inst);
        alloca_inst->setDestVReg(alloca_vreg);

        for (base::u32 i = 0; i < static_cast<base::u32>(value.fields.size()); ++i) {
            auto field_it = value_map_.find(value.fields[i].value.value);
            unsigned field_val = (field_it != value_map_.end()) ? field_it->second : 0;

            aurora::SmallVector<unsigned, 4> gep_indices;
            gep_indices.push_back(0);
            gep_indices.push_back(i);

            unsigned gep_vreg = fn->nextVReg();
            fn->recordVRegType(gep_vreg, ptr_ty);
            aurora::AIRInstruction* gep_inst = aurora::AIRInstruction::createGEP(ptr_ty, alloca_vreg, gep_indices);
            bb->pushBack(gep_inst);
            gep_inst->setDestVReg(gep_vreg);

            aurora::AIRInstruction* store_inst = aurora::AIRInstruction::createStore(field_val, gep_vreg);
            bb->pushBack(store_inst);
        }

        aurora::Type* result_ty = type_translator_.translate_type(value.type);
        unsigned result_vreg = fn->nextVReg();
        fn->recordVRegType(result_vreg, result_ty);
        aurora::AIRInstruction* load_inst = aurora::AIRInstruction::createLoad(result_ty, alloca_vreg);
        bb->pushBack(load_inst);
        load_inst->setDestVReg(result_vreg);

        return result_vreg;
    }
    case ir::ValueKind::cast: {
        auto src_it = value_map_.find(value.lhs.value);
        unsigned src = (src_it != value_map_.end()) ? src_it->second : 0;
        aurora::Type* dst_ty = type_translator_.translate_type(value.type);
        aurora::Type* src_ty = type_translator_.translate_type(value.lhs.value < src_module_.values.size()
            ? src_module_.values[value.lhs.value].type : sema::invalid_type_handle);

        switch (value.cast_kind) {
        case ir::CastKind::numeric: {
            unsigned src_bits = (src_ty != nullptr) ? src_ty->getSizeInBits() : 32;
            unsigned dst_bits = (dst_ty != nullptr) ? dst_ty->getSizeInBits() : 32;
            if (dst_bits > src_bits) {
                bool signed_src = is_signed_integer(value.lhs.value < src_module_.values.size()
                    ? src_module_.values[value.lhs.value].type : sema::invalid_type_handle);
                if (signed_src) {
                    return builder.createSExt(dst_ty, src);
                } else {
                    return builder.createZExt(dst_ty, src);
                }
            } else if (dst_bits < src_bits) {
                return builder.createTrunc(dst_ty, src);
            } else {
                return src;
            }
        }
        case ir::CastKind::pointer:
        case ir::CastKind::ptr_addr:
        case ir::CastKind::ptr_from_addr:
        case ir::CastKind::bitcast:
            return src;
        }
        return src;
    }
    case ir::ValueKind::size_of:
    case ir::ValueKind::align_of: {
        if (sema::is_valid(value.type)) {
            aurora::Type* ty = type_translator_.translate_type(value.type);
            if (ty != nullptr) {
                int64_t result_val = 0;
                if (value.kind == ir::ValueKind::size_of) {
                    result_val = static_cast<int64_t>(ty->getSizeInBits() / 8);
                } else {
                    result_val = static_cast<int64_t>(ty->getAlignInBits() / 8);
                }
                return builder.createConstantInt(result_val);
            }
        }
        return builder.createConstantInt(0);
    }
    default:
        return builder.createConstantInt(0);
    }
}

void IrTranslator::translate_terminator(const ir::Terminator& terminator, aurora::AIRBuilder& builder) {
    switch (terminator.kind) {
    case ir::TerminatorKind::branch: {
        auto it = block_map_.find(terminator.target.value);
        if (it != block_map_.end()) {
            builder.createBr(it->second);
        }
        break;
    }
    case ir::TerminatorKind::cond_branch: {
        auto cond_it = value_map_.find(terminator.condition.value);
        auto then_it = block_map_.find(terminator.then_target.value);
        auto else_it = block_map_.find(terminator.else_target.value);
        if (cond_it != value_map_.end() && then_it != block_map_.end() && else_it != block_map_.end()) {
            builder.createCondBr(cond_it->second, then_it->second, else_it->second);
        }
        break;
    }
    case ir::TerminatorKind::return_: {
        if (ir::is_valid(terminator.value)) {
            auto it = value_map_.find(terminator.value.value);
            if (it != value_map_.end()) {
                builder.createRet(it->second);
            } else {
                builder.createRetVoid();
            }
        } else {
            builder.createRetVoid();
        }
        break;
    }
    default:
        break;
    }
}

} // namespace aurex::backend

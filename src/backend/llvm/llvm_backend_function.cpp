#include <backend/llvm/llvm_backend_internal.hpp>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

namespace aurex::backend {

void LlvmEmitter::emit_function(const FunctionId function_id, const Function& function) {
    llvm::Function* llvm_function = functions_.at(function_id.value);
    current_function_ = &function;
    values_.clear();
    blocks_.clear();
    pending_phis_.clear();

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
        emit_block_phi_nodes(function, i);
    }

    for (base::u32 i = 0; i < function.blocks.size(); ++i) {
        builder_.SetInsertPoint(blocks_.at(i));
        for (const ValueId value_id : function.blocks[i].values) {
            const Value& value = source_.values[value_id.value];
            if (value.kind == ValueKind::param || value.kind == ValueKind::phi) {
                continue;
            }
            values_[value_id.value] = emit_value(value_id);
        }
        emit_terminator(function.blocks[i].terminator);
    }

    populate_phi_edges();
    current_function_ = nullptr;
}

void LlvmEmitter::emit_block_phi_nodes(const Function& function, const base::u32 block_index) {
    builder_.SetInsertPoint(blocks_.at(block_index));
    for (const ValueId value_id : function.blocks[block_index].values) {
        const Value& value = source_.values[value_id.value];
        if (value.kind != ValueKind::phi) {
            continue;
        }
        llvm::PHINode* phi = builder_.CreatePHI(llvm_type(value.type), static_cast<unsigned>(value.incoming.size()));
        pending_phis_[value_id.value] = phi;
        values_[value_id.value] = phi;
    }
}

void LlvmEmitter::populate_phi_edges() {
    for (const auto& entry : pending_phis_) {
        const Value& value = source_.values[entry.first];
        for (const PhiInput& incoming : value.incoming) {
            entry.second->addIncoming(get(incoming.value), blocks_.at(incoming.predecessor.value));
        }
    }
}

void LlvmEmitter::emit_terminator(const Terminator& terminator) {
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

llvm::Value* LlvmEmitter::get(const ValueId id) const {
    return values_.at(id.value);
}

} // namespace aurex::backend

#include "llvm_backend_internal.hpp"

#include "aurex/ir/verify.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace aurex::backend {

LlvmEmitter::LlvmEmitter(const Module& module, std::string module_name)
    : source_(module),
      context_(),
      module_(std::make_unique<llvm::Module>(std::move(module_name), context_)),
      builder_(context_) {}

base::Result<LlvmIrOutput> LlvmEmitter::run() {
    if (auto verified = verify_module(source_); !verified) {
        return base::Result<LlvmIrOutput>::fail(verified.error());
    }

    if (auto target = configure_target(); !target) {
        return base::Result<LlvmIrOutput>::fail(target.error());
    }
    declare_records();
    declare_constants();
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

base::Result<void> LlvmEmitter::configure_target() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    const std::string triple_text = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(triple_text);
    module_->setTargetTriple(triple);

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (target == nullptr) {
        return base::Result<void>::fail({base::ErrorCode::codegen_error, "LLVM target lookup failed for " + triple_text + ": " + error});
    }

    target_machine_.reset(target->createTargetMachine(
        triple,
        llvm::sys::getHostCPUName(),
        "",
        llvm::TargetOptions {},
        std::nullopt
    ));
    if (target_machine_ == nullptr) {
        return base::Result<void>::fail({base::ErrorCode::codegen_error, "LLVM target machine creation failed for " + triple_text});
    }
    module_->setDataLayout(target_machine_->createDataLayout());
    return base::Result<void>::ok();
}

void LlvmEmitter::declare_constants() {
    for (base::u32 i = 0; i < source_.constants.size(); ++i) {
        const GlobalConstant& constant = source_.constants[i];
        llvm::Constant* initializer = emit_constant_initializer(source_.values[constant.initializer.value]);
        llvm::GlobalVariable* global = new llvm::GlobalVariable(
            *module_,
            llvm_type(constant.type),
            true,
            llvm::GlobalValue::InternalLinkage,
            initializer,
            constant.symbol
        );
        global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        constants_[i] = global;
    }
}

void LlvmEmitter::declare_functions() {
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

void LlvmEmitter::declare_main_wrapper() {
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

} // namespace aurex::backend

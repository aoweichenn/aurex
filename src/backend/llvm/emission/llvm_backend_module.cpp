#include <aurex/backend/backend_messages.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <backend/llvm/internal/llvm_backend_internal.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

namespace aurex::backend {

namespace {

constexpr unsigned LLVM_BACKEND_MODULE_DEFAULT_ADDRESS_SPACE = 0U;

} // namespace

LlvmEmitter::LlvmEmitter(const Module& module, const std::string_view module_name)
    : source_(module), context_(), module_(std::make_unique<llvm::Module>(module_name, context_)), builder_(context_)
{
}

std::string_view LlvmEmitter::text(const IrTextId id) const noexcept
{
    return this->source_.text(id);
}

std::string LlvmEmitter::suffixed_name(const IrTextId id, const std::string_view suffix) const
{
    std::string result(this->text(id));
    result.append(suffix);
    return result;
}

base::Result<LlvmIrOutput> LlvmEmitter::run()
{
    if (const auto verified = verify_module(source_); !verified) {
        return base::Result<LlvmIrOutput>::fail(verified.error());
    }

    if (const auto target = configure_target(); !target) {
        return base::Result<LlvmIrOutput>::fail(target.error());
    }
    declare_records();
    declare_constants();
    declare_functions();
    declare_trait_object_vtables();
    for (base::u32 i = 0; i < source_.functions.size(); ++i) {
        const Function& function = source_.functions[i];
        if (function.linkage != Linkage::extern_c) {
            emit_function(FunctionId{i}, function);
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
    return base::Result<LlvmIrOutput>::ok(LlvmIrOutput{std::move(text)});
}

base::Result<void> LlvmEmitter::configure_target()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    const std::string triple_text = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(triple_text);
    module_->setTargetTriple(triple);

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (target == nullptr) {
        return base::Result<void>::fail(
            {base::ErrorCode::codegen_error, backend_llvm_target_lookup_failed_message(triple_text, error)});
    }

    target_machine_.reset(
        target->createTargetMachine(triple, llvm::sys::getHostCPUName(), "", llvm::TargetOptions{}, std::nullopt));
    if (target_machine_ == nullptr) {
        return base::Result<void>::fail(
            {base::ErrorCode::codegen_error, backend_llvm_target_machine_creation_failed_message(triple_text)});
    }
    module_->setDataLayout(target_machine_->createDataLayout());
    return base::Result<void>::ok();
}

void LlvmEmitter::declare_constants()
{
    for (base::u32 i = 0; i < source_.constants.size(); ++i) {
        const GlobalConstant& constant = source_.constants[i];
        llvm::Constant* initializer = emit_constant_initializer(source_.values[constant.initializer.value]);
        llvm::GlobalVariable* global = new llvm::GlobalVariable(*module_, llvm_type(constant.type), true,
            llvm::GlobalValue::InternalLinkage, initializer, this->text(constant.symbol));
        global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        constants_[i] = global;
    }
}

void LlvmEmitter::declare_functions()
{
    for (base::u32 i = 0; i < source_.functions.size(); ++i) {
        functions_[i] = declare_function(FunctionId{i}, source_.functions[i]);
    }
    declare_main_wrapper();
}

void LlvmEmitter::declare_trait_object_vtables()
{
    for (const TraitObjectVTableLayout& layout : this->source_.trait_object_vtables) {
        llvm::StructType* vtable_type = this->llvm_vtable_type(layout);
        llvm::GlobalVariable* global = new llvm::GlobalVariable(*this->module_, vtable_type, true,
            llvm::GlobalValue::InternalLinkage, nullptr, this->text(layout.symbol));
        global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        this->trait_object_vtables_[layout.layout_key.global_id] = global;
    }

    for (const TraitObjectVTableLayout& layout : this->source_.trait_object_vtables) {
        llvm::ArrayType* method_array_type = this->llvm_vtable_array_type(layout);
        std::vector<llvm::Constant*> entries(
            layout.method_slots.size(),
            llvm::ConstantPointerNull::get(
                llvm::PointerType::get(this->context_, LLVM_BACKEND_MODULE_DEFAULT_ADDRESS_SPACE)));
        for (const TraitObjectVTableMethodSlot& slot : layout.method_slots) {
            if (slot.slot >= entries.size()) {
                continue;
            }
            entries[slot.slot] = llvm::ConstantExpr::getBitCast(this->functions_.at(slot.function.value),
                llvm::PointerType::get(this->context_, LLVM_BACKEND_MODULE_DEFAULT_ADDRESS_SPACE));
        }
        llvm::ArrayType* supertrait_array_type = this->llvm_vtable_supertrait_array_type(layout);
        std::vector<llvm::Constant*> supertrait_entries(
            layout.supertrait_edges.size(),
            llvm::ConstantPointerNull::get(
                llvm::PointerType::get(this->context_, LLVM_BACKEND_MODULE_DEFAULT_ADDRESS_SPACE)));
        for (const TraitObjectVTableSupertraitEdge& edge : layout.supertrait_edges) {
            if (edge.edge_index >= supertrait_entries.size()) {
                continue;
            }
            const auto target = this->trait_object_vtables_.find(edge.target_layout.global_id);
            if (target == this->trait_object_vtables_.end()) {
                continue;
            }
            supertrait_entries[edge.edge_index] = llvm::ConstantExpr::getBitCast(target->second,
                llvm::PointerType::get(this->context_, LLVM_BACKEND_MODULE_DEFAULT_ADDRESS_SPACE));
        }
        llvm::Constant* initializer = llvm::ConstantStruct::get(this->llvm_vtable_type(layout),
            {llvm::ConstantArray::get(method_array_type, entries),
                llvm::ConstantArray::get(supertrait_array_type, supertrait_entries)});
        this->trait_object_vtables_.at(layout.layout_key.global_id)->setInitializer(initializer);
    }
}

llvm::Function* LlvmEmitter::declare_function(const FunctionId function_id, const Function& function)
{
    llvm::FunctionType* function_type = this->llvm_function_type(function);

    if (function.linkage == Linkage::extern_c) {
        if (const auto found = extern_functions_.find(function.symbol); found != extern_functions_.end()) {
            return found->second;
        }
        if (llvm::Function* existing = module_->getFunction(this->text(function.symbol)); existing != nullptr) {
            extern_functions_[function.symbol] = existing;
            return existing;
        }
    }

    llvm::Function* llvm_function = llvm::Function::Create(function_type,
        function.linkage == Linkage::internal ? llvm::GlobalValue::InternalLinkage : llvm::GlobalValue::ExternalLinkage,
        this->text(function.symbol), module_.get());
    llvm_function->setCallingConv(llvm::CallingConv::C);
    llvm_function->addFnAttr("aurex.ir.fn_id", std::to_string(function_id.value));
    if (function.linkage == Linkage::extern_c) {
        extern_functions_[function.symbol] = llvm_function;
    }
    return llvm_function;
}

void LlvmEmitter::declare_main_wrapper()
{
    for (base::u32 i = 0; i < source_.functions.size(); ++i) {
        const Function& function = source_.functions[i];
        if (!function.is_entry) {
            continue;
        }
        llvm::FunctionType* main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context_),
            {llvm::Type::getInt32Ty(context_), llvm::PointerType::get(context_, 0)}, false);
        llvm::Function* wrapper =
            llvm::Function::Create(main_type, llvm::GlobalValue::ExternalLinkage, "main", module_.get());
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", wrapper);
        builder_.SetInsertPoint(entry);
        std::vector<llvm::Value*> args;
        if (function.signature_params.size() == 2) {
            auto arg_it = wrapper->arg_begin();
            args.push_back(&*arg_it++);
            args.push_back(&*arg_it);
        }
        if (source_.types.is_void(function.return_type)) {
            builder_.CreateCall(functions_.at(i), args);
            builder_.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0));
        } else {
            llvm::Value* result = builder_.CreateCall(functions_.at(i), args, "aurex.main.result");
            builder_.CreateRet(result);
        }
        break;
    }
}

} // namespace aurex::backend

#include "aurora_backend_internal.hpp"

#include "Aurora/Air/Module.h"
#include "Aurora/Air/Function.h"
#include "Aurora/Air/Type.h"
#include "Aurora/CodeGen/PassManager.h"
#include "Aurora/CodeGen/MachineFunction.h"
#include "Aurora/Target/TargetMachine.h"
#include "Aurora/Target/X86/X86RegisterInfo.h"
#include "Aurora/MC/MCStreamer.h"
#include "Aurora/MC/X86AsmPrinter.h"
#include "Aurora/MC/ObjectWriter.h"

#include <sstream>
#include <fstream>

namespace aurex::backend {

AuroraEmitter::AuroraEmitter(base::DiagnosticSink& diagnostics)
    : diag_(diagnostics) {}

base::Result<AuroraEmitOutput> AuroraEmitter::emit_asm(const AuroraEmitRequest& request) {
    if (request.module == nullptr) {
        return base::Result<AuroraEmitOutput>::fail({
            base::ErrorCode::codegen_error, "null IR module for Aurora backend"});
    }

    IrTranslator translator(*request.module, diag_);
    std::unique_ptr<aurora::Module> air_module = translator.translate();
    if (air_module == nullptr) {
        return base::Result<AuroraEmitOutput>::fail({
            base::ErrorCode::codegen_error, "IR translation to Aurora AIR failed"});
    }

    if (air_module->getFunctions().empty()) {
        diag_.push(base::Diagnostic{
            base::Severity::warning,
            {},
            "Aurora backend: no functions to compile"
        });
        AuroraEmitOutput output;
        output.text = "\t.text\n";
        return base::Result<AuroraEmitOutput>::ok(std::move(output));
    }

    auto target_machine = aurora::TargetMachine::createX86_64();

    std::ostringstream oss;
    aurora::AsmTextStreamer streamer(oss);
    const auto& x86_reg_info = dynamic_cast<const aurora::X86RegisterInfo&>(target_machine->getRegisterInfo());
    aurora::X86AsmPrinter printer(streamer, x86_reg_info);

    for (auto& fn : air_module->getFunctions()) {
        if (fn->getBlocks().size() <= 1 &&
            fn->getBlocks()[0]->getFirst() == nullptr) {
            continue;
        }
        aurora::MachineFunction mf(*fn, *target_machine);
        aurora::PassManager pm;
        aurora::CodeGenContext::addStandardPasses(pm, *target_machine);
        pm.run(mf);

        printer.emitFunction(mf);
    }

    AuroraEmitOutput output;
    output.text = oss.str();
    return base::Result<AuroraEmitOutput>::ok(std::move(output));
}

base::Result<void> AuroraEmitter::emit_obj(const AuroraEmitRequest& request) {
    if (request.module == nullptr) {
        return base::Result<void>::fail({
            base::ErrorCode::codegen_error, "null IR module for Aurora backend"});
    }

    IrTranslator translator(*request.module, diag_);
    std::unique_ptr<aurora::Module> air_module = translator.translate();
    if (air_module == nullptr) {
        return base::Result<void>::fail({
            base::ErrorCode::codegen_error, "IR translation to Aurora AIR failed"});
    }

    if (air_module->getFunctions().empty()) {
        diag_.push(base::Diagnostic{
            base::Severity::warning,
            {},
            "Aurora backend: no functions to compile for object output"
        });
        return base::Result<void>::ok();
    }

    auto target_machine = aurora::TargetMachine::createX86_64();
    aurora::ObjectWriter writer;

    for (auto& fn : air_module->getFunctions()) {
        if (fn->getBlocks().size() <= 1 &&
            fn->getBlocks()[0]->getFirst() == nullptr) {
            writer.addExternSymbol(fn->getName());
            continue;
        }
        aurora::MachineFunction mf(*fn, *target_machine);
        aurora::PassManager pm;
        aurora::CodeGenContext::addStandardPasses(pm, *target_machine);
        pm.run(mf);

        writer.addFunction(mf);
    }

    for (auto& gv : air_module->getGlobals()) {
        writer.addGlobal(*gv);
    }

    if (!writer.write(request.output_path)) {
        return base::Result<void>::fail({
            base::ErrorCode::io_error, "failed to write object file"});
    }

    return base::Result<void>::ok();
}

base::Result<AuroraEmitOutput> emit_aurora_asm(const AuroraEmitRequest& request) {
    base::DiagnosticSink diag;
    AuroraEmitter emitter(diag);
    return emitter.emit_asm(request);
}

base::Result<void> emit_aurora_obj(const AuroraEmitRequest& request) {
    base::DiagnosticSink diag;
    AuroraEmitter emitter(diag);
    return emitter.emit_obj(request);
}

} // namespace aurex::backend

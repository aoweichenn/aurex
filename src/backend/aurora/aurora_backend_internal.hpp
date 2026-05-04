#pragma once

#include "aurex/backend/aurora_backend.hpp"
#include "aurex/base/diagnostic.hpp"
#include "aurex/ir/ir.hpp"
#include "aurex/sema/type.hpp"

#include "Aurora/Air/Instruction.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aurora {
class AIRBuilder;
class BasicBlock;
class Function;
class FunctionType;
class GlobalVariable;
class Module;
class Type;
} // namespace aurora

namespace aurex::backend {

class TypeTranslator {
public:
    explicit TypeTranslator(const ir::Module& src_module);

    [[nodiscard]] aurora::Type* translate_type(sema::TypeHandle handle);
    [[nodiscard]] aurora::FunctionType* translate_function_type(
        sema::TypeHandle return_type,
        const std::vector<ir::FunctionParam>& params);
    [[nodiscard]] aurora::Type* translate_record_layout(const ir::RecordLayout& record);
    void translate_all_records();

private:
    [[nodiscard]] aurora::Type* build_type(sema::TypeHandle handle);

    const ir::Module& src_module_;
    std::unordered_map<base::u32, aurora::Type*> type_map_;
    std::unordered_map<base::u32, aurora::Type*> struct_map_;
};

class IrTranslator {
public:
    IrTranslator(const ir::Module& src_module, base::DiagnosticSink& diagnostics);

    [[nodiscard]] std::unique_ptr<aurora::Module> translate();

private:
    void translate_function(const ir::Function& src_fn, base::u32 fn_idx);
    void translate_block(base::u32 block_idx, const ir::BasicBlock& src_block, aurora::Function* dst_fn);
    [[nodiscard]] unsigned translate_value(const ir::Value& value, aurora::AIRBuilder& builder);
    void translate_terminator(const ir::Terminator& terminator, aurora::AIRBuilder& builder);
    void translate_global_constant(const ir::GlobalConstant& constant, base::u32 idx);
    void translate_record(const ir::RecordLayout& record);

    [[nodiscard]] bool is_signed_integer(sema::TypeHandle type) const;
    [[nodiscard]] aurora::ICmpCond map_compare(aurex::ir::BinaryOp op, bool is_signed) const;

    const ir::Module& src_module_;
    base::DiagnosticSink& diag_;
    std::unique_ptr<aurora::Module> dst_module_;
    TypeTranslator type_translator_;

    std::unordered_map<base::u32, unsigned> value_map_;
    std::unordered_map<base::u32, aurora::BasicBlock*> block_map_;
    std::unordered_map<base::u32, aurora::GlobalVariable*> const_map_;
    std::unordered_map<base::u32, aurora::Function*> func_map_;
    std::unordered_map<std::string, aurora::GlobalVariable*> string_map_;
    std::unordered_map<std::string, aurora::Function*> extern_func_map_;
    int string_counter_ = 0;
};

class AuroraEmitter {
public:
    explicit AuroraEmitter(base::DiagnosticSink& diagnostics);

    [[nodiscard]] base::Result<AuroraEmitOutput> emit_asm(const AuroraEmitRequest& request);
    [[nodiscard]] base::Result<void> emit_obj(const AuroraEmitRequest& request);

private:
    base::DiagnosticSink& diag_;
};

} // namespace aurex::backend

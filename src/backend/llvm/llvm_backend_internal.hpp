#pragma once

#include "aurex/backend/llvm_backend.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace llvm {
class BasicBlock;
class DataLayout;
class Function;
class GlobalVariable;
class PHINode;
class Type;
class Value;
} // namespace llvm

namespace aurex::backend {

using aurex::ir::BasicBlock;
using aurex::ir::BinaryOp;
using aurex::ir::BlockId;
using aurex::ir::CastKind;
using aurex::ir::FieldValue;
using aurex::ir::Function;
using aurex::ir::FunctionId;
using aurex::ir::FunctionParam;
using aurex::ir::GlobalConstant;
using aurex::ir::GlobalConstantId;
using aurex::ir::Linkage;
using aurex::ir::Module;
using aurex::ir::PhiInput;
using aurex::ir::RecordField;
using aurex::ir::RecordLayout;
using aurex::ir::Terminator;
using aurex::ir::TerminatorKind;
using aurex::ir::UnaryOp;
using aurex::ir::Value;
using aurex::ir::ValueId;
using aurex::ir::ValueKind;
using aurex::ir::find_global_constant;
using aurex::ir::find_record;
using aurex::ir::is_valid;
using aurex::ir::record_field_index;

class LlvmEmitter final {
public:
    LlvmEmitter(const Module& module, std::string module_name);

    [[nodiscard]] base::Result<LlvmIrOutput> run();

private:
    [[nodiscard]] base::Result<void> configure_target();
    void declare_records();
    void declare_constants();
    void declare_functions();
    void declare_main_wrapper();
    void emit_function(FunctionId function_id, const Function& function);
    void emit_block_phi_nodes(const Function& function, base::u32 block_index);
    void populate_phi_edges();

    [[nodiscard]] llvm::Value* emit_value(ValueId id);
    [[nodiscard]] llvm::Value* emit_runtime_value(const Value& value);
    [[nodiscard]] llvm::Value* emit_constant_ref(const Value& value);
    [[nodiscard]] llvm::Constant* emit_constant_initializer(const Value& value);
    [[nodiscard]] llvm::Constant* emit_constant_cast(const Value& value);
    [[nodiscard]] llvm::Constant* emit_constant_aggregate(const Value& value);
    [[nodiscard]] llvm::Constant* emit_constant_string(const std::string& literal, bool c_string);
    [[nodiscard]] llvm::Value* emit_unary(const Value& value);
    [[nodiscard]] llvm::Value* emit_binary(const Value& value);
    [[nodiscard]] llvm::Value* emit_call(const Value& value);
    [[nodiscard]] llvm::Value* emit_field_addr(const Value& value);
    [[nodiscard]] llvm::Value* emit_index_addr(const Value& value);
    [[nodiscard]] llvm::Value* emit_aggregate(const Value& value);
    [[nodiscard]] llvm::Value* emit_cast(const Value& value);
    [[nodiscard]] llvm::Value* emit_size_of(sema::TypeHandle type);
    [[nodiscard]] llvm::Value* emit_align_of(sema::TypeHandle type);
    void emit_terminator(const Terminator& terminator);

    [[nodiscard]] llvm::Value* integer_constant(sema::TypeHandle type, const std::string& text);
    [[nodiscard]] llvm::Value* emit_string_literal(const std::string& literal, bool c_string);
    [[nodiscard]] llvm::Value* global_string_pointer(const std::string& text, const std::string& name, bool add_null);
    [[nodiscard]] llvm::Type* llvm_type(sema::TypeHandle type);
    [[nodiscard]] llvm::Type* pointee_llvm_type(sema::TypeHandle pointer_type);
    [[nodiscard]] sema::TypeHandle pointee_type(ValueId value) const noexcept;
    [[nodiscard]] bool is_unsigned_integer(sema::TypeHandle type) const noexcept;
    [[nodiscard]] const llvm::DataLayout& data_layout() const;
    [[nodiscard]] llvm::Value* get(ValueId id) const;

    const Module& source_;
    llvm::LLVMContext context_;
    std::unique_ptr<llvm::Module> module_;
    llvm::IRBuilder<> builder_;
    std::unique_ptr<llvm::TargetMachine> target_machine_;
    const Function* current_function_ = nullptr;
    std::unordered_map<base::u32, llvm::StructType*> records_;
    std::unordered_map<base::u32, llvm::GlobalVariable*> constants_;
    std::unordered_map<base::u32, llvm::Function*> functions_;
    std::unordered_map<base::u32, llvm::BasicBlock*> blocks_;
    std::unordered_map<base::u32, llvm::Value*> values_;
    std::unordered_map<base::u32, llvm::PHINode*> pending_phis_;
};

[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& out) noexcept;
[[nodiscard]] std::string decode_string_literal(const std::string& literal, bool has_c_prefix);
[[nodiscard]] std::uint64_t parse_byte_literal(const std::string& literal);

} // namespace aurex::backend

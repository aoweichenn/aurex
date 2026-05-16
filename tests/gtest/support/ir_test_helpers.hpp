#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/sema/type.hpp>
#include <support/test_support.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace aurex::test::irtest {

using ir::AbiCallConv;
using ir::BasicBlock;
using ir::BinaryOp;
using ir::BlockId;
using ir::CastKind;
using ir::Function;
using ir::FunctionId;
using ir::FunctionParam;
using ir::GlobalConstant;
using ir::GlobalConstantId;
using ir::Linkage;
using ir::Module;
using ir::PassPipelineOptions;
using ir::PhiInput;
using ir::RecordField;
using ir::RecordLayout;
using ir::TerminatorKind;
using ir::UnaryOp;
using ir::Value;
using ir::ValueId;
using ir::ValueKind;
using ir::add_block;
using ir::add_global_constant;
using ir::add_value;
using ir::INVALID_BLOCK_ID;
using ir::INVALID_FUNCTION_ID;
using ir::INVALID_GLOBAL_CONSTANT_ID;
using ir::INVALID_VALUE_ID;
using ir::is_valid;
using sema::BuiltinType;
using sema::PointerMutability;
using sema::TypeHandle;

struct FunctionBuilder {
    Module& module;
    Function& function;

    [[nodiscard]] ValueId add(Value value) {
        return add_value(module, std::move(value));
    }

    [[nodiscard]] BlockId block(std::string name) {
        return add_block(function, std::move(name));
    }
};

[[nodiscard]] inline TypeHandle builtin(Module& module, const BuiltinType type) {
    return module.types.builtin(type);
}

[[nodiscard]] inline TypeHandle ptr(Module& module, const PointerMutability mutability, const TypeHandle pointee) {
    return module.types.pointer(mutability, pointee);
}

[[nodiscard]] inline Value integer_value(const TypeHandle type, std::string text) {
    Value value;
    value.kind = ValueKind::integer_literal;
    value.type = type;
    value.text = std::move(text);
    return value;
}

[[nodiscard]] inline Value bool_value(Module& module, const bool value) {
    Value result;
    result.kind = ValueKind::bool_literal;
    result.type = builtin(module, BuiltinType::bool_);
    result.text = value ? "true" : "false";
    return result;
}

[[nodiscard]] inline Function make_function(
    Module&,
    const std::string& name,
    const TypeHandle return_type,
    const Linkage linkage = Linkage::internal,
    const AbiCallConv call_conv = AbiCallConv::aurex
) {
    Function function;
    function.name = name;
    function.symbol = "test_" + name;
    function.return_type = return_type;
    function.linkage = linkage;
    function.call_conv = call_conv;
    return function;
}

[[nodiscard]] inline Function make_return_function(
    Module& module,
    const std::string& name,
    const TypeHandle return_type,
    const Value& return_value
) {
    Function function = make_function(module, name, return_type);
    FunctionBuilder builder {module, function};
    const ValueId value = builder.add(return_value);
    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values.push_back(value);
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = value;
    return function;
}

[[nodiscard]] inline Module make_simple_module() {
    Module module;
    module.functions.push_back(make_return_function(
        module,
        "answer",
        builtin(module, BuiltinType::i32),
        integer_value(builtin(module, BuiltinType::i32), "42")
    ));
    return module;
}

inline void expect_error_contains(const base::Result<void>& result, const std::string_view text) {
    ASSERT_FALSE(result);
    expect_contains(result.error().message, text);
}

} // namespace aurex::test::irtest

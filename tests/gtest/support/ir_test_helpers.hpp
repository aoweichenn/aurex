#pragma once

#include <aurex/frontend/sema/type.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/midend/ir/ir.hpp>
#include <aurex/midend/ir/pass_pipeline.hpp>

#include <support/test_support.hpp>

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::test::irtest {

using ir::AbiCallConv;
using ir::add_block;
using ir::add_function;
using ir::add_global_constant;
using ir::add_record;
using ir::add_value;
using ir::BasicBlock;
using ir::BinaryOp;
using ir::BlockId;
using ir::CastKind;
using ir::FieldValue;
using ir::Function;
using ir::FunctionId;
using ir::FunctionParam;
using ir::GlobalConstantId;
using ir::INVALID_BLOCK_ID;
using ir::INVALID_FUNCTION_ID;
using ir::INVALID_GLOBAL_CONSTANT_ID;
using ir::INVALID_VALUE_ID;
using ir::IrTextId;
using ir::is_valid;
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
using sema::BuiltinType;
using sema::PointerMutability;
using sema::TypeHandle;

struct GlobalConstant {
    std::string_view name;
    std::string_view symbol;
    TypeHandle type = sema::INVALID_TYPE_HANDLE;
    ValueId initializer = INVALID_VALUE_ID;
};

struct FunctionBuilder {
    Module& module;
    Function& function;

    [[nodiscard]] ValueId add(const Value& value) const
    {
        return add_value(module, value);
    }

    [[nodiscard]] BlockId block(const std::string_view name) const
    {
        return add_block(module, function, name);
    }
};

[[nodiscard]] inline IrTextId text_id(Module& module, const std::string_view text)
{
    return module.intern(text);
}

inline void set_name(Module& module, Value& value, const std::string_view name)
{
    value.name = text_id(module, name);
}

inline void set_text(Module& module, Value& value, const std::string_view text)
{
    value.text = text_id(module, text);
}

inline void set_symbol(Module& module, Function& function, const std::string_view symbol)
{
    function.symbol = text_id(module, symbol);
}

[[nodiscard]] inline FunctionParam function_param(Module& module, const std::string_view name, const TypeHandle type)
{
    return FunctionParam{text_id(module, name), type};
}

[[nodiscard]] inline RecordField record_field(Module& module, const std::string_view name, const TypeHandle type)
{
    return RecordField{text_id(module, name), type};
}

[[nodiscard]] inline FieldValue field_value(Module& module, const std::string_view name, const ValueId value)
{
    return FieldValue{text_id(module, name), value};
}

[[nodiscard]] inline GlobalConstant global_constant(Module& module, const std::string_view name,
    const std::string_view symbol, const TypeHandle type, const ValueId initializer)
{
    static_cast<void>(module);
    return GlobalConstant{name, symbol, type, initializer};
}

[[nodiscard]] inline ir::GlobalConstant intern_global_constant(Module& module, const GlobalConstant constant)
{
    return ir::GlobalConstant{
        text_id(module, constant.name),
        text_id(module, constant.symbol),
        constant.type,
        constant.initializer,
    };
}

[[nodiscard]] inline GlobalConstantId add_global_constant(Module& module, const GlobalConstant constant)
{
    return ir::add_global_constant(module, intern_global_constant(module, constant));
}

[[nodiscard]] inline RecordLayout record_layout(Module& module, const TypeHandle type, const std::string_view name,
    const std::string_view symbol, const bool is_opaque)
{
    RecordLayout record = module.make_record_layout();
    record.type = type;
    record.name = text_id(module, name);
    record.symbol = text_id(module, symbol);
    record.is_opaque = is_opaque;
    return record;
}

inline void append_function(Module& module, const Function& function)
{
    static_cast<void>(add_function(module, function));
}

inline void append_record(Module& module, const RecordLayout& record)
{
    static_cast<void>(add_record(module, record));
}

inline void append_record_with_fields(Module& module, const TypeHandle type, const std::string_view name,
    const std::string_view symbol, const bool is_opaque, std::initializer_list<RecordField> fields)
{
    RecordLayout record = record_layout(module, type, name, symbol, is_opaque);
    record.fields.reserve(fields.size());
    record.fields.insert(record.fields.end(), fields.begin(), fields.end());
    append_record(module, record);
}

template <typename T>
inline void assign_ir_vector(ir::IrVector<T>& target, std::initializer_list<T> values)
{
    target.clear();
    target.reserve(values.size());
    target.insert(target.end(), values.begin(), values.end());
}

template <typename T>
inline void assign_ir_vector(ir::IrVector<T>& target, const std::vector<T>& values)
{
    target.clear();
    target.reserve(values.size());
    target.insert(target.end(), values.begin(), values.end());
}

[[nodiscard]] inline TypeHandle builtin(Module& module, const BuiltinType type)
{
    return module.types.builtin(type);
}

[[nodiscard]] inline TypeHandle ptr(Module& module, const PointerMutability mutability, const TypeHandle pointee)
{
    return module.types.pointer(mutability, pointee);
}

[[nodiscard]] inline Value integer_value(Module& module, const TypeHandle type, const std::string_view text)
{
    Value value = module.make_value();
    value.kind = ValueKind::integer_literal;
    value.type = type;
    set_text(module, value, text);
    return value;
}

[[nodiscard]] inline Value bool_value(Module& module, const bool value)
{
    Value result = module.make_value();
    result.kind = ValueKind::bool_literal;
    result.type = builtin(module, BuiltinType::bool_);
    set_text(module, result, value ? "true" : "false");
    return result;
}

[[nodiscard]] inline Function make_function(Module& module, const std::string& name, const TypeHandle return_type,
    const Linkage linkage = Linkage::internal, const AbiCallConv call_conv = AbiCallConv::aurex)
{
    Function function = module.make_function();
    function.name = text_id(module, name);
    function.symbol = text_id(module, "test_" + name);
    function.return_type = return_type;
    function.linkage = linkage;
    function.call_conv = call_conv;
    return function;
}

[[nodiscard]] inline Function make_return_function(
    Module& module, const std::string& name, const TypeHandle return_type, const Value& return_value)
{
    Function function = make_function(module, name, return_type);
    FunctionBuilder builder{module, function};
    const ValueId value = builder.add(return_value);
    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values.push_back(value);
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = value;
    return function;
}

[[nodiscard]] inline Module make_simple_module()
{
    Module module;
    append_function(module,
        make_return_function(module, "answer", builtin(module, BuiltinType::i32),
            integer_value(module, builtin(module, BuiltinType::i32), "42")));
    return module;
}

inline void expect_error_contains(const base::Result<void>& result, const std::string_view text)
{
    ASSERT_FALSE(result);
    expect_contains(result.error().message, text);
}

} // namespace aurex::test::irtest

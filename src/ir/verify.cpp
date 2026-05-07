#include "aurex/ir/verify.hpp"

#include <sstream>
#include <string>
#include <unordered_set>

namespace aurex::ir {

namespace {

class Verifier final {
public:
    explicit Verifier(const Module& module) noexcept : module_(module) {}

    [[nodiscard]] base::Result<void> run() {
        std::unordered_set<base::u32> constant_stack;
        for (base::u32 i = 0; i < module_.constants.size(); ++i) {
            verify_constant(GlobalConstantId {i}, constant_stack);
        }
        for (base::usize i = 0; i < module_.functions.size(); ++i) {
            verify_function(FunctionId {static_cast<base::u32>(i)}, module_.functions[i]);
        }
        if (!errors_.empty()) {
            std::ostringstream out;
            out << "IR verification failed:";
            for (const std::string& error : errors_) {
                out << "\n  - " << error;
            }
            return base::Result<void>::fail({base::ErrorCode::internal_error, out.str()});
        }
        return base::Result<void>::ok();
    }

private:
    void verify_constant(const GlobalConstantId id, std::unordered_set<base::u32>& constant_stack) {
        const GlobalConstant* constant = find_global_constant(module_, id);
        if (constant == nullptr) {
            fail("constant id is invalid");
            return;
        }
        if (!constant_stack.insert(id.value).second) {
            fail("cyclic constant definition");
            return;
        }
        verify_type(constant->type, "constant type");
        verify_constant_value(constant->initializer, constant->type, constant_stack);
        constant_stack.erase(id.value);
    }

    void verify_constant_value(
        const ValueId initializer,
        const sema::TypeHandle expected_type,
        std::unordered_set<base::u32>& constant_stack
    ) {
        const Value* value = get(initializer);
        if (value == nullptr) {
            fail("constant initializer value id is invalid");
            return;
        }
        if (!module_.types.same(value->type, expected_type)) {
            fail("constant initializer type mismatch");
        }
        switch (value->kind) {
        case ValueKind::integer_literal:
        case ValueKind::bool_literal:
        case ValueKind::byte_literal:
        case ValueKind::undef:
        case ValueKind::null_literal:
        case ValueKind::string_literal:
        case ValueKind::c_string_literal:
        case ValueKind::size_of:
        case ValueKind::align_of:
            verify_type(value->type, "constant value type");
            break;
        case ValueKind::constant_ref: {
            verify_constant_ref(*value);
            const GlobalConstant* constant = find_global_constant(module_, value->constant);
            if (constant == nullptr) {
                fail("constant reference id is invalid");
                return;
            }
            if (!constant_stack.insert(value->constant.value).second) {
                fail("cyclic constant reference");
                return;
            }
            verify_constant_value(constant->initializer, constant->type, constant_stack);
            constant_stack.erase(value->constant.value);
            break;
        }
        case ValueKind::aggregate: {
            verify_constant_aggregate(*value, constant_stack);
            break;
        }
        case ValueKind::unary: {
            verify_value_id(value->lhs, "constant unary operand");
            verify_type(value->type, "constant unary result");
            if (value->unary_op == UnaryOp::address_of || value->unary_op == UnaryOp::dereference) {
                fail("constant initializer contains a runtime-only unary operator");
                break;
            }
            const Value* operand = get(value->lhs);
            if (operand != nullptr) {
                verify_constant_value(value->lhs, operand->type, constant_stack);
            }
            break;
        }
        case ValueKind::binary: {
            verify_binary(*value);
            const Value* lhs = get(value->lhs);
            if (lhs != nullptr) {
                verify_constant_value(value->lhs, lhs->type, constant_stack);
            }
            const Value* rhs = get(value->rhs);
            if (rhs != nullptr) {
                verify_constant_value(value->rhs, rhs->type, constant_stack);
            }
            break;
        }
        case ValueKind::cast: {
            verify_value_id(value->lhs, "cast operand");
            verify_type(value->type, "cast result");
            verify_type(value->target_type, "cast target");
            const Value* operand = get(value->lhs);
            if (operand != nullptr) {
                verify_constant_value(value->lhs, operand->type, constant_stack);
            }
            break;
        }
        default:
            fail("constant initializer is not compile-time constant");
            break;
        }
    }

    void verify_constant_aggregate(const Value& value, std::unordered_set<base::u32>& constant_stack) {
        verify_type(value.type, "aggregate result");
        const RecordLayout* record = find_record(module_, value.type);
        if (record == nullptr) {
            fail("aggregate result is not a record");
            return;
        }
        std::unordered_set<std::string> seen;
        for (const FieldValue& field : value.fields) {
            if (!seen.insert(field.name).second) {
                fail("duplicate aggregate field " + field.name);
            }
            const RecordField* expected = find_record_field(module_, value.type, field.name);
            if (expected == nullptr) {
                fail("unknown aggregate field " + field.name);
                continue;
            }
            verify_constant_value(field.value, expected->type, constant_stack);
        }
        if (seen.size() != record->fields.size()) {
            fail("aggregate constant does not initialize every field");
        }
    }

    void verify_function(const FunctionId function_id, const Function& function) {
        verify_function_symbol(function_id, function);
        if (function.linkage == Linkage::extern_c && function.call_conv != AbiCallConv::c) {
            fail("extern function @" + function.symbol + " must use C ABI");
        }
        if (function.linkage == Linkage::export_c && function.call_conv != AbiCallConv::c) {
            fail("exported function @" + function.symbol + " must use C ABI");
        }
        if (function.is_entry) {
            if (function.linkage != Linkage::internal) {
                fail("entry function @" + function.symbol + " must use internal linkage");
            }
            if (function.call_conv != AbiCallConv::aurex) {
                fail("entry function @" + function.symbol + " must use Aurex ABI");
            }
            const bool no_params = function.signature_params.empty();
            bool argc_argv_params = false;
            if (function.signature_params.size() == 2 &&
                module_.types.same(function.signature_params[0].type, module_.types.builtin(sema::BuiltinType::i32))) {
                const sema::TypeHandle argv_type = function.signature_params[1].type;
                if (module_.types.is_pointer(argv_type)) {
                    const sema::TypeInfo& outer = module_.types.get(argv_type);
                    const sema::TypeHandle outer_pointee = outer.pointee;
                    argc_argv_params = outer.pointer_mutability == sema::PointerMutability::mut &&
                        module_.types.is_pointer(outer_pointee) &&
                        module_.types.get(outer_pointee).pointer_mutability == sema::PointerMutability::mut &&
                        module_.types.same(module_.types.get(outer_pointee).pointee, module_.types.builtin(sema::BuiltinType::u8));
                }
            }
            if (!no_params && !argc_argv_params) {
                fail("entry function @" + function.symbol + " must use no parameters or argc/argv parameters");
            }
            if (!module_.types.same(function.return_type, module_.types.builtin(sema::BuiltinType::i32)) &&
                !module_.types.same(function.return_type, module_.types.builtin(sema::BuiltinType::void_))) {
                fail("entry function @" + function.symbol + " must return i32 or void");
            }
        }
        if (function.linkage != Linkage::extern_c && function.blocks.empty()) {
            fail("function @" + function.symbol + " has no blocks");
        }
        if (function.linkage == Linkage::extern_c && !function.blocks.empty()) {
            fail("extern function @" + function.symbol + " must not have blocks");
        }
        if (function.signature_params.size() != function.param_values.size() && !function.blocks.empty()) {
            fail("function @" + function.symbol + " parameter signature/value count mismatch");
        }
        for (base::usize i = 0; i < function.param_values.size(); ++i) {
            const Value* value = get(function.param_values[i]);
            if (value == nullptr || value->kind != ValueKind::param) {
                fail("function @" + function.symbol + " has a non-param value in parameter list");
                continue;
            }
            if (!module_.types.same(value->type, function.signature_params[i].type)) {
                fail("function @" + function.symbol + " parameter type mismatch");
            }
        }

        for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
            verify_block(function, BlockId {block_index}, function.blocks[block_index]);
        }
    }

    void verify_function_symbol(const FunctionId function_id, const Function& function) {
        if (function.symbol.empty()) {
            fail("function has an empty ABI symbol");
            return;
        }
        for (base::u32 i = 0; i < module_.functions.size(); ++i) {
            if (i == function_id.value) {
                continue;
            }
            const Function& other = module_.functions[i];
            if (other.symbol != function.symbol) {
                continue;
            }
            if (function.linkage != Linkage::extern_c || other.linkage != Linkage::extern_c) {
                fail("duplicate non-extern function ABI symbol @" + function.symbol);
                return;
            }
            if (!same_signature(function, other)) {
                fail("extern function @" + function.symbol + " has inconsistent declarations");
                return;
            }
        }
    }

    void verify_block(const Function& function, const BlockId block_id, const BasicBlock& block) {
        for (const ValueId value_id : block.values) {
            verify_value(function, block_id, value_id);
        }
        switch (block.terminator.kind) {
        case TerminatorKind::none:
            fail("block ^" + block.name + " has no terminator");
            break;
        case TerminatorKind::branch:
            verify_block_id(function, block.terminator.target, "branch target");
            break;
        case TerminatorKind::cond_branch:
            verify_value_type(block.terminator.condition, module_.types.builtin(sema::BuiltinType::bool_), "branch condition");
            verify_block_id(function, block.terminator.then_target, "then target");
            verify_block_id(function, block.terminator.else_target, "else target");
            break;
        case TerminatorKind::return_: {
            if (module_.types.is_void(function.return_type)) {
                if (is_valid(block.terminator.value)) {
                    fail("void function @" + function.symbol + " returns a value");
                }
            } else {
                verify_value_type(block.terminator.value, function.return_type, "return value");
            }
            break;
        }
        }
    }

    void verify_value(const Function& function, const BlockId block_id, const ValueId value_id) {
        static_cast<void>(block_id);
        const Value* value = get(value_id);
        if (value == nullptr) {
            fail("invalid value in block");
            return;
        }

        switch (value->kind) {
        case ValueKind::param:
        case ValueKind::integer_literal:
        case ValueKind::bool_literal:
        case ValueKind::null_literal:
        case ValueKind::string_literal:
        case ValueKind::c_string_literal:
        case ValueKind::byte_literal:
        case ValueKind::undef:
        case ValueKind::alloca:
        case ValueKind::size_of:
        case ValueKind::align_of:
            verify_type(value->type, "value type");
            break;
        case ValueKind::constant_ref:
            verify_constant_ref(*value);
            break;
        case ValueKind::load:
            verify_pointer_value(value->object, "load object");
            verify_type(value->type, "load result");
            break;
        case ValueKind::store:
            verify_pointer_value(value->object, "store target");
            verify_value_id(value->lhs, "store source");
            verify_type(value->type, "store result");
            if (!module_.types.is_void(value->type)) {
                fail("store result must be void");
            }
            if (const sema::TypeHandle target = pointee_type(value->object);
                sema::is_valid(target)) {
                verify_value_type(value->lhs, target, "store source");
            }
            break;
        case ValueKind::unary:
            verify_unary(*value);
            break;
        case ValueKind::binary:
            verify_binary(*value);
            break;
        case ValueKind::phi:
            verify_phi(function, block_id, *value);
            break;
        case ValueKind::call:
            verify_call(*value);
            break;
        case ValueKind::field_addr:
            verify_field_addr(*value);
            break;
        case ValueKind::index_addr:
            verify_pointer_value(value->object, "index object");
            verify_value_id(value->index, "index");
            verify_type(value->type, "index result");
            break;
        case ValueKind::aggregate:
            verify_aggregate(*value);
            break;
        case ValueKind::cast:
            verify_value_id(value->lhs, "cast operand");
            verify_type(value->type, "cast result");
            verify_type(value->target_type, "cast target");
            if (!module_.types.same(value->type, value->target_type)) {
                fail("cast result type must match cast target type");
            }
            break;
        }
    }

    void verify_unary(const Value& value) {
        verify_value_id(value.lhs, "unary operand");
        verify_type(value.type, "unary result");
        const Value* operand = get(value.lhs);
        if (operand == nullptr) {
            return;
        }
        if (!sema::is_valid(operand->type)) {
            fail("unary operand type is invalid");
            return;
        }
        switch (value.unary_op) {
        case UnaryOp::logical_not:
            if (!module_.types.is_bool(operand->type) || !module_.types.is_bool(value.type)) {
                fail("logical unary operator requires bool operand and result");
            }
            break;
        case UnaryOp::numeric_negate:
            if (!module_.types.same(operand->type, value.type) ||
                (!module_.types.is_integer(value.type) && !module_.types.is_float(value.type))) {
                fail("numeric unary operator requires matching numeric operand and result");
            }
            break;
        case UnaryOp::bitwise_not:
            if (!module_.types.same(operand->type, value.type) || !module_.types.is_integer(value.type)) {
                fail("bitwise unary operator requires matching integer operand and result");
            }
            break;
        case UnaryOp::address_of:
        case UnaryOp::dereference:
            if (!module_.types.same(operand->type, value.type)) {
                fail("address/dereference unary passthrough type mismatch");
            }
            break;
        }
    }

    void verify_binary(const Value& value) {
        verify_value_id(value.lhs, "binary lhs");
        verify_value_id(value.rhs, "binary rhs");
        verify_type(value.type, "binary result");
        const Value* lhs = get(value.lhs);
        const Value* rhs = get(value.rhs);
        if (lhs == nullptr || rhs == nullptr) {
            return;
        }
        if (!module_.types.same(lhs->type, rhs->type)) {
            fail("binary operand type mismatch");
            return;
        }

        const sema::TypeHandle operand_type = lhs->type;
        if (!sema::is_valid(operand_type)) {
            fail("binary operand type is invalid");
            return;
        }
        switch (value.binary_op) {
        case BinaryOp::less:
        case BinaryOp::less_equal:
        case BinaryOp::greater:
        case BinaryOp::greater_equal:
            if (!module_.types.is_bool(value.type)) {
                fail("comparison binary result must be bool");
            }
            if (!module_.types.is_integer(operand_type) && !module_.types.is_float(operand_type)) {
                fail("comparison binary operands must be numeric");
            }
            break;
        case BinaryOp::equal:
        case BinaryOp::not_equal:
            if (!module_.types.is_bool(value.type)) {
                fail("equality binary result must be bool");
            }
            if (!module_.types.is_bool(operand_type) &&
                !module_.types.is_integer(operand_type) &&
                !module_.types.is_float(operand_type) &&
                !module_.types.is_pointer(operand_type)) {
                const sema::TypeInfo& info = module_.types.get(operand_type);
                if (info.kind != sema::TypeKind::enum_ ||
                    sema::is_valid(info.enum_payload_storage)) {
                    fail("equality binary operands must be scalar");
                }
            }
            break;
        case BinaryOp::logical_and:
        case BinaryOp::logical_or:
            if (!module_.types.is_bool(value.type) || !module_.types.is_bool(operand_type)) {
                fail("logical binary operator requires bool operands and result");
            }
            break;
        case BinaryOp::bit_and:
        case BinaryOp::bit_xor:
        case BinaryOp::bit_or:
        case BinaryOp::shl:
        case BinaryOp::shr:
        case BinaryOp::mod:
            if (!module_.types.same(value.type, operand_type)) {
                fail("integer binary result must match operand type");
            }
            if (!module_.types.is_integer(operand_type)) {
                fail("integer binary operator requires integer operands");
            }
            break;
        case BinaryOp::add:
        case BinaryOp::sub:
        case BinaryOp::mul:
        case BinaryOp::div:
            if (!module_.types.same(value.type, operand_type)) {
                fail("numeric binary result must match operand type");
            }
            if (!module_.types.is_integer(operand_type) && !module_.types.is_float(operand_type)) {
                fail("numeric binary operator requires numeric operands");
            }
            break;
        }
    }

    void verify_phi(const Function& function, const BlockId block_id, const Value& value) {
        verify_type(value.type, "phi result");
        if (value.incoming.empty()) {
            fail("phi has no incoming values");
        }
        std::unordered_set<base::u32> incoming_predecessors;
        for (const PhiInput& incoming : value.incoming) {
            verify_block_id(function, incoming.predecessor, "phi predecessor");
            verify_value_type(incoming.value, value.type, "phi incoming");
            if (is_valid(incoming.predecessor) &&
                incoming.predecessor.value < function.blocks.size() &&
                !incoming_predecessors.insert(incoming.predecessor.value).second) {
                fail("phi has duplicate incoming predecessor");
            }
            if (is_valid(incoming.predecessor) &&
                incoming.predecessor.value < function.blocks.size() &&
                !block_has_edge_to(function.blocks[incoming.predecessor.value], block_id)) {
                fail("phi predecessor has no edge to block");
            }
        }
        for (base::u32 predecessor = 0; predecessor < function.blocks.size(); ++predecessor) {
            if (block_has_edge_to(function.blocks[predecessor], block_id) &&
                !incoming_predecessors.contains(predecessor)) {
                fail("phi is missing incoming predecessor");
            }
        }
    }

    void verify_call(const Value& value) {
        verify_type(value.type, "call result");
        if (!is_valid(value.call_target)) {
            fail(value.name.empty() ? "call has no target symbol" : "call target @" + value.name + " is unresolved");
            return;
        }
        if (value.call_target.value >= module_.functions.size()) {
            fail("call target out of range");
            return;
        }
        const Function& target = module_.functions[value.call_target.value];
        if (target.is_variadic ? value.args.size() < target.signature_params.size() : value.args.size() != target.signature_params.size()) {
            fail("call to @" + target.symbol + " has wrong argument count");
            return;
        }
        for (base::usize i = 0; i < target.signature_params.size(); ++i) {
            verify_value_type(value.args[i], target.signature_params[i].type, "call argument");
        }
        for (base::usize i = target.signature_params.size(); i < value.args.size(); ++i) {
            const Value* arg = get(value.args[i]);
            if (arg == nullptr) {
                fail("call argument out of range");
                continue;
            }
            verify_type(arg->type, "variadic call argument");
        }
        if (!module_.types.same(value.type, target.return_type)) {
            fail("call to @" + target.symbol + " result type mismatch");
        }
    }

    [[nodiscard]] bool same_signature(const Function& lhs, const Function& rhs) const noexcept {
        if (!module_.types.same(lhs.return_type, rhs.return_type)) {
            return false;
        }
        if (lhs.is_variadic != rhs.is_variadic || lhs.signature_params.size() != rhs.signature_params.size()) {
            return false;
        }
        for (base::usize i = 0; i < lhs.signature_params.size(); ++i) {
            if (!module_.types.same(lhs.signature_params[i].type, rhs.signature_params[i].type)) {
                return false;
            }
        }
        return true;
    }

    void verify_constant_ref(const Value& value) {
        verify_type(value.type, "constant reference type");
        const GlobalConstant* constant = find_global_constant(module_, value.constant);
        if (constant == nullptr) {
            fail("constant reference id is invalid");
            return;
        }
        if (!module_.types.same(value.type, constant->type)) {
            fail("constant reference type mismatch");
        }
    }

    void verify_field_addr(const Value& value) {
        verify_pointer_value(value.object, "field object");
        verify_type(value.type, "field address type");
        const sema::TypeHandle object_type = pointee_type(value.object);
        const sema::TypeHandle record_type = module_.types.is_pointer(object_type)
            ? module_.types.get(object_type).pointee
            : object_type;
        if (find_record_field(module_, record_type, value.name) == nullptr) {
            fail("unknown field '" + value.name + "'");
        }
    }

    void verify_aggregate(const Value& value) {
        verify_type(value.type, "aggregate result");
        const RecordLayout* record = find_record(module_, value.type);
        if (record == nullptr) {
            fail("aggregate result is not a record");
            return;
        }
        std::unordered_set<std::string> seen;
        for (const FieldValue& field : value.fields) {
            if (!seen.insert(field.name).second) {
                fail("duplicate aggregate field " + field.name);
            }
            const RecordField* expected = find_record_field(module_, value.type, field.name);
            if (expected == nullptr) {
                fail("unknown aggregate field " + field.name);
                continue;
            }
            verify_value_type(field.value, expected->type, "aggregate field");
        }
        if (seen.size() != record->fields.size()) {
            fail("aggregate does not initialize every field");
        }
    }

    void verify_block_id(const Function& function, const BlockId block, const std::string& context) {
        if (!is_valid(block) || block.value >= function.blocks.size()) {
            fail(context + " block id is invalid");
        }
    }

    [[nodiscard]] bool block_has_edge_to(const BasicBlock& block, const BlockId target) const noexcept {
        switch (block.terminator.kind) {
        case TerminatorKind::branch:
            return block.terminator.target.value == target.value;
        case TerminatorKind::cond_branch:
            return block.terminator.then_target.value == target.value ||
                   block.terminator.else_target.value == target.value;
        case TerminatorKind::none:
        case TerminatorKind::return_:
            return false;
        }
        return false;
    }

    void verify_value_id(const ValueId value, const std::string& context) {
        if (get(value) == nullptr) {
            fail(context + " value id is invalid");
        }
    }

    void verify_type(const sema::TypeHandle type, const std::string& context) {
        if (!sema::is_valid(type)) {
            fail(context + " is invalid");
        }
    }

    void verify_value_type(const ValueId value_id, const sema::TypeHandle expected, const std::string& context) {
        const Value* value = get(value_id);
        if (value == nullptr) {
            fail(context + " value id is invalid");
            return;
        }
        if (!module_.types.same(value->type, expected)) {
            fail(context + " type mismatch");
        }
    }

    void verify_pointer_value(const ValueId value_id, const std::string& context) {
        const Value* value = get(value_id);
        if (value == nullptr) {
            fail(context + " value id is invalid");
            return;
        }
        if (!module_.types.is_pointer(value->type)) {
            fail(context + " is not a pointer");
        }
    }

    [[nodiscard]] sema::TypeHandle pointee_type(const ValueId value_id) const noexcept {
        const Value* value = get(value_id);
        if (value == nullptr || !module_.types.is_pointer(value->type)) {
            return sema::invalid_type_handle;
        }
        return module_.types.get(value->type).pointee;
    }

    [[nodiscard]] const Value* get(const ValueId value) const noexcept {
        if (!is_valid(value) || value.value >= module_.values.size()) {
            return nullptr;
        }
        return &module_.values[value.value];
    }

    void fail(std::string message) {
        errors_.push_back(std::move(message));
    }

    const Module& module_;
    std::vector<std::string> errors_;
};

} // namespace

base::Result<void> verify_module(const Module& module) {
    Verifier verifier(module);
    return verifier.run();
}

} // namespace aurex::ir

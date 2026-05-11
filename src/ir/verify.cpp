#include <aurex/ir/verify.hpp>

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::ir {

namespace {

enum class ConstantWorkItemKind {
    value,
    constant_exit,
};

struct ConstantWorkItem {
    ConstantWorkItemKind kind = ConstantWorkItemKind::value;
    ValueId value_id = INVALID_VALUE_ID;
    sema::TypeHandle expected_type = sema::INVALID_TYPE_HANDLE;
    GlobalConstantId constant_id = INVALID_GLOBAL_CONSTANT_ID;
};

[[nodiscard]] ConstantWorkItem make_constant_value_work_item(
    const ValueId value_id,
    const sema::TypeHandle expected_type
) noexcept {
    return ConstantWorkItem {
        ConstantWorkItemKind::value,
        value_id,
        expected_type,
        INVALID_GLOBAL_CONSTANT_ID,
    };
}

[[nodiscard]] ConstantWorkItem make_constant_exit_work_item(const GlobalConstantId constant_id) noexcept {
    return ConstantWorkItem {
        ConstantWorkItemKind::constant_exit,
        INVALID_VALUE_ID,
        sema::INVALID_TYPE_HANDLE,
        constant_id,
    };
}

struct StorageTypeWorkItem {
    sema::TypeHandle type = sema::INVALID_TYPE_HANDLE;
    std::string context;
};

constexpr base::usize IR_VERIFIER_ENTRY_ARGC_ARGV_PARAM_COUNT = 2;
constexpr base::usize IR_VERIFIER_STR_FROM_BYTES_UNCHECKED_ARGUMENT_COUNT = 2;

class Verifier final {
public:
    explicit Verifier(const Module& module) noexcept : module_(module) {}

    [[nodiscard]] base::Result<void> run() {
        std::unordered_set<base::u32> constant_stack;
        for (base::u32 i = 0; i < this->module_.constants.size(); ++i) {
            this->verify_constant(GlobalConstantId {i}, constant_stack);
        }
        for (base::usize i = 0; i < this->module_.functions.size(); ++i) {
            this->verify_function(FunctionId {static_cast<base::u32>(i)}, this->module_.functions[i]);
        }
        if (!this->errors_.empty()) {
            std::ostringstream out;
            out << "IR verification failed:";
            for (const std::string& error : this->errors_) {
                out << "\n  - " << error;
            }
            return base::Result<void>::fail({base::ErrorCode::internal_error, out.str()});
        }
        return base::Result<void>::ok();
    }

private:
    void verify_constant(const GlobalConstantId id, std::unordered_set<base::u32>& constant_stack) {
        const GlobalConstant* constant = find_global_constant(this->module_, id);
        if (constant == nullptr) {
            this->fail("constant id is invalid");
            return;
        }
        if (!constant_stack.insert(id.value).second) {
            this->fail("cyclic constant definition");
            return;
        }
        this->verify_type(constant->type, "constant type");
        this->verify_constant_value(constant->initializer, constant->type, constant_stack);
        constant_stack.erase(id.value);
    }

    void verify_constant_value(
        const ValueId initializer,
        const sema::TypeHandle expected_type,
        std::unordered_set<base::u32>& constant_stack
    ) {
        // Use an explicit worklist so nested constant initializers stay iterative.
        std::vector<ConstantWorkItem> worklist;
        worklist.push_back(make_constant_value_work_item(initializer, expected_type));
        while (!worklist.empty()) {
            const ConstantWorkItem item = worklist.back();
            worklist.pop_back();
            if (item.kind == ConstantWorkItemKind::constant_exit) {
                constant_stack.erase(item.constant_id.value);
                continue;
            }
            this->verify_constant_value_item(item.value_id, item.expected_type, constant_stack, worklist);
        }
    }

    void verify_constant_value_item(
        const ValueId value_id,
        const sema::TypeHandle expected_type,
        std::unordered_set<base::u32>& constant_stack,
        std::vector<ConstantWorkItem>& worklist
    ) {
        const Value* value = this->get(value_id);
        if (value == nullptr) {
            this->fail("constant initializer value id is invalid");
            return;
        }
        if (!this->module_.types.same(value->type, expected_type)) {
            this->fail("constant initializer type mismatch");
        }
        switch (value->kind) {
        case ValueKind::integer_literal:
        case ValueKind::float_literal:
        case ValueKind::bool_literal:
        case ValueKind::byte_literal:
        case ValueKind::undef:
        case ValueKind::null_literal:
        case ValueKind::string_literal:
        case ValueKind::c_string_literal:
        case ValueKind::size_of:
        case ValueKind::align_of:
            if (value->kind == ValueKind::size_of || value->kind == ValueKind::align_of) {
                this->verify_size_or_align(*value);
            } else {
                this->verify_literal_value(*value);
            }
            break;
        case ValueKind::constant_ref: {
            const GlobalConstant* constant = this->verify_constant_ref(*value);
            if (constant == nullptr) {
                return;
            }
            if (!constant_stack.insert(value->constant.value).second) {
                this->fail("cyclic constant reference");
                return;
            }
            worklist.push_back(make_constant_exit_work_item(value->constant));
            worklist.push_back(make_constant_value_work_item(constant->initializer, constant->type));
            break;
        }
        case ValueKind::aggregate: {
            this->verify_constant_aggregate(*value, worklist);
            break;
        }
        case ValueKind::unary: {
            this->verify_value_id(value->lhs, "constant unary operand");
            this->verify_type(value->type, "constant unary result");
            if (value->unary_op == UnaryOp::address_of || value->unary_op == UnaryOp::dereference) {
                this->fail("constant initializer contains a runtime-only unary operator");
                break;
            }
            const Value* operand = this->get(value->lhs);
            if (operand != nullptr) {
                worklist.push_back(make_constant_value_work_item(value->lhs, operand->type));
            }
            break;
        }
        case ValueKind::binary: {
            this->verify_binary(*value);
            const Value* rhs = this->get(value->rhs);
            if (rhs != nullptr) {
                worklist.push_back(make_constant_value_work_item(value->rhs, rhs->type));
            }
            const Value* lhs = this->get(value->lhs);
            if (lhs != nullptr) {
                worklist.push_back(make_constant_value_work_item(value->lhs, lhs->type));
            }
            break;
        }
        case ValueKind::cast: {
            this->verify_value_id(value->lhs, "cast operand");
            this->verify_type(value->type, "cast result");
            this->verify_type(value->target_type, "cast target");
            const Value* operand = this->get(value->lhs);
            if (operand != nullptr) {
                worklist.push_back(make_constant_value_work_item(value->lhs, operand->type));
            }
            break;
        }
        default:
            this->fail("constant initializer is not compile-time constant");
            break;
        }
    }

    void verify_constant_aggregate(const Value& value, std::vector<ConstantWorkItem>& worklist) {
        this->verify_type(value.type, "aggregate result");
        const RecordLayout* record = find_record(this->module_, value.type);
        if (record == nullptr) {
            this->fail("aggregate result is not a record");
            return;
        }
        std::unordered_set<std::string> seen;
        for (auto field = value.fields.rbegin(); field != value.fields.rend(); ++field) {
            if (!seen.insert(field->name).second) {
                this->fail("duplicate aggregate field " + field->name);
            }
            const RecordField* expected = find_record_field(this->module_, value.type, field->name);
            if (expected == nullptr) {
                this->fail("unknown aggregate field " + field->name);
                continue;
            }
            worklist.push_back(make_constant_value_work_item(field->value, expected->type));
        }
        if (seen.size() != record->fields.size()) {
            this->fail("aggregate constant does not initialize every field");
        }
    }

    void verify_function(const FunctionId function_id, const Function& function) {
        this->verify_function_symbol(function_id, function);
        if (function.linkage == Linkage::extern_c && function.call_conv != AbiCallConv::c) {
            this->fail("extern function @" + function.symbol + " must use C ABI");
        }
        if (function.linkage == Linkage::export_c && function.call_conv != AbiCallConv::c) {
            this->fail("exported function @" + function.symbol + " must use C ABI");
        }
        if (function.is_entry) {
            if (function.linkage != Linkage::internal) {
                this->fail("entry function @" + function.symbol + " must use internal linkage");
            }
            if (function.call_conv != AbiCallConv::aurex) {
                this->fail("entry function @" + function.symbol + " must use Aurex ABI");
            }
            const bool no_params = function.signature_params.empty();
            bool argc_argv_params = false;
            if (function.signature_params.size() == IR_VERIFIER_ENTRY_ARGC_ARGV_PARAM_COUNT &&
                this->module_.types.same(function.signature_params[0].type, this->module_.types.builtin(sema::BuiltinType::i32))) {
                const sema::TypeHandle argv_type = function.signature_params[1].type;
                if (this->module_.types.is_pointer(argv_type)) {
                    const sema::TypeInfo& outer = this->module_.types.get(argv_type);
                    const sema::TypeHandle outer_pointee = outer.pointee;
                    argc_argv_params = outer.pointer_mutability == sema::PointerMutability::mut &&
                        this->module_.types.is_pointer(outer_pointee) &&
                        this->module_.types.get(outer_pointee).pointer_mutability == sema::PointerMutability::mut &&
                        this->module_.types.same(this->module_.types.get(outer_pointee).pointee, this->module_.types.builtin(sema::BuiltinType::u8));
                }
            }
            if (!no_params && !argc_argv_params) {
                this->fail("entry function @" + function.symbol + " must use no parameters or argc/argv parameters");
            }
            if (!this->module_.types.same(function.return_type, this->module_.types.builtin(sema::BuiltinType::i32)) &&
                !this->module_.types.same(function.return_type, this->module_.types.builtin(sema::BuiltinType::void_))) {
                this->fail("entry function @" + function.symbol + " must return i32 or void");
            }
        }
        if (function.linkage != Linkage::extern_c && function.blocks.empty()) {
            this->fail("function @" + function.symbol + " has no blocks");
        }
        if (function.linkage == Linkage::extern_c && !function.blocks.empty()) {
            this->fail("extern function @" + function.symbol + " must not have blocks");
        }
        if (function.signature_params.size() != function.param_values.size() && !function.blocks.empty()) {
            this->fail("function @" + function.symbol + " parameter signature/value count mismatch");
        }
        for (base::usize i = 0; i < function.param_values.size(); ++i) {
            const Value* value = this->get(function.param_values[i]);
            if (value == nullptr || value->kind != ValueKind::param) {
                this->fail("function @" + function.symbol + " has a non-param value in parameter list");
                continue;
            }
            if (!this->module_.types.same(value->type, function.signature_params[i].type)) {
                this->fail("function @" + function.symbol + " parameter type mismatch");
            }
        }

        for (base::u32 block_index = 0; block_index < function.blocks.size(); ++block_index) {
            this->verify_block(function, BlockId {block_index}, function.blocks[block_index]);
        }
    }

    void verify_function_symbol(const FunctionId function_id, const Function& function) {
        if (function.symbol.empty()) {
            this->fail("function has an empty ABI symbol");
            return;
        }
        const auto existing = this->function_symbols_.find(function.symbol);
        if (existing == this->function_symbols_.end()) {
            this->function_symbols_.emplace(function.symbol, function_id);
            return;
        }

        const FunctionId other_id = existing->second;
        if (!is_valid(other_id) || other_id.value >= this->module_.functions.size() || other_id.value == function_id.value) {
            return;
        }

        const Function& other = this->module_.functions[other_id.value];
        if (other.symbol == function.symbol) {
            if (function.linkage != Linkage::extern_c || other.linkage != Linkage::extern_c) {
                this->fail("duplicate non-extern function ABI symbol @" + function.symbol);
                return;
            }
            if (!this->same_signature(function, other)) {
                this->fail("extern function @" + function.symbol + " has inconsistent declarations");
                return;
            }
        }
    }

    void verify_block(const Function& function, const BlockId block_id, const BasicBlock& block) {
        for (const ValueId value_id : block.values) {
            this->verify_value(function, block_id, value_id);
        }
        switch (block.terminator.kind) {
        case TerminatorKind::none:
            this->fail("block ^" + block.name + " has no terminator");
            break;
        case TerminatorKind::branch:
            this->verify_block_id(function, block.terminator.target, "branch target");
            break;
        case TerminatorKind::cond_branch:
            this->verify_value_type(block.terminator.condition, this->module_.types.builtin(sema::BuiltinType::bool_), "branch condition");
            this->verify_block_id(function, block.terminator.then_target, "then target");
            this->verify_block_id(function, block.terminator.else_target, "else target");
            break;
        case TerminatorKind::return_: {
            if (this->module_.types.is_void(function.return_type)) {
                if (is_valid(block.terminator.value)) {
                    this->fail("void function @" + function.symbol + " returns a value");
                }
            } else {
                this->verify_value_type(block.terminator.value, function.return_type, "return value");
            }
            break;
        }
        }
    }

    void verify_value(const Function& function, const BlockId block_id, const ValueId value_id) {
        static_cast<void>(block_id);
        const Value* value = this->get(value_id);
        if (value == nullptr) {
            this->fail("invalid value in block");
            return;
        }

        switch (value->kind) {
        case ValueKind::param:
            this->verify_type(value->type, "value type");
            break;
        case ValueKind::integer_literal:
        case ValueKind::float_literal:
        case ValueKind::bool_literal:
        case ValueKind::null_literal:
        case ValueKind::string_literal:
        case ValueKind::c_string_literal:
        case ValueKind::byte_literal:
        case ValueKind::undef:
            this->verify_literal_value(*value);
            break;
        case ValueKind::alloca:
            this->verify_alloca(*value);
            break;
        case ValueKind::size_of:
        case ValueKind::align_of:
            this->verify_size_or_align(*value);
            break;
        case ValueKind::str_data:
            this->verify_str_data(*value);
            break;
        case ValueKind::str_byte_len:
            this->verify_str_byte_len(*value);
            break;
        case ValueKind::str_from_bytes_unchecked:
            this->verify_str_from_bytes_unchecked(*value);
            break;
        case ValueKind::constant_ref:
            static_cast<void>(this->verify_constant_ref(*value));
            break;
        case ValueKind::load:
            this->verify_load(*value);
            break;
        case ValueKind::store:
            this->verify_pointer_value(value->object, "store target");
            this->verify_value_id(value->lhs, "store source");
            this->verify_type(value->type, "store result");
            if (!this->module_.types.is_void(value->type)) {
                this->fail("store result must be void");
            }
            if (const Value* object = this->get(value->object);
                object != nullptr &&
                this->module_.types.is_pointer(object->type) &&
                this->module_.types.get(object->type).pointer_mutability != sema::PointerMutability::mut) {
                this->fail("store target must be mutable");
            }
            if (const sema::TypeHandle target = this->pointee_type(value->object);
                sema::is_valid(target)) {
                this->verify_value_type(value->lhs, target, "store source");
            }
            break;
        case ValueKind::unary:
            this->verify_unary(*value);
            break;
        case ValueKind::binary:
            this->verify_binary(*value);
            break;
        case ValueKind::phi:
            this->verify_phi(function, block_id, *value);
            break;
        case ValueKind::call:
            this->verify_call(*value);
            break;
        case ValueKind::field_addr:
            this->verify_field_addr(*value);
            break;
        case ValueKind::index_addr:
            this->verify_index_addr(*value);
            break;
        case ValueKind::aggregate:
            this->verify_aggregate(*value);
            break;
        case ValueKind::cast:
            this->verify_value_id(value->lhs, "cast operand");
            this->verify_type(value->type, "cast result");
            this->verify_type(value->target_type, "cast target");
            if (!this->module_.types.same(value->type, value->target_type)) {
                this->fail("cast result type must match cast target type");
            }
            break;
        }
    }

    void verify_unary(const Value& value) {
        this->verify_value_id(value.lhs, "unary operand");
        this->verify_type(value.type, "unary result");
        const Value* operand = this->get(value.lhs);
        if (operand == nullptr) {
            return;
        }
        if (!sema::is_valid(operand->type)) {
            this->fail("unary operand type is invalid");
            return;
        }
        switch (value.unary_op) {
        case UnaryOp::logical_not:
            if (!this->module_.types.is_bool(operand->type) || !this->module_.types.is_bool(value.type)) {
                this->fail("logical unary operator requires bool operand and result");
            }
            break;
        case UnaryOp::numeric_negate:
            if (!this->module_.types.same(operand->type, value.type) ||
                (!this->module_.types.is_integer(value.type) && !this->module_.types.is_float(value.type))) {
                this->fail("numeric unary operator requires matching numeric operand and result");
            }
            break;
        case UnaryOp::bitwise_not:
            if (!this->module_.types.same(operand->type, value.type) || !this->module_.types.is_integer(value.type)) {
                this->fail("bitwise unary operator requires matching integer operand and result");
            }
            break;
        case UnaryOp::address_of:
        case UnaryOp::dereference:
            if (!this->module_.types.same(operand->type, value.type)) {
                this->fail("address/dereference unary passthrough type mismatch");
            }
            break;
        }
    }

    void verify_binary(const Value& value) {
        this->verify_value_id(value.lhs, "binary lhs");
        this->verify_value_id(value.rhs, "binary rhs");
        this->verify_type(value.type, "binary result");
        const Value* lhs = this->get(value.lhs);
        const Value* rhs = this->get(value.rhs);
        if (lhs == nullptr || rhs == nullptr) {
            return;
        }
        if (!this->module_.types.same(lhs->type, rhs->type)) {
            this->fail("binary operand type mismatch");
            return;
        }

        const sema::TypeHandle operand_type = lhs->type;
        if (!sema::is_valid(operand_type)) {
            this->fail("binary operand type is invalid");
            return;
        }
        switch (value.binary_op) {
        case BinaryOp::less:
        case BinaryOp::less_equal:
        case BinaryOp::greater:
        case BinaryOp::greater_equal:
            if (!this->module_.types.is_bool(value.type)) {
                this->fail("comparison binary result must be bool");
            }
            if (!this->module_.types.is_integer(operand_type) && !this->module_.types.is_float(operand_type)) {
                this->fail("comparison binary operands must be numeric");
            }
            break;
        case BinaryOp::equal:
        case BinaryOp::not_equal:
            if (!this->module_.types.is_bool(value.type)) {
                this->fail("equality binary result must be bool");
            }
            if (!this->module_.types.is_bool(operand_type) &&
                !this->module_.types.is_integer(operand_type) &&
                !this->module_.types.is_float(operand_type) &&
                !this->module_.types.is_pointer(operand_type)) {
                const sema::TypeInfo& info = this->module_.types.get(operand_type);
                if (info.kind != sema::TypeKind::enum_ ||
                    sema::is_valid(info.enum_payload_storage)) {
                    this->fail("equality binary operands must be scalar");
                }
            }
            break;
        case BinaryOp::logical_and:
        case BinaryOp::logical_or:
            if (!this->module_.types.is_bool(value.type) || !this->module_.types.is_bool(operand_type)) {
                this->fail("logical binary operator requires bool operands and result");
            }
            break;
        case BinaryOp::bit_and:
        case BinaryOp::bit_xor:
        case BinaryOp::bit_or:
        case BinaryOp::shl:
        case BinaryOp::shr:
        case BinaryOp::mod:
            if (!this->module_.types.same(value.type, operand_type)) {
                this->fail("integer binary result must match operand type");
            }
            if (!this->module_.types.is_integer(operand_type)) {
                this->fail("integer binary operator requires integer operands");
            }
            break;
        case BinaryOp::add:
        case BinaryOp::sub:
        case BinaryOp::mul:
        case BinaryOp::div:
            if (!this->module_.types.same(value.type, operand_type)) {
                this->fail("numeric binary result must match operand type");
            }
            if (!this->module_.types.is_integer(operand_type) && !this->module_.types.is_float(operand_type)) {
                this->fail("numeric binary operator requires numeric operands");
            }
            break;
        }
    }

    void verify_phi(const Function& function, const BlockId block_id, const Value& value) {
        this->verify_type(value.type, "phi result");
        if (value.incoming.empty()) {
            this->fail("phi has no incoming values");
        }
        std::unordered_set<base::u32> incoming_predecessors;
        for (const PhiInput& incoming : value.incoming) {
            this->verify_block_id(function, incoming.predecessor, "phi predecessor");
            this->verify_value_type(incoming.value, value.type, "phi incoming");
            if (is_valid(incoming.predecessor) &&
                incoming.predecessor.value < function.blocks.size() &&
                !incoming_predecessors.insert(incoming.predecessor.value).second) {
                this->fail("phi has duplicate incoming predecessor");
            }
            if (is_valid(incoming.predecessor) &&
                incoming.predecessor.value < function.blocks.size() &&
                !this->block_has_edge_to(function.blocks[incoming.predecessor.value], block_id)) {
                this->fail("phi predecessor has no edge to block");
            }
        }
        for (base::u32 predecessor = 0; predecessor < function.blocks.size(); ++predecessor) {
            if (this->block_has_edge_to(function.blocks[predecessor], block_id) &&
                !incoming_predecessors.contains(predecessor)) {
                this->fail("phi is missing incoming predecessor");
            }
        }
    }

    void verify_call(const Value& value) {
        this->verify_type(value.type, "call result");
        if (!is_valid(value.call_target)) {
            this->fail(value.name.empty() ? "call has no target symbol" : "call target @" + value.name + " is unresolved");
            return;
        }
        if (value.call_target.value >= this->module_.functions.size()) {
            this->fail("call target out of range");
            return;
        }
        const Function& target = this->module_.functions[value.call_target.value];
        if (target.is_variadic ? value.args.size() < target.signature_params.size() : value.args.size() != target.signature_params.size()) {
            this->fail("call to @" + target.symbol + " has wrong argument count");
            return;
        }
        for (base::usize i = 0; i < target.signature_params.size(); ++i) {
            this->verify_value_type(value.args[i], target.signature_params[i].type, "call argument");
        }
        for (base::usize i = target.signature_params.size(); i < value.args.size(); ++i) {
            const Value* arg = this->get(value.args[i]);
            if (arg == nullptr) {
                this->fail("call argument out of range");
                continue;
            }
            this->verify_type(arg->type, "variadic call argument");
        }
        if (!this->module_.types.same(value.type, target.return_type)) {
            this->fail("call to @" + target.symbol + " result type mismatch");
        }
    }

    [[nodiscard]] bool same_signature(const Function& lhs, const Function& rhs) const noexcept {
        if (!this->module_.types.same(lhs.return_type, rhs.return_type)) {
            return false;
        }
        if (lhs.is_variadic != rhs.is_variadic || lhs.signature_params.size() != rhs.signature_params.size()) {
            return false;
        }
        for (base::usize i = 0; i < lhs.signature_params.size(); ++i) {
            if (!this->module_.types.same(lhs.signature_params[i].type, rhs.signature_params[i].type)) {
                return false;
            }
        }
        return true;
    }

    void verify_literal_value(const Value& value) {
        this->verify_type(value.type, "literal value type");
        switch (value.kind) {
        case ValueKind::integer_literal:
            if (!this->is_integer_literal_type(value.type)) {
                this->fail("integer literal type must be integer, got " + this->module_.types.display_name(value.type));
            }
            break;
        case ValueKind::float_literal:
            if (!this->module_.types.is_float(value.type)) {
                this->fail("float literal type must be float, got " + this->module_.types.display_name(value.type));
            }
            break;
        case ValueKind::bool_literal:
            if (!this->module_.types.is_bool(value.type)) {
                this->fail("bool literal type must be bool");
            }
            break;
        case ValueKind::null_literal:
            if (!this->module_.types.is_pointer(value.type)) {
                this->fail("null literal type must be pointer");
            }
            break;
        case ValueKind::string_literal:
            if (!this->module_.types.is_str(value.type)) {
                this->fail("string literal type must be str");
            }
            break;
        case ValueKind::c_string_literal:
            if (!this->is_const_u8_pointer(value.type)) {
                this->fail("c string literal type must be *const u8");
            }
            break;
        case ValueKind::byte_literal:
            if (!this->module_.types.same(value.type, this->module_.types.builtin(sema::BuiltinType::u8))) {
                this->fail("byte literal type must be u8");
            }
            break;
        case ValueKind::undef:
            if (this->module_.types.is_void(value.type)) {
                this->fail("undef value cannot have void type");
            }
            break;
        default:
            break;
        }
    }

    void verify_alloca(const Value& value) {
        this->verify_type(value.type, "alloca result");
        if (!this->module_.types.is_pointer(value.type)) {
            this->fail("alloca result must be a pointer");
            return;
        }
        const sema::TypeInfo& pointer = this->module_.types.get(value.type);
        if (pointer.pointer_mutability != sema::PointerMutability::mut) {
            this->fail("alloca result must be a mutable pointer");
        }
        this->verify_storage_type(pointer.pointee, "alloca pointee");
    }

    void verify_load(const Value& value) {
        this->verify_pointer_value(value.object, "load object");
        this->verify_type(value.type, "load result");
        if (this->module_.types.is_void(value.type)) {
            this->fail("load result must not be void");
        }
        if (const sema::TypeHandle source = this->pointee_type(value.object);
            sema::is_valid(source) && !this->module_.types.same(value.type, source)) {
            this->fail("load result type mismatch");
        }
    }

    void verify_size_or_align(const Value& value) {
        const std::string op = value.kind == ValueKind::size_of ? "sizeof" : "alignof";
        this->verify_type(value.type, op + " result");
        if (!this->module_.types.same(value.type, this->module_.types.builtin(sema::BuiltinType::usize))) {
            this->fail(op + " result must be usize");
        }
        this->verify_type(value.target_type, op + " target");
        this->verify_storage_type(value.target_type, op + " target");
    }

    void verify_str_data(const Value& value) {
        this->verify_type(value.type, "strptr result");
        if (!this->is_const_u8_pointer(value.type)) {
            this->fail("strptr result must be *const u8");
        }
        this->verify_value_type(value.object, this->module_.types.builtin(sema::BuiltinType::str), "strptr operand");
    }

    void verify_str_byte_len(const Value& value) {
        this->verify_type(value.type, "strlen result");
        if (!this->module_.types.same(value.type, this->module_.types.builtin(sema::BuiltinType::usize))) {
            this->fail("strlen result must be usize");
        }
        this->verify_value_type(value.object, this->module_.types.builtin(sema::BuiltinType::str), "strlen operand");
    }

    void verify_str_from_bytes_unchecked(const Value& value) {
        this->verify_type(value.type, "strraw result");
        if (!this->module_.types.is_str(value.type)) {
            this->fail("strraw result must be str");
        }
        if (value.args.size() != IR_VERIFIER_STR_FROM_BYTES_UNCHECKED_ARGUMENT_COUNT) {
            this->fail("strraw requires data and length arguments");
            return;
        }
        this->verify_value_id(value.args[0], "strraw data");
        if (const Value* data = this->get(value.args[0]); data != nullptr && !this->is_const_u8_pointer(data->type)) {
            this->fail("strraw data must be *const u8");
        }
        this->verify_value_type(value.args[1], this->module_.types.builtin(sema::BuiltinType::usize), "strraw length");
    }

    [[nodiscard]] const GlobalConstant* verify_constant_ref(const Value& value) {
        this->verify_type(value.type, "constant reference type");
        const GlobalConstant* constant = find_global_constant(this->module_, value.constant);
        if (constant == nullptr) {
            this->fail("constant reference id is invalid");
            return nullptr;
        }
        if (!this->module_.types.same(value.type, constant->type)) {
            this->fail("constant reference type mismatch");
        }
        return constant;
    }

    void verify_field_addr(const Value& value) {
        this->verify_pointer_value(value.object, "field object");
        this->verify_type(value.type, "field address type");
        if (!this->module_.types.is_pointer(value.type)) {
            this->fail("field address result is not a pointer");
            return;
        }
        const sema::TypeHandle object_type = this->pointee_type(value.object);
        const sema::TypeHandle record_type = this->module_.types.is_pointer(object_type)
            ? this->module_.types.get(object_type).pointee
            : object_type;
        const RecordField* field = find_record_field(this->module_, record_type, value.name);
        if (field == nullptr) {
            this->fail("unknown field '" + value.name + "'");
            return;
        }
        const sema::TypeInfo& address = this->module_.types.get(value.type);
        if (!this->module_.types.same(address.pointee, field->type)) {
            this->fail("field address result type mismatch");
        }
        const Value* object = this->get(value.object);
        if (object != nullptr &&
            this->module_.types.is_pointer(object->type) &&
            this->module_.types.get(object->type).pointer_mutability == sema::PointerMutability::const_ &&
            address.pointer_mutability == sema::PointerMutability::mut) {
            this->fail("field address cannot be mutable through const object");
        }
    }

    void verify_index_addr(const Value& value) {
        this->verify_pointer_value(value.object, "index object");
        this->verify_value_id(value.index, "index");
        this->verify_type(value.type, "index result");
        if (!this->module_.types.is_pointer(value.type)) {
            this->fail("index address result is not a pointer");
            return;
        }
        const Value* index = this->get(value.index);
        if (index != nullptr && !this->module_.types.is_integer(index->type)) {
            this->fail("index must be an integer");
        }
        const sema::TypeHandle object_type = this->pointee_type(value.object);
        const sema::TypeHandle element_type = this->module_.types.is_array(object_type)
            ? this->module_.types.get(object_type).array_element
            : object_type;
        const sema::TypeInfo& address = this->module_.types.get(value.type);
        if (sema::is_valid(element_type) && !this->module_.types.same(address.pointee, element_type)) {
            this->fail("index address result type mismatch");
        }
        const Value* object = this->get(value.object);
        if (object != nullptr &&
            this->module_.types.is_pointer(object->type) &&
            this->module_.types.get(object->type).pointer_mutability == sema::PointerMutability::const_ &&
            address.pointer_mutability == sema::PointerMutability::mut) {
            this->fail("index address cannot be mutable through const object");
        }
    }

    void verify_aggregate(const Value& value) {
        this->verify_type(value.type, "aggregate result");
        const RecordLayout* record = find_record(this->module_, value.type);
        if (record == nullptr) {
            this->fail("aggregate result is not a record");
            return;
        }
        std::unordered_set<std::string> seen;
        for (const FieldValue& field : value.fields) {
            if (!seen.insert(field.name).second) {
                this->fail("duplicate aggregate field " + field.name);
            }
            const RecordField* expected = find_record_field(this->module_, value.type, field.name);
            if (expected == nullptr) {
                this->fail("unknown aggregate field " + field.name);
                continue;
            }
            this->verify_value_type(field.value, expected->type, "aggregate field");
        }
        if (seen.size() != record->fields.size()) {
            this->fail("aggregate does not initialize every field");
        }
    }

    void verify_storage_type(const sema::TypeHandle type, const std::string& context) {
        // Use an explicit stack so nested array storage checks stay iterative.
        std::vector<StorageTypeWorkItem> worklist;
        worklist.push_back(StorageTypeWorkItem {type, context});
        while (!worklist.empty()) {
            StorageTypeWorkItem item = std::move(worklist.back());
            worklist.pop_back();
            if (!sema::is_valid(item.type)) {
                this->fail(item.context + " type is invalid");
                continue;
            }
            if (this->module_.types.is_void(item.type)) {
                this->fail(item.context + " type is not valid storage");
                continue;
            }
            if (this->module_.types.is_array(item.type)) {
                worklist.push_back(StorageTypeWorkItem {
                    this->module_.types.get(item.type).array_element,
                    item.context + " element",
                });
                continue;
            }
            if (this->module_.types.get(item.type).kind == sema::TypeKind::opaque_struct) {
                this->fail(item.context + " type is not valid storage");
            }
        }
    }

    [[nodiscard]] bool is_const_u8_pointer(const sema::TypeHandle type) const noexcept {
        if (!this->module_.types.is_pointer(type)) {
            return false;
        }
        const sema::TypeInfo& pointer = this->module_.types.get(type);
        return pointer.pointer_mutability == sema::PointerMutability::const_ &&
               this->module_.types.same(pointer.pointee, this->module_.types.builtin(sema::BuiltinType::u8));
    }

    [[nodiscard]] bool is_integer_literal_type(const sema::TypeHandle type) const noexcept {
        if (this->module_.types.is_integer(type)) {
            return true;
        }
        if (!sema::is_valid(type)) {
            return false;
        }
        const sema::TypeInfo& info = this->module_.types.get(type);
        return info.kind == sema::TypeKind::enum_ &&
               !sema::is_valid(info.enum_payload_storage);
    }

    void verify_block_id(const Function& function, const BlockId block, const std::string& context) {
        if (!is_valid(block) || block.value >= function.blocks.size()) {
            this->fail(context + " block id is invalid");
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
        if (this->get(value) == nullptr) {
            this->fail(context + " value id is invalid");
        }
    }

    void verify_type(const sema::TypeHandle type, const std::string& context) {
        if (!sema::is_valid(type)) {
            this->fail(context + " is invalid");
        }
    }

    void verify_value_type(const ValueId value_id, const sema::TypeHandle expected, const std::string& context) {
        const Value* value = this->get(value_id);
        if (value == nullptr) {
            this->fail(context + " value id is invalid");
            return;
        }
        if (!this->module_.types.same(value->type, expected)) {
            this->fail(context + " type mismatch");
        }
    }

    void verify_pointer_value(const ValueId value_id, const std::string& context) {
        const Value* value = this->get(value_id);
        if (value == nullptr) {
            this->fail(context + " value id is invalid");
            return;
        }
        if (!this->module_.types.is_pointer(value->type)) {
            this->fail(context + " is not a pointer");
        }
    }

    [[nodiscard]] sema::TypeHandle pointee_type(const ValueId value_id) const noexcept {
        const Value* value = this->get(value_id);
        if (value == nullptr || !this->module_.types.is_pointer(value->type)) {
            return sema::INVALID_TYPE_HANDLE;
        }
        return this->module_.types.get(value->type).pointee;
    }

    [[nodiscard]] const Value* get(const ValueId value) const noexcept {
        if (!is_valid(value) || value.value >= this->module_.values.size()) {
            return nullptr;
        }
        return &this->module_.values[value.value];
    }

    void fail(std::string message) {
        this->errors_.push_back(std::move(message));
    }

    const Module& module_;
    std::vector<std::string> errors_;
    std::unordered_map<std::string, FunctionId> function_symbols_;
};

} // namespace

base::Result<void> verify_module(const Module& module) {
    Verifier verifier(module);
    return verifier.run();
}

} // namespace aurex::ir

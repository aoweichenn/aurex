#include "aurex/ir/ir_dump.hpp"

#include <sstream>

namespace aurex::ir {

namespace {

[[nodiscard]] std::string value_ref(const ValueId id) {
    if (!is_valid(id)) {
        return "%invalid";
    }
    return "%" + std::to_string(id.value);
}

[[nodiscard]] std::string block_ref(const BlockId id, const Function& function) {
    if (!is_valid(id) || id.value >= function.blocks.size()) {
        return "^invalid";
    }
    return "^" + function.blocks[id.value].name;
}

[[nodiscard]] std::string linkage_name(const Linkage linkage) {
    switch (linkage) {
    case Linkage::internal: return "internal";
    case Linkage::export_c: return "export_c";
    case Linkage::extern_c: return "extern_c";
    }
    return "internal";
}

[[nodiscard]] std::string unary_name(const UnaryOp op) {
    switch (op) {
    case UnaryOp::logical_not: return "not";
    case UnaryOp::numeric_negate: return "neg";
    case UnaryOp::bitwise_not: return "bitnot";
    case UnaryOp::address_of: return "addr_of";
    case UnaryOp::dereference: return "deref";
    }
    return "unknown";
}

[[nodiscard]] std::string binary_name(const BinaryOp op) {
    switch (op) {
    case BinaryOp::add: return "add";
    case BinaryOp::sub: return "sub";
    case BinaryOp::mul: return "mul";
    case BinaryOp::div: return "div";
    case BinaryOp::mod: return "mod";
    case BinaryOp::shl: return "shl";
    case BinaryOp::shr: return "shr";
    case BinaryOp::less: return "lt";
    case BinaryOp::less_equal: return "le";
    case BinaryOp::greater: return "gt";
    case BinaryOp::greater_equal: return "ge";
    case BinaryOp::equal: return "eq";
    case BinaryOp::not_equal: return "ne";
    case BinaryOp::bit_and: return "bitand";
    case BinaryOp::bit_xor: return "bitxor";
    case BinaryOp::bit_or: return "bitor";
    case BinaryOp::logical_and: return "and";
    case BinaryOp::logical_or: return "or";
    }
    return "unknown";
}

[[nodiscard]] std::string cast_name(const CastKind kind) {
    switch (kind) {
    case CastKind::numeric: return "cast";
    case CastKind::pointer: return "ptr_cast";
    case CastKind::bitcast: return "bit_cast";
    case CastKind::ptr_addr: return "ptr_addr";
    case CastKind::ptr_from_addr: return "ptr_from_addr";
    }
    return "cast";
}

void dump_value(std::ostream& out, const Module& module, const Function& function, const ValueId id) {
    const Value& value = module.values[id.value];
    out << "    " << value_ref(id) << " : " << module.types.display_name(value.type) << " = ";
    switch (value.kind) {
    case ValueKind::param:
        out << "param " << value.name;
        break;
    case ValueKind::integer_literal:
    case ValueKind::bool_literal:
    case ValueKind::byte_literal:
        out << "literal " << value.text;
        break;
    case ValueKind::null_literal:
        out << "null";
        break;
    case ValueKind::string_literal:
        out << "string " << value.text;
        break;
    case ValueKind::c_string_literal:
        out << "c_string " << value.text;
        break;
    case ValueKind::alloca:
        out << "alloca " << value.name;
        break;
    case ValueKind::load:
        out << "load " << value_ref(value.object);
        break;
    case ValueKind::store:
        out << "store " << value_ref(value.object) << ", " << value_ref(value.lhs);
        break;
    case ValueKind::unary:
        out << unary_name(value.unary_op) << " " << value_ref(value.lhs);
        break;
    case ValueKind::binary:
        out << binary_name(value.binary_op) << " " << value_ref(value.lhs) << ", " << value_ref(value.rhs);
        break;
    case ValueKind::phi:
        out << "phi ";
        for (base::usize i = 0; i < value.incoming.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << "[" << block_ref(value.incoming[i].predecessor, function) << ": "
                << value_ref(value.incoming[i].value) << "]";
        }
        break;
    case ValueKind::call:
        out << "call " << value.name << "(";
        for (base::usize i = 0; i < value.args.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << value_ref(value.args[i]);
        }
        out << ")";
        break;
    case ValueKind::field_addr:
        out << "field_addr " << value_ref(value.object) << "." << value.name;
        break;
    case ValueKind::index_addr:
        out << "index_addr " << value_ref(value.object) << "[" << value_ref(value.index) << "]";
        break;
    case ValueKind::aggregate:
        out << "aggregate {";
        for (base::usize i = 0; i < value.fields.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << "." << value.fields[i].name << " = " << value_ref(value.fields[i].value);
        }
        out << "}";
        break;
    case ValueKind::cast:
        out << cast_name(value.cast_kind) << " " << value_ref(value.lhs) << " to " << module.types.display_name(value.target_type);
        break;
    case ValueKind::size_of:
        out << "size_of " << module.types.display_name(value.target_type);
        break;
    case ValueKind::align_of:
        out << "align_of " << module.types.display_name(value.target_type);
        break;
    }
    out << "\n";
}

void dump_terminator(std::ostream& out, const Function& function, const Terminator& term) {
    switch (term.kind) {
    case TerminatorKind::none:
        out << "    unreachable\n";
        break;
    case TerminatorKind::branch:
        out << "    br " << block_ref(term.target, function) << "\n";
        break;
    case TerminatorKind::cond_branch:
        out << "    br_if " << value_ref(term.condition) << ", "
            << block_ref(term.then_target, function) << ", "
            << block_ref(term.else_target, function) << "\n";
        break;
    case TerminatorKind::return_:
        if (is_valid(term.value)) {
            out << "    ret " << value_ref(term.value) << "\n";
        } else {
            out << "    ret\n";
        }
        break;
    }
}

} // namespace

std::string dump_module(const Module& module) {
    std::ostringstream out;
    out << "aurex_ir v0\n";
    for (const Function& function : module.functions) {
        out << "fn " << function.name << "(";
        for (base::usize i = 0; i < function.signature_params.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            const FunctionParam& param = function.signature_params[i];
            out << param.name << ": " << module.types.display_name(param.type);
        }
        out << ") @" << function.symbol
            << " linkage(" << linkage_name(function.linkage) << ") -> "
            << module.types.display_name(function.return_type) << " {\n";
        if (!function.param_values.empty()) {
            out << "  params";
            for (ValueId param : function.param_values) {
                const Value& value = module.values[param.value];
                out << " " << value_ref(param) << ":" << module.types.display_name(value.type);
            }
            out << "\n";
        }
        for (const BasicBlock& block : function.blocks) {
            out << "  ^" << block.name << ":\n";
            for (ValueId value : block.values) {
                dump_value(out, module, function, value);
            }
            dump_terminator(out, function, block.terminator);
        }
        out << "}\n";
    }
    return out.str();
}

} // namespace aurex::ir

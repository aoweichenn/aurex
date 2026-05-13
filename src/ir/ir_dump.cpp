#include <aurex/ir/ir_dump.hpp>

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

[[nodiscard]] std::string call_conv_name(const AbiCallConv call_conv) {
    switch (call_conv) {
    case AbiCallConv::aurex: return "aurex";
    case AbiCallConv::c: return "c";
    }
    return "aurex";
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
    case CastKind::pointer: return "ptrcast";
    case CastKind::bcast: return "bitcast";
    case CastKind::ptr_addr: return "ptraddr";
    case CastKind::paddr: return "ptrat";
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
    case ValueKind::float_literal:
    case ValueKind::bool_literal:
    case ValueKind::char_literal:
    case ValueKind::byte_literal:
        out << "literal " << value.text;
        break;
    case ValueKind::undef:
        out << "undef";
        break;
    case ValueKind::constant_ref: {
        const GlobalConstant* constant = find_global_constant(module, value.constant);
        out << "const_ref @" << (constant == nullptr ? value.name : constant->symbol);
        break;
    }
    case ValueKind::function_ref:
        out << "function_ref @" << value.name;
        break;
    case ValueKind::null_literal:
        out << "null";
        break;
    case ValueKind::string_literal:
        out << "string " << value.text;
        break;
    case ValueKind::raw_string_literal:
        out << "raw_string " << value.text;
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
        if (is_valid(value.call_target)) {
            out << "call " << value.name << "(";
        } else {
            out << "call " << value_ref(value.object) << "(";
        }
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
        if (module.types.is_array(value.type)) {
            out << "aggregate [";
            for (base::usize i = 0; i < value.elements.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << value_ref(value.elements[i]);
            }
            out << "]";
        } else {
            out << "aggregate {";
            for (base::usize i = 0; i < value.fields.size(); ++i) {
                if (i != 0) {
                    out << ", ";
                }
                out << "." << value.fields[i].name << " = " << value_ref(value.fields[i].value);
            }
            out << "}";
        }
        break;
    case ValueKind::slice:
        out << "slice " << value_ref(value.lhs) << ", " << value_ref(value.rhs);
        break;
    case ValueKind::slice_data:
        out << "slice_data " << value_ref(value.object);
        break;
    case ValueKind::slice_len:
        out << "slice_len " << value_ref(value.object);
        break;
    case ValueKind::cast:
        out << cast_name(value.cast_kind) << " " << value_ref(value.lhs) << " to " << module.types.display_name(value.target_type);
        break;
    case ValueKind::size_of:
        out << "sizeof " << module.types.display_name(value.target_type);
        break;
    case ValueKind::align_of:
        out << "alignof " << module.types.display_name(value.target_type);
        break;
    case ValueKind::str_data:
        out << "strptr " << value_ref(value.object);
        break;
    case ValueKind::str_byte_len:
        out << "strblen " << value_ref(value.object);
        break;
    case ValueKind::str_is_valid_utf8:
        out << "strvalid " << value_ref(value.object);
        break;
    case ValueKind::str_from_utf8_checked:
        out << "strfromutf8 " << value_ref(value.object);
        break;
    case ValueKind::str_slice_checked:
        out << "strslice.checked " << value_ref(value.object) << "["
            << value_ref(value.lhs) << ":" << value_ref(value.rhs) << "]";
        break;
    case ValueKind::str_from_bytes_unchecked:
        out << "strraw(";
        for (base::usize i = 0; i < value.args.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << value_ref(value.args[i]);
        }
        out << ")";
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
    for (const GlobalConstant& constant : module.constants) {
        out << "const " << constant.name << " @" << constant.symbol
            << ": " << module.types.display_name(constant.type)
            << " = " << value_ref(constant.initializer) << "\n";
    }
    for (const RecordLayout& record : module.records) {
        out << "record " << record.name << " @" << record.symbol;
        if (record.is_opaque) {
            out << " opaque";
        }
        out << " {\n";
        for (const RecordField& field : record.fields) {
            out << "  ." << field.name << ": " << module.types.display_name(field.type) << "\n";
        }
        out << "}\n";
    }
    for (const Function& function : module.functions) {
        out << "fn " << function.name << "(";
        for (base::usize i = 0; i < function.signature_params.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            const FunctionParam& param = function.signature_params[i];
            out << param.name << ": " << module.types.display_name(param.type);
        }
        if (function.is_variadic) {
            if (!function.signature_params.empty()) {
                out << ", ";
            }
            out << "...";
        }
        out << ") @" << function.symbol
            << " linkage(" << linkage_name(function.linkage) << ")"
            << " abi(" << call_conv_name(function.call_conv) << ")";
        if (function.is_entry) {
            out << " entry";
        }
        if (function.is_unsafe) {
            out << " unsafe";
        }
        out << " -> " << module.types.display_name(function.return_type) << " {\n";
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

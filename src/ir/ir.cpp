#include "aurex/ir/ir.hpp"

#include <utility>

namespace aurex::ir {

ValueId add_value(Module& module, Value value) {
    const ValueId id {static_cast<base::u32>(module.values.size())};
    module.values.push_back(std::move(value));
    return id;
}

GlobalConstantId add_global_constant(Module& module, GlobalConstant constant) {
    const GlobalConstantId id {static_cast<base::u32>(module.constants.size())};
    module.constants.push_back(std::move(constant));
    return id;
}

BlockId add_block(Function& function, std::string name) {
    const BlockId id {static_cast<base::u32>(function.blocks.size())};
    BasicBlock block;
    block.name = std::move(name);
    function.blocks.push_back(std::move(block));
    return id;
}

const GlobalConstant* find_global_constant(const Module& module, const GlobalConstantId id) noexcept {
    if (!is_valid(id) || id.value >= module.constants.size()) {
        return nullptr;
    }
    return &module.constants[id.value];
}

const RecordLayout* find_record(const Module& module, const sema::TypeHandle type) noexcept {
    if (!sema::is_valid(type)) {
        return nullptr;
    }
    for (const RecordLayout& record : module.records) {
        if (module.types.same(record.type, type)) {
            return &record;
        }
    }
    return nullptr;
}

const RecordField* find_record_field(const Module& module, const sema::TypeHandle type, const std::string& name) noexcept {
    const RecordLayout* record = find_record(module, type);
    if (record == nullptr) {
        return nullptr;
    }
    for (const RecordField& field : record->fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

base::usize record_field_index(const RecordLayout& record, const std::string& name) noexcept {
    for (base::usize i = 0; i < record.fields.size(); ++i) {
        if (record.fields[i].name == name) {
            return i;
        }
    }
    return static_cast<base::usize>(-1);
}

} // namespace aurex::ir

#include "aurex/ir/ir.hpp"

#include <utility>

namespace aurex::ir {

ValueId add_value(Module& module, Value value) {
    const ValueId id {static_cast<base::u32>(module.values.size())};
    module.values.push_back(std::move(value));
    return id;
}

BlockId add_block(Function& function, std::string name) {
    const BlockId id {static_cast<base::u32>(function.blocks.size())};
    BasicBlock block;
    block.name = std::move(name);
    function.blocks.push_back(std::move(block));
    return id;
}

} // namespace aurex::ir

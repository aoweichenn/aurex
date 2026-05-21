#pragma once

#include <aurex/base/result.hpp>

#include <string>

namespace aurex::ir {
struct Module;
} // namespace aurex::ir

namespace aurex::driver {

struct LlvmIrOutput {
    std::string text;
};

struct LlvmIrEmitRequest {
    const ir::Module* module = nullptr;
    std::string module_name;
};

using LlvmIrEmitter = base::Result<LlvmIrOutput> (*)(const LlvmIrEmitRequest&);

} // namespace aurex::driver

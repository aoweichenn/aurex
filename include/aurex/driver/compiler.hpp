#pragma once

#include <aurex/base/result.hpp>
#include <aurex/driver/invocation.hpp>

namespace aurex::driver {

class Compiler final {
public:
    [[nodiscard]] base::Result<void> run(const CompilerInvocation& invocation);
};

} // namespace aurex::driver

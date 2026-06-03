#pragma once

namespace aurex::driver {

enum class EmitKind {
    tokens,
    lossless,
    ast,
    modules,
    checked,
    typed,
    ir,
    llvm_ir,
    check,
    assembly,
    object,
    executable,
};

} // namespace aurex::driver

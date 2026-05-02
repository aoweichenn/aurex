# Aurex M0 C Backend Design

Version: M0V0.1.8

## Responsibility

The C backend turns syntax plus semantic side tables into C. It must not perform
name resolution or type inference. If codegen needs a fact, sema should provide
it through `CheckedModule`.

## Current Inputs

- `syntax::AstModule`
- `sema::CheckedModule`

`CheckedModule` provides:

- function signatures and ABI names;
- expression types;
- struct metadata;
- enum case metadata.

## Output Shape

Every generated C file starts with:

```c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
```

M0 `export c fn main(...)` is emitted as:

```c
static int32_t aurex_m0_main(...m0 signature...) {
    ...
}

int main(int argc, char **argv) {
    return (int)aurex_m0_main((int32_t)argc, (uint8_t **)argv);
}
```

This keeps the M0 signature visible while satisfying host C compilers that
require the platform `main` signature.

## Known Limitation

The backend still emits many expressions directly. That is acceptable for the
current seed language, but it is not enough for industrial-grade semantics.

The next major backend step is a lowering layer:

```text
AST + CheckedModule
  -> lowered C-like IR with explicit temporaries
  -> C printer
```

Lowering must guarantee:

- function arguments evaluate left to right;
- binary operands evaluate left to right;
- `&&` and `||` short-circuit;
- side-effectful expressions are stored in typed temporaries;
- no hidden array or struct copy is introduced by codegen.

# Runtime Flow

## Compile Pipeline

1. `aurexc` parses CLI options into `CompilerInvocation`.
2. `ModuleLoader` loads the root file and resolves imports through the importer
   directory and explicit `-I` entries.
3. The lexer produces tokens and the parser builds the AST.
4. The semantic analyzer performs name resolution, type checking, generic
   instantiation, value-semantics checks, and control-flow checks over the
   combined module.
5. Dump/check modes return at their requested stage.
6. IR lowering produces Aurex IR and the pass pipeline runs according to
   `--opt-level`.
7. The LLVM backend emits LLVM IR.
8. Native modes pass temporary LLVM IR to clang for assembly, object, or
   executable output.

## Module Lookup

This branch has only two lookup sources:

1. The importing file's directory.
2. Explicit `-I path` entries.

There is no standard-library root, no environment-variable lookup, no install
prefix probing, and no automatic host support source linking.

## Native Output

Native output compiles only the LLVM IR generated for the current Aurex program.
When a sample needs libc, it declares a narrow local `extern c` boundary and
lets clang use the platform's normal libc linkage.

#include "aurex/driver/compiler.hpp"

#include "aurex/base/config.hpp"

#include <iostream>
#include <string_view>

namespace {

void print_usage(std::ostream& out, const std::string_view argv0) {
    out
        << "usage: " << argv0 << " [options] input.ax [-o output.c]\n"
        << "options:\n"
        << "  --help           show this help text\n"
        << "  --version        print compiler version\n"
        << "  --dump-tokens    lex only and print tokens\n"
        << "  --dump-ast       parse and print AST\n"
        << "  --dump-modules   resolve imports and print loaded modules\n"
        << "  --dump-checked   run sema and print checked summary\n"
        << "  --check          run lexer, parser, and sema without emitting C\n"
        << "  --emit=tokens    same as --dump-tokens\n"
        << "  --emit=ast       same as --dump-ast\n"
        << "  --emit=modules   same as --dump-modules\n"
        << "  --emit=checked   same as --dump-checked\n"
        << "  --emit=check     same as --check\n"
        << "  --emit=c         emit C (default)\n"
        << "  -I path          add an import search path\n";
}

} // namespace

int main(const int argc, char** argv) {
    aurex::driver::CompilerInvocation invocation;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--dump-tokens" || arg == "--emit=tokens") {
            invocation.emit_kind = aurex::driver::EmitKind::tokens;
        } else if (arg == "--dump-ast" || arg == "--emit=ast") {
            invocation.emit_kind = aurex::driver::EmitKind::ast;
        } else if (arg == "--dump-modules" || arg == "--emit=modules") {
            invocation.emit_kind = aurex::driver::EmitKind::modules;
        } else if (arg == "--dump-checked" || arg == "--emit=checked") {
            invocation.emit_kind = aurex::driver::EmitKind::checked;
        } else if (arg == "--check" || arg == "--emit=check") {
            invocation.emit_kind = aurex::driver::EmitKind::check;
        } else if (arg == "--emit=c") {
            invocation.emit_kind = aurex::driver::EmitKind::c;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(std::cout, argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << aurex::base::config::version_string << "\n";
            return 0;
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                print_usage(std::cerr, argv[0]);
                return 2;
            }
            invocation.output_path = argv[++i];
        } else if (arg == "-I") {
            if (i + 1 >= argc) {
                print_usage(std::cerr, argv[0]);
                return 2;
            }
            invocation.import_paths.push_back(argv[++i]);
        } else if (arg.starts_with("-I") && arg.size() > 2) {
            invocation.import_paths.push_back(std::string(arg.substr(2)));
        } else if (!arg.empty() && arg.front() == '-') {
            print_usage(std::cerr, argv[0]);
            return 2;
        } else {
            invocation.input_path = arg;
        }
    }

    if (invocation.input_path.empty()) {
        print_usage(std::cerr, argv[0]);
        return 2;
    }

    aurex::driver::Compiler compiler;
    auto result = compiler.run(invocation);
    if (!result) {
        std::cerr << "m0c: " << result.error().message << "\n";
        return 1;
    }
    return 0;
}

#include <aurex/driver/cli.hpp>

#include <aurex/base/config.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/driver_messages.hpp>

#include <array>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace aurex::driver {

namespace {

inline constexpr int CLI_SUCCESS_EXIT_CODE = 0;
inline constexpr int CLI_COMPILATION_FAILURE_EXIT_CODE = 1;
inline constexpr int CLI_ARGUMENT_ERROR_EXIT_CODE = 2;
inline constexpr std::size_t CLI_HELP_OPTION_COLUMN_WIDTH = 20;

inline constexpr std::string_view CLI_DEFAULT_TOOL_NAME = "aurexc";
inline constexpr std::string_view CLI_OUTPUT_OBJECT_EXTENSION = ".o";
inline constexpr std::string_view CLI_OUTPUT_ASSEMBLY_EXTENSION = ".s";
inline constexpr std::string_view CLI_LONG_OPTION_PREFIX = "--";

enum class OptionId {
    end_options,
    help,
    version,
    check,
    compile_only,
    assembly_only,
    dump_tokens,
    dump_ast,
    dump_modules,
    dump_checked,
    dump_ir,
    dump_llvm_ir,
    emit,
    output,
    import_path,
    clang,
    clang_arg,
    opt_level,
};

enum class OptionValueStyle {
    flag,
    separate,
    joined_or_separate,
};

struct OptionSpec {
    OptionId id;
    std::string_view spelling;
    OptionValueStyle value_style;
    std::string_view value_name;
    std::string_view help;
    bool show_in_help = true;
};

inline constexpr std::array<OptionSpec, 22> OPTION_SPECS {{
    {OptionId::end_options, "--", OptionValueStyle::flag, {}, {}, false},
    {OptionId::help, "--help", OptionValueStyle::flag, {}, "show this help text"},
    {OptionId::help, "-h", OptionValueStyle::flag, {}, "same as --help", false},
    {OptionId::version, "--version", OptionValueStyle::flag, {}, "print compiler version"},
    {OptionId::check, "--check", OptionValueStyle::flag, {}, "run lexer, parser, and sema without code generation"},
    {OptionId::check, "-fsyntax-only", OptionValueStyle::flag, {}, "same as --check"},
    {OptionId::compile_only, "-c", OptionValueStyle::flag, {}, "compile to an object file"},
    {OptionId::assembly_only, "-S", OptionValueStyle::flag, {}, "compile to assembly"},
    {OptionId::dump_tokens, "--dump-tokens", OptionValueStyle::flag, {}, "lex only and print tokens"},
    {OptionId::dump_ast, "--dump-ast", OptionValueStyle::flag, {}, "parse and print AST"},
    {OptionId::dump_modules, "--dump-modules", OptionValueStyle::flag, {}, "resolve imports and print loaded modules"},
    {OptionId::dump_checked, "--dump-checked", OptionValueStyle::flag, {}, "run sema and print checked summary"},
    {OptionId::dump_ir, "--dump-ir", OptionValueStyle::flag, {}, "lower to Aurex IR and print it"},
    {OptionId::dump_llvm_ir, "--dump-llvm-ir", OptionValueStyle::flag, {}, "lower to LLVM IR and print it"},
    {OptionId::emit, "--emit", OptionValueStyle::separate, "kind", "emit kind; examples: --emit=ast, --emit=checked, --emit=ir, --emit=llvm-ir, --emit=asm, --emit=obj, --emit=exe"},
    {OptionId::output, "-o", OptionValueStyle::separate, "path", "set output path"},
    {OptionId::import_path, "-I", OptionValueStyle::joined_or_separate, "path", "add an import search path"},
    {OptionId::import_path, "--import-path", OptionValueStyle::separate, "path", "add an import search path"},
    {OptionId::clang, "--clang", OptionValueStyle::separate, "path", "clang executable to use for native output"},
    {OptionId::clang_arg, "--clang-arg", OptionValueStyle::separate, "arg", "pass one raw argument to clang; repeat as needed"},
    {OptionId::opt_level, "--opt-level", OptionValueStyle::separate, "n", "run Aurex IR passes at O0, O1, O2, or O3 (default O0)"},
    {OptionId::opt_level, "-O", OptionValueStyle::joined_or_separate, "n", "same as --opt-level n; joined forms -O0, -O1, -O2, and -O3 are accepted"},
}};

struct ParsedOption {
    const OptionSpec* spec = nullptr;
    std::string_view value;
};

[[nodiscard]] bool starts_with(const std::string_view value, const std::string_view prefix) noexcept {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool is_long_option(const std::string_view value) noexcept {
    return starts_with(value, CLI_LONG_OPTION_PREFIX);
}

[[nodiscard]] std::string cli_argument_error_message(const std::string_view message) {
    return "argument error: " + std::string(message);
}

[[nodiscard]] std::string cli_missing_value_message(const std::string_view option) {
    return "missing value for option: " + std::string(option);
}

[[nodiscard]] std::string cli_unknown_option_message(const std::string_view option) {
    return "unknown option: " + std::string(option);
}

[[nodiscard]] std::string cli_unexpected_value_message(const std::string_view option) {
    return "option does not take a value: " + std::string(option);
}

[[nodiscard]] std::string cli_invalid_emit_kind_message(const std::string_view kind) {
    return "invalid emit kind: " + std::string(kind);
}

[[nodiscard]] base::Error cli_argument_error(std::string message) {
    return {base::ErrorCode::invalid_source, cli_argument_error_message(message)};
}

[[nodiscard]] base::Result<CliParseResult> fail_cli_parse(std::string message) {
    return base::Result<CliParseResult>::fail(cli_argument_error(std::move(message)));
}

[[nodiscard]] base::Result<ParsedOption> fail_option_parse(std::string message) {
    return base::Result<ParsedOption>::fail(cli_argument_error(std::move(message)));
}

[[nodiscard]] base::Result<void> fail_option_apply(std::string message) {
    return base::Result<void>::fail(cli_argument_error(std::move(message)));
}

[[nodiscard]] const OptionSpec* find_exact_option(const std::string_view spelling) noexcept {
    for (const OptionSpec& spec : OPTION_SPECS) {
        if (spec.spelling == spelling) {
            return &spec;
        }
    }
    return nullptr;
}

[[nodiscard]] const OptionSpec* find_joined_option(const std::string_view spelling) noexcept {
    for (const OptionSpec& spec : OPTION_SPECS) {
        if (spec.value_style == OptionValueStyle::joined_or_separate &&
            starts_with(spelling, spec.spelling) &&
            spelling.size() > spec.spelling.size()) {
            return &spec;
        }
    }
    return nullptr;
}

[[nodiscard]] bool parse_emit_kind(const std::string_view kind, EmitKind& emit_kind) noexcept {
    struct EmitKindSpec {
        std::string_view spelling;
        EmitKind emit_kind;
    };

    constexpr std::array<EmitKindSpec, 11> EMIT_KIND_SPECS {{
        {"tokens", EmitKind::tokens},
        {"ast", EmitKind::ast},
        {"modules", EmitKind::modules},
        {"checked", EmitKind::checked},
        {"ir", EmitKind::ir},
        {"llvm-ir", EmitKind::llvm_ir},
        {"check", EmitKind::check},
        {"asm", EmitKind::assembly},
        {"obj", EmitKind::object},
        {"object", EmitKind::object},
        {"exe", EmitKind::executable},
    }};

    for (const EmitKindSpec& spec : EMIT_KIND_SPECS) {
        if (spec.spelling == kind) {
            emit_kind = spec.emit_kind;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool parse_optimization_level(
    const std::string_view level,
    ir::OptimizationLevel& optimization_level
) noexcept {
    struct OptimizationLevelSpec {
        std::string_view spelling;
        ir::OptimizationLevel level;
    };

    constexpr std::array<OptimizationLevelSpec, 8> OPTIMIZATION_LEVEL_SPECS {{
        {"0", ir::OptimizationLevel::none},
        {"O0", ir::OptimizationLevel::none},
        {"1", ir::OptimizationLevel::basic},
        {"O1", ir::OptimizationLevel::basic},
        {"2", ir::OptimizationLevel::standard},
        {"O2", ir::OptimizationLevel::standard},
        {"3", ir::OptimizationLevel::aggressive},
        {"O3", ir::OptimizationLevel::aggressive},
    }};

    for (const OptimizationLevelSpec& spec : OPTIMIZATION_LEVEL_SPECS) {
        if (spec.spelling == level) {
            optimization_level = spec.level;
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::filesystem::path inferred_native_output_path(
    const std::filesystem::path& input_path,
    const EmitKind emit_kind
) {
    std::filesystem::path output = input_path.filename();
    output.replace_extension(emit_kind == EmitKind::assembly ? CLI_OUTPUT_ASSEMBLY_EXTENSION : CLI_OUTPUT_OBJECT_EXTENSION);
    return output;
}

class CliParser final {
public:
    explicit CliParser(const std::span<const std::string_view> arguments) noexcept
        : arguments_(arguments) {}

    [[nodiscard]] base::Result<CliParseResult> parse() {
        CliParseResult result;
        result.invocation.tool_path = this->tool_name();

        for (this->index_ = 1; this->index_ < this->arguments_.size(); ++this->index_) {
            const std::string_view arg = this->arguments_[this->index_];
            if (this->is_input_argument(arg)) {
                auto input_result = this->set_input_path(result, arg);
                if (!input_result) {
                    return base::Result<CliParseResult>::fail(input_result.error());
                }
                continue;
            }

            auto option_result = this->parse_current_option(arg);
            if (!option_result) {
                return base::Result<CliParseResult>::fail(option_result.error());
            }
            auto apply_result = this->apply_option(result, option_result.value());
            if (!apply_result) {
                return base::Result<CliParseResult>::fail(apply_result.error());
            }
            if (result.action != CliAction::compile) {
                return base::Result<CliParseResult>::ok(std::move(result));
            }
        }

        if (result.invocation.input_path.empty()) {
            return fail_cli_parse("missing input file");
        }
        if (this->infer_native_output_ && result.invocation.output_path.empty()) {
            result.invocation.output_path =
                inferred_native_output_path(result.invocation.input_path, result.invocation.emit_kind);
        }
        return base::Result<CliParseResult>::ok(std::move(result));
    }

private:
    [[nodiscard]] std::string_view tool_name() const noexcept {
        return this->arguments_.empty() ? CLI_DEFAULT_TOOL_NAME : this->arguments_.front();
    }

    [[nodiscard]] bool is_input_argument(const std::string_view arg) const noexcept {
        return this->after_option_end_ || arg.empty() || arg.front() != '-';
    }

    [[nodiscard]] base::Result<void> set_input_path(
        CliParseResult& result,
        const std::string_view path
    ) const {
        if (!result.invocation.input_path.empty()) {
            return fail_option_apply(std::string(DRIVER_MULTIPLE_INPUT_FILES_UNSUPPORTED));
        }
        result.invocation.input_path = path;
        return base::Result<void>::ok();
    }

    [[nodiscard]] base::Result<std::string_view> take_value(const OptionSpec& spec) {
        if (this->index_ + 1 >= this->arguments_.size()) {
            return base::Result<std::string_view>::fail(cli_argument_error(cli_missing_value_message(spec.spelling)));
        }
        ++this->index_;
        return base::Result<std::string_view>::ok(this->arguments_[this->index_]);
    }

    [[nodiscard]] base::Result<ParsedOption> parse_current_option(const std::string_view arg) {
        if (const auto equal = arg.find('='); equal != std::string_view::npos && is_long_option(arg)) {
            return this->parse_equal_option(arg, equal);
        }
        if (const OptionSpec* spec = find_exact_option(arg); spec != nullptr) {
            return this->parse_exact_option(*spec);
        }
        if (const OptionSpec* spec = find_joined_option(arg); spec != nullptr) {
            return base::Result<ParsedOption>::ok(ParsedOption {spec, arg.substr(spec->spelling.size())});
        }
        return fail_option_parse(cli_unknown_option_message(arg));
    }

    [[nodiscard]] base::Result<ParsedOption> parse_equal_option(
        const std::string_view arg,
        const std::size_t equal
    ) const {
        const std::string_view spelling = arg.substr(0, equal);
        const OptionSpec* spec = find_exact_option(spelling);
        if (spec == nullptr) {
            return fail_option_parse(cli_unknown_option_message(spelling));
        }
        if (spec->value_style == OptionValueStyle::flag) {
            return fail_option_parse(cli_unexpected_value_message(spelling));
        }
        return base::Result<ParsedOption>::ok(ParsedOption {spec, arg.substr(equal + 1)});
    }

    [[nodiscard]] base::Result<ParsedOption> parse_exact_option(const OptionSpec& spec) {
        if (spec.value_style == OptionValueStyle::flag) {
            return base::Result<ParsedOption>::ok(ParsedOption {&spec, {}});
        }
        auto value_result = this->take_value(spec);
        if (!value_result) {
            return base::Result<ParsedOption>::fail(value_result.error());
        }
        return base::Result<ParsedOption>::ok(ParsedOption {&spec, value_result.value()});
    }

    [[nodiscard]] base::Result<void> apply_option(
        CliParseResult& result,
        const ParsedOption& option
    ) {
        switch (option.spec->id) {
        case OptionId::end_options:
            this->after_option_end_ = true;
            return base::Result<void>::ok();
        case OptionId::help:
            result.action = CliAction::help;
            return base::Result<void>::ok();
        case OptionId::version:
            result.action = CliAction::version;
            return base::Result<void>::ok();
        case OptionId::check:
            result.invocation.emit_kind = EmitKind::check;
            return base::Result<void>::ok();
        case OptionId::compile_only:
            result.invocation.emit_kind = EmitKind::object;
            this->infer_native_output_ = true;
            return base::Result<void>::ok();
        case OptionId::assembly_only:
            result.invocation.emit_kind = EmitKind::assembly;
            this->infer_native_output_ = true;
            return base::Result<void>::ok();
        case OptionId::dump_tokens:
            result.invocation.emit_kind = EmitKind::tokens;
            return base::Result<void>::ok();
        case OptionId::dump_ast:
            result.invocation.emit_kind = EmitKind::ast;
            return base::Result<void>::ok();
        case OptionId::dump_modules:
            result.invocation.emit_kind = EmitKind::modules;
            return base::Result<void>::ok();
        case OptionId::dump_checked:
            result.invocation.emit_kind = EmitKind::checked;
            return base::Result<void>::ok();
        case OptionId::dump_ir:
            result.invocation.emit_kind = EmitKind::ir;
            return base::Result<void>::ok();
        case OptionId::dump_llvm_ir:
            result.invocation.emit_kind = EmitKind::llvm_ir;
            return base::Result<void>::ok();
        case OptionId::emit:
            return this->apply_emit_kind(result, option.value);
        case OptionId::output:
            result.invocation.output_path = option.value;
            return base::Result<void>::ok();
        case OptionId::import_path:
            result.invocation.import_paths.push_back(std::filesystem::path(option.value));
            return base::Result<void>::ok();
        case OptionId::clang:
            result.invocation.clang_path = option.value;
            return base::Result<void>::ok();
        case OptionId::clang_arg:
            result.invocation.clang_args.push_back(std::string(option.value));
            return base::Result<void>::ok();
        case OptionId::opt_level:
            return this->apply_optimization_level(result, option.value);
        }
        return fail_option_apply("unsupported option");
    }

    [[nodiscard]] base::Result<void> apply_emit_kind(
        CliParseResult& result,
        const std::string_view kind
    ) const {
        EmitKind emit_kind = result.invocation.emit_kind;
        if (!parse_emit_kind(kind, emit_kind)) {
            return fail_option_apply(cli_invalid_emit_kind_message(kind));
        }
        result.invocation.emit_kind = emit_kind;
        return base::Result<void>::ok();
    }

    [[nodiscard]] base::Result<void> apply_optimization_level(
        CliParseResult& result,
        const std::string_view level
    ) const {
        if (!parse_optimization_level(level, result.invocation.optimization_level)) {
            return fail_option_apply(driver_invalid_optimization_level_message(level));
        }
        return base::Result<void>::ok();
    }

    std::span<const std::string_view> arguments_;
    std::size_t index_ = 0;
    bool after_option_end_ = false;
    bool infer_native_output_ = false;
};

} // namespace

base::Result<CliParseResult> parse_cli_arguments(const std::span<const std::string_view> arguments) {
    CliParser parser(arguments);
    return parser.parse();
}

void print_cli_usage(std::ostream& out, const std::string_view tool_name) {
    const std::string_view displayed_tool = tool_name.empty() ? CLI_DEFAULT_TOOL_NAME : tool_name;
    out << "usage: " << displayed_tool << " [options] input.ax [-o output]\n"
        << "options:\n";
    for (const OptionSpec& spec : OPTION_SPECS) {
        if (!spec.show_in_help) {
            continue;
        }
        out << "  " << spec.spelling;
        if (!spec.value_name.empty()) {
            out << ' ' << spec.value_name;
        }
        const std::size_t option_width = spec.spelling.size() + (spec.value_name.empty() ? 0 : 1 + spec.value_name.size());
        const std::size_t padding = option_width < CLI_HELP_OPTION_COLUMN_WIDTH ?
            CLI_HELP_OPTION_COLUMN_WIDTH - option_width :
            1;
        for (std::size_t i = 0; i < padding; ++i) {
            out << ' ';
        }
        out << spec.help << "\n";
    }
}

int run_cli(
    const std::span<const std::string_view> arguments,
    std::ostream& out,
    std::ostream& err
) {
    auto parse_result = parse_cli_arguments(arguments);
    const std::string_view tool_name = arguments.empty() ? CLI_DEFAULT_TOOL_NAME : arguments.front();
    if (!parse_result) {
        err << parse_result.error().message << "\n";
        print_cli_usage(err, tool_name);
        return CLI_ARGUMENT_ERROR_EXIT_CODE;
    }

    const CliParseResult& parsed = parse_result.value();
    if (parsed.action == CliAction::help) {
        print_cli_usage(out, tool_name);
        return CLI_SUCCESS_EXIT_CODE;
    }
    if (parsed.action == CliAction::version) {
        out << base::config::AUREX_VERSION_STRING << "\n";
        return CLI_SUCCESS_EXIT_CODE;
    }

    Compiler compiler;
    auto compile_result = compiler.run(parsed.invocation);
    if (!compile_result) {
        err << DRIVER_ERROR_PREFIX << compile_result.error().message << "\n";
        return CLI_COMPILATION_FAILURE_EXIT_CODE;
    }
    return CLI_SUCCESS_EXIT_CODE;
}

} // namespace aurex::driver

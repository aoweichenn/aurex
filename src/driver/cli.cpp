#include <aurex/base/config.hpp>
#include <aurex/driver/cli.hpp>
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
inline constexpr std::string_view CLI_HELP_GROUP_INDENT = "  ";
inline constexpr std::string_view CLI_HELP_OPTION_INDENT = "    ";
inline constexpr std::string_view CLI_OUTPUT_OBJECT_EXTENSION = ".o";
inline constexpr std::string_view CLI_OUTPUT_ASSEMBLY_EXTENSION = ".s";
inline constexpr std::string_view CLI_LONG_OPTION_PREFIX = "--";
inline constexpr char CLI_JSON_QUOTE = '"';
inline constexpr char CLI_JSON_BACKSLASH = '\\';
inline constexpr char CLI_JSON_NEWLINE = '\n';
inline constexpr char CLI_JSON_CARRIAGE_RETURN = '\r';
inline constexpr char CLI_JSON_TAB = '\t';
inline constexpr unsigned int CLI_JSON_CONTROL_CHAR_LIMIT = 0x20U;
inline constexpr unsigned int CLI_JSON_NIBBLE_BITS = 4U;
inline constexpr unsigned int CLI_JSON_LOW_NIBBLE_MASK = 0x0fU;
inline constexpr char CLI_JSON_HEX_DIGITS[] = "0123456789abcdef";

enum class OptionLevel {
    primary,
    secondary,
};

enum class OptionGroup {
    general,
    primary_action,
    frontend_debug_output,
    output,
    search_path,
    incremental,
    diagnostics,
    profiling,
    optimization,
    native_backend,
};

enum class OptionValueStyle {
    flag,
    separate,
    joined_or_separate,
};

enum class OptionApplicability {
    any,
    native_output,
};

enum class OptionConflictGroup {
    none,
    primary_action,
};

enum class OptionEffectKind {
    end_options,
    set_cli_action,
    set_emit_kind,
    parse_emit_kind,
    set_output_path,
    append_import_path,
    set_incremental_cache_path,
    enable_query_pruning,
    disable_query_pruning,
    set_profile_output_path,
    set_clang_path,
    append_clang_arg,
    parse_diagnostic_output_format,
    parse_optimization_level,
};

struct OptionEffect {
    OptionEffectKind kind;
    CliAction action = CliAction::compile;
    EmitKind emit_kind = EmitKind::executable;
    bool infer_native_output = false;
    OptionConflictGroup conflict_group = OptionConflictGroup::none;
};

struct OptionSpec {
    OptionLevel level;
    OptionGroup group;
    OptionApplicability applicability;
    std::string_view spelling;
    OptionValueStyle value_style;
    OptionEffect effect;
    std::string_view value_name;
    std::string_view help;
    bool show_in_help = true;
};

struct OptionLevelSpec {
    OptionLevel level;
    std::string_view title;
};

struct OptionGroupSpec {
    OptionLevel level;
    OptionGroup group;
    std::string_view title;
};

inline constexpr OptionEffect CLI_EFFECT_END_OPTIONS{OptionEffectKind::end_options};
inline constexpr OptionEffect CLI_EFFECT_HELP{OptionEffectKind::set_cli_action, CliAction::help};
inline constexpr OptionEffect CLI_EFFECT_VERSION{OptionEffectKind::set_cli_action, CliAction::version};
inline constexpr OptionEffect CLI_EFFECT_EMIT_CHECK{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::check,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_OBJECT_INFER{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::object,
    true,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_ASSEMBLY_INFER{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::assembly,
    true,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_TOKENS{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::tokens,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_LOSSLESS{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::lossless,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_AST{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::ast,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_MODULES{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::modules,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_CHECKED{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::checked,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_IR{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::ir,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_EMIT_LLVM_IR{
    OptionEffectKind::set_emit_kind,
    CliAction::compile,
    EmitKind::llvm_ir,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_PARSE_EMIT_KIND{
    OptionEffectKind::parse_emit_kind,
    CliAction::compile,
    EmitKind::executable,
    false,
    OptionConflictGroup::primary_action,
};
inline constexpr OptionEffect CLI_EFFECT_SET_OUTPUT_PATH{OptionEffectKind::set_output_path};
inline constexpr OptionEffect CLI_EFFECT_APPEND_IMPORT_PATH{OptionEffectKind::append_import_path};
inline constexpr OptionEffect CLI_EFFECT_SET_INCREMENTAL_CACHE_PATH{OptionEffectKind::set_incremental_cache_path};
inline constexpr OptionEffect CLI_EFFECT_ENABLE_QUERY_PRUNING{OptionEffectKind::enable_query_pruning};
inline constexpr OptionEffect CLI_EFFECT_DISABLE_QUERY_PRUNING{OptionEffectKind::disable_query_pruning};
inline constexpr OptionEffect CLI_EFFECT_SET_PROFILE_OUTPUT_PATH{OptionEffectKind::set_profile_output_path};
inline constexpr OptionEffect CLI_EFFECT_SET_CLANG_PATH{OptionEffectKind::set_clang_path};
inline constexpr OptionEffect CLI_EFFECT_APPEND_CLANG_ARG{OptionEffectKind::append_clang_arg};
inline constexpr OptionEffect CLI_EFFECT_PARSE_DIAGNOSTIC_OUTPUT_FORMAT{
    OptionEffectKind::parse_diagnostic_output_format};
inline constexpr OptionEffect CLI_EFFECT_PARSE_OPTIMIZATION_LEVEL{OptionEffectKind::parse_optimization_level};

inline constexpr auto OPTION_LEVEL_SPECS = std::to_array<OptionLevelSpec>({
    {OptionLevel::primary, "primary options"},
    {OptionLevel::secondary, "secondary options"},
});

inline constexpr auto OPTION_GROUP_SPECS = std::to_array<OptionGroupSpec>({
    {OptionLevel::primary, OptionGroup::general, "general"},
    {OptionLevel::primary, OptionGroup::primary_action, "actions"},
    {OptionLevel::primary, OptionGroup::frontend_debug_output, "frontend and debug output"},
    {OptionLevel::secondary, OptionGroup::output, "output"},
    {OptionLevel::secondary, OptionGroup::search_path, "search paths"},
    {OptionLevel::secondary, OptionGroup::incremental, "incremental"},
    {OptionLevel::secondary, OptionGroup::diagnostics, "diagnostics"},
    {OptionLevel::secondary, OptionGroup::profiling, "profiling"},
    {OptionLevel::secondary, OptionGroup::optimization, "optimization"},
    {OptionLevel::secondary, OptionGroup::native_backend, "native backend"},
});

inline constexpr auto OPTION_SPECS = std::to_array<OptionSpec>({
    {
        OptionLevel::primary,
        OptionGroup::general,
        OptionApplicability::any,
        "--",
        OptionValueStyle::flag,
        CLI_EFFECT_END_OPTIONS,
        {},
        {},
        false,
    },
    {
        OptionLevel::primary,
        OptionGroup::general,
        OptionApplicability::any,
        "--help",
        OptionValueStyle::flag,
        CLI_EFFECT_HELP,
        {},
        "show this help text",
    },
    {
        OptionLevel::primary,
        OptionGroup::general,
        OptionApplicability::any,
        "-h",
        OptionValueStyle::flag,
        CLI_EFFECT_HELP,
        {},
        "same as --help",
        false,
    },
    {
        OptionLevel::primary,
        OptionGroup::general,
        OptionApplicability::any,
        "--version",
        OptionValueStyle::flag,
        CLI_EFFECT_VERSION,
        {},
        "print compiler version",
    },
    {
        OptionLevel::primary,
        OptionGroup::primary_action,
        OptionApplicability::any,
        "--check",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_CHECK,
        {},
        "run lexer, parser, and sema without code generation",
    },
    {
        OptionLevel::primary,
        OptionGroup::primary_action,
        OptionApplicability::any,
        "-fsyntax-only",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_CHECK,
        {},
        "same as --check",
    },
    {
        OptionLevel::primary,
        OptionGroup::primary_action,
        OptionApplicability::any,
        "-c",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_OBJECT_INFER,
        {},
        "compile to an object file",
    },
    {
        OptionLevel::primary,
        OptionGroup::primary_action,
        OptionApplicability::any,
        "-S",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_ASSEMBLY_INFER,
        {},
        "compile to assembly",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-tokens",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_TOKENS,
        {},
        "lex only and print tokens",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-ast",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_AST,
        {},
        "parse and print AST",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-lossless",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_LOSSLESS,
        {},
        "lex and print lossless syntax tokens with trivia",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-modules",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_MODULES,
        {},
        "resolve imports and print loaded modules",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-checked",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_CHECKED,
        {},
        "run sema and print checked summary",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-ir",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_IR,
        {},
        "lower to Aurex IR and print it",
    },
    {
        OptionLevel::primary,
        OptionGroup::frontend_debug_output,
        OptionApplicability::any,
        "--dump-llvm-ir",
        OptionValueStyle::flag,
        CLI_EFFECT_EMIT_LLVM_IR,
        {},
        "lower to LLVM IR and print it",
    },
    {
        OptionLevel::primary,
        OptionGroup::primary_action,
        OptionApplicability::any,
        "--emit",
        OptionValueStyle::separate,
        CLI_EFFECT_PARSE_EMIT_KIND,
        "kind",
        "emit kind; examples: --emit=lossless, --emit=ast, --emit=checked, --emit=typed, --emit=ir, "
        "--emit=llvm-ir, --emit=asm, --emit=obj, --emit=exe",
    },
    {
        OptionLevel::secondary,
        OptionGroup::output,
        OptionApplicability::any,
        "-o",
        OptionValueStyle::separate,
        CLI_EFFECT_SET_OUTPUT_PATH,
        "path",
        "set output path",
    },
    {
        OptionLevel::secondary,
        OptionGroup::search_path,
        OptionApplicability::any,
        "-I",
        OptionValueStyle::joined_or_separate,
        CLI_EFFECT_APPEND_IMPORT_PATH,
        "path",
        "add an import search path",
    },
    {
        OptionLevel::secondary,
        OptionGroup::search_path,
        OptionApplicability::any,
        "--import-path",
        OptionValueStyle::separate,
        CLI_EFFECT_APPEND_IMPORT_PATH,
        "path",
        "add an import search path",
    },
    {
        OptionLevel::secondary,
        OptionGroup::incremental,
        OptionApplicability::any,
        "--incremental-cache",
        OptionValueStyle::separate,
        CLI_EFFECT_SET_INCREMENTAL_CACHE_PATH,
        "path",
        "read and write a query-key incremental cache",
    },
    {
        OptionLevel::secondary,
        OptionGroup::incremental,
        OptionApplicability::any,
        "--query-pruning",
        OptionValueStyle::flag,
        CLI_EFFECT_ENABLE_QUERY_PRUNING,
        {},
        "enable query-key pruning for incremental cache (default)",
    },
    {
        OptionLevel::secondary,
        OptionGroup::incremental,
        OptionApplicability::any,
        "--no-query-pruning",
        OptionValueStyle::flag,
        CLI_EFFECT_DISABLE_QUERY_PRUNING,
        {},
        "disable query-key pruning and use coarse source-fingerprint cache reuse",
    },
    {
        OptionLevel::secondary,
        OptionGroup::diagnostics,
        OptionApplicability::any,
        "--diagnostics",
        OptionValueStyle::separate,
        CLI_EFFECT_PARSE_DIAGNOSTIC_OUTPUT_FORMAT,
        "format",
        "diagnostic output format: text or json",
    },
    {
        OptionLevel::secondary,
        OptionGroup::profiling,
        OptionApplicability::any,
        "--profile-output",
        OptionValueStyle::separate,
        CLI_EFFECT_SET_PROFILE_OUTPUT_PATH,
        "path",
        "write compiler phase timing and RSS profile JSON",
    },
    {
        OptionLevel::secondary,
        OptionGroup::native_backend,
        OptionApplicability::native_output,
        "--clang",
        OptionValueStyle::separate,
        CLI_EFFECT_SET_CLANG_PATH,
        "path",
        "clang executable to use for native output",
    },
    {
        OptionLevel::secondary,
        OptionGroup::native_backend,
        OptionApplicability::native_output,
        "--clang-arg",
        OptionValueStyle::separate,
        CLI_EFFECT_APPEND_CLANG_ARG,
        "arg",
        "pass one raw argument to clang; repeat as needed",
    },
    {
        OptionLevel::secondary,
        OptionGroup::optimization,
        OptionApplicability::any,
        "--opt-level",
        OptionValueStyle::separate,
        CLI_EFFECT_PARSE_OPTIMIZATION_LEVEL,
        "n",
        "run Aurex IR passes at O0, O1, O2, or O3 (default O0)",
    },
    {
        OptionLevel::secondary,
        OptionGroup::optimization,
        OptionApplicability::any,
        "-O",
        OptionValueStyle::joined_or_separate,
        CLI_EFFECT_PARSE_OPTIMIZATION_LEVEL,
        "n",
        "same as --opt-level n; joined forms -O0, -O1, -O2, and -O3 are accepted",
    },
});

struct ParsedOption {
    const OptionSpec* spec = nullptr;
    std::string_view value;
};

[[nodiscard]] bool starts_with(const std::string_view value, const std::string_view prefix) noexcept
{
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool is_long_option(const std::string_view value) noexcept
{
    return starts_with(value, CLI_LONG_OPTION_PREFIX);
}

[[nodiscard]] std::string cli_argument_error_message(const std::string_view message)
{
    return "argument error: " + std::string(message);
}

[[nodiscard]] std::string cli_missing_value_message(const std::string_view option)
{
    return "missing value for option: " + std::string(option);
}

[[nodiscard]] std::string cli_unknown_option_message(const std::string_view option)
{
    return "unknown option: " + std::string(option);
}

[[nodiscard]] std::string cli_unexpected_value_message(const std::string_view option)
{
    return "option does not take a value: " + std::string(option);
}

[[nodiscard]] std::string cli_invalid_emit_kind_message(const std::string_view kind)
{
    return "invalid emit kind: " + std::string(kind);
}

[[nodiscard]] std::string cli_invalid_diagnostic_format_message(const std::string_view format)
{
    return "invalid diagnostic output format: " + std::string(format);
}

[[nodiscard]] std::string cli_inapplicable_option_message(const std::string_view option)
{
    return "option requires native output: " + std::string(option);
}

[[nodiscard]] base::Error cli_argument_error(const std::string_view message)
{
    return {base::ErrorCode::invalid_source, cli_argument_error_message(message)};
}

[[nodiscard]] base::Result<CliParseResult> fail_cli_parse(const std::string_view message)
{
    return base::Result<CliParseResult>::fail(cli_argument_error(message));
}

[[nodiscard]] base::Result<ParsedOption> fail_option_parse(const std::string_view message)
{
    return base::Result<ParsedOption>::fail(cli_argument_error(message));
}

[[nodiscard]] base::Result<void> fail_option_apply(const std::string_view message)
{
    return base::Result<void>::fail(cli_argument_error(message));
}

[[nodiscard]] const OptionSpec* find_exact_option(const std::string_view spelling) noexcept
{
    for (const OptionSpec& spec : OPTION_SPECS) {
        if (spec.spelling == spelling) {
            return &spec;
        }
    }
    return nullptr;
}

[[nodiscard]] const OptionSpec* find_joined_option(const std::string_view spelling) noexcept
{
    for (const OptionSpec& spec : OPTION_SPECS) {
        if (spec.value_style == OptionValueStyle::joined_or_separate && starts_with(spelling, spec.spelling)
            && spelling.size() > spec.spelling.size()) {
            return &spec;
        }
    }
    return nullptr;
}

[[nodiscard]] bool parse_emit_kind(const std::string_view kind, EmitKind& emit_kind) noexcept
{
    struct EmitKindSpec {
        std::string_view spelling;
        EmitKind emit_kind;
    };

    constexpr auto EMIT_KIND_SPECS = std::to_array<EmitKindSpec>({
        {"tokens", EmitKind::tokens},
        {"lossless", EmitKind::lossless},
        {"ast", EmitKind::ast},
        {"modules", EmitKind::modules},
        {"checked", EmitKind::checked},
        {"typed", EmitKind::typed},
        {"ir", EmitKind::ir},
        {"llvm-ir", EmitKind::llvm_ir},
        {"check", EmitKind::check},
        {"asm", EmitKind::assembly},
        {"obj", EmitKind::object},
        {"object", EmitKind::object},
        {"exe", EmitKind::executable},
    });

    for (const EmitKindSpec& spec : EMIT_KIND_SPECS) {
        if (spec.spelling == kind) {
            emit_kind = spec.emit_kind;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool parse_optimization_level(
    const std::string_view level, ir::OptimizationLevel& optimization_level) noexcept
{
    struct OptimizationLevelSpec {
        std::string_view spelling;
        ir::OptimizationLevel level;
    };

    constexpr auto OPTIMIZATION_LEVEL_SPECS = std::to_array<OptimizationLevelSpec>({
        {"0", ir::OptimizationLevel::none},
        {"O0", ir::OptimizationLevel::none},
        {"1", ir::OptimizationLevel::basic},
        {"O1", ir::OptimizationLevel::basic},
        {"2", ir::OptimizationLevel::standard},
        {"O2", ir::OptimizationLevel::standard},
        {"3", ir::OptimizationLevel::aggressive},
        {"O3", ir::OptimizationLevel::aggressive},
    });

    for (const OptimizationLevelSpec& spec : OPTIMIZATION_LEVEL_SPECS) {
        if (spec.spelling == level) {
            optimization_level = spec.level;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool parse_diagnostic_output_format(
    const std::string_view format, DiagnosticOutputFormat& diagnostic_format) noexcept
{
    if (format == "text") {
        diagnostic_format = DiagnosticOutputFormat::text;
        return true;
    }
    if (format == "json") {
        diagnostic_format = DiagnosticOutputFormat::json;
        return true;
    }
    return false;
}

[[nodiscard]] bool is_native_emit_kind(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::assembly || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] bool compiler_error_already_printed_diagnostics(const base::ErrorCode code) noexcept
{
    return code == base::ErrorCode::lex_error || code == base::ErrorCode::parse_error
        || code == base::ErrorCode::sema_error;
}

void print_cli_json_escaped(std::ostream& out, const std::string_view text)
{
    out << CLI_JSON_QUOTE;
    for (const unsigned char byte : text) {
        switch (byte) {
            case CLI_JSON_QUOTE:
                out << "\\\"";
                break;
            case CLI_JSON_BACKSLASH:
                out << "\\\\";
                break;
            case CLI_JSON_NEWLINE:
                out << "\\n";
                break;
            case CLI_JSON_CARRIAGE_RETURN:
                out << "\\r";
                break;
            case CLI_JSON_TAB:
                out << "\\t";
                break;
            default:
                if (byte < CLI_JSON_CONTROL_CHAR_LIMIT) {
                    out << "\\u00" << CLI_JSON_HEX_DIGITS[(byte >> CLI_JSON_NIBBLE_BITS) & CLI_JSON_LOW_NIBBLE_MASK]
                        << CLI_JSON_HEX_DIGITS[byte & CLI_JSON_LOW_NIBBLE_MASK];
                } else {
                    out << static_cast<char>(byte);
                }
                break;
        }
    }
    out << CLI_JSON_QUOTE;
}

void print_cli_driver_error_json(std::ostream& err, const std::string_view message)
{
    err << "{\n";
    err << "  \"format\": \"aurex-diagnostics-v1\",\n";
    err << "  \"diagnostics\": [\n";
    err << "    {\n";
    err << "      \"severity\": \"fatal\",\n";
    err << "      \"category\": \"general\",\n";
    err << "      \"code\": \"none\",\n";
    err << "      \"message\": ";
    print_cli_json_escaped(err, message);
    err << ",\n";
    err << "      \"range\": null\n";
    err << "    }\n";
    err << "  ],\n";
    err << "  \"suppressed\": 0\n";
    err << "}\n";
}

[[nodiscard]] std::filesystem::path inferred_native_output_path(
    const std::filesystem::path& input_path, const EmitKind emit_kind)
{
    std::filesystem::path output = input_path.filename();
    output.replace_extension(
        emit_kind == EmitKind::assembly ? CLI_OUTPUT_ASSEMBLY_EXTENSION : CLI_OUTPUT_OBJECT_EXTENSION);
    return output;
}

class CliParser final {
public:
    explicit CliParser(const std::span<const std::string_view> arguments) noexcept : arguments_(arguments)
    {
    }

    [[nodiscard]] base::Result<CliParseResult> parse()
    {
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
        auto applicability_result = this->validate_applicability(result);
        if (!applicability_result) {
            return base::Result<CliParseResult>::fail(applicability_result.error());
        }
        if (this->infer_native_output_ && result.invocation.output_path.empty()
            && is_native_emit_kind(result.invocation.emit_kind)) {
            result.invocation.output_path =
                inferred_native_output_path(result.invocation.input_path, result.invocation.emit_kind);
        }
        return base::Result<CliParseResult>::ok(std::move(result));
    }

private:
    [[nodiscard]] std::string_view tool_name() const noexcept
    {
        return this->arguments_.empty() ? CLI_DEFAULT_TOOL_NAME : this->arguments_.front();
    }

    [[nodiscard]] bool is_input_argument(const std::string_view arg) const noexcept
    {
        return this->after_option_end_ || arg.empty() || arg.front() != '-';
    }

    [[nodiscard]] base::Result<void> set_input_path(CliParseResult& result, const std::string_view path) const
    {
        if (!result.invocation.input_path.empty()) {
            return fail_option_apply(DRIVER_MULTIPLE_INPUT_FILES_UNSUPPORTED);
        }
        result.invocation.input_path = path;
        return base::Result<void>::ok();
    }

    void record_applicability(const OptionSpec& spec) noexcept
    {
        if (spec.applicability == OptionApplicability::native_output && !this->has_native_backend_option_) {
            this->has_native_backend_option_ = true;
            this->first_native_backend_option_ = spec.spelling;
        }
    }

    [[nodiscard]] base::Result<void> validate_applicability(const CliParseResult& result) const
    {
        if (this->has_native_backend_option_ && !is_native_emit_kind(result.invocation.emit_kind)) {
            return fail_option_apply(cli_inapplicable_option_message(this->first_native_backend_option_));
        }
        return base::Result<void>::ok();
    }

    [[nodiscard]] base::Result<std::string_view> take_value(const OptionSpec& spec)
    {
        if (this->index_ + 1 >= this->arguments_.size()) {
            return base::Result<std::string_view>::fail(cli_argument_error(cli_missing_value_message(spec.spelling)));
        }
        ++this->index_;
        return base::Result<std::string_view>::ok(this->arguments_[this->index_]);
    }

    [[nodiscard]] base::Result<ParsedOption> parse_current_option(const std::string_view arg)
    {
        if (const auto equal = arg.find('='); equal != std::string_view::npos && is_long_option(arg)) {
            return this->parse_equal_option(arg, equal);
        }
        if (const OptionSpec* spec = find_exact_option(arg); spec != nullptr) {
            return this->parse_exact_option(*spec);
        }
        if (const OptionSpec* spec = find_joined_option(arg); spec != nullptr) {
            return base::Result<ParsedOption>::ok(ParsedOption{spec, arg.substr(spec->spelling.size())});
        }
        return fail_option_parse(cli_unknown_option_message(arg));
    }

    [[nodiscard]] base::Result<ParsedOption> parse_equal_option(
        const std::string_view arg, const std::size_t equal) const
    {
        const std::string_view spelling = arg.substr(0, equal);
        const OptionSpec* spec = find_exact_option(spelling);
        if (spec == nullptr) {
            return fail_option_parse(cli_unknown_option_message(spelling));
        }
        if (spec->value_style == OptionValueStyle::flag) {
            return fail_option_parse(cli_unexpected_value_message(spelling));
        }
        return base::Result<ParsedOption>::ok(ParsedOption{spec, arg.substr(equal + 1)});
    }

    [[nodiscard]] base::Result<ParsedOption> parse_exact_option(const OptionSpec& spec)
    {
        if (spec.value_style == OptionValueStyle::flag) {
            return base::Result<ParsedOption>::ok(ParsedOption{&spec, {}});
        }
        auto value_result = this->take_value(spec);
        if (!value_result) {
            return base::Result<ParsedOption>::fail(value_result.error());
        }
        return base::Result<ParsedOption>::ok(ParsedOption{&spec, value_result.value()});
    }

    [[nodiscard]] base::Result<void> apply_option(CliParseResult& result, const ParsedOption& option)
    {
        this->record_applicability(*option.spec);

        const OptionEffect& effect = option.spec->effect;
        if (effect.conflict_group == OptionConflictGroup::primary_action) {
            this->infer_native_output_ = false;
        }
        switch (effect.kind) {
            case OptionEffectKind::end_options:
                this->after_option_end_ = true;
                return base::Result<void>::ok();
            case OptionEffectKind::set_cli_action:
                result.action = effect.action;
                return base::Result<void>::ok();
            case OptionEffectKind::set_emit_kind:
                result.invocation.emit_kind = effect.emit_kind;
                this->infer_native_output_ = effect.infer_native_output;
                return base::Result<void>::ok();
            case OptionEffectKind::parse_emit_kind:
                return this->apply_emit_kind(result, option.value);
            case OptionEffectKind::set_output_path:
                result.invocation.output_path = option.value;
                return base::Result<void>::ok();
            case OptionEffectKind::append_import_path:
                result.invocation.import_paths.push_back(std::filesystem::path(option.value));
                return base::Result<void>::ok();
            case OptionEffectKind::set_incremental_cache_path:
                result.invocation.incremental_cache_path = std::filesystem::path(option.value);
                return base::Result<void>::ok();
            case OptionEffectKind::enable_query_pruning:
                result.invocation.query_pruning_enabled = true;
                return base::Result<void>::ok();
            case OptionEffectKind::disable_query_pruning:
                result.invocation.query_pruning_enabled = false;
                return base::Result<void>::ok();
            case OptionEffectKind::set_profile_output_path:
                result.invocation.profile_output_path = std::filesystem::path(option.value);
                return base::Result<void>::ok();
            case OptionEffectKind::set_clang_path:
                result.invocation.clang_path = option.value;
                return base::Result<void>::ok();
            case OptionEffectKind::append_clang_arg:
                result.invocation.clang_args.push_back(std::string(option.value));
                return base::Result<void>::ok();
            case OptionEffectKind::parse_diagnostic_output_format:
                return this->apply_diagnostic_output_format(result, option.value);
            case OptionEffectKind::parse_optimization_level:
                return this->apply_optimization_level(result, option.value);
        }
        return fail_option_apply("unsupported option");
    }

    [[nodiscard]] base::Result<void> apply_emit_kind(CliParseResult& result, const std::string_view kind)
    {
        EmitKind emit_kind = result.invocation.emit_kind;
        if (!parse_emit_kind(kind, emit_kind)) {
            return fail_option_apply(cli_invalid_emit_kind_message(kind));
        }
        result.invocation.emit_kind = emit_kind;
        this->infer_native_output_ = false;
        return base::Result<void>::ok();
    }

    [[nodiscard]] base::Result<void> apply_optimization_level(
        CliParseResult& result, const std::string_view level) const
    {
        if (!parse_optimization_level(level, result.invocation.optimization_level)) {
            return fail_option_apply(driver_invalid_optimization_level_message(level));
        }
        return base::Result<void>::ok();
    }

    [[nodiscard]] base::Result<void> apply_diagnostic_output_format(
        CliParseResult& result, const std::string_view format) const
    {
        if (!parse_diagnostic_output_format(format, result.invocation.diagnostic_format)) {
            return fail_option_apply(cli_invalid_diagnostic_format_message(format));
        }
        return base::Result<void>::ok();
    }

    std::span<const std::string_view> arguments_;
    std::size_t index_ = 0;
    bool after_option_end_ = false;
    bool infer_native_output_ = false;
    bool has_native_backend_option_ = false;
    std::string_view first_native_backend_option_;
};

} // namespace

base::Result<CliParseResult> parse_cli_arguments(const std::span<const std::string_view> arguments)
{
    CliParser parser(arguments);
    return parser.parse();
}

void print_cli_usage(std::ostream& out, const std::string_view tool_name)
{
    const std::string_view displayed_tool = tool_name.empty() ? CLI_DEFAULT_TOOL_NAME : tool_name;
    out << "usage: " << displayed_tool << " [primary-option] [secondary-options] input.ax [-o output]\n";
    for (const OptionLevelSpec& level : OPTION_LEVEL_SPECS) {
        out << "\n" << level.title << ":\n";
        for (const OptionGroupSpec& group : OPTION_GROUP_SPECS) {
            if (group.level != level.level) {
                continue;
            }
            out << CLI_HELP_GROUP_INDENT << group.title << ":\n";
            for (const OptionSpec& spec : OPTION_SPECS) {
                if (!spec.show_in_help || spec.level != level.level || spec.group != group.group) {
                    continue;
                }
                out << CLI_HELP_OPTION_INDENT << spec.spelling;
                if (!spec.value_name.empty()) {
                    out << ' ' << spec.value_name;
                }
                const std::size_t option_width =
                    spec.spelling.size() + (spec.value_name.empty() ? 0 : 1 + spec.value_name.size());
                const std::size_t padding =
                    option_width < CLI_HELP_OPTION_COLUMN_WIDTH ? CLI_HELP_OPTION_COLUMN_WIDTH - option_width : 1;
                for (std::size_t i = 0; i < padding; ++i) {
                    out << ' ';
                }
                out << spec.help << "\n";
            }
        }
    }
}

int run_cli(const std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err,
    const LlvmIrEmitter llvm_ir_emitter)
{
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

    Compiler compiler(llvm_ir_emitter);
    const auto compile_result = compiler.run(parsed.invocation);
    if (!compile_result) {
        if (parsed.invocation.diagnostic_format == DiagnosticOutputFormat::json) {
            if (!compiler_error_already_printed_diagnostics(compile_result.error().code)) {
                print_cli_driver_error_json(err, compile_result.error().message);
            }
        } else {
            err << DRIVER_ERROR_PREFIX << compile_result.error().message << "\n";
        }
        return CLI_COMPILATION_FAILURE_EXIT_CODE;
    }
    return CLI_SUCCESS_EXIT_CODE;
}

int run_cli(const std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err)
{
    return run_cli(arguments, out, err, nullptr);
}

} // namespace aurex::driver

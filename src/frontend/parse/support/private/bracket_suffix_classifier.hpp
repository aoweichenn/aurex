#pragma once

#include <aurex/frontend/parse/expr_context.hpp>
#include <aurex/frontend/parse/parser_part_core.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>

namespace aurex::parse {

enum class BracketSuffixKind : base::u8 {
    index,
    slice,
    generic_apply,
    malformed,
};

struct BracketSuffixClassificationInput final {
    syntax::ExprId base = syntax::INVALID_EXPR_ID;
    bool has_type_only_arg = false;
    bool args_are_type_like = false;
    ExprContext context = ExprContext::normal;
};

struct BracketSuffixDecision final {
    BracketSuffixKind kind = BracketSuffixKind::malformed;
    bool report_type_arg_errors = false;
    bool base_is_type_like = false;
    bool args_are_type_like = false;
    bool has_type_only_arg = false;
};

class BracketSuffixClassifier final : private ParserPartCore {
public:
    explicit BracketSuffixClassifier(Parser& parser) noexcept : ParserPartCore(parser)
    {
    }

    [[nodiscard]] BracketSuffixDecision classify_empty_suffix() const noexcept;
    [[nodiscard]] BracketSuffixDecision classify_slice_suffix(bool start_is_type_only) const noexcept;
    [[nodiscard]] BracketSuffixDecision classify_after_expr(BracketSuffixClassificationInput input) const;
    [[nodiscard]] bool arg_starts_type_only() const noexcept;
    [[nodiscard]] bool arg_expr_is_type_like(syntax::ExprId expr) const;

private:
    [[nodiscard]] bool parenthesized_arg_is_type() const noexcept;
    [[nodiscard]] bool suffix_is_inside_generic_continuation() const noexcept;
};

} // namespace aurex::parse

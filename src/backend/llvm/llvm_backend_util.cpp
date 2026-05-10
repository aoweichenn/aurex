#include <backend/llvm/llvm_backend_internal.hpp>

#include <aurex/base/string_literal.hpp>

#include <charconv>
#include <utility>

namespace aurex::backend {

bool parse_u64(const std::string& text, std::uint64_t& out) noexcept {
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
    }
    std::string digits;
    digits.reserve(text.size());
    for (std::size_t i = base == 10 ? 0U : 2U; i < text.size(); ++i) {
        if (text[i] != '_') {
            digits.push_back(text[i]);
        }
    }
    if (digits.empty()) {
        return false;
    }
    const char* begin = digits.data();
    const char* end = digits.data() + digits.size();
    const auto result = std::from_chars(begin, end, out, base);
    return result.ec == std::errc {} && result.ptr == end;
}

bool parse_f64(const std::string& text, double& out) noexcept {
    std::string digits;
    digits.reserve(text.size());
    for (const char c : text) {
        if (c != '_') {
            digits.push_back(c);
        }
    }
    const char* begin = digits.data();
    const char* end = digits.data() + digits.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc {} && result.ptr == end;
}

std::string decode_string_literal(const std::string& literal, const bool has_c_prefix) {
    base::StringLiteralDecode decoded = base::decode_string_literal(
        literal,
        has_c_prefix ? base::StringLiteralKind::c_string : base::StringLiteralKind::string
    );
    return std::move(decoded.decoded);
}

std::uint64_t parse_byte_literal(const std::string& literal) {
    std::string text = literal;
    if (!text.empty() && text.front() == 'b') {
        text.erase(text.begin());
    }
    if (text.size() >= 2 && text.front() == '\'' && text.back() == '\'') {
        text = text.substr(1, text.size() - 2);
    }
    if (text.size() >= 2 && text.front() == '\\') {
        switch (text[1]) {
        case '0': return 0;
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        case '\'': return '\'';
        default: return static_cast<unsigned char>(text[1]);
        }
    }
    return text.empty() ? 0 : static_cast<unsigned char>(text.front());
}

} // namespace aurex::backend

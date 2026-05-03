#include "llvm_backend_internal.hpp"

#include <charconv>

namespace aurex::backend {

bool parse_u64(const std::string& text, std::uint64_t& out) noexcept {
    int base = 10;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        begin += 2;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
        begin += 2;
    }
    const auto result = std::from_chars(begin, end, out, base);
    return result.ec == std::errc {};
}

std::string decode_string_literal(const std::string& literal, const bool has_c_prefix) {
    std::string text = has_c_prefix && !literal.empty() && literal.front() == 'c'
        ? literal.substr(1)
        : literal;
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }

    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            decoded.push_back(text[i]);
            continue;
        }
        const char escaped = text[++i];
        switch (escaped) {
        case '0': decoded.push_back('\0'); break;
        case 'n': decoded.push_back('\n'); break;
        case 'r': decoded.push_back('\r'); break;
        case 't': decoded.push_back('\t'); break;
        case '\\': decoded.push_back('\\'); break;
        case '"': decoded.push_back('"'); break;
        default: decoded.push_back(escaped); break;
        }
    }
    return decoded;
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

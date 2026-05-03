#include "nex/scanner.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace nex {
namespace {

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool is_ident_continue(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

class FindScanner {
public:
    explicit FindScanner(std::string_view src) : src_(src) {}

    void scan() {
        while (pos_ < src_.size()) {
            skip_trivia();
            if (pos_ >= src_.size()) break;
            read_token();
        }
    }

    std::string mod_name;
    std::vector<std::string> imports;

private:
    void skip_trivia() {
        while (pos_ < src_.size()) {
            if (is_space(peek())) { advance(); continue; }
            if (peek() == '/' && peek_next() == '/') {
                while (pos_ < src_.size() && peek() != '\n') advance();
                continue;
            }
            if (peek() == '/' && peek_next() == '*') {
                advance(); advance();
                while (pos_ < src_.size() && !(peek() == '*' && peek_next() == '/')) advance();
                if (pos_ < src_.size()) { advance(); advance(); }
                continue;
            }
            break;
        }
    }

    char peek() const { return pos_ < src_.size() ? src_[pos_] : '\0'; }
    char peek_next() const { return pos_ + 1 < src_.size() ? src_[pos_ + 1] : '\0'; }
    char advance() { return pos_ < src_.size() ? src_[pos_++] : '\0'; }

    void read_token() {
        if (is_ident_start(peek())) {
            auto begin = pos_;
            while (is_ident_continue(peek())) advance();
            on_ident(src_.substr(begin, pos_ - begin));
            return;
        }
        auto c = advance();
        switch (c) {
        case '.': on_dot(); break;
        case ';': on_semicolon(); break;
        default: break;
        }
    }

    void on_ident(std::string_view ident) {
        if (state_ == State::normal) {
            if (ident == "module") state_ = State::in_module;
            else if (ident == "import") state_ = State::in_import;
        } else if (state_ == State::in_dot || state_ == State::in_module || state_ == State::in_import) {
            path_.push_back(std::string(ident));
            state_ = (state_ == State::in_dot) ? State::in_path : state_;
        }
        ident_ = ident;
    }

    void on_dot() {
        if (state_ == State::in_path || state_ == State::in_module || state_ == State::in_import) {
            state_ = State::in_dot;
        }
    }

    void on_semicolon() {
        if (state_ == State::in_path || state_ == State::in_module || state_ == State::in_import) {
            if (!ident_.empty() && std::isalpha(static_cast<unsigned char>(ident_.front()))) {
                path_.push_back(std::string(ident_));
            }
        }
        if (state_ == State::in_module && !path_.empty()) {
            mod_name.clear();
            for (size_t i = 0; i < path_.size(); ++i) {
                if (i != 0) mod_name += '.';
                mod_name += path_[i];
            }
        } else if (state_ == State::in_import && !path_.empty()) {
            std::string imp;
            for (size_t i = 0; i < path_.size(); ++i) {
                if (i != 0) imp += '.';
                imp += path_[i];
            }
            imports.push_back(std::move(imp));
        }
        state_ = State::normal;
        path_.clear();
        ident_ = {};
    }

    enum class State { normal, in_module, in_import, in_path, in_dot };
    std::string_view src_;
    size_t pos_ = 0;
    State state_ = State::normal;
    std::string_view ident_;
    std::vector<std::string> path_;
};

} // namespace

ModuleInfo scan_source(std::string_view text) {
    FindScanner scanner(text);
    scanner.scan();
    return {std::move(scanner.mod_name), std::move(scanner.imports)};
}

std::optional<fs::path> resolve_module(
    std::string_view module_path,
    const std::vector<fs::path>& import_dirs)
{
    fs::path rel;
    size_t pos = 0;
    while (pos < module_path.size()) {
        auto dot = module_path.find('.', pos);
        auto seg = module_path.substr(pos, dot - pos);
        rel /= std::string(seg);
        pos = (dot == std::string_view::npos) ? module_path.size() : dot + 1;
    }
    rel += ".ax";

    for (const auto& dir : import_dirs) {
        auto candidate = dir / rel;
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec) {
            return fs::canonical(candidate, ec);
        }
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, fs::path>> discover_modules(
    const std::vector<fs::path>& search_dirs)
{
    std::vector<std::pair<std::string, fs::path>> result;
    for (const auto& dir : search_dirs) {
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir, ec);
             it != fs::recursive_directory_iterator(); ++it) {
            if (it->path().extension() == ".ax") {
                auto source = [] (const fs::path& p) -> std::optional<std::string> {
                    std::ifstream in(p, std::ios::binary);
                    if (!in) return std::nullopt;
                    std::ostringstream buf;
                    buf << in.rdbuf();
                    return buf.str();
                }(it->path());
                if (source) {
                    auto info = scan_source(*source);
                    if (!info.name.empty()) {
                        result.emplace_back(std::move(info.name), it->path());
                    }
                }
            }
        }
    }
    return result;
}

} // namespace nex

#include "scanner.hpp"

#include <cctype>
#include <string_view>

namespace nex {
namespace {

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool is_ident_continue(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }

class FindScanner {
public:
    explicit FindScanner(std::string_view source) : src_(source) {}

    // Called per token. Return true to keep scanning.
    bool consume_token() {
        skip_ws_and_comments();
        if (pos_ >= src_.size()) return false;
        const std::size_t begin = pos_;
        if (is_ident_start(peek())) {
            while (is_ident_continue(peek())) advance();
            on_ident(src_.substr(begin, pos_ - begin));
            return true;
        }
        switch (advance()) {
            case '.':
                on_dot();
                return true;
            case ';':
                on_semicolon();
                return true;
            default:
                return true;
        }
    }

private:
    void skip_ws_and_comments() {
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

    void on_ident(std::string_view ident) {
        ident_ = ident;
    }

    void on_dot() {
        if (in_module_ || in_import_) {
            path_.push_back(std::string(ident_));
        }
        ident_ = {};
    }

    void on_semicolon() {
        if (in_module_) {
            if (!ident_.empty()) {
                path_.push_back(std::string(ident_));
            }
            in_module_ = false;
            if (path_.empty()) {
                path_.clear();
                return;
            }
            for (std::size_t i = 0; i < path_.size(); ++i) {
                if (i != 0) name_ += ".";
                name_ += path_[i];
            }
            path_.clear();
            ident_ = {};
            return;
        }
        if (in_import_) {
            if (!ident_.empty()) {
                path_.push_back(std::string(ident_));
            }
            in_import_ = false;
            if (path_.empty()) {
                path_.clear();
                ident_ = {};
                return;
            }
            std::string import_name;
            for (std::size_t i = 0; i < path_.size(); ++i) {
                if (i != 0) import_name += ".";
                import_name += path_[i];
            }
            imports_.push_back(std::move(import_name));
            path_.clear();
            ident_ = {};
            return;
        }
        path_.clear();
        ident_ = {};
    }

    void on_keyword(std::string_view kw) {
        if (kw == "module") {
            in_module_ = true;
            in_import_ = false;
            path_.clear();
            ident_ = {};
        } else if (kw == "import") {
            in_module_ = false;
            in_import_ = true;
            path_.clear();
            ident_ = {};
        }
    }

    void scan() {
        while (consume_token()) {
            if (!ident_.empty() && !in_module_ && !in_import_) {
                on_keyword(ident_);
                ident_ = {};
            }
        }
    }

    friend SourceInfo nex::scan_source(std::string_view text);

    std::string_view src_;
    std::size_t pos_ = 0;

    std::string_view ident_;
    bool in_module_ = false;
    bool in_import_ = false;
    std::vector<std::string> path_;

    std::string name_;
    std::vector<std::string> imports_;
};

} // namespace

SourceInfo scan_source(std::string_view text) {
    FindScanner scanner(text);
    scanner.scan();
    SourceInfo info;
    info.module_name = std::move(scanner.name_);
    info.imports = std::move(scanner.imports_);
    return info;
}

} // namespace nex

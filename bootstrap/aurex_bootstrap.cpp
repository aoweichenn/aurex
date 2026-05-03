// Aurex 0.1.2 standalone bootstrap compiler.
//
// This file is intentionally standalone: it uses only the C++20 standard
// library and can be built with `make -C bootstrap`. It is not the full
// production compiler. Its job is to keep a tiny, inspectable Stage0 path in
// the repository so contributors can understand the minimum shape of an M0 to
// C translator without first reading the modular compiler.
//
// Supported bootstrap subset:
//   module declarations are skipped
//   extern c { fn name(...) -> T @name("real"); }
//   export c fn main(...) -> i32 { calls; return int; }
//   *mut/*const pointer types, integer primitive types, c"..." literals
//
// The full compiler in src/ remains the authoritative implementation.

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

enum class Tok {
    eof,
    ident,
    int_lit,
    str_lit,
    c_str_lit,
    l_paren,
    r_paren,
    l_brace,
    r_brace,
    comma,
    colon,
    semicolon,
    arrow,
    star,
    at,
    kw_module,
    kw_extern,
    kw_export,
    kw_c,
    kw_fn,
    kw_return,
    kw_mut,
    kw_const,
    kw_i32,
    kw_u8,
};

std::string_view tok_name(Tok tok) {
    switch (tok) {
    case Tok::eof: return "eof";
    case Tok::ident: return "ident";
    case Tok::int_lit: return "int_lit";
    case Tok::str_lit: return "str_lit";
    case Tok::c_str_lit: return "c_str_lit";
    case Tok::l_paren: return "l_paren";
    case Tok::r_paren: return "r_paren";
    case Tok::l_brace: return "l_brace";
    case Tok::r_brace: return "r_brace";
    case Tok::comma: return "comma";
    case Tok::colon: return "colon";
    case Tok::semicolon: return "semicolon";
    case Tok::arrow: return "arrow";
    case Tok::star: return "star";
    case Tok::at: return "at";
    case Tok::kw_module: return "kw_module";
    case Tok::kw_extern: return "kw_extern";
    case Tok::kw_export: return "kw_export";
    case Tok::kw_c: return "kw_c";
    case Tok::kw_fn: return "kw_fn";
    case Tok::kw_return: return "kw_return";
    case Tok::kw_mut: return "kw_mut";
    case Tok::kw_const: return "kw_const";
    case Tok::kw_i32: return "kw_i32";
    case Tok::kw_u8: return "kw_u8";
    }
    return "unknown";
}

struct Token {
    Tok kind = Tok::eof;
    std::string_view text;
};

struct Param {
    std::string name;
    std::string c_type;
};

struct Function {
    std::string name;
    std::string c_name;
    std::string return_type;
    std::vector<Param> params;
    std::vector<std::string> body_lines;
    bool is_extern = false;
    bool is_export = false;
};

bool is_puts_abi(const Function& fn) {
    return fn.c_name == "puts";
}

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}

    std::vector<Token> run() {
        std::vector<Token> tokens;
        while (true) {
            skip_ws_and_comments();
            const std::size_t start = pos_;
            if (pos_ >= source_.size()) {
                tokens.push_back({Tok::eof, source_.substr(pos_, 0)});
                return tokens;
            }
            if (peek() == 'c' && peek_next() == '"') {
                advance();
                advance();
                scan_string();
                tokens.push_back({Tok::c_str_lit, source_.substr(start, pos_ - start)});
                continue;
            }
            if (is_ident_start(peek())) {
                while (is_ident_continue(peek())) {
                    advance();
                }
                const std::string_view text = source_.substr(start, pos_ - start);
                tokens.push_back({keyword(text), text});
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                    advance();
                }
                tokens.push_back({Tok::int_lit, source_.substr(start, pos_ - start)});
                continue;
            }
            const char c = advance();
            switch (c) {
            case '"':
                scan_string();
                tokens.push_back({Tok::str_lit, source_.substr(start, pos_ - start)});
                break;
            case '(':
                tokens.push_back({Tok::l_paren, source_.substr(start, 1)});
                break;
            case ')':
                tokens.push_back({Tok::r_paren, source_.substr(start, 1)});
                break;
            case '{':
                tokens.push_back({Tok::l_brace, source_.substr(start, 1)});
                break;
            case '}':
                tokens.push_back({Tok::r_brace, source_.substr(start, 1)});
                break;
            case ',':
                tokens.push_back({Tok::comma, source_.substr(start, 1)});
                break;
            case ':':
                tokens.push_back({Tok::colon, source_.substr(start, 1)});
                break;
            case ';':
                tokens.push_back({Tok::semicolon, source_.substr(start, 1)});
                break;
            case '*':
                tokens.push_back({Tok::star, source_.substr(start, 1)});
                break;
            case '@':
                tokens.push_back({Tok::at, source_.substr(start, 1)});
                break;
            case '-':
                if (peek() == '>') {
                    advance();
                    tokens.push_back({Tok::arrow, source_.substr(start, 2)});
                }
                break;
            default:
                break;
            }
        }
    }

private:
    static bool is_ident_start(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

    static bool is_ident_continue(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

    static Tok keyword(std::string_view text) {
        if (text == "module") return Tok::kw_module;
        if (text == "extern") return Tok::kw_extern;
        if (text == "export") return Tok::kw_export;
        if (text == "c") return Tok::kw_c;
        if (text == "fn") return Tok::kw_fn;
        if (text == "return") return Tok::kw_return;
        if (text == "mut") return Tok::kw_mut;
        if (text == "const") return Tok::kw_const;
        if (text == "i32") return Tok::kw_i32;
        if (text == "u8") return Tok::kw_u8;
        return Tok::ident;
    }

    char peek() const {
        return pos_ < source_.size() ? source_[pos_] : '\0';
    }

    char peek_next() const {
        const std::size_t next = pos_ + 1;
        return next < source_.size() ? source_[next] : '\0';
    }

    char advance() {
        return pos_ < source_.size() ? source_[pos_++] : '\0';
    }

    void skip_ws_and_comments() {
        while (pos_ < source_.size()) {
            if (std::isspace(static_cast<unsigned char>(peek())) != 0) {
                advance();
                continue;
            }
            if (peek() == '/' && peek_next() == '/') {
                while (pos_ < source_.size() && peek() != '\n') {
                    advance();
                }
                continue;
            }
            break;
        }
    }

    void scan_string() {
        bool escaped = false;
        while (pos_ < source_.size()) {
            const char c = advance();
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                return;
            }
        }
    }

    std::string_view source_;
    std::size_t pos_ = 0;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::vector<Function> parse() {
        while (!check(Tok::eof)) {
            if (match(Tok::kw_module)) {
                while (!check(Tok::semicolon) && !check(Tok::eof)) advance();
                match(Tok::semicolon);
            } else if (match(Tok::kw_extern)) {
                expect(Tok::kw_c);
                expect(Tok::l_brace);
                while (!check(Tok::r_brace) && !check(Tok::eof)) {
                    functions_.push_back(parse_fn(true, false));
                }
                expect(Tok::r_brace);
            } else if (match(Tok::kw_export)) {
                expect(Tok::kw_c);
                functions_.push_back(parse_fn(false, true));
            } else {
                advance();
            }
        }
        return functions_;
    }

private:
    Function parse_fn(bool is_extern, bool is_export) {
        expect(Tok::kw_fn);
        Function fn;
        fn.is_extern = is_extern;
        fn.is_export = is_export;
        fn.name = std::string(expect(Tok::ident).text);
        fn.c_name = fn.name;
        expect(Tok::l_paren);
        if (!check(Tok::r_paren)) {
            do {
                Param param;
                param.name = std::string(expect(Tok::ident).text);
                expect(Tok::colon);
                param.c_type = parse_type();
                fn.params.push_back(std::move(param));
            } while (match(Tok::comma) && !check(Tok::r_paren));
        }
        expect(Tok::r_paren);
        fn.return_type = match(Tok::arrow) ? parse_type() : "void";
        if (match(Tok::at)) {
            expect(Tok::ident);
            expect(Tok::l_paren);
            const Token& name = expect(Tok::str_lit);
            fn.c_name = std::string(name.text.substr(1, name.text.size() - 2));
            expect(Tok::r_paren);
        }
        if (is_extern) {
            expect(Tok::semicolon);
        } else {
            parse_body(fn);
        }
        return fn;
    }

    std::string parse_type() {
        if (match(Tok::star)) {
            const bool is_const = match(Tok::kw_const);
            if (!is_const) {
                expect(Tok::kw_mut);
            }
            std::string inner = parse_type();
            return (is_const ? "const " : "") + inner + "*";
        }
        if (match(Tok::kw_i32)) return "int32_t";
        if (match(Tok::kw_u8)) return "uint8_t";
        const Token& name = expect(Tok::ident);
        return std::string(name.text);
    }

    void parse_body(Function& fn) {
        expect(Tok::l_brace);
        while (!check(Tok::r_brace) && !check(Tok::eof)) {
            if (match(Tok::kw_return)) {
                const Token& value = expect(Tok::int_lit);
                expect(Tok::semicolon);
                fn.body_lines.push_back("return " + std::string(value.text) + ";");
            } else {
                const std::string callee = std::string(expect(Tok::ident).text);
                expect(Tok::l_paren);
                std::string arg;
                if (!check(Tok::r_paren)) {
                    const Token& value = advance();
                    arg = value.kind == Tok::c_str_lit
                        ? "(const uint8_t *)" + std::string(value.text.substr(1))
                        : std::string(value.text);
                }
                expect(Tok::r_paren);
                expect(Tok::semicolon);
                fn.body_lines.push_back(callee + "(" + arg + ");");
            }
        }
        expect(Tok::r_brace);
    }

    bool check(Tok kind) const {
        return peek().kind == kind;
    }

    bool match(Tok kind) {
        if (!check(kind)) return false;
        advance();
        return true;
    }

    const Token& expect(Tok kind) {
        if (!check(kind)) {
            std::cerr << "bootstrap parse error: expected " << tok_name(kind)
                      << ", got " << tok_name(peek().kind)
                      << " `" << peek().text << "`\n";
            std::exit(1);
        }
        return advance();
    }

    const Token& peek() const {
        return tokens_[pos_];
    }

    const Token& advance() {
        if (!check(Tok::eof)) {
            ++pos_;
        }
        return tokens_[pos_ - 1];
    }

    std::vector<Token> tokens_;
    std::vector<Function> functions_;
    std::size_t pos_ = 0;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "failed to open " << path << "\n";
        std::exit(1);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_c(const std::vector<Function>& functions, std::ostream& out) {
    out << "#include <stdint.h>\n#include <stddef.h>\n#include <stdbool.h>\n#include <stdio.h>\n\n";
    out << "/* Generated by standalone bootstrap 0.1.2. */\n\n";
    for (const Function& fn : functions) {
        if (!fn.is_extern) continue;
        if (is_puts_abi(fn)) {
            out << "#undef " << fn.name << "\n";
            out << "#define " << fn.name << "(s) ((int32_t)puts((const char *)(s)))\n";
            continue;
        }
        out << fn.return_type << " " << fn.c_name << "(";
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            if (i != 0) out << ", ";
            out << fn.params[i].c_type << " " << fn.params[i].name;
        }
        out << ");\n";
    }
    for (const Function& fn : functions) {
        if (fn.is_extern) continue;
        const bool is_main = fn.is_export && fn.name == "main";
        out << (is_main ? "static " : "") << fn.return_type << " "
            << (is_main ? "aurex_bootstrap_main" : fn.c_name) << "(";
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            if (i != 0) out << ", ";
            out << fn.params[i].c_type << " " << fn.params[i].name;
        }
        out << ") {\n";
        for (const std::string& line : fn.body_lines) {
            out << "    " << line << "\n";
        }
        out << "}\n\n";
        if (is_main) {
            out << "int main(int argc, char **argv) {\n";
            out << "    return (int)aurex_bootstrap_main((int32_t)argc, (uint8_t **)argv);\n";
            out << "}\n";
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: aurex_bootstrap input.ax [-o output.c]\n";
        return 2;
    }
    std::filesystem::path input = argv[1];
    std::filesystem::path output;
    if (argc == 4 && std::string_view(argv[2]) == "-o") {
        output = argv[3];
    }
    const std::string source_text = read_file(input);
    Lexer lexer(source_text);
    Parser parser(lexer.run());
    const std::vector<Function> functions = parser.parse();
    if (output.empty()) {
        write_c(functions, std::cout);
        return 0;
    }
    std::ofstream out(output, std::ios::binary);
    write_c(functions, out);
    return out ? 0 : 1;
}

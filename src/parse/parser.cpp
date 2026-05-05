#include "aurex/parse/parser.hpp"

#include <charconv>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

[[nodiscard]] bool is_primitive_type_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_void:
    case TokenKind::kw_bool:
    case TokenKind::kw_i8:
    case TokenKind::kw_u8:
    case TokenKind::kw_i16:
    case TokenKind::kw_u16:
    case TokenKind::kw_i32:
    case TokenKind::kw_u32:
    case TokenKind::kw_i64:
    case TokenKind::kw_u64:
    case TokenKind::kw_isize:
    case TokenKind::kw_usize:
    case TokenKind::kw_f32:
    case TokenKind::kw_f64:
    case TokenKind::kw_str:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_path_segment_token(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier || kind == TokenKind::kw_c;
}

[[nodiscard]] syntax::PrimitiveTypeKind primitive_from_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_void: return syntax::PrimitiveTypeKind::void_;
    case TokenKind::kw_bool: return syntax::PrimitiveTypeKind::bool_;
    case TokenKind::kw_i8: return syntax::PrimitiveTypeKind::i8;
    case TokenKind::kw_u8: return syntax::PrimitiveTypeKind::u8;
    case TokenKind::kw_i16: return syntax::PrimitiveTypeKind::i16;
    case TokenKind::kw_u16: return syntax::PrimitiveTypeKind::u16;
    case TokenKind::kw_i32: return syntax::PrimitiveTypeKind::i32;
    case TokenKind::kw_u32: return syntax::PrimitiveTypeKind::u32;
    case TokenKind::kw_i64: return syntax::PrimitiveTypeKind::i64;
    case TokenKind::kw_u64: return syntax::PrimitiveTypeKind::u64;
    case TokenKind::kw_isize: return syntax::PrimitiveTypeKind::isize;
    case TokenKind::kw_usize: return syntax::PrimitiveTypeKind::usize;
    case TokenKind::kw_f32: return syntax::PrimitiveTypeKind::f32;
    case TokenKind::kw_f64: return syntax::PrimitiveTypeKind::f64;
    case TokenKind::kw_str: return syntax::PrimitiveTypeKind::str;
    default: return syntax::PrimitiveTypeKind::void_;
    }
}

[[nodiscard]] base::u64 parse_u64_literal(const std::string_view text) noexcept {
    base::u64 value = 0;
    int base = 10;
    base::usize begin = 0;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        begin = 2;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
        begin = 2;
    }
    for (base::usize i = begin; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '_') {
            continue;
        }
        base::u64 digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<base::u64>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<base::u64>(10 + c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<base::u64>(10 + c - 'A');
        }
        value = (value * static_cast<base::u64>(base)) + digit;
    }
    return value;
}

} // namespace

Parser::Parser(
    const std::span<const syntax::Token> tokens,
    base::DiagnosticSink& diagnostics
) noexcept
    : tokens_(tokens), diagnostics_(diagnostics) {}

base::Result<syntax::AstModule> Parser::parse_module() {
    if (match(TokenKind::kw_module)) {
        module_.module_path = parse_path();
        expect(TokenKind::semicolon, "expected ';' after module declaration");
    }

    while (match(TokenKind::kw_import)) {
        module_.imports.push_back(parse_path());
        expect(TokenKind::semicolon, "expected ';' after import declaration");
    }

    while (!is_eof()) {
        const syntax::ItemId item = parse_item();
        if (!syntax::is_valid(item)) {
            synchronize();
        }
    }

    if (diagnostics_.has_error()) {
        return base::Result<syntax::AstModule>::fail(
            {base::ErrorCode::parse_error, "parsing failed"}
        );
    }
    return base::Result<syntax::AstModule>::ok(std::move(module_));
}

bool Parser::is_eof() const noexcept {
    return peek().kind == TokenKind::eof;
}

const syntax::Token& Parser::peek() const noexcept {
    if (current_ >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[current_];
}

const syntax::Token& Parser::previous() const noexcept {
    if (current_ == 0) {
        return tokens_.front();
    }
    return tokens_[current_ - 1];
}

bool Parser::check(const TokenKind kind) const noexcept {
    return peek().kind == kind;
}

bool Parser::check_next(const TokenKind kind) const noexcept {
    const base::usize next = current_ + 1;
    if (next >= tokens_.size()) {
        return false;
    }
    return tokens_[next].kind == kind;
}

bool Parser::next_angle_list_is_type_scope() const noexcept {
    if (!check(TokenKind::less)) {
        return false;
    }
    base::usize index = current_ + 1;
    int depth = 1;
    while (index < tokens_.size()) {
        const TokenKind kind = tokens_[index].kind;
        if (kind == TokenKind::less) {
            ++depth;
        } else if (kind == TokenKind::greater) {
            --depth;
            if (depth == 0) {
                const base::usize after = index + 1;
                return after < tokens_.size() && tokens_[after].kind == TokenKind::dot;
            }
        } else if (kind == TokenKind::semicolon ||
                   kind == TokenKind::l_brace ||
                   kind == TokenKind::r_brace ||
                   kind == TokenKind::fat_arrow) {
            return false;
        }
        ++index;
    }
    return false;
}

bool Parser::match(const TokenKind kind) noexcept {
    if (!check(kind)) {
        return false;
    }
    advance();
    return true;
}

const syntax::Token& Parser::advance() noexcept {
    if (!is_eof()) {
        ++current_;
    }
    return previous();
}

const syntax::Token& Parser::expect(const TokenKind kind, std::string message) {
    if (check(kind)) {
        return advance();
    }
    report_here(std::move(message));
    static const syntax::Token fallback {};
    return fallback;
}

void Parser::synchronize() {
    panic_ = false;
    if (is_eof()) {
        return;
    }
    advance();
    while (!is_eof()) {
        if (previous().kind == TokenKind::semicolon) {
            return;
        }
        switch (peek().kind) {
        case TokenKind::r_brace:
        case TokenKind::kw_fn:
        case TokenKind::kw_struct:
        case TokenKind::kw_enum:
        case TokenKind::kw_opaque:
        case TokenKind::kw_const:
        case TokenKind::kw_type:
        case TokenKind::kw_extern:
        case TokenKind::kw_export:
        case TokenKind::kw_let:
        case TokenKind::kw_var:
        case TokenKind::kw_if:
        case TokenKind::kw_while:
        case TokenKind::kw_return:
            return;
        default:
            advance();
            break;
        }
    }
}

void Parser::report_here(std::string message) {
    report_at(peek(), std::move(message));
}

void Parser::report_at(const syntax::Token& token, std::string message) {
    if (panic_) {
        return;
    }
    panic_ = true;
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        token.range,
        std::move(message),
    });
}

syntax::ModulePath Parser::parse_path() {
    syntax::ModulePath path;
    const syntax::Token& first = is_path_segment_token(peek().kind)
        ? advance()
        : expect(TokenKind::identifier, "expected identifier in path");
    base::SourceRange range = first.range;
    if (is_path_segment_token(first.kind)) {
        path.parts.push_back(first.text);
    }
    while (match(TokenKind::dot)) {
        const syntax::Token& part = is_path_segment_token(peek().kind)
            ? advance()
            : expect(TokenKind::identifier, "expected identifier after '.'");
        if (is_path_segment_token(part.kind)) {
            path.parts.push_back(part.text);
            range = merge(range, part.range);
        }
    }
    path.range = range;
    panic_ = false;
    return path;
}

syntax::ItemId Parser::parse_item() {
    panic_ = false;
    if (check(TokenKind::kw_const)) {
        return parse_const_decl();
    }
    if (check(TokenKind::kw_type)) {
        return parse_type_alias_decl();
    }
    if (check(TokenKind::kw_struct)) {
        return parse_struct_decl();
    }
    if (check(TokenKind::kw_enum)) {
        return parse_enum_decl();
    }
    if (check(TokenKind::kw_opaque)) {
        return parse_opaque_struct_decl();
    }
    if (check(TokenKind::kw_extern)) {
        return parse_extern_block();
    }
    if (check(TokenKind::kw_export)) {
        const syntax::Token& begin = advance();
        expect(TokenKind::kw_c, "expected 'c' after 'export'");
        if (!check(TokenKind::kw_fn)) {
            report_here("expected function declaration after 'export c'");
            return syntax::invalid_item_id;
        }
        syntax::ItemId id = parse_fn_decl(true, false);
        if (syntax::is_valid(id)) {
            module_.items[id.value].range.begin = begin.range.begin;
        }
        return id;
    }
    if (check(TokenKind::kw_fn)) {
        return parse_fn_decl(false, false);
    }

    report_here("expected item declaration");
    return syntax::invalid_item_id;
}

syntax::ItemId Parser::parse_const_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_const, "expected 'const'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected const name");
    expect(TokenKind::colon, "expected ':' after const name");
    const syntax::TypeId type = parse_type();
    expect(TokenKind::equal, "expected '=' in const declaration");
    const syntax::ExprId value = parse_expr();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after const declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::const_decl;
    item.range = merge(begin.range, end.range);
    item.name = name.text;
    item.const_type = type;
    item.const_value = value;
    panic_ = false;
    return module_.push_item(std::move(item));
}

syntax::ItemId Parser::parse_type_alias_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_type, "expected 'type'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected type alias name");
    expect(TokenKind::equal, "expected '=' in type alias declaration");
    const syntax::TypeId target = parse_type();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after type alias declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::type_alias;
    item.range = merge(begin.range, end.range);
    item.name = name.text;
    item.alias_type = target;
    panic_ = false;
    return module_.push_item(std::move(item));
}

syntax::ItemId Parser::parse_struct_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_struct, "expected 'struct'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected struct name");
    expect(TokenKind::l_brace, "expected '{' after struct name");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::struct_decl;
    item.name = name.text;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::Token& field_name = expect(TokenKind::identifier, "expected field name");
        expect(TokenKind::colon, "expected ':' after field name");
        const syntax::TypeId field_type = parse_type();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after field declaration");
        if (field_name.kind == TokenKind::identifier) {
            item.fields.push_back(syntax::FieldDecl {
                field_name.text,
                field_type,
                merge(field_name.range, end.range),
            });
        }
        panic_ = false;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after struct declaration");
    item.range = merge(begin.range, end.range);
    panic_ = false;
    return module_.push_item(std::move(item));
}

syntax::ItemId Parser::parse_enum_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_enum, "expected 'enum'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected enum name");
    std::vector<std::string_view> generic_params;
    if (check(TokenKind::less)) {
        generic_params = parse_generic_param_list();
    }
    expect(TokenKind::colon, "expected ':' after enum name");
    const syntax::TypeId base_type = parse_type();
    expect(TokenKind::l_brace, "expected '{' after enum base type");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::enum_decl;
    item.name = name.text;
    item.generic_params = std::move(generic_params);
    item.enum_base_type = base_type;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name");
        syntax::TypeId payload_type = syntax::invalid_type_id;
        if (match(TokenKind::l_paren)) {
            payload_type = parse_type();
            expect(TokenKind::r_paren, "expected ')' after enum case payload type");
        }
        expect(TokenKind::equal, "expected '=' after enum case name");
        const syntax::Token& value = expect(TokenKind::integer_literal, "expected integer literal enum value");
        const syntax::Token& comma = expect(TokenKind::comma, "expected ',' after enum case");
        if (case_name.kind == TokenKind::identifier) {
            item.enum_cases.push_back(syntax::EnumCaseDecl {
                case_name.text,
                payload_type,
                value.text,
                merge(case_name.range, comma.range),
            });
        }
        panic_ = false;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after enum declaration");
    item.range = merge(begin.range, end.range);
    panic_ = false;
    return module_.push_item(std::move(item));
}

std::vector<std::string_view> Parser::parse_generic_param_list() {
    std::vector<std::string_view> params;
    expect(TokenKind::less, "expected '<' before generic parameter list");
    if (!check(TokenKind::greater)) {
        do {
            const syntax::Token& name = expect(TokenKind::identifier, "expected generic parameter name");
            if (name.kind == TokenKind::identifier) {
                params.push_back(name.text);
            }
            panic_ = false;
            if (check(TokenKind::greater)) {
                break;
            }
        } while (match(TokenKind::comma) && !check(TokenKind::greater));
    }
    expect(TokenKind::greater, "expected '>' after generic parameter list");
    panic_ = false;
    return params;
}

std::vector<syntax::TypeId> Parser::parse_type_arg_list() {
    std::vector<syntax::TypeId> args;
    expect(TokenKind::less, "expected '<' before type argument list");
    if (!check(TokenKind::greater)) {
        do {
            args.push_back(parse_type());
            panic_ = false;
            if (check(TokenKind::greater)) {
                break;
            }
        } while (match(TokenKind::comma) && !check(TokenKind::greater));
    }
    expect(TokenKind::greater, "expected '>' after type argument list");
    panic_ = false;
    return args;
}

syntax::ItemId Parser::parse_extern_block() {
    const syntax::Token& begin = expect(TokenKind::kw_extern, "expected 'extern'");
    expect(TokenKind::kw_c, "expected 'c' after 'extern'");
    expect(TokenKind::l_brace, "expected '{' after 'extern c'");

    syntax::ItemNode block;
    block.kind = syntax::ItemKind::extern_block;
    block.is_extern_c = true;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        if (check(TokenKind::kw_fn)) {
            const syntax::ItemId item = parse_fn_decl(false, true);
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else if (check(TokenKind::kw_opaque)) {
            const syntax::ItemId item = parse_opaque_struct_decl();
            if (syntax::is_valid(item)) {
                block.extern_items.push_back(item);
            }
        } else {
            report_here("expected extern item");
            synchronize();
        }
        panic_ = false;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after extern block");
    block.range = merge(begin.range, end.range);
    panic_ = false;
    return module_.push_item(std::move(block));
}

syntax::ItemId Parser::parse_opaque_struct_decl() {
    const syntax::Token& begin = expect(TokenKind::kw_opaque, "expected 'opaque'");
    expect(TokenKind::kw_struct, "expected 'struct' after 'opaque'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected opaque struct name");
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after opaque struct declaration");

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::opaque_struct_decl;
    item.range = merge(begin.range, end.range);
    item.name = name.text;
    item.is_extern_c = true;
    panic_ = false;
    return module_.push_item(std::move(item));
}

syntax::ItemId Parser::parse_fn_decl(const bool is_export_c, const bool is_extern_c) {
    const syntax::Token& begin = expect(TokenKind::kw_fn, "expected 'fn'");
    const syntax::Token& name = expect(TokenKind::identifier, "expected function name");
    expect(TokenKind::l_paren, "expected '(' after function name");
    std::vector<syntax::ParamDecl> params;
    if (!check(TokenKind::r_paren)) {
        params = parse_param_list();
    }
    expect(TokenKind::r_paren, "expected ')' after parameter list");
    const syntax::TypeId return_type = parse_optional_return_type();

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = name.text;
    item.params = std::move(params);
    item.return_type = return_type;
    item.is_export_c = is_export_c;
    item.is_extern_c = is_extern_c;

    parse_optional_abi_name(item);

    if (is_extern_c) {
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after extern function declaration");
        item.range = merge(begin.range, end.range);
    } else if (match(TokenKind::semicolon)) {
        item.is_prototype = true;
        item.range = merge(begin.range, previous().range);
    } else {
        item.body = parse_block();
        item.range = syntax::is_valid(item.body) ? merge(begin.range, module_.stmts[item.body.value].range) : begin.range;
    }

    panic_ = false;
    return module_.push_item(std::move(item));
}

std::vector<syntax::ParamDecl> Parser::parse_param_list() {
    std::vector<syntax::ParamDecl> params;
    do {
        const syntax::Token& name = expect(TokenKind::identifier, "expected parameter name");
        expect(TokenKind::colon, "expected ':' after parameter name");
        const syntax::TypeId type = parse_type();
        if (name.kind == TokenKind::identifier) {
            params.push_back(syntax::ParamDecl {name.text, type, merge(name.range, module_.types[type.value].range)});
        }
        panic_ = false;
        if (check(TokenKind::r_paren)) {
            break;
        }
    } while (match(TokenKind::comma) && !check(TokenKind::r_paren));
    return params;
}

syntax::TypeId Parser::parse_optional_return_type() {
    if (!match(TokenKind::arrow)) {
        return syntax::invalid_type_id;
    }
    return parse_type();
}

void Parser::parse_optional_abi_name(syntax::ItemNode& item) {
    if (!match(TokenKind::at)) {
        return;
    }
    const syntax::Token& attr = expect(TokenKind::identifier, "expected ABI attribute name");
    if (attr.text != "name") {
        report_at(attr, "expected ABI attribute 'name'");
    }
    expect(TokenKind::l_paren, "expected '(' after ABI attribute");
    const syntax::Token& value = expect(TokenKind::string_literal, "expected string literal in ABI name");
    if (value.text.size() >= 2) {
        item.abi_name = value.text.substr(1, value.text.size() - 2);
    }
    expect(TokenKind::r_paren, "expected ')' after ABI attribute");
    panic_ = false;
}

syntax::TypeId Parser::parse_type() {
    panic_ = false;
    if (is_primitive_type_token(peek().kind)) {
        return parse_primitive_type();
    }
    if (check(TokenKind::identifier)) {
        const syntax::Token& name = advance();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::named;
        type.range = name.range;
        type.name = name.text;
        if (check(TokenKind::less)) {
            type.type_args = parse_type_arg_list();
            type.range = merge(name.range, previous().range);
        }
        return module_.push_type(type);
    }
    if (match(TokenKind::star)) {
        const syntax::Token& begin = previous();
        syntax::PointerMutability mutability = syntax::PointerMutability::const_;
        if (match(TokenKind::kw_mut)) {
            mutability = syntax::PointerMutability::mut;
        } else if (match(TokenKind::kw_const)) {
            mutability = syntax::PointerMutability::const_;
        } else {
            report_here("expected 'mut' or 'const' after '*'");
        }
        const syntax::TypeId pointee = parse_type();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::pointer;
        type.range = merge(begin.range, module_.types[pointee.value].range);
        type.pointer_mutability = mutability;
        type.pointee = pointee;
        return module_.push_type(type);
    }
    if (match(TokenKind::l_bracket)) {
        const syntax::Token& begin = previous();
        const syntax::Token& count = expect(TokenKind::integer_literal, "expected array length");
        expect(TokenKind::r_bracket, "expected ']' after array length");
        const syntax::TypeId element = parse_type();
        syntax::TypeNode type;
        type.kind = syntax::TypeKind::array;
        type.range = merge(begin.range, module_.types[element.value].range);
        type.array_count = parse_u64_literal(count.text);
        type.array_element = element;
        return module_.push_type(type);
    }

    report_here("expected type");
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.primitive = syntax::PrimitiveTypeKind::void_;
    type.range = peek().range;
    return module_.push_type(type);
}

syntax::TypeId Parser::parse_primitive_type() {
    const syntax::Token& token = advance();
    syntax::TypeNode type;
    type.kind = syntax::TypeKind::primitive;
    type.range = token.range;
    type.primitive = primitive_from_token(token.kind);
    return module_.push_type(type);
}

syntax::StmtId Parser::parse_block() {
    const syntax::Token& begin = expect(TokenKind::l_brace, "expected block");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::StmtId stmt = parse_stmt();
        if (syntax::is_valid(stmt)) {
            block.statements.push_back(stmt);
        } else {
            synchronize();
        }
        panic_ = false;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after block");
    block.range = merge(begin.range, end.range);
    panic_ = false;
    return module_.push_stmt(std::move(block));
}

base::SourceRange Parser::merge(base::SourceRange begin, base::SourceRange end) const noexcept {
    return base::SourceRange {begin.source, begin.begin, end.end};
}

} // namespace aurex::parse

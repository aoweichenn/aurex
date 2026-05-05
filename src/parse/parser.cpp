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

[[nodiscard]] syntax::BinaryOp binary_op_from_token(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::plus: return syntax::BinaryOp::add;
    case TokenKind::minus: return syntax::BinaryOp::sub;
    case TokenKind::star: return syntax::BinaryOp::mul;
    case TokenKind::slash: return syntax::BinaryOp::div;
    case TokenKind::percent: return syntax::BinaryOp::mod;
    case TokenKind::less_less: return syntax::BinaryOp::shl;
    case TokenKind::greater_greater: return syntax::BinaryOp::shr;
    case TokenKind::less: return syntax::BinaryOp::less;
    case TokenKind::less_equal: return syntax::BinaryOp::less_equal;
    case TokenKind::greater: return syntax::BinaryOp::greater;
    case TokenKind::greater_equal: return syntax::BinaryOp::greater_equal;
    case TokenKind::equal_equal: return syntax::BinaryOp::equal;
    case TokenKind::bang_equal: return syntax::BinaryOp::not_equal;
    case TokenKind::amp: return syntax::BinaryOp::bit_and;
    case TokenKind::caret: return syntax::BinaryOp::bit_xor;
    case TokenKind::pipe: return syntax::BinaryOp::bit_or;
    case TokenKind::amp_amp: return syntax::BinaryOp::logical_and;
    case TokenKind::pipe_pipe: return syntax::BinaryOp::logical_or;
    default: return syntax::BinaryOp::add;
    }
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

syntax::ExprId Parser::parse_block_expr() {
    const syntax::Token& begin = expect(TokenKind::l_brace, "expected block expression");
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    syntax::ExprId result = syntax::invalid_expr_id;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        if (check(TokenKind::kw_let) || check(TokenKind::kw_var)) {
            const syntax::StmtId stmt = parse_stmt();
            if (syntax::is_valid(stmt)) {
                block.statements.push_back(stmt);
            } else {
                synchronize();
            }
            panic_ = false;
            continue;
        }

        const syntax::ExprId expr = parse_expr();
        if (match(TokenKind::equal)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::assign;
            stmt.lhs = expr;
            stmt.rhs = parse_expr();
            const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after assignment");
            stmt.range = syntax::is_valid(expr) ? merge(module_.exprs[expr.value].range, end.range) : end.range;
            block.statements.push_back(module_.push_stmt(std::move(stmt)));
            panic_ = false;
            continue;
        }
        if (match(TokenKind::semicolon)) {
            syntax::StmtNode stmt;
            stmt.kind = syntax::StmtKind::expr;
            stmt.init = expr;
            stmt.range = syntax::is_valid(expr) ? merge(module_.exprs[expr.value].range, previous().range) : previous().range;
            block.statements.push_back(module_.push_stmt(std::move(stmt)));
            panic_ = false;
            continue;
        }

        result = expr;
        break;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after block expression");
    block.range = merge(begin.range, end.range);
    const syntax::StmtId block_id = module_.push_stmt(std::move(block));

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::block_expr;
    expr.range = merge(begin.range, end.range);
    expr.block = block_id;
    expr.block_result = result;
    panic_ = false;
    return module_.push_expr(std::move(expr));
}

syntax::StmtId Parser::parse_stmt() {
    panic_ = false;
    if (check(TokenKind::kw_let)) {
        return parse_let_or_var_stmt(syntax::StmtKind::let);
    }
    if (check(TokenKind::kw_var)) {
        return parse_let_or_var_stmt(syntax::StmtKind::var);
    }
    if (check(TokenKind::kw_if)) {
        return parse_if_stmt();
    }
    if (check(TokenKind::kw_while)) {
        return parse_while_stmt();
    }
    if (match(TokenKind::kw_break)) {
        const syntax::Token& begin = previous();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after break");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::break_;
        stmt.range = merge(begin.range, end.range);
        return module_.push_stmt(stmt);
    }
    if (match(TokenKind::kw_continue)) {
        const syntax::Token& begin = previous();
        const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after continue");
        syntax::StmtNode stmt;
        stmt.kind = syntax::StmtKind::continue_;
        stmt.range = merge(begin.range, end.range);
        return module_.push_stmt(stmt);
    }
    if (check(TokenKind::kw_return)) {
        return parse_return_stmt();
    }
    if (check(TokenKind::l_brace)) {
        return parse_block();
    }
    return parse_expr_or_assign_stmt();
}

syntax::StmtId Parser::parse_let_or_var_stmt(const syntax::StmtKind kind) {
    const syntax::Token& begin = advance();
    const syntax::Token& name = expect(TokenKind::identifier, "expected local name");
    syntax::TypeId type = syntax::invalid_type_id;
    if (match(TokenKind::colon)) {
        type = parse_type();
    }
    expect(TokenKind::equal, "expected initializer");
    const syntax::ExprId init = parse_expr();
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after local declaration");

    syntax::StmtNode stmt;
    stmt.kind = kind;
    stmt.range = merge(begin.range, end.range);
    stmt.name = name.text;
    stmt.declared_type = type;
    stmt.init = init;
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_if_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_if, "expected 'if'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId condition = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;
    const syntax::StmtId then_block = parse_block();
    syntax::StmtId else_block = syntax::invalid_stmt_id;
    syntax::StmtId else_if = syntax::invalid_stmt_id;
    if (match(TokenKind::kw_else)) {
        if (check(TokenKind::kw_if)) {
            else_if = parse_if_stmt();
        } else {
            else_block = parse_block();
        }
    }

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::if_;
    if (syntax::is_valid(else_if)) {
        stmt.range = merge(begin.range, module_.stmts[else_if.value].range);
    } else if (syntax::is_valid(else_block)) {
        stmt.range = merge(begin.range, module_.stmts[else_block.value].range);
    } else {
        stmt.range = merge(begin.range, module_.stmts[then_block.value].range);
    }
    stmt.condition = condition;
    stmt.then_block = then_block;
    stmt.else_block = else_block;
    stmt.else_if = else_if;
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_while_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_while, "expected 'while'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId condition = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;
    const syntax::StmtId body = parse_block();

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::while_;
    stmt.range = merge(begin.range, module_.stmts[body.value].range);
    stmt.condition = condition;
    stmt.body = body;
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_return_stmt() {
    const syntax::Token& begin = expect(TokenKind::kw_return, "expected 'return'");
    syntax::ExprId value = syntax::invalid_expr_id;
    if (!check(TokenKind::semicolon)) {
        value = parse_expr();
    }
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after return");
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::return_;
    stmt.range = merge(begin.range, end.range);
    stmt.return_value = value;
    return module_.push_stmt(std::move(stmt));
}

syntax::StmtId Parser::parse_expr_or_assign_stmt() {
    const syntax::ExprId lhs = parse_expr();
    syntax::StmtNode stmt;
    if (match(TokenKind::equal)) {
        stmt.kind = syntax::StmtKind::assign;
        stmt.lhs = lhs;
        stmt.rhs = parse_expr();
    } else {
        stmt.kind = syntax::StmtKind::expr;
        stmt.init = lhs;
    }
    const syntax::Token& end = expect(TokenKind::semicolon, "expected ';' after expression statement");
    stmt.range = syntax::is_valid(lhs) ? merge(module_.exprs[lhs.value].range, end.range) : end.range;
    return module_.push_stmt(std::move(stmt));
}

syntax::ExprId Parser::parse_expr() {
    if (check(TokenKind::kw_if)) {
        return parse_if_expr();
    }
    if (check(TokenKind::kw_match)) {
        return parse_match_expr();
    }
    return parse_logical_or();
}

syntax::ExprId Parser::parse_if_expr() {
    const syntax::Token& begin = expect(TokenKind::kw_if, "expected 'if'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId condition = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;

    const syntax::ExprId then_expr = parse_block_expr();
    expect(TokenKind::kw_else, "if expression requires else branch");
    const syntax::ExprId else_expr = parse_block_expr();

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::if_expr;
    expr.range = merge(begin.range, module_.exprs[else_expr.value].range);
    expr.condition = condition;
    expr.then_expr = then_expr;
    expr.else_expr = else_expr;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::parse_match_expr() {
    const syntax::Token& begin = expect(TokenKind::kw_match, "expected 'match'");
    const bool previous_struct_literal_mode = allow_struct_literal_;
    allow_struct_literal_ = false;
    const syntax::ExprId value = parse_expr();
    allow_struct_literal_ = previous_struct_literal_mode;
    expect(TokenKind::l_brace, "expected '{' after match value");

    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::match_expr;
    expr.match_value = value;

    while (!is_eof() && !check(TokenKind::r_brace)) {
        const syntax::PatternId pattern = parse_pattern();
        syntax::ExprId guard = syntax::invalid_expr_id;
        if (match(TokenKind::kw_if)) {
            guard = parse_expr();
        }
        expect(TokenKind::fat_arrow, "expected '=>' after match case");
        const syntax::ExprId arm_value = parse_expr();
        base::SourceRange pattern_range = {};
        if (syntax::is_valid(pattern) && pattern.value < module_.patterns.size()) {
            pattern_range = module_.patterns[pattern.value].range;
        }
        base::SourceRange arm_range = syntax::is_valid(arm_value)
            ? merge(pattern_range, module_.exprs[arm_value.value].range)
            : pattern_range;
        expr.match_arms.push_back(syntax::MatchArm {
            pattern,
            guard,
            arm_value,
            arm_range,
        });
        if (check(TokenKind::r_brace)) {
            break;
        }
        expect(TokenKind::comma, "expected ',' after match arm");
        panic_ = false;
    }

    const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after match expression");
    expr.range = merge(begin.range, end.range);
    panic_ = false;
    return module_.push_expr(std::move(expr));
}

syntax::PatternId Parser::parse_pattern() {
    const syntax::PatternId first = parse_pattern_atom();
    if (!match(TokenKind::pipe)) {
        return first;
    }
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::or_pattern;
    pattern.alternatives.push_back(first);
    base::SourceRange range = syntax::is_valid(first) && first.value < module_.patterns.size()
        ? module_.patterns[first.value].range
        : previous().range;
    do {
        const syntax::PatternId alternative = parse_pattern_atom();
        pattern.alternatives.push_back(alternative);
        if (syntax::is_valid(alternative) && alternative.value < module_.patterns.size()) {
            range = merge(range, module_.patterns[alternative.value].range);
        }
    } while (match(TokenKind::pipe));
    pattern.range = range;
    return module_.push_pattern(pattern);
}

syntax::PatternId Parser::parse_pattern_atom() {
    if (match(TokenKind::identifier)) {
        const syntax::Token& first = previous();
        if (first.text == "_") {
            syntax::PatternNode pattern;
            pattern.kind = syntax::PatternKind::wildcard;
            pattern.range = first.range;
            return module_.push_pattern(pattern);
        }
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = first.text;
        pattern.range = first.range;
        if (match(TokenKind::dot)) {
            const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name after '.'");
            pattern.enum_name = first.text;
            pattern.case_name = case_name.text;
            pattern.scoped = true;
            pattern.range = merge(first.range, case_name.range);
        }
        if (match(TokenKind::l_paren)) {
            const syntax::Token& binding = expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = merge(pattern.range, end.range);
        }
        return module_.push_pattern(pattern);
    }
    if (match(TokenKind::integer_literal) || match(TokenKind::kw_true) || match(TokenKind::kw_false)) {
        const syntax::Token& token = previous();
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::literal;
        pattern.case_name = token.text;
        pattern.range = token.range;
        return module_.push_pattern(pattern);
    }
    if (match(TokenKind::dot)) {
        const syntax::Token& dot = previous();
        const syntax::Token& case_name = expect(TokenKind::identifier, "expected enum case name after '.'");
        syntax::PatternNode pattern;
        pattern.kind = syntax::PatternKind::enum_case;
        pattern.case_name = case_name.text;
        pattern.scoped = true;
        pattern.range = merge(dot.range, case_name.range);
        if (match(TokenKind::l_paren)) {
            const syntax::Token& binding = expect(TokenKind::identifier, "expected payload binding name");
            if (binding.kind == TokenKind::identifier) {
                pattern.binding_name = binding.text;
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after payload binding");
            pattern.range = merge(pattern.range, end.range);
        }
        return module_.push_pattern(pattern);
    }
    report_here("expected match pattern");
    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::wildcard;
    pattern.range = peek().range;
    advance();
    return module_.push_pattern(pattern);
}

syntax::ExprId Parser::parse_logical_or() {
    syntax::ExprId expr = parse_logical_and();
    while (match(TokenKind::pipe_pipe)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_logical_and();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_logical_and() {
    syntax::ExprId expr = parse_bit_or();
    while (match(TokenKind::amp_amp)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_or();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_bit_or() {
    syntax::ExprId expr = parse_bit_xor();
    while (match(TokenKind::pipe)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_xor();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_bit_xor() {
    syntax::ExprId expr = parse_bit_and();
    while (match(TokenKind::caret)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_bit_and();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_bit_and() {
    syntax::ExprId expr = parse_equality();
    while (match(TokenKind::amp)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_equality();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_equality() {
    syntax::ExprId expr = parse_compare();
    while (match(TokenKind::equal_equal) || match(TokenKind::bang_equal)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_compare();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_compare() {
    syntax::ExprId expr = parse_shift();
    while (match(TokenKind::less) || match(TokenKind::less_equal) || match(TokenKind::greater) || match(TokenKind::greater_equal)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_shift();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_shift() {
    syntax::ExprId expr = parse_add();
    while (match(TokenKind::less_less) || match(TokenKind::greater_greater)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_add();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_add() {
    syntax::ExprId expr = parse_mul();
    while (match(TokenKind::plus) || match(TokenKind::minus)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_mul();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_mul() {
    syntax::ExprId expr = parse_unary();
    while (match(TokenKind::star) || match(TokenKind::slash) || match(TokenKind::percent)) {
        const TokenKind op = previous().kind;
        const syntax::ExprId rhs = parse_unary();
        expr = make_binary(binary_op_from_token(op), expr, rhs, merge(module_.exprs[expr.value].range, module_.exprs[rhs.value].range));
    }
    return expr;
}

syntax::ExprId Parser::parse_unary() {
    if (match(TokenKind::bang) || match(TokenKind::minus) || match(TokenKind::tilde) || match(TokenKind::amp) || match(TokenKind::star)) {
        const syntax::Token& op = previous();
        const syntax::ExprId operand = parse_unary();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::unary;
        expr.range = merge(op.range, module_.exprs[operand.value].range);
        expr.text = op.text;
        switch (op.kind) {
        case TokenKind::bang:
            expr.unary_op = syntax::UnaryOp::logical_not;
            break;
        case TokenKind::minus:
            expr.unary_op = syntax::UnaryOp::numeric_negate;
            break;
        case TokenKind::tilde:
            expr.unary_op = syntax::UnaryOp::bitwise_not;
            break;
        case TokenKind::amp:
            expr.unary_op = syntax::UnaryOp::address_of;
            break;
        case TokenKind::star:
            expr.unary_op = syntax::UnaryOp::dereference;
            break;
        default:
            break;
        }
        expr.unary_operand = operand;
        return module_.push_expr(std::move(expr));
    }
    return parse_postfix();
}

syntax::ExprId Parser::parse_postfix() {
    syntax::ExprId expr = parse_primary();
    while (true) {
        if (next_angle_list_is_type_scope() && match(TokenKind::less)) {
            if (!syntax::is_valid(expr) || expr.value >= module_.exprs.size()) {
                continue;
            }
            syntax::ExprNode& node = module_.exprs[expr.value];
            if (node.kind != syntax::ExprKind::name) {
                report_at(previous(), "type arguments are only supported on named enum constructors in M1");
            }
            if (!check(TokenKind::greater)) {
                do {
                    node.type_args.push_back(parse_type());
                    panic_ = false;
                    if (check(TokenKind::greater)) {
                        break;
                    }
                } while (match(TokenKind::comma) && !check(TokenKind::greater));
            }
            const syntax::Token& end = expect(TokenKind::greater, "expected '>' after type argument list");
            node.range = merge(node.range, end.range);
        } else if (match(TokenKind::dot)) {
            const syntax::Token& field = expect(TokenKind::identifier, "expected field name after '.'");
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::field;
            node.range = merge(module_.exprs[expr.value].range, field.range);
            node.object = expr;
            node.field_name = field.text;
            expr = module_.push_expr(std::move(node));
        } else if (match(TokenKind::l_bracket)) {
            const syntax::ExprId index = parse_expr();
            const syntax::Token& end = expect(TokenKind::r_bracket, "expected ']' after index");
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::index;
            node.range = merge(module_.exprs[expr.value].range, end.range);
            node.object = expr;
            node.index = index;
            expr = module_.push_expr(std::move(node));
        } else if (match(TokenKind::l_paren)) {
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::call;
            node.callee = expr;
            if (!check(TokenKind::r_paren)) {
                do {
                    node.args.push_back(parse_expr());
                    if (check(TokenKind::r_paren)) {
                        break;
                    }
                } while (match(TokenKind::comma) && !check(TokenKind::r_paren));
            }
            const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after argument list");
            node.range = merge(module_.exprs[expr.value].range, end.range);
            expr = module_.push_expr(std::move(node));
        } else {
            break;
        }
        panic_ = false;
    }
    return expr;
}

syntax::ExprId Parser::parse_primary() {
    if (match(TokenKind::identifier)) {
        const syntax::Token& name = previous();
        if (allow_struct_literal_ && check(TokenKind::l_brace)) {
            advance();
            syntax::ExprNode node;
            node.kind = syntax::ExprKind::struct_literal;
            node.struct_name = name.text;
            node.range = name.range;
            if (!check(TokenKind::r_brace)) {
                do {
                    const syntax::Token& field = expect(TokenKind::identifier, "expected field name in struct literal");
                    expect(TokenKind::colon, "expected ':' after field name");
                    const syntax::ExprId value = parse_expr();
                    node.field_inits.push_back(syntax::FieldInit {field.text, value, merge(field.range, module_.exprs[value.value].range)});
                    if (check(TokenKind::r_brace)) {
                        break;
                    }
                } while (match(TokenKind::comma) && !check(TokenKind::r_brace));
            }
            const syntax::Token& end = expect(TokenKind::r_brace, "expected '}' after struct literal");
            node.range = merge(name.range, end.range);
            return module_.push_expr(std::move(node));
        }
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::name;
        expr.range = name.range;
        expr.text = name.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::integer_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::integer_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::kw_true) || match(TokenKind::kw_false)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::bool_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::kw_null)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::null_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::string_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::string_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::c_string_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::c_string_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::byte_literal)) {
        const syntax::Token& token = previous();
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::byte_literal;
        expr.range = token.range;
        expr.text = token.text;
        return module_.push_expr(expr);
    }
    if (match(TokenKind::l_paren)) {
        const syntax::ExprId expr = parse_expr();
        expect(TokenKind::r_paren, "expected ')' after expression");
        return expr;
    }
    if (check(TokenKind::l_brace)) {
        return parse_block_expr();
    }
    if (match(TokenKind::kw_cast)) {
        return parse_builtin_cast(syntax::ExprKind::cast);
    }
    if (match(TokenKind::kw_ptr_cast)) {
        return parse_builtin_cast(syntax::ExprKind::ptr_cast);
    }
    if (match(TokenKind::kw_bit_cast)) {
        return parse_builtin_cast(syntax::ExprKind::bit_cast);
    }
    if (match(TokenKind::kw_size_of)) {
        return parse_type_builtin(syntax::ExprKind::size_of);
    }
    if (match(TokenKind::kw_align_of)) {
        return parse_type_builtin(syntax::ExprKind::align_of);
    }
    if (match(TokenKind::kw_ptr_addr)) {
        const syntax::Token& begin = previous();
        expect(TokenKind::l_paren, "expected '(' after ptr_addr");
        const syntax::ExprId value = parse_expr();
        const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after ptr_addr argument");
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::ptr_addr;
        expr.range = merge(begin.range, end.range);
        expr.cast_expr = value;
        return module_.push_expr(std::move(expr));
    }
    if (match(TokenKind::kw_ptr_from_addr)) {
        const syntax::Token& begin = previous();
        expect(TokenKind::l_paren, "expected '(' after ptr_from_addr");
        const syntax::TypeId type = parse_type();
        expect(TokenKind::comma, "expected ',' after ptr_from_addr type");
        const syntax::ExprId value = parse_expr();
        const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after ptr_from_addr argument");
        syntax::ExprNode expr;
        expr.kind = syntax::ExprKind::ptr_from_addr;
        expr.range = merge(begin.range, end.range);
        expr.cast_type = type;
        expr.cast_expr = value;
        return module_.push_expr(std::move(expr));
    }

    report_here("expected expression");
    return make_invalid_expr();
}

syntax::ExprId Parser::parse_builtin_cast(const syntax::ExprKind kind) {
    const syntax::Token& begin = previous();
    expect(TokenKind::l_paren, "expected '(' after cast builtin");
    const syntax::TypeId type = parse_type();
    expect(TokenKind::comma, "expected ',' after cast type");
    const syntax::ExprId value = parse_expr();
    const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after cast expression");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = merge(begin.range, end.range);
    expr.cast_type = type;
    expr.cast_expr = value;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::parse_type_builtin(const syntax::ExprKind kind) {
    const syntax::Token& begin = previous();
    expect(TokenKind::l_paren, "expected '(' after type builtin");
    const syntax::TypeId type = parse_type();
    const syntax::Token& end = expect(TokenKind::r_paren, "expected ')' after type builtin");

    syntax::ExprNode expr;
    expr.kind = kind;
    expr.range = merge(begin.range, end.range);
    expr.cast_type = type;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::make_binary(const syntax::BinaryOp op, const syntax::ExprId lhs, const syntax::ExprId rhs, base::SourceRange range) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::binary;
    expr.range = range;
    expr.binary_op = op;
    expr.binary_lhs = lhs;
    expr.binary_rhs = rhs;
    return module_.push_expr(std::move(expr));
}

syntax::ExprId Parser::make_invalid_expr() {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::invalid;
    expr.range = peek().range;
    if (!is_eof()) {
        advance();
    }
    return module_.push_expr(expr);
}

base::SourceRange Parser::merge(base::SourceRange begin, base::SourceRange end) const noexcept {
    return base::SourceRange {begin.source, begin.begin, end.end};
}

} // namespace aurex::parse

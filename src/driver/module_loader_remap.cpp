#include "module_loader_remap.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace aurex::driver {
namespace {

struct IdMap {
    std::vector<syntax::TypeId> types;
    std::vector<syntax::ExprId> exprs;
    std::vector<syntax::PatternId> patterns;
    std::vector<syntax::StmtId> stmts;
    std::vector<syntax::ItemId> items;
};

[[nodiscard]] syntax::TypeId remap_type(const syntax::TypeId id, const IdMap& map)
{
    return syntax::is_valid(id) && id.value < map.types.size() ? map.types[id.value] : syntax::INVALID_TYPE_ID;
}

[[nodiscard]] syntax::ExprId remap_expr(const syntax::ExprId id, const IdMap& map)
{
    return syntax::is_valid(id) && id.value < map.exprs.size() ? map.exprs[id.value] : syntax::INVALID_EXPR_ID;
}

[[nodiscard]] syntax::StmtId remap_stmt(const syntax::StmtId id, const IdMap& map)
{
    return syntax::is_valid(id) && id.value < map.stmts.size() ? map.stmts[id.value] : syntax::INVALID_STMT_ID;
}

[[nodiscard]] syntax::PatternId remap_pattern(const syntax::PatternId id, const IdMap& map)
{
    return syntax::is_valid(id) && id.value < map.patterns.size() ? map.patterns[id.value] : syntax::INVALID_PATTERN_ID;
}

[[nodiscard]] syntax::ItemId remap_item(const syntax::ItemId id, const IdMap& map)
{
    return syntax::is_valid(id) && id.value < map.items.size() ? map.items[id.value] : syntax::INVALID_ITEM_ID;
}

void remap_type_node(syntax::TypeNode& node, const IdMap& map)
{
    for (syntax::TypeId& arg : node.type_args) {
        arg = remap_type(arg, map);
    }
    node.pointee = remap_type(node.pointee, map);
    node.array_element = remap_type(node.array_element, map);
    node.slice_element = remap_type(node.slice_element, map);
    for (syntax::TypeId& param : node.function_params) {
        param = remap_type(param, map);
    }
    node.function_return = remap_type(node.function_return, map);
}

template <typename Allocator>
void remap_expr_ids(std::vector<syntax::ExprId, Allocator>& args, const IdMap& map)
{
    for (syntax::ExprId& arg : args) {
        arg = remap_expr(arg, map);
    }
}

template <typename Allocator>
void remap_type_ids(std::vector<syntax::TypeId, Allocator>& args, const IdMap& map)
{
    for (syntax::TypeId& arg : args) {
        arg = remap_type(arg, map);
    }
}

template <typename Allocator>
void remap_field_inits(std::vector<syntax::FieldInit, Allocator>& inits, const IdMap& map)
{
    for (syntax::FieldInit& init : inits) {
        init.value = remap_expr(init.value, map);
    }
}

template <typename Allocator>
void remap_match_arms(std::vector<syntax::MatchArm, Allocator>& arms, const IdMap& map)
{
    for (syntax::MatchArm& arm : arms) {
        arm.pattern = remap_pattern(arm.pattern, map);
        arm.guard = remap_expr(arm.guard, map);
        arm.value = remap_expr(arm.value, map);
    }
}

template <typename T, typename Allocator>
[[nodiscard]] syntax::AstArenaVector<T> copy_expr_arena_list(
    syntax::AstModule& destination, const std::vector<T, Allocator>& values)
{
    syntax::AstArenaVector<T> copy = destination.make_expr_list<T>();
    copy.reserve(values.size());
    copy.insert(copy.end(), values.begin(), values.end());
    return copy;
}

[[nodiscard]] syntax::ExprId append_remapped_expr(
    syntax::AstModule& source, syntax::AstModule& destination, const base::usize index, const IdMap& map)
{
    const syntax::ExprKind kind = source.exprs.kind(index);
    const base::SourceRange range = source.exprs.range(index);
    switch (kind) {
        case syntax::ExprKind::invalid:
            return destination.push_invalid_expr(range);
        case syntax::ExprKind::integer_literal:
        case syntax::ExprKind::float_literal:
        case syntax::ExprKind::bool_literal:
        case syntax::ExprKind::null_literal:
        case syntax::ExprKind::string_literal:
        case syntax::ExprKind::c_string_literal:
        case syntax::ExprKind::raw_string_literal:
        case syntax::ExprKind::byte_string_literal:
        case syntax::ExprKind::byte_literal:
        case syntax::ExprKind::char_literal: {
            const syntax::LiteralExprPayload* const payload = source.exprs.literal_payload(index);
            return destination.push_literal_expr(kind, range, payload != nullptr ? payload->text : std::string_view{});
        }
        case syntax::ExprKind::name: {
            std::string_view scope_name;
            base::SourceRange scope_range;
            std::string_view text;
            syntax::IdentId scope_name_id = syntax::INVALID_IDENT_ID;
            syntax::IdentId text_id = syntax::INVALID_IDENT_ID;
            syntax::AstArenaVector<syntax::TypeId> type_args = destination.make_expr_list<syntax::TypeId>();
            if (syntax::NameExprPayload* const source_payload = source.exprs.name_payload(index);
                source_payload != nullptr) {
                scope_name = source_payload->scope_name;
                scope_range = source_payload->scope_range;
                text = source_payload->text;
                scope_name_id = source_payload->scope_name_id;
                text_id = source_payload->text_id;
                type_args = copy_expr_arena_list(destination, source_payload->type_args);
                remap_type_ids(type_args, map);
            }
            return destination.push_name_expr(
                range, scope_name, scope_range, text, std::move(type_args), scope_name_id, text_id);
        }
        case syntax::ExprKind::generic_apply: {
            syntax::ExprId callee = syntax::INVALID_EXPR_ID;
            syntax::AstArenaVector<syntax::TypeId> type_args = destination.make_expr_list<syntax::TypeId>();
            if (syntax::GenericApplyExprPayload* const source_payload = source.exprs.generic_apply_payload(index);
                source_payload != nullptr) {
                callee = source_payload->callee;
                type_args = copy_expr_arena_list(destination, source_payload->type_args);
            }
            callee = remap_expr(callee, map);
            remap_type_ids(type_args, map);
            return destination.push_generic_apply_expr(range, callee, std::move(type_args));
        }
        case syntax::ExprKind::unary: {
            syntax::UnaryOp op = syntax::UnaryOp::logical_not;
            syntax::ExprId operand = syntax::INVALID_EXPR_ID;
            if (syntax::UnaryExprPayload* const source_payload = source.exprs.unary_payload(index);
                source_payload != nullptr) {
                op = source_payload->op;
                operand = source_payload->operand;
            }
            operand = remap_expr(operand, map);
            return destination.push_unary_expr(syntax::ExprKind::unary, range, op, operand);
        }
        case syntax::ExprKind::try_expr: {
            syntax::ExprId operand = syntax::INVALID_EXPR_ID;
            if (syntax::TryExprPayload* const source_payload = source.exprs.try_payload(index);
                source_payload != nullptr) {
                operand = source_payload->operand;
            }
            operand = remap_expr(operand, map);
            return destination.push_try_expr(range, operand);
        }
        case syntax::ExprKind::binary: {
            syntax::BinaryOp op = syntax::BinaryOp::add;
            syntax::ExprId lhs = syntax::INVALID_EXPR_ID;
            syntax::ExprId rhs = syntax::INVALID_EXPR_ID;
            if (syntax::BinaryExprPayload* const source_payload = source.exprs.binary_payload(index);
                source_payload != nullptr) {
                op = source_payload->op;
                lhs = source_payload->lhs;
                rhs = source_payload->rhs;
            }
            lhs = remap_expr(lhs, map);
            rhs = remap_expr(rhs, map);
            return destination.push_binary_expr(range, op, lhs, rhs);
        }
        case syntax::ExprKind::call:
        case syntax::ExprKind::str_from_bytes_unchecked: {
            syntax::ExprId callee = syntax::INVALID_EXPR_ID;
            syntax::AstArenaVector<syntax::ExprId> args = destination.make_expr_list<syntax::ExprId>();
            if (syntax::CallExprPayload* const source_payload = source.exprs.call_payload(index);
                source_payload != nullptr) {
                callee = source_payload->callee;
                args = copy_expr_arena_list(destination, source_payload->args);
            }
            callee = remap_expr(callee, map);
            remap_expr_ids(args, map);
            return destination.push_call_expr(kind, range, callee, std::move(args));
        }
        case syntax::ExprKind::if_expr: {
            syntax::ExprId condition = syntax::INVALID_EXPR_ID;
            syntax::PatternId condition_pattern = syntax::INVALID_PATTERN_ID;
            syntax::ExprId then_expr = syntax::INVALID_EXPR_ID;
            syntax::ExprId else_expr = syntax::INVALID_EXPR_ID;
            if (syntax::IfExprPayload* const source_payload = source.exprs.if_payload(index);
                source_payload != nullptr) {
                condition = source_payload->condition;
                condition_pattern = source_payload->condition_pattern;
                then_expr = source_payload->then_expr;
                else_expr = source_payload->else_expr;
            }
            condition = remap_expr(condition, map);
            condition_pattern = remap_pattern(condition_pattern, map);
            then_expr = remap_expr(then_expr, map);
            else_expr = remap_expr(else_expr, map);
            return destination.push_if_expr(range, condition, condition_pattern, then_expr, else_expr);
        }
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block: {
            syntax::StmtId block = syntax::INVALID_STMT_ID;
            syntax::ExprId result = syntax::INVALID_EXPR_ID;
            if (syntax::BlockExprPayload* const source_payload = source.exprs.block_payload(index);
                source_payload != nullptr) {
                block = source_payload->block;
                result = source_payload->result;
            }
            block = remap_stmt(block, map);
            result = remap_expr(result, map);
            return destination.push_block_expr(kind, range, block, result);
        }
        case syntax::ExprKind::match_expr: {
            syntax::ExprId value = syntax::INVALID_EXPR_ID;
            syntax::AstArenaVector<syntax::MatchArm> arms = destination.make_expr_list<syntax::MatchArm>();
            if (syntax::MatchExprPayload* const source_payload = source.exprs.match_payload(index);
                source_payload != nullptr) {
                value = source_payload->value;
                arms = copy_expr_arena_list(destination, source_payload->arms);
            }
            value = remap_expr(value, map);
            remap_match_arms(arms, map);
            return destination.push_match_expr(range, value, std::move(arms));
        }
        case syntax::ExprKind::array_literal: {
            syntax::AstArenaVector<syntax::ExprId> elements = destination.make_expr_list<syntax::ExprId>();
            syntax::ExprId repeat_value = syntax::INVALID_EXPR_ID;
            syntax::ExprId repeat_count = syntax::INVALID_EXPR_ID;
            if (syntax::ArrayExprPayload* const source_payload = source.exprs.array_payload(index);
                source_payload != nullptr) {
                elements = copy_expr_arena_list(destination, source_payload->elements);
                repeat_value = source_payload->repeat_value;
                repeat_count = source_payload->repeat_count;
            }
            remap_expr_ids(elements, map);
            repeat_value = remap_expr(repeat_value, map);
            repeat_count = remap_expr(repeat_count, map);
            return destination.push_array_expr(range, std::move(elements), repeat_value, repeat_count);
        }
        case syntax::ExprKind::tuple_literal: {
            syntax::AstArenaVector<syntax::ExprId> elements = destination.make_expr_list<syntax::ExprId>();
            if (syntax::AstArenaVector<syntax::ExprId>* const source_payload = source.exprs.tuple_elements(index);
                source_payload != nullptr) {
                elements = copy_expr_arena_list(destination, *source_payload);
            }
            remap_expr_ids(elements, map);
            return destination.push_tuple_expr(range, std::move(elements));
        }
        case syntax::ExprKind::field: {
            syntax::ExprId object = syntax::INVALID_EXPR_ID;
            std::string_view field_name;
            syntax::IdentId field_name_id = syntax::INVALID_IDENT_ID;
            if (syntax::FieldExprPayload* const source_payload = source.exprs.field_payload(index);
                source_payload != nullptr) {
                object = source_payload->object;
                field_name = source_payload->field_name;
                field_name_id = source_payload->field_name_id;
            }
            object = remap_expr(object, map);
            return destination.push_field_expr(range, object, field_name, field_name_id);
        }
        case syntax::ExprKind::index: {
            syntax::ExprId object = syntax::INVALID_EXPR_ID;
            syntax::ExprId index_expr = syntax::INVALID_EXPR_ID;
            if (syntax::IndexExprPayload* const source_payload = source.exprs.index_payload(index);
                source_payload != nullptr) {
                object = source_payload->object;
                index_expr = source_payload->index;
            }
            object = remap_expr(object, map);
            index_expr = remap_expr(index_expr, map);
            return destination.push_index_expr(range, object, index_expr);
        }
        case syntax::ExprKind::slice: {
            syntax::ExprId object = syntax::INVALID_EXPR_ID;
            syntax::ExprId start = syntax::INVALID_EXPR_ID;
            syntax::ExprId end = syntax::INVALID_EXPR_ID;
            if (syntax::SliceExprPayload* const source_payload = source.exprs.slice_payload(index);
                source_payload != nullptr) {
                object = source_payload->object;
                start = source_payload->start;
                end = source_payload->end;
            }
            object = remap_expr(object, map);
            start = remap_expr(start, map);
            end = remap_expr(end, map);
            return destination.push_slice_expr(range, object, start, end);
        }
        case syntax::ExprKind::struct_literal: {
            syntax::ExprId object = syntax::INVALID_EXPR_ID;
            std::string_view scope_name;
            base::SourceRange scope_range;
            std::string_view name;
            syntax::IdentId scope_name_id = syntax::INVALID_IDENT_ID;
            syntax::IdentId name_id = syntax::INVALID_IDENT_ID;
            syntax::AstArenaVector<syntax::TypeId> type_args = destination.make_expr_list<syntax::TypeId>();
            syntax::AstArenaVector<syntax::FieldInit> field_inits = destination.make_expr_list<syntax::FieldInit>();
            if (syntax::StructLiteralExprPayload* const source_payload = source.exprs.struct_literal_payload(index);
                source_payload != nullptr) {
                object = source_payload->object;
                scope_name = source_payload->scope_name;
                scope_range = source_payload->scope_range;
                name = source_payload->name;
                scope_name_id = source_payload->scope_name_id;
                name_id = source_payload->name_id;
                type_args = copy_expr_arena_list(destination, source_payload->type_args);
                field_inits = copy_expr_arena_list(destination, source_payload->field_inits);
            }
            object = remap_expr(object, map);
            remap_type_ids(type_args, map);
            remap_field_inits(field_inits, map);
            return destination.push_struct_literal_expr(range, object, scope_name, scope_range, name,
                std::move(type_args), std::move(field_inits), scope_name_id, name_id);
        }
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
        case syntax::ExprKind::ptr_addr:
        case syntax::ExprKind::paddr:
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked: {
            syntax::TypeId type = syntax::INVALID_TYPE_ID;
            syntax::ExprId expr = syntax::INVALID_EXPR_ID;
            if (syntax::CastExprPayload* const source_payload = source.exprs.cast_payload(index);
                source_payload != nullptr) {
                type = source_payload->type;
                expr = source_payload->expr;
            }
            type = remap_type(type, map);
            expr = remap_expr(expr, map);
            return destination.push_cast_like_expr(kind, range, type, expr);
        }
        default:
            return destination.push_invalid_expr(range);
    }
}

void remap_pattern_node(syntax::PatternNode& node, const IdMap& map)
{
    node.enum_type = remap_type(node.enum_type, map);
    for (syntax::PatternId& payload : node.payload_patterns) {
        payload = remap_pattern(payload, map);
    }
    for (syntax::PatternId& element : node.elements) {
        element = remap_pattern(element, map);
    }
    for (syntax::FieldPattern& field : node.field_patterns) {
        field.pattern = remap_pattern(field.pattern, map);
    }
    for (syntax::PatternId& alternative : node.alternatives) {
        alternative = remap_pattern(alternative, map);
    }
}

void remap_stmt_node(syntax::StmtNode& node, const IdMap& map)
{
    node.declared_type = remap_type(node.declared_type, map);
    node.init = remap_expr(node.init, map);
    node.lhs = remap_expr(node.lhs, map);
    node.rhs = remap_expr(node.rhs, map);
    node.condition = remap_expr(node.condition, map);
    node.pattern = remap_pattern(node.pattern, map);
    node.range_start = remap_expr(node.range_start, map);
    node.range_end = remap_expr(node.range_end, map);
    node.range_step = remap_expr(node.range_step, map);
    node.then_block = remap_stmt(node.then_block, map);
    node.else_block = remap_stmt(node.else_block, map);
    node.else_if = remap_stmt(node.else_if, map);
    node.body = remap_stmt(node.body, map);
    node.for_init = remap_stmt(node.for_init, map);
    node.for_update = remap_stmt(node.for_update, map);
    node.return_value = remap_expr(node.return_value, map);
    for (syntax::StmtId& stmt : node.statements) {
        stmt = remap_stmt(stmt, map);
    }
}

void remap_item_node(syntax::ItemNode& node, const IdMap& map)
{
    node.const_type = remap_type(node.const_type, map);
    node.const_value = remap_expr(node.const_value, map);
    node.alias_type = remap_type(node.alias_type, map);
    for (syntax::FieldDecl& field : node.fields) {
        field.type = remap_type(field.type, map);
    }
    node.enum_base_type = remap_type(node.enum_base_type, map);
    for (syntax::EnumCaseDecl& enum_case : node.enum_cases) {
        enum_case.payload_type = remap_type(enum_case.payload_type, map);
        for (syntax::TypeId& payload_type : enum_case.payload_types) {
            payload_type = remap_type(payload_type, map);
        }
    }
    for (syntax::ParamDecl& param : node.params) {
        param.type = remap_type(param.type, map);
    }
    node.return_type = remap_type(node.return_type, map);
    node.body = remap_stmt(node.body, map);
    node.impl_type = remap_type(node.impl_type, map);
    for (syntax::ItemId& item : node.extern_items) {
        item = remap_item(item, map);
    }
    for (syntax::ItemId& item : node.impl_items) {
        item = remap_item(item, map);
    }
}

} // namespace

[[nodiscard]] bool ast_payloads_empty(const syntax::AstModule& module) noexcept
{
    return module.types.empty() && module.exprs.empty() && module.patterns.empty() && module.stmts.empty()
        && module.items.empty() && module.item_modules.empty() && module.item_part_indices.empty()
        && module.item_import_scopes.empty();
}

void move_root_module_into_empty_combined(
    syntax::AstModule& combined, syntax::AstModule&& module, const syntax::ModuleId owner_module)
{
    syntax::ModuleInfo root_info;
    root_info.path = module.module_path;
    module.intern_module_path(root_info.path);

    combined = std::move(module);
    combined.modules.clear();
    combined.modules.push_back(std::move(root_info));
    combined.item_modules.assign(combined.items.size(), owner_module);
    combined.item_part_indices.assign(combined.items.size(), 0);
}

void append_module_into(syntax::AstModule& destination, syntax::AstModule&& source, const bool keep_imports,
    const syntax::ModuleId owner_module, const base::u32 owner_part_index,
    const std::span<const syntax::ResolvedImport> visible_imports)
{
    IdMap map;
    const base::usize source_type_count = source.types.size();
    const base::usize source_expr_count = source.exprs.size();
    const base::usize source_pattern_count = source.patterns.size();
    const base::usize source_stmt_count = source.stmts.size();
    const base::usize source_item_count = source.items.size();
    const base::usize type_begin = destination.types.size();
    const base::usize expr_begin = destination.exprs.size();
    const base::usize pattern_begin = destination.patterns.size();
    const base::usize stmt_begin = destination.stmts.size();
    const base::usize item_begin = destination.items.size();

    map.types.reserve(source_type_count);
    map.exprs.reserve(source_expr_count);
    map.patterns.reserve(source_pattern_count);
    map.stmts.reserve(source_stmt_count);
    map.items.reserve(source_item_count);
    destination.types.reserve(type_begin + source_type_count);
    destination.exprs.reserve(expr_begin + source_expr_count);
    destination.patterns.reserve(pattern_begin + source_pattern_count);
    destination.stmts.reserve(stmt_begin + source_stmt_count);
    destination.items.reserve(item_begin + source_item_count);
    destination.item_modules.reserve(destination.item_modules.size() + source_item_count);
    destination.item_part_indices.reserve(destination.item_part_indices.size() + source_item_count);

    for (base::usize i = 0; i < source_type_count; ++i) {
        map.types.push_back(syntax::TypeId{static_cast<base::u32>(type_begin + i)});
    }
    for (base::usize i = 0; i < source_expr_count; ++i) {
        map.exprs.push_back(syntax::ExprId{static_cast<base::u32>(expr_begin + i)});
    }
    for (base::usize i = 0; i < source_pattern_count; ++i) {
        map.patterns.push_back(syntax::PatternId{static_cast<base::u32>(pattern_begin + i)});
    }
    for (base::usize i = 0; i < source_stmt_count; ++i) {
        map.stmts.push_back(syntax::StmtId{static_cast<base::u32>(stmt_begin + i)});
    }
    for (base::usize i = 0; i < source_item_count; ++i) {
        map.items.push_back(syntax::ItemId{static_cast<base::u32>(item_begin + i)});
    }

    for (base::usize i = 0; i < source_type_count; ++i) {
        syntax::TypeNode node = source.types.take(i);
        remap_type_node(node, map);
        static_cast<void>(destination.push_type(std::move(node)));
    }
    for (base::usize i = 0; i < source_expr_count; ++i) {
        static_cast<void>(append_remapped_expr(source, destination, i, map));
    }
    for (base::usize i = 0; i < source_pattern_count; ++i) {
        syntax::PatternNode node = source.patterns.take(i);
        remap_pattern_node(node, map);
        static_cast<void>(destination.push_pattern(std::move(node)));
    }
    for (base::usize i = 0; i < source_stmt_count; ++i) {
        syntax::StmtNode node = source.stmts.take(i);
        remap_stmt_node(node, map);
        static_cast<void>(destination.push_stmt(std::move(node)));
    }
    for (base::usize i = 0; i < source_item_count; ++i) {
        syntax::ItemNode node = source.items.take(i);
        remap_item_node(node, map);
        static_cast<void>(destination.push_item_for_module(std::move(node), owner_module, owner_part_index));
    }
    if (source_item_count != 0) {
        syntax::ItemImportScope scope;
        scope.item_begin = static_cast<base::u32>(item_begin);
        scope.item_count = static_cast<base::u32>(source_item_count);
        scope.part_index = owner_part_index;
        scope.imports.assign(visible_imports.begin(), visible_imports.end());
        for (syntax::ResolvedImport& import : scope.imports) {
            destination.intern_resolved_import(import);
        }
        destination.item_import_scopes.push_back(std::move(scope));
    }

    if (keep_imports) {
        for (syntax::ImportDecl import : source.imports) {
            destination.intern_import_decl(import);
            destination.imports.push_back(std::move(import));
        }
    }
}

} // namespace aurex::driver

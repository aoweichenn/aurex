#include <ir/lower_ast_internal.hpp>

#include <aurex/ir/enum_layout.hpp>

#include <unordered_set>
#include <utility>

namespace aurex::ir::detail {

namespace {

[[nodiscard]] Linkage item_linkage(const syntax::ItemNode& item) noexcept {
    if (item.is_extern_c) {
        return Linkage::extern_c;
    }
    if (item.is_export_c) {
        return Linkage::export_c;
    }
    return Linkage::internal;
}

[[nodiscard]] bool is_root_aurex_entry(
    const syntax::AstModule& ast,
    const base::u32 index,
    const syntax::ItemNode& item
) noexcept {
    return item.kind == syntax::ItemKind::fn_decl &&
           item.name == "main" &&
           !syntax::is_valid(item.impl_type) &&
           !item.is_extern_c &&
           !item.is_export_c &&
           index < ast.item_modules.size() &&
           ast.item_modules[index].value == 0;
}

} // namespace

Lowerer::Lowerer(const syntax::AstModule& ast, const sema::CheckedModule& checked)
    : ast_(ast), checked_(checked) {
    module_.types = checked_.types;
    item_functions_.assign(ast_.items.size(), INVALID_FUNCTION_ID);
    index_enum_cases();
}

Module Lowerer::lower() {
    lower_record_layouts();
    declare_global_constants();
    lower_function_declarations();
    lower_global_constant_initializers();
    for (base::u32 index = 0; index < ast_.items.size(); ++index) {
        const syntax::ItemNode& item = ast_.items[index];
        if (item.kind != syntax::ItemKind::fn_decl || item.is_extern_c || !syntax::is_valid(item.body)) {
            continue;
        }
        lower_function_body(item_functions_[index], item);
    }
    return std::move(module_);
}

void Lowerer::lower_record_layouts() {
    for (const auto& entry : checked_.structs) {
        const sema::StructInfo& info = entry.second;
        RecordLayout record;
        record.type = info.type;
        record.name = info.name;
        record.symbol = info.c_name;
        record.is_opaque = info.is_opaque;
        for (const sema::StructFieldInfo& field : info.fields) {
            record.fields.push_back(RecordField {
                field.name,
                field.type,
            });
        }
        if (sema::is_valid(record.type)) {
            module_.record_indices[record.type.value] = static_cast<base::u32>(module_.records.size());
        }
        module_.records.push_back(std::move(record));
    }

    std::unordered_set<base::u32> lowered_enum_types;
    for (const auto& entry : checked_.enum_cases) {
        const sema::EnumCaseInfo& enum_case = entry.second;
        if (sema::is_valid(enum_case.type) && !lowered_enum_types.insert(enum_case.type.value).second) {
            continue;
        }
        if (!is_payload_enum(module_.types, enum_case.type)) {
            continue;
        }
        RecordLayout record = make_payload_enum_record(module_.types, enum_case.type);
        if (sema::is_valid(record.type)) {
            module_.record_indices[record.type.value] = static_cast<base::u32>(module_.records.size());
        }
        module_.records.push_back(std::move(record));
    }
}

void Lowerer::index_enum_cases() {
    enum_cases_by_name_.reserve(checked_.enum_cases.size());
    enum_cases_by_c_name_.reserve(checked_.enum_cases.size());
    enum_cases_by_type_and_case_.reserve(checked_.enum_cases.size());
    for (const auto& entry : checked_.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        enum_cases_by_name_.emplace(std::string_view(info.name), &info);
        enum_cases_by_c_name_.emplace(std::string_view(info.c_name), &info);
        enum_cases_by_type_and_case_.emplace(EnumCaseTypeKey {info.type.value, info.case_name}, &info);
    }
}

void Lowerer::declare_global_constants() {
    for (base::u32 index = 0; index < ast_.items.size(); ++index) {
        const syntax::ItemNode& item = ast_.items[index];
        if (item.kind == syntax::ItemKind::const_decl) {
            GlobalConstant constant;
            constant.name = std::string(item.name);
            constant.symbol = item_symbol(index, item);
            constant.type = syntax_type(item.const_type);
            const GlobalConstantId id = add_global_constant(module_, std::move(constant));
            constant_symbols_[module_.constants[id.value].symbol] = id;
            pending_constants_.push_back(PendingConstant {
                id,
                item.const_value,
                module_.constants[id.value].type,
                {},
                false,
            });
            continue;
        }
        if (item.kind != syntax::ItemKind::enum_decl) {
            continue;
        }
        for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
            const sema::TypeHandle case_type = enum_case_type(enum_case_symbol(index, item, enum_case));
            if (is_payload_enum(module_.types, case_type) || syntax::is_valid(enum_case.payload_type)) {
                continue;
            }
            GlobalConstant constant;
            constant.name = std::string(item.name) + "_" + std::string(enum_case.name);
            constant.symbol = enum_case_symbol(index, item, enum_case);
            constant.type = case_type;
            const GlobalConstantId id = add_global_constant(module_, std::move(constant));
            constant_symbols_[module_.constants[id.value].symbol] = id;
            pending_constants_.push_back(PendingConstant {
                id,
                syntax::INVALID_EXPR_ID,
                module_.constants[id.value].type,
                std::string(enum_case.value_text),
                true,
            });
        }
    }

    for (const auto& entry : checked_.enum_cases) {
        const sema::EnumCaseInfo& enum_case = entry.second;
        if (is_payload_enum(module_.types, enum_case.type) || sema::is_valid(enum_case.payload_type)) {
            continue;
        }
        if (constant_symbols_.contains(enum_case.c_name)) {
            continue;
        }
        GlobalConstant constant;
        constant.name = enum_case.name;
        constant.symbol = enum_case.c_name;
        constant.type = enum_case.type;
        const GlobalConstantId id = add_global_constant(module_, std::move(constant));
        constant_symbols_[module_.constants[id.value].symbol] = id;
        pending_constants_.push_back(PendingConstant {
            id,
            syntax::INVALID_EXPR_ID,
            module_.constants[id.value].type,
            enum_case.value_text,
            true,
        });
    }
}

void Lowerer::lower_function_declarations() {
    for (base::u32 index = 0; index < ast_.items.size(); ++index) {
        const syntax::ItemNode& item = ast_.items[index];
        if (item.kind != syntax::ItemKind::fn_decl || item.is_prototype) {
            continue;
        }
        Function function;
        function.name = std::string(item.name);
        function.symbol = item_symbol(index, item);
        function.linkage = item_linkage(item);
        function.call_conv = item.is_extern_c || item.is_export_c
            ? AbiCallConv::c
            : AbiCallConv::aurex;
        function.is_entry = is_root_aurex_entry(ast_, index, item);
        function.is_variadic = item.is_variadic;
        function.return_type = function_return_type(index, item);
        for (const syntax::ParamDecl& param : item.params) {
            function.signature_params.push_back(FunctionParam {
                std::string(param.name),
                syntax_type(param.type),
            });
        }
        const FunctionId function_id {static_cast<base::u32>(module_.functions.size())};
        item_functions_[index] = function_id;
        function_symbols_[function.symbol] = function_id;
        module_.functions.push_back(std::move(function));
    }
}

void Lowerer::lower_global_constant_initializers() {
    const bool previous_constant_initializer = this->lowering_constant_initializer_;
    this->lowering_constant_initializer_ = true;
    for (const PendingConstant& pending : this->pending_constants_) {
        if (!is_valid(pending.id) || pending.id.value >= this->module_.constants.size()) {
            continue;
        }
        ValueId initializer = INVALID_VALUE_ID;
        if (pending.is_literal) {
            Value value;
            value.kind = ValueKind::integer_literal;
            value.type = pending.type;
            value.text = pending.literal_text;
            initializer = this->append_value(value);
        } else {
            initializer = this->lower_expr(pending.initializer, pending.type);
            initializer = this->coerce_value(initializer, pending.type);
        }
        this->module_.constants[pending.id.value].initializer = initializer;
    }
    this->lowering_constant_initializer_ = previous_constant_initializer;
}

std::string Lowerer::item_symbol(const base::u32 index, const syntax::ItemNode& item) const {
    if (index < checked_.item_c_names.size() && !checked_.item_c_names[index].empty()) {
        return checked_.item_c_names[index];
    }
    if (!item.abi_name.empty()) {
        return std::string(item.abi_name);
    }
    return std::string(item.name);
}

std::string Lowerer::enum_case_symbol(
    const base::u32 index,
    const syntax::ItemNode& item,
    const syntax::EnumCaseDecl& enum_case
) const {
    return item_symbol(index, item) + "_" + std::string(enum_case.name);
}

sema::TypeHandle Lowerer::enum_case_type(const std::string& symbol) const noexcept {
    for (const auto& entry : checked_.enum_cases) {
        if (entry.second.c_name == symbol) {
            return entry.second.type;
        }
    }
    return sema::INVALID_TYPE_HANDLE;
}

sema::TypeHandle Lowerer::function_return_type(const base::u32 index, const syntax::ItemNode& item) const noexcept {
    const std::string symbol = item_symbol(index, item);
    for (const auto& entry : checked_.functions) {
        if (entry.second.c_name == symbol) {
            return entry.second.return_type;
        }
    }
    return syntax_type(item.return_type);
}

} // namespace aurex::ir::detail

namespace aurex::ir {

base::Result<Module> lower_ast(const syntax::AstModule& ast, const sema::CheckedModule& checked) {
    detail::Lowerer lowerer(ast, checked);
    return base::Result<Module>::ok(lowerer.lower());
}

} // namespace aurex::ir

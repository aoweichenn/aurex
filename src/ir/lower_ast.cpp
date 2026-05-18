#include <aurex/ir/enum_layout.hpp>

#include <ir/lower_ast_internal.hpp>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::ir::detail {

namespace {

[[nodiscard]] Linkage item_linkage(const syntax::ItemNode& item) noexcept
{
    if (item.is_extern_c) {
        return Linkage::extern_c;
    }
    if (item.is_export_c) {
        return Linkage::export_c;
    }
    return Linkage::internal;
}

[[nodiscard]] bool is_root_aurex_entry(
    const syntax::AstModule& ast, const base::u32 index, const syntax::ItemNode& item) noexcept
{
    return item.kind == syntax::ItemKind::fn_decl && item.name == "main" && !syntax::is_valid(item.impl_type)
        && !item.is_extern_c && !item.is_export_c && index < ast.item_modules.size()
        && ast.item_modules[index].value == 0;
}

[[nodiscard]] bool type_contains_generic_param(const sema::TypeTable& types, const sema::TypeHandle type)
{
    std::vector<sema::TypeHandle> pending;
    pending.push_back(type);
    while (!pending.empty()) {
        const sema::TypeHandle current = pending.back();
        pending.pop_back();
        if (!sema::is_valid(current)) {
            continue;
        }
        const sema::TypeInfo& info = types.get(current);
        switch (info.kind) {
            case sema::TypeKind::generic_param:
                return true;
            case sema::TypeKind::pointer:
            case sema::TypeKind::reference:
                pending.push_back(info.pointee);
                break;
            case sema::TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case sema::TypeKind::slice:
                pending.push_back(info.slice_element);
                break;
            case sema::TypeKind::tuple:
                for (const sema::TypeHandle element : info.tuple_elements) {
                    pending.push_back(element);
                }
                break;
            case sema::TypeKind::function:
                pending.push_back(info.function_return);
                for (const sema::TypeHandle param : info.function_params) {
                    pending.push_back(param);
                }
                break;
            case sema::TypeKind::struct_:
                for (const sema::TypeHandle arg : info.generic_args) {
                    pending.push_back(arg);
                }
                break;
            case sema::TypeKind::builtin:
            case sema::TypeKind::enum_:
            case sema::TypeKind::opaque_struct:
                break;
        }
    }
    return false;
}

} // namespace

Lowerer::Lowerer(const syntax::AstModule& ast, const sema::CheckedModule& checked) : ast_(ast), checked_(checked)
{
    this->module_.types = this->checked_.types;
    this->module_.reserve(this->ast_.exprs.size(),
        this->ast_.items.size() + this->checked_.generic_function_instances.size(),
        this->checked_.structs.size() + this->checked_.enum_cases.size(),
        this->ast_.items.size() + this->checked_.enum_cases.size());
    this->item_functions_.assign(this->ast_.items.size(), INVALID_FUNCTION_ID);
    this->generic_instance_functions_.assign(this->checked_.generic_function_instances.size(), INVALID_FUNCTION_ID);
    this->active_side_tables_ = ActiveSideTables{
        nullptr,
        &this->checked_.expr_types,
        &this->checked_.expr_c_name_ids,
        &this->checked_.pattern_c_name_ids,
        &this->checked_.syntax_type_handles,
        &this->checked_.stmt_local_types,
    };
    this->index_enum_cases();
}

Module Lowerer::lower()
{
    this->lower_record_layouts();
    this->declare_global_constants();
    this->lower_function_declarations();
    this->lower_global_constant_initializers();
    for (base::u32 index = 0; index < this->ast_.items.size(); ++index) {
        if (this->ast_.items.kind(index) != syntax::ItemKind::fn_decl) {
            continue;
        }
        const syntax::ItemNode item = this->ast_.items[index];
        if (item.is_extern_c || !syntax::is_valid(item.body)) {
            continue;
        }
        this->lower_function_body(this->item_functions_[index], item);
    }
    for (base::u32 index = 0; index < this->checked_.generic_function_instances.size(); ++index) {
        this->lower_generic_function_body(
            this->generic_instance_functions_[index], this->checked_.generic_function_instances[index]);
    }
    return std::move(this->module_);
}

void Lowerer::lower_record_layouts()
{
    for (const auto& entry : this->checked_.structs) {
        const sema::StructInfo& info = entry.second;
        if (info.is_generic_placeholder) {
            continue;
        }
        RecordLayout record = this->module_.make_record_layout();
        record.type = info.type;
        record.name = this->module_.intern(sema::struct_display_name(this->checked_.types, info));
        record.symbol = this->module_.intern(info.c_name);
        record.is_opaque = info.is_opaque;
        for (const sema::StructFieldInfo& field : info.fields) {
            record.fields.push_back(RecordField{
                this->module_.intern(field.name),
                field.type,
            });
        }
        if (sema::is_valid(record.type)) {
            this->module_.record_indices[record.type.value] = static_cast<base::u32>(this->module_.records.size());
        }
        static_cast<void>(add_record(this->module_, record));
    }

    for (base::usize i = 0; i < this->module_.types.size(); ++i) {
        const sema::TypeHandle type{static_cast<base::u32>(i)};
        const sema::TypeInfo& info = this->module_.types.get(type);
        if (info.kind != sema::TypeKind::tuple || type_contains_generic_param(this->module_.types, type)) {
            continue;
        }
        RecordLayout record = this->module_.make_record_layout();
        record.type = type;
        record.name = this->module_.intern("tuple." + std::to_string(i));
        record.symbol = this->module_.intern("__aurex_tuple_" + std::to_string(i));
        record.fields.reserve(info.tuple_elements.size());
        for (base::usize field_index = 0; field_index < info.tuple_elements.size(); ++field_index) {
            record.fields.push_back(RecordField{
                this->module_.intern(std::to_string(field_index)),
                info.tuple_elements[field_index],
            });
        }
        this->module_.record_indices[type.value] = static_cast<base::u32>(this->module_.records.size());
        static_cast<void>(add_record(this->module_, record));
    }

    std::unordered_set<base::u32> lowered_enum_types;
    for (const auto& entry : this->checked_.enum_cases) {
        const sema::EnumCaseInfo& enum_case = entry.second;
        if (sema::is_valid(enum_case.type) && !lowered_enum_types.insert(enum_case.type.value).second) {
            continue;
        }
        if (!is_payload_enum(this->module_.types, enum_case.type)) {
            continue;
        }
        RecordLayout record = make_payload_enum_record(this->module_, enum_case.type);
        if (sema::is_valid(record.type)) {
            this->module_.record_indices[record.type.value] = static_cast<base::u32>(this->module_.records.size());
        }
        static_cast<void>(add_record(this->module_, record));
    }
}

void Lowerer::index_enum_cases()
{
    enum_cases_by_name_.reserve(checked_.enum_cases.size());
    enum_cases_by_c_name_.reserve(checked_.enum_cases.size());
    enum_cases_by_type_and_case_.reserve(checked_.enum_cases.size());
    for (const auto& entry : checked_.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        enum_cases_by_name_.emplace(info.name_id, &info);
        enum_cases_by_c_name_.emplace(this->module_.intern(info.c_name), &info);
        enum_cases_by_type_and_case_.emplace(EnumCaseTypeKey{info.type.value, info.case_name_id}, &info);
    }
}

void Lowerer::declare_global_constants()
{
    for (base::u32 index = 0; index < ast_.items.size(); ++index) {
        const syntax::ItemKind kind = ast_.items.kind(index);
        if (kind != syntax::ItemKind::const_decl && kind != syntax::ItemKind::enum_decl) {
            continue;
        }
        const syntax::ItemNode item = ast_.items[index];
        if (item.kind == syntax::ItemKind::const_decl) {
            GlobalConstant constant;
            constant.name = this->module_.intern(item.name);
            constant.symbol = this->item_symbol(index, item);
            constant.type = syntax_type(item.const_type);
            const GlobalConstantId id = add_global_constant(module_, constant);
            constant_symbols_[module_.constants[id.value].symbol] = id;
            pending_constants_.push_back(PendingConstant{
                id,
                item.const_value,
                module_.constants[id.value].type,
                INVALID_IR_TEXT_ID,
                false,
            });
            continue;
        }
        if (item.kind != syntax::ItemKind::enum_decl) {
            continue;
        }
        for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
            const sema::TypeHandle case_type = enum_case_type(enum_case_symbol(index, item, enum_case));
            if (enum_case.value_text.empty() || is_payload_enum(module_.types, case_type)
                || syntax::is_valid(enum_case.payload_type) || !enum_case.payload_types.empty()) {
                continue;
            }
            GlobalConstant constant;
            constant.name = this->module_.intern(std::string(item.name) + "_" + std::string(enum_case.name));
            constant.symbol = this->enum_case_symbol(index, item, enum_case);
            constant.type = case_type;
            const GlobalConstantId id = add_global_constant(module_, constant);
            constant_symbols_[module_.constants[id.value].symbol] = id;
            pending_constants_.push_back(PendingConstant{
                id,
                syntax::INVALID_EXPR_ID,
                module_.constants[id.value].type,
                this->module_.intern(enum_case.value_text),
                true,
            });
        }
    }

    for (const auto& entry : checked_.enum_cases) {
        const sema::EnumCaseInfo& enum_case = entry.second;
        if (is_payload_enum(module_.types, enum_case.type) || sema::is_valid(enum_case.payload_type)) {
            continue;
        }
        if (constant_symbols_.contains(this->module_.find_text(enum_case.c_name))) {
            continue;
        }
        GlobalConstant constant;
        constant.name = this->module_.intern(sema::enum_case_display_name(this->checked_.types, enum_case));
        constant.symbol = this->module_.intern(enum_case.c_name);
        constant.type = enum_case.type;
        const GlobalConstantId id = add_global_constant(module_, constant);
        constant_symbols_[module_.constants[id.value].symbol] = id;
        pending_constants_.push_back(PendingConstant{
            id,
            syntax::INVALID_EXPR_ID,
            module_.constants[id.value].type,
            this->module_.intern(enum_case.value_text),
            true,
        });
    }
}

void Lowerer::lower_function_declarations()
{
    for (base::u32 index = 0; index < this->ast_.items.size(); ++index) {
        if (this->ast_.items.kind(index) != syntax::ItemKind::fn_decl) {
            continue;
        }
        const syntax::ItemNode item = this->ast_.items[index];
        if (item.is_prototype || !item.generic_params.empty()) {
            continue;
        }
        Function function = this->module_.make_function();
        function.name = this->module_.intern(item.name);
        function.symbol = this->item_symbol(index, item);
        function.linkage = item_linkage(item);
        function.call_conv = item.is_extern_c || item.is_export_c ? AbiCallConv::c : AbiCallConv::aurex;
        function.is_entry = is_root_aurex_entry(this->ast_, index, item);
        function.is_unsafe = item.is_unsafe;
        function.is_variadic = item.is_variadic;
        function.return_type = this->function_return_type(index, item);
        for (const syntax::ParamDecl& param : item.params) {
            function.signature_params.push_back(FunctionParam{
                this->module_.intern(param.name),
                this->syntax_type(param.type),
            });
        }
        const FunctionId function_id = add_function(this->module_, function);
        this->item_functions_[index] = function_id;
        this->function_symbols_[this->module_.functions[function_id.value].symbol] = function_id;
    }

    for (base::u32 index = 0; index < this->checked_.generic_function_instances.size(); ++index) {
        const sema::GenericFunctionInstanceInfo& instance = this->checked_.generic_function_instances[index];
        Function function = this->module_.make_function();
        function.name = this->module_.intern(sema::function_display_name(this->checked_.types, instance.signature));
        function.symbol = this->module_.intern(instance.signature.c_name);
        function.linkage = Linkage::internal;
        function.call_conv = AbiCallConv::aurex;
        function.is_entry = false;
        function.is_unsafe = instance.signature.is_unsafe;
        function.is_variadic = false;
        function.return_type = instance.signature.return_type;
        if (syntax::is_valid(instance.item) && instance.item.value < this->ast_.items.size()) {
            const syntax::ItemNode item = this->ast_.items[instance.item.value];
            for (base::usize param_index = 0; param_index < item.params.size(); ++param_index) {
                const sema::TypeHandle param_type = param_index < instance.signature.param_types.size()
                    ? instance.signature.param_types[param_index]
                    : sema::INVALID_TYPE_HANDLE;
                function.signature_params.push_back(FunctionParam{
                    this->module_.intern(item.params[param_index].name),
                    param_type,
                });
            }
        }
        const FunctionId function_id = add_function(this->module_, function);
        this->generic_instance_functions_[index] = function_id;
        this->function_symbols_[this->module_.functions[function_id.value].symbol] = function_id;
    }
}

void Lowerer::lower_global_constant_initializers()
{
    const bool previous_constant_initializer = this->lowering_constant_initializer_;
    this->lowering_constant_initializer_ = true;
    for (const PendingConstant& pending : this->pending_constants_) {
        if (!is_valid(pending.id) || pending.id.value >= this->module_.constants.size()) {
            continue;
        }
        ValueId initializer = INVALID_VALUE_ID;
        if (pending.is_literal) {
            Value value = this->module_.make_value();
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

std::string_view Lowerer::item_symbol_text(const base::u32 index, const syntax::ItemNode& item) const
{
    if (index < this->checked_.item_c_name_ids.size()) {
        const std::string_view c_name = this->checked_.c_name_text(this->checked_.item_c_name_ids[index]);
        if (!c_name.empty()) {
            return c_name;
        }
    }
    if (!item.abi_name.empty()) {
        return item.abi_name;
    }
    return item.name;
}

IrTextId Lowerer::item_symbol(const base::u32 index, const syntax::ItemNode& item)
{
    return this->module_.intern(this->item_symbol_text(index, item));
}

IrTextId Lowerer::enum_case_symbol(
    const base::u32 index, const syntax::ItemNode& item, const syntax::EnumCaseDecl& enum_case)
{
    const std::string_view item_symbol = this->item_symbol_text(index, item);
    std::string symbol;
    symbol.reserve(item_symbol.size() + 1U + enum_case.name.size());
    symbol.append(item_symbol);
    symbol.push_back('_');
    symbol.append(enum_case.name);
    return this->module_.intern(symbol);
}

sema::TypeHandle Lowerer::enum_case_type(const IrTextId symbol) const noexcept
{
    if (const auto found = this->enum_cases_by_c_name_.find(symbol); found != this->enum_cases_by_c_name_.end()) {
        return found->second->type;
    }
    return sema::INVALID_TYPE_HANDLE;
}

sema::TypeHandle Lowerer::function_return_type(const base::u32 index, const syntax::ItemNode& item) noexcept
{
    const std::string_view symbol = this->item_symbol_text(index, item);
    for (const auto& entry : checked_.functions) {
        if (entry.second.c_name == symbol) {
            return entry.second.return_type;
        }
    }
    return syntax_type(item.return_type);
}

} // namespace aurex::ir::detail

namespace aurex::ir {

base::Result<Module> lower_ast(const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    detail::Lowerer lowerer(ast, checked);
    return base::Result<Module>::ok(lowerer.lower());
}

} // namespace aurex::ir

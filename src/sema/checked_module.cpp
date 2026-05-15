#include <aurex/sema/checked_module.hpp>

#include <algorithm>
#include <sstream>

namespace aurex::sema {

std::string dump_checked_module(const CheckedModule& checked) {
    std::ostringstream out;
    out << "checked_module\n";
    out << "  expr_types " << checked.expr_types.size() << "\n";

    std::vector<std::string> function_names;
    function_names.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        function_names.push_back(entry.first);
    }
    std::sort(function_names.begin(), function_names.end());
    out << "  functions " << function_names.size() << "\n";
    for (const std::string& name : function_names) {
        const FunctionSignature& fn = checked.functions.at(name);
        const std::string display_name = function_display_name(checked.types, fn);
        out << "    fn ";
        if (fn.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        if (fn.is_method) {
            out << "method " << checked.types.display_name(fn.method_owner_type) << ".";
        }
        out << display_name << " -> " << checked.types.display_name(fn.return_type);
        if (fn.is_unsafe) {
            out << " unsafe";
        }
        if (fn.c_name != display_name) {
            out << " @c_name=" << fn.c_name;
        }
        if (fn.is_extern_c) {
            out << " extern_c";
        }
        if (fn.is_variadic) {
            out << " variadic";
        }
        if (fn.is_export_c) {
            out << " export_c";
        }
        out << "\n";
    }

    std::vector<std::string> struct_names;
    struct_names.reserve(checked.structs.size());
    for (const auto& entry : checked.structs) {
        struct_names.push_back(entry.first);
    }
    std::sort(struct_names.begin(), struct_names.end());
    out << "  structs " << struct_names.size() << "\n";
    for (const std::string& name : struct_names) {
        const StructInfo& info = checked.structs.at(name);
        out << "    struct ";
        if (info.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        out << info.name;
        if (info.is_opaque) {
            out << " opaque";
        }
        if (info.is_generic_placeholder) {
            out << " generic_placeholder";
        }
        out << " fields=" << info.fields.size() << "\n";
    }

    std::vector<std::string> alias_names;
    alias_names.reserve(checked.type_aliases.size());
    for (const auto& entry : checked.type_aliases) {
        alias_names.push_back(entry.first);
    }
    std::sort(alias_names.begin(), alias_names.end());
    out << "  type_aliases " << alias_names.size() << "\n";
    for (const std::string& name : alias_names) {
        const TypeAliasInfo& alias = checked.type_aliases.at(name);
        TypeHandle resolved = INVALID_TYPE_HANDLE;
        if (alias.target.value < checked.syntax_type_handles.size()) {
            resolved = checked.syntax_type_handles[alias.target.value];
        }
        out << "    type ";
        if (alias.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        out << alias.name << " = " << checked.types.display_name(resolved) << "\n";
    }

    out << "  enum_cases " << checked.enum_cases.size() << "\n";
    std::vector<std::string> enum_case_names;
    enum_case_names.reserve(checked.enum_cases.size());
    for (const auto& entry : checked.enum_cases) {
        enum_case_names.push_back(entry.first);
    }
    std::sort(enum_case_names.begin(), enum_case_names.end());
    for (const std::string& name : enum_case_names) {
        const EnumCaseInfo& info = checked.enum_cases.at(name);
        out << "    case " << info.name << " : " << checked.types.display_name(info.type);
        if (!info.payload_types.empty()) {
            out << "(";
            for (base::usize i = 0; i < info.payload_types.size(); ++i) {
                if (i > 0) {
                    out << ",";
                }
                out << checked.types.display_name(info.payload_types[i]);
            }
            out << ")";
        } else if (is_valid(info.payload_type)) {
            out << "(" << checked.types.display_name(info.payload_type) << ")";
        }
        out << " @c_name=" << info.c_name << "\n";
    }
    return out.str();
}

} // namespace aurex::sema

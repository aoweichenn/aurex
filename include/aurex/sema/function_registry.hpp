#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/sema/function.hpp>
#include <aurex/syntax/ast.hpp>

#include <string>
#include <unordered_map>

namespace aurex::sema {

struct CheckedModule;
struct Symbol;

class FunctionRegistry final {
public:
    FunctionRegistry(
        CheckedModule& checked,
        std::unordered_map<std::string, Symbol>& global_values,
        base::DiagnosticSink& diagnostics
    ) noexcept;

    void register_function(
        const syntax::ItemNode& item,
        syntax::ModuleId owner,
        std::string key,
        std::string c_name,
        TypeHandle method_owner_type,
        TypeHandle return_type,
        std::vector<TypeHandle> param_types,
        syntax::ItemId item_id
    );

private:
    [[nodiscard]] bool same_signature(
        const FunctionSignature& existing,
        TypeHandle return_type,
        const std::vector<TypeHandle>& param_types,
        bool is_variadic
    ) const noexcept;
    void merge_function(std::string key, FunctionSignature signature, bool is_prototype);
    void insert_function_value(const std::string& key, const FunctionSignature& signature);
    void refresh_function_value(const std::string& key, const FunctionSignature& signature);
    void report(base::SourceRange range, std::string message);

    CheckedModule& checked_;
    std::unordered_map<std::string, Symbol>& global_values_;
    base::DiagnosticSink& diagnostics_;
};

} // namespace aurex::sema

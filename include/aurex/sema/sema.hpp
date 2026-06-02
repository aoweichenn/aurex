#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/base/result.hpp>
#include <aurex/query/query_key.hpp>
#include <aurex/sema/checked_module.hpp>

#include <memory>
#include <vector>

namespace aurex::base {
class DiagnosticSink;
} // namespace aurex::base

namespace aurex::syntax {
struct AstModule;
} // namespace aurex::syntax

namespace aurex::sema {

inline constexpr base::u64 SEMA_DEFAULT_TARGET_BOOL_SIZE = 1;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_BOOL_ALIGN = 1;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I8_SIZE = 1;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I8_ALIGN = 1;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I16_SIZE = 2;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I16_ALIGN = 2;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I32_SIZE = 4;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I32_ALIGN = 4;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I64_SIZE = 8;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_I64_ALIGN = 8;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_POINTER_SIZE = 8;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_POINTER_ALIGN = 8;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_F32_SIZE = 4;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_F32_ALIGN = 4;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_F64_SIZE = 8;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_F64_ALIGN = 8;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_CHAR_SIZE = 4;
inline constexpr base::u64 SEMA_DEFAULT_TARGET_CHAR_ALIGN = 4;

struct SemanticTargetLayout {
    base::u64 bool_size = SEMA_DEFAULT_TARGET_BOOL_SIZE;
    base::u64 bool_align = SEMA_DEFAULT_TARGET_BOOL_ALIGN;
    base::u64 i8_size = SEMA_DEFAULT_TARGET_I8_SIZE;
    base::u64 i8_align = SEMA_DEFAULT_TARGET_I8_ALIGN;
    base::u64 i16_size = SEMA_DEFAULT_TARGET_I16_SIZE;
    base::u64 i16_align = SEMA_DEFAULT_TARGET_I16_ALIGN;
    base::u64 i32_size = SEMA_DEFAULT_TARGET_I32_SIZE;
    base::u64 i32_align = SEMA_DEFAULT_TARGET_I32_ALIGN;
    base::u64 i64_size = SEMA_DEFAULT_TARGET_I64_SIZE;
    base::u64 i64_align = SEMA_DEFAULT_TARGET_I64_ALIGN;
    base::u64 pointer_size = SEMA_DEFAULT_TARGET_POINTER_SIZE;
    base::u64 pointer_align = SEMA_DEFAULT_TARGET_POINTER_ALIGN;
    base::u64 f32_size = SEMA_DEFAULT_TARGET_F32_SIZE;
    base::u64 f32_align = SEMA_DEFAULT_TARGET_F32_ALIGN;
    base::u64 f64_size = SEMA_DEFAULT_TARGET_F64_SIZE;
    base::u64 f64_align = SEMA_DEFAULT_TARGET_F64_ALIGN;
    base::u64 char_size = SEMA_DEFAULT_TARGET_CHAR_SIZE;
    base::u64 char_align = SEMA_DEFAULT_TARGET_CHAR_ALIGN;
};

struct SemanticOptions {
    query::PackageKey default_package;
    std::vector<query::PackageKey> module_packages;
    std::vector<std::vector<query::ModulePartKey>> module_part_keys;
    SemanticTargetLayout target_layout;
    bool retain_generic_side_tables = true;
    bool retain_body_flow_graphs = true;
};

class SemanticAnalyzer final {
public:
    SemanticAnalyzer(
        syntax::AstModule& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) noexcept;
    SemanticAnalyzer(
        const syntax::AstModule& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) = delete;
    SemanticAnalyzer(
        syntax::AstModule&& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) noexcept;
    ~SemanticAnalyzer();

    [[nodiscard]] base::Result<CheckedModule> analyze();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aurex::sema

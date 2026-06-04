#include <frontend/sema/internal/diagnostics/private/sema_diagnostics.hpp>
#include <frontend/sema/internal/dropck/private/dropck_analysis.hpp>

#include <unordered_set>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_DROPCK_BORROWED_DROP =
    "drop check rejected dropping storage while it is still borrowed";
constexpr std::string_view SEMA_DROPCK_BORROWED_FIELD_DANGLING =
    "drop check rejected destructor access to a borrowed field that may dangle";
constexpr std::string_view SEMA_DROPCK_GENERIC_TYPE_OUTLIVES =
    "drop check rejected generic drop glue because borrowed contents may not outlive the drop";
constexpr std::string_view SEMA_DROPCK_DESTRUCTOR_ESCAPE =
    "drop check rejected destructor body because it may leak borrowed storage";
constexpr std::string_view SEMA_DROPCK_GLUE_MISSING =
    "drop check could not build drop glue for this value";

} // namespace

std::string_view drop_check_violation_message(const DropCheckViolationKind kind) noexcept
{
    switch (kind) {
        case DropCheckViolationKind::borrowed_drop:
            return SEMA_DROPCK_BORROWED_DROP;
        case DropCheckViolationKind::borrowed_field_dangling:
            return SEMA_DROPCK_BORROWED_FIELD_DANGLING;
        case DropCheckViolationKind::generic_type_outlives:
            return SEMA_DROPCK_GENERIC_TYPE_OUTLIVES;
        case DropCheckViolationKind::destructor_escape:
            return SEMA_DROPCK_DESTRUCTOR_ESCAPE;
        case DropCheckViolationKind::drop_glue_missing:
            return SEMA_DROPCK_GLUE_MISSING;
    }
    return SEMA_DROPCK_GENERIC_TYPE_OUTLIVES;
}

void SemanticAnalyzerCore::DropCheckAnalyzer::analyze(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->reset(key, signature);
    this->collect(function);
    this->solve();
    this->enforce();
    this->finalize();
}

void SemanticAnalyzerCore::DropCheckAnalyzer::enforce()
{
    std::unordered_set<base::u8> emitted_violation_kinds;
    for (DropCheckViolation& violation : this->facts_.violations) {
        const base::u8 kind_key = static_cast<base::u8>(violation.kind);
        if (violation.diagnostic_emitted) {
            emitted_violation_kinds.insert(kind_key);
            continue;
        }
        if (!emitted_violation_kinds.insert(kind_key).second) {
            violation.diagnostic_emitted = true;
            continue;
        }
        this->core_.report_general(violation.range, std::string(drop_check_violation_message(violation.kind)));
        violation.diagnostic_emitted = true;
    }
}

void SemanticAnalyzerCore::DropCheckAnalyzer::finalize()
{
    for (DropCheckFact& fact : this->facts_.facts) {
        fact.fingerprint = drop_check_fact_fingerprint(fact);
    }
    this->facts_.fingerprint = function_drop_check_facts_fingerprint(this->facts_);
    this->core_.state_.checked.dropck_facts[this->key_] = this->facts_;
}

void SemanticAnalyzerCore::analyze_dropck(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    DropCheckAnalyzer(*this).analyze(function, key, signature);
}

} // namespace aurex::sema

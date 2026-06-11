#include <aurex/frontend/sema/sema_messages.hpp>

#include <support/test_support.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aurex::test {
namespace {

fs::path write_struct_field_reference_source(const fs::path& path, const std::string_view text)
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open " + path.string());
    }
    out << text;
    out.close();
    return path;
}

} // namespace

TEST_F(AurexIntegrationTest, StructFieldReferencesBorrowAndLowerToFieldAddress)
{
    const fs::path source = write_struct_field_reference_source(tmp_root() / "struct_field_reference_success.ax",
        "module struct_field_reference_success;\n"
        "\n"
        "struct Pair {\n"
        "    left: i32;\n"
        "    right: i32;\n"
        "}\n"
        "\n"
        "fn read(value: &i32) -> i32 {\n"
        "    return *value;\n"
        "}\n"
        "\n"
        "fn write(value: &mut i32, replacement: i32) -> void {\n"
        "    *value = replacement;\n"
        "}\n"
        "\n"
        "fn main() -> i32 {\n"
        "    var pair: Pair = Pair { left: 1, right: 2 };\n"
        "    let left: &i32 = &pair.left;\n"
        "    if read(left) != 1 {\n"
        "        return 1;\n"
        "    }\n"
        "    let right: &mut i32 = &mut pair.right;\n"
        "    write(right, 7);\n"
        "    if pair.right != 7 {\n"
        "        return 2;\n"
        "    }\n"
        "    return 0;\n"
        "}\n");

    require_success(aurexc() + " --check " + q(source));
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(source)).output;
    expect_contains_all(ir,
        {
            "fn read(value: &i32)",
            "fn write(value: &mut i32",
            "field_addr",
            ".left",
            ".right",
        });
}

TEST_F(AurexIntegrationTest, StructFieldReferencesKeepDisjointFieldsSeparate)
{
    const fs::path source = write_struct_field_reference_source(tmp_root() / "struct_field_reference_disjoint.ax",
        "module struct_field_reference_disjoint;\n"
        "\n"
        "struct Pair {\n"
        "    left: i32;\n"
        "    right: i32;\n"
        "}\n"
        "\n"
        "fn inspect(value: &i32) -> void {}\n"
        "\n"
        "fn main() -> void {\n"
        "    var pair: Pair = Pair { left: 1, right: 2 };\n"
        "    let left: &i32 = &pair.left;\n"
        "    pair.right = 3;\n"
        "    inspect(left);\n"
        "}\n");

    require_success(aurexc() + " --check " + q(source));
}

TEST_F(AurexIntegrationTest, StructFieldReferencesRejectConflictingFieldAndParentAccess)
{
    const fs::path field_conflict =
        write_struct_field_reference_source(tmp_root() / "struct_field_reference_field_conflict.ax",
            "module struct_field_reference_field_conflict;\n"
            "\n"
            "struct Pair {\n"
            "    left: i32;\n"
            "    right: i32;\n"
            "}\n"
            "\n"
            "fn inspect(value: &i32) -> void {}\n"
            "\n"
            "fn main() -> void {\n"
            "    var pair: Pair = Pair { left: 1, right: 2 };\n"
            "    let left: &i32 = &pair.left;\n"
            "    pair.left = 3;\n"
            "    inspect(left);\n"
            "}\n");
    const std::string field_output = require_failure(aurexc() + " --check " + q(field_conflict)).output;
    expect_contains_all(field_output,
        {
            std::string(sema::SEMA_ACTIVE_BORROW_CONFLICT),
            std::string(sema::SEMA_ACTIVE_BORROW_CREATED),
            std::string(sema::SEMA_ACTIVE_BORROW_INVALIDATING_ACTION),
            std::string(sema::SEMA_ACTIVE_BORROW_LATER_CARRIER_USE),
        });

    const fs::path parent_conflict =
        write_struct_field_reference_source(tmp_root() / "struct_field_reference_parent_conflict.ax",
            "module struct_field_reference_parent_conflict;\n"
            "\n"
            "struct Pair {\n"
            "    left: i32;\n"
            "    right: i32;\n"
            "}\n"
            "\n"
            "fn inspect(value: &i32) -> void {}\n"
            "\n"
            "fn main() -> void {\n"
            "    var pair: Pair = Pair { left: 1, right: 2 };\n"
            "    let left: &i32 = &pair.left;\n"
            "    pair = Pair { left: 3, right: 4 };\n"
            "    inspect(left);\n"
            "}\n");
    const std::string parent_output = require_failure(aurexc() + " --check " + q(parent_conflict)).output;
    expect_contains_all(parent_output,
        {
            std::string(sema::SEMA_ACTIVE_BORROW_CONFLICT),
            std::string(sema::SEMA_ACTIVE_BORROW_CREATED),
            std::string(sema::SEMA_ACTIVE_BORROW_INVALIDATING_ACTION),
            std::string(sema::SEMA_ACTIVE_BORROW_LATER_CARRIER_USE),
        });
}

TEST_F(AurexIntegrationTest, StructFieldReferencesRejectInvalidMutableAndEscapingBorrows)
{
    const fs::path immutable_base =
        write_struct_field_reference_source(tmp_root() / "struct_field_reference_immutable_base.ax",
            "module struct_field_reference_immutable_base;\n"
            "\n"
            "struct Pair {\n"
            "    left: i32;\n"
            "    right: i32;\n"
            "}\n"
            "\n"
            "fn main() -> void {\n"
            "    let pair: Pair = Pair { left: 1, right: 2 };\n"
            "    let left: &mut i32 = &mut pair.left;\n"
            "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(immutable_base)).output,
        std::string(sema::SEMA_MUTABLE_REFERENCE_PLACE));

    const fs::path escaping_field =
        write_struct_field_reference_source(tmp_root() / "struct_field_reference_escape.ax",
            "module struct_field_reference_escape;\n"
            "\n"
            "struct Pair {\n"
            "    left: i32;\n"
            "    right: i32;\n"
            "}\n"
            "\n"
            "fn leak() -> &i32 {\n"
            "    let pair: Pair = Pair { left: 1, right: 2 };\n"
            "    return &pair.left;\n"
            "}\n"
            "\n"
            "fn main() -> i32 {\n"
            "    return *leak();\n"
            "}\n");
    expect_contains(require_failure(aurexc() + " --check " + q(escaping_field)).output,
        std::string(sema::SEMA_BORROWED_LOCAL_ESCAPE));
}

} // namespace aurex::test

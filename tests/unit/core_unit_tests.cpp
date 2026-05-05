#include "aurex/backend/llvm_backend.hpp"
#include "aurex/base/abi.hpp"
#include "aurex/base/diagnostic.hpp"
#include "aurex/base/source.hpp"
#include "aurex/driver/native_toolchain.hpp"
#include "aurex/ir/ir.hpp"
#include "aurex/ir/pass_pipeline.hpp"
#include "aurex/ir/verify.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/parse/parser.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast_dump.hpp"
#include "aurex/syntax/token.hpp"
#include "support/test_support.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::backend {
[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& out) noexcept;
[[nodiscard]] std::string decode_string_literal(const std::string& literal, bool has_c_prefix);
[[nodiscard]] std::uint64_t parse_byte_literal(const std::string& literal);
} // namespace aurex::backend

namespace aurex::test {
namespace {

using base::Diagnostic;
using base::DiagnosticSink;
using base::ErrorCode;
using base::Severity;
using ir::AbiCallConv;
using ir::BasicBlock;
using ir::BinaryOp;
using ir::BlockId;
using ir::CastKind;
using ir::Function;
using ir::FunctionId;
using ir::FunctionParam;
using ir::GlobalConstant;
using ir::GlobalConstantId;
using ir::Linkage;
using ir::Module;
using ir::PassPipelineOptions;
using ir::PhiInput;
using ir::RecordField;
using ir::RecordLayout;
using ir::TerminatorKind;
using ir::Value;
using ir::ValueId;
using ir::ValueKind;
using ir::add_block;
using ir::add_global_constant;
using ir::add_value;
using ir::invalid_block_id;
using ir::invalid_function_id;
using ir::invalid_global_constant_id;
using ir::invalid_value_id;
using ir::is_valid;
using sema::BuiltinType;
using sema::PointerMutability;
using sema::TypeHandle;
using syntax::Token;
using syntax::TokenKind;

struct FunctionBuilder {
    Module& module;
    Function& function;

    [[nodiscard]] ValueId add(Value value) {
        return add_value(module, std::move(value));
    }

    [[nodiscard]] BlockId block(std::string name) {
        return add_block(function, std::move(name));
    }
};

[[nodiscard]] TypeHandle builtin(Module& module, const BuiltinType type) {
    return module.types.builtin(type);
}

[[nodiscard]] TypeHandle ptr(Module& module, const PointerMutability mutability, const TypeHandle pointee) {
    return module.types.pointer(mutability, pointee);
}

[[nodiscard]] Value integer_value(const TypeHandle type, std::string text) {
    Value value;
    value.kind = ValueKind::integer_literal;
    value.type = type;
    value.text = std::move(text);
    return value;
}

[[nodiscard]] Value bool_value(Module& module, const bool value) {
    Value result;
    result.kind = ValueKind::bool_literal;
    result.type = builtin(module, BuiltinType::bool_);
    result.text = value ? "true" : "false";
    return result;
}

[[nodiscard]] Function make_function(
    Module&,
    std::string name,
    const TypeHandle return_type,
    const Linkage linkage = Linkage::internal,
    const AbiCallConv call_conv = AbiCallConv::aurex
) {
    Function function;
    function.name = name;
    function.symbol = "test_" + name;
    function.return_type = return_type;
    function.linkage = linkage;
    function.call_conv = call_conv;
    return function;
}

[[nodiscard]] Function make_return_function(Module& module, const std::string& name, const TypeHandle return_type, const Value return_value) {
    Function function = make_function(module, name, return_type);
    FunctionBuilder builder {module, function};
    const ValueId value = builder.add(return_value);
    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values.push_back(value);
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = value;
    return function;
}

[[nodiscard]] Module make_simple_module() {
    Module module;
    module.functions.push_back(make_return_function(
        module,
        "answer",
        builtin(module, BuiltinType::i32),
        integer_value(builtin(module, BuiltinType::i32), "42")
    ));
    return module;
}

void expect_error_contains(const base::Result<void>& result, const std::string_view text) {
    ASSERT_FALSE(result);
    expect_contains(result.error().message, text);
}

std::vector<TokenKind> token_kinds(const std::vector<Token>& tokens) {
    std::vector<TokenKind> kinds;
    kinds.reserve(tokens.size());
    for (const Token& token : tokens) {
        kinds.push_back(token.kind);
    }
    return kinds;
}

} // namespace

TEST(CoreUnit, BaseDiagnosticsSourcesAndResult) {
    EXPECT_EQ(base::abi::std_support_symbol("open"), "aurex_std_v0_open");
    EXPECT_EQ(base::abi::legacy_std_support_symbol("open"), "aurex_std_open");

    base::SourceRange forward {{7}, 3, 9};
    EXPECT_EQ(forward.length(), 6U);
    EXPECT_FALSE(forward.empty());

    base::SourceRange reversed {{7}, 9, 3};
    EXPECT_EQ(reversed.length(), 0U);
    EXPECT_FALSE(reversed.empty());

    base::SourceManager sources;
    const base::SourceId id = sources.add_source("unit.ax", "module unit;");
    EXPECT_EQ(sources.get(id).id().value, id.value);
    EXPECT_EQ(sources.get(id).path(), "unit.ax");
    EXPECT_EQ(sources.text(id), "module unit;");

    DiagnosticSink diagnostics;
    EXPECT_FALSE(diagnostics.has_error());
    diagnostics.push(Diagnostic {Severity::note, forward, "note"});
    diagnostics.push(Diagnostic {Severity::warning, forward, "warning"});
    EXPECT_FALSE(diagnostics.has_error());
    diagnostics.push(Diagnostic {Severity::error, forward, "error"});
    EXPECT_TRUE(diagnostics.has_error());
    diagnostics.push(Diagnostic {Severity::fatal, forward, "fatal"});
    ASSERT_EQ(diagnostics.diagnostics().size(), 4U);

    EXPECT_EQ(base::severity_name(Severity::note), "note");
    EXPECT_EQ(base::severity_name(Severity::warning), "warning");
    EXPECT_EQ(base::severity_name(Severity::error), "error");
    EXPECT_EQ(base::severity_name(Severity::fatal), "fatal");

    auto ok_int = base::Result<int>::ok(11);
    ASSERT_TRUE(ok_int);
    EXPECT_EQ(ok_int.value(), 11);
    auto failed = base::Result<int>::fail({ErrorCode::io_error, "missing"});
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code, ErrorCode::io_error);
    EXPECT_EQ(failed.error().message, "missing");

    auto ok_void = base::Result<void>::ok();
    EXPECT_TRUE(ok_void);
    auto failed_void = base::Result<void>::fail({ErrorCode::internal_error, "bad"});
    ASSERT_FALSE(failed_void);
    EXPECT_EQ(failed_void.error().code, ErrorCode::internal_error);
}

TEST_F(AurexIntegrationTest, CliArgumentDiagnosticsCoverParseBranches) {
    expect_contains(require_success(aurexc() + " -h").output, "usage:");

    expect_contains(require_failure(aurexc()).output, "usage:");
    expect_contains(require_failure(aurexc() + " --unknown").output, "usage:");
    expect_contains(require_failure(aurexc() + " -o").output, "usage:");
    expect_contains(require_failure(aurexc() + " -I").output, "usage:");
    expect_contains(require_failure(aurexc() + " --clang").output, "usage:");
    expect_contains(require_failure(aurexc() + " --clang-arg").output, "usage:");
    expect_contains(require_failure(aurexc() + " --opt-level").output, "usage:");
    expect_contains(require_failure(aurexc() + " --opt-level bad " + q(source_root() / "examples" / "hello.ax")).output, "invalid optimization level");
    expect_contains(require_failure(aurexc() + " --std-backend").output, "usage:");
    expect_contains(require_failure(aurexc() + " --std-backend bad " + q(source_root() / "examples" / "hello.ax")).output, "invalid std backend");

    const fs::path hello = source_root() / "examples" / "hello.ax";
    const fs::path out = test_bin_root() / "cli_args";
    require_success(
        aurexc() + " -I" + q(source_root() / "tests" / "imports") +
        " --no-stdlib --emit=object --clang-arg -fno-color-diagnostics -O2 " +
        q(hello) + " -o " + q(out)
    );
    EXPECT_GT(fs::file_size(out), 0U);
}

TEST(CoreUnit, ParserAndAstDumpCoverLowLevelSyntaxBranches) {
    constexpr std::string_view source =
        "module parser.dump;\n"
        "pub import c.host;\n"
        "extern c {\n"
        "  opaque struct Handle;\n"
        "  fn puts(s: *const u8) -> i32 @name(\"puts\");\n"
        "}\n"
        "export c fn exported(argc: i32, argv: *mut *mut u8) -> i32 @name(\"exported\") {\n"
        "  var i: i32 = 0;\n"
        "  while i < argc {\n"
        "    i = i + 1;\n"
        "    if i == 1 { continue; } else { break; }\n"
        "  }\n"
        "  let p: *mut i32 = ptr_from_addr(*mut i32, ptr_addr(argv));\n"
        "  let n: *const u8 = null;\n"
        "  let s: str = \"hello\";\n"
        "  let b: u8 = b'\\n';\n"
        "  let a: i32 = cast(i32, argc) + bit_cast(i32, argc) + align_of(*mut i32);\n"
        "  let q: *mut i32 = ptr_cast(*mut i32, p);\n"
        "  let idx: u8 = argv[0][0];\n"
        "  return a;\n"
        "}\n";

    DiagnosticSink diagnostics;
    lex::Lexer lexer({3}, source, diagnostics);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(tokens) << tokens.error().message;

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::string token_dump = syntax::dump_tokens(tokens.value());
    expect_contains_all(token_dump, {
        "kw_export",
        "kw_opaque",
        "kw_while",
        "kw_break",
        "kw_continue",
        "kw_null",
        "kw_ptr_cast",
        "kw_bit_cast",
        "kw_align_of",
        "kw_ptr_addr",
        "kw_ptr_from_addr",
        "byte_literal",
        "string_literal",
    });

    const std::string ast = syntax::dump_ast(parsed.value());
    expect_contains_all(ast, {
        "pub import c.host",
        "opaque_struct Handle extern_c",
        "fn exported export_c @name=exported",
        "stmt #",
        "while",
        "break",
        "continue",
        "expr #",
        "null_literal",
        "string_literal",
        "byte_literal",
        "index",
        "ptr_cast",
        "bit_cast",
        "align_of",
        "ptr_addr",
        "ptr_from_addr",
    });
}

TEST(CoreUnit, LexerCoversCommentsLiteralsOperatorsAndErrors) {
    DiagnosticSink diagnostics;
    constexpr std::string_view source =
        "// line comment\n"
        "module lex.unit; /* block comment */\n"
        "const hex: i32 = 0x2A;\n"
        "const bin: i32 = 0b1010;\n"
        "const dec: i32 = 1_000;\n"
        "const s: str = \"hi\\\\n\";\n"
        "const c: *const u8 = c\"hi\\n\";\n"
        "const b: u8 = b'\\n';\n"
        "fn ops(a: i32, b: i32) -> i32 { return (a / b) ^ (a << 1) >> 1; }\n";
    lex::Lexer lexer({1}, source, diagnostics);
    auto result = lexer.tokenize();
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(diagnostics.has_error());

    const std::vector<TokenKind> kinds = token_kinds(result.value());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::integer_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::string_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::c_string_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::byte_literal), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::slash), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::caret), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::less_less), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), TokenKind::greater_greater), kinds.end());
    EXPECT_EQ(kinds.back(), TokenKind::eof);

    DiagnosticSink invalid_diagnostics;
    lex::Lexer invalid({2}, "@#$ \"unterminated\n c\"unterminated\n b'wide' /* unterminated", invalid_diagnostics);
    auto invalid_result = invalid.tokenize();
    ASSERT_FALSE(invalid_result);
    EXPECT_EQ(invalid_result.error().code, ErrorCode::lex_error);
    ASSERT_TRUE(invalid_diagnostics.has_error());
    EXPECT_GE(invalid_diagnostics.diagnostics().size(), 4U);
}

TEST(CoreUnit, TypeTableAndIrHelpersCoverInvalidAndCompositePaths) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle ptr_i32_again = ptr(module, PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
    const TypeHandle array_i32 = module.types.array(4, i32);
    const TypeHandle array_i32_again = module.types.array(4, i32);
    const TypeHandle record_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
    const TypeHandle enum_type = module.types.named_enum("unit.Tag", "unit_Tag");
    const TypeHandle opaque = module.types.opaque_struct("unit.Opaque", "unit_Opaque");

    EXPECT_TRUE(module.types.same(ptr_i32, ptr_i32_again));
    EXPECT_FALSE(module.types.same(ptr_i32, const_ptr_i32));
    EXPECT_TRUE(module.types.same(array_i32, array_i32_again));
    EXPECT_TRUE(module.types.is_integer(i32));
    EXPECT_TRUE(module.types.is_integer(u32));
    EXPECT_TRUE(module.types.is_float(f64));
    EXPECT_TRUE(module.types.is_pointer(ptr_i32));
    EXPECT_TRUE(module.types.is_array(array_i32));
    EXPECT_TRUE(module.types.contains_array(array_i32));
    EXPECT_FALSE(module.types.is_copyable(array_i32));
    EXPECT_EQ(module.types.display_name(ptr_i32), "*mut i32");
    EXPECT_EQ(module.types.display_name(array_i32), "[4]i32");
    EXPECT_EQ(module.types.display_name(sema::invalid_type_handle), "<invalid>");
    EXPECT_EQ(module.types.c_name(sema::invalid_type_handle), "void");

    module.types.set_record_properties(record_type, true, false);
    module.types.set_enum_underlying(enum_type, u32);
    module.types.set_enum_payload_layout(enum_type, record_type, 8, 4);
    EXPECT_TRUE(module.types.contains_array(record_type));
    EXPECT_FALSE(module.types.is_copyable(record_type));
    EXPECT_EQ(module.types.get(enum_type).enum_payload_size, 8U);
    EXPECT_FALSE(module.types.is_copyable(opaque));

    RecordLayout record;
    record.type = record_type;
    record.name = "unit.Pair";
    record.symbol = "unit_Pair";
    record.fields = {RecordField {"left", i32}, RecordField {"right", ptr_i32}};
    module.records.push_back(record);

    EXPECT_NE(ir::find_record(module, record_type), nullptr);
    EXPECT_EQ(ir::find_record(module, sema::invalid_type_handle), nullptr);
    EXPECT_NE(ir::find_record_field(module, record_type, "right"), nullptr);
    EXPECT_EQ(ir::find_record_field(module, record_type, "missing"), nullptr);
    EXPECT_EQ(ir::record_field_index(record, "left"), 0U);
    EXPECT_EQ(ir::record_field_index(record, "missing"), static_cast<base::usize>(-1));

    const ValueId literal = add_value(module, integer_value(i32, "7"));
    const GlobalConstantId constant = add_global_constant(module, GlobalConstant {"seven", "unit_seven", i32, literal});
    EXPECT_TRUE(is_valid(literal));
    EXPECT_TRUE(is_valid(constant));
    EXPECT_NE(ir::find_global_constant(module, constant), nullptr);
    EXPECT_EQ(ir::find_global_constant(module, invalid_global_constant_id), nullptr);

    Function function = make_function(module, "helper", i32);
    const BlockId entry = add_block(function, "entry");
    EXPECT_TRUE(is_valid(entry));
    EXPECT_FALSE(is_valid(invalid_block_id));
}

TEST(CoreUnit, IrVerifierReportsRepresentativeStructuralErrors) {
    {
        Module module = make_simple_module();
        EXPECT_TRUE(ir::verify_module(module));
    }
    {
        Module module = make_simple_module();
        module.functions[0].symbol.clear();
        expect_error_contains(ir::verify_module(module), "empty ABI symbol");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        module.functions.push_back(make_function(module, "dup_a", i32));
        module.functions.push_back(make_function(module, "dup_b", i32));
        module.functions[1].symbol = module.functions[0].symbol;
        expect_error_contains(ir::verify_module(module), "duplicate non-extern function ABI symbol");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        module.functions.push_back(make_function(module, "extern_a", i32, Linkage::extern_c, AbiCallConv::c));
        module.functions.push_back(make_function(module, "extern_b", i32, Linkage::extern_c, AbiCallConv::c));
        module.functions[1].symbol = module.functions[0].symbol;
        module.functions[1].signature_params.push_back(FunctionParam {"value", i32});
        expect_error_contains(ir::verify_module(module), "inconsistent declarations");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_extern", i32, Linkage::extern_c, AbiCallConv::aurex);
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "must use C ABI");
    }
    {
        Module module;
        Function function = make_function(module, "bad_entry", builtin(module, BuiltinType::i64));
        function.is_entry = true;
        function.linkage = Linkage::export_c;
        module.functions.push_back(function);
        const auto result = ir::verify_module(module);
        ASSERT_FALSE(result);
        expect_contains_all(result.error().message, {
            "must use internal linkage",
            "must return i32 or void",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_params", i32);
        const ValueId param = add_value(module, integer_value(i32, "1"));
        function.signature_params.push_back(FunctionParam {"x", i32});
        function.param_values.push_back(param);
        const BlockId entry = add_block(function, "entry");
        function.blocks[entry.value].values.push_back(param);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = param;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "non-param value");
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.kind = TerminatorKind::none;
        expect_error_contains(ir::verify_module(module), "has no terminator");
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.value = invalid_value_id;
        expect_error_contains(ir::verify_module(module), "return value value id is invalid");
    }
    {
        Module module;
        Function function = make_function(module, "void_value", builtin(module, BuiltinType::void_));
        FunctionBuilder builder {module, function};
        const ValueId value = builder.add(integer_value(builtin(module, BuiltinType::i32), "1"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(value);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = value;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "returns a value");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_branch", i32);
        FunctionBuilder builder {module, function};
        const ValueId value = builder.add(integer_value(i32, "1"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(value);
        function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
        function.blocks[entry.value].terminator.target = BlockId {42};
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "branch target block id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_cond", i32);
        FunctionBuilder builder {module, function};
        const ValueId value = builder.add(integer_value(i32, "1"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(value);
        function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
        function.blocks[entry.value].terminator.condition = value;
        function.blocks[entry.value].terminator.then_target = entry;
        function.blocks[entry.value].terminator.else_target = entry;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "branch condition type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "empty_phi", i32);
        FunctionBuilder builder {module, function};
        Value phi;
        phi.kind = ValueKind::phi;
        phi.type = i32;
        const ValueId phi_id = builder.add(phi);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(phi_id);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = phi_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "phi has no incoming values");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function target = make_function(module, "target", i32);
        module.functions.push_back(target);
        Function caller = make_function(module, "caller", i32);
        FunctionBuilder builder {module, caller};
        Value call;
        call.kind = ValueKind::call;
        call.type = i32;
        call.call_target = FunctionId {0};
        call.args.push_back(builder.add(integer_value(i32, "1")));
        const ValueId call_id = builder.add(call);
        const BlockId entry = builder.block("entry");
        caller.blocks[entry.value].values.push_back(call.args[0]);
        caller.blocks[entry.value].values.push_back(call_id);
        caller.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        caller.blocks[entry.value].terminator.value = call_id;
        module.functions.push_back(caller);
        expect_error_contains(ir::verify_module(module), "wrong argument count");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_load", i32);
        FunctionBuilder builder {module, function};
        const ValueId not_pointer = builder.add(integer_value(i32, "1"));
        Value load;
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = not_pointer;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {not_pointer, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "load object is not a pointer");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "bad_field", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::alloca;
        object.type = ptr_i32;
        const ValueId object_id = builder.add(object);
        Value field;
        field.kind = ValueKind::field_addr;
        field.type = ptr_i32;
        field.object = object_id;
        field.name = "missing";
        const ValueId field_id = builder.add(field);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, field_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "unknown field");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId first = add_value(module, Value {ValueKind::constant_ref, i32, "", "", invalid_function_id, invalid_value_id, invalid_value_id, invalid_value_id, invalid_value_id, {}, {}, {}, GlobalConstantId {1}});
        const ValueId second = add_value(module, Value {ValueKind::constant_ref, i32, "", "", invalid_function_id, invalid_value_id, invalid_value_id, invalid_value_id, invalid_value_id, {}, {}, {}, GlobalConstantId {0}});
        [[maybe_unused]] const GlobalConstantId first_constant =
            add_global_constant(module, GlobalConstant {"a", "unit_a", i32, first});
        [[maybe_unused]] const GlobalConstantId second_constant =
            add_global_constant(module, GlobalConstant {"b", "unit_b", i32, second});
        expect_error_contains(ir::verify_module(module), "cyclic constant reference");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId invalid_ref = add_value(module, Value {ValueKind::constant_ref, i32, "", "", invalid_function_id, invalid_value_id, invalid_value_id, invalid_value_id, invalid_value_id, {}, {}, {}, GlobalConstantId {42}});
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"bad", "unit_bad", i32, invalid_ref});
        expect_error_contains(ir::verify_module(module), "constant reference id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const ValueId value = add_value(module, integer_value(i32, "1"));
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"mismatch", "unit_mismatch", bool_type, value});
        expect_error_contains(ir::verify_module(module), "constant initializer type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}, RecordField {"y", i32}},
        });
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {
            {"x", add_value(module, integer_value(i32, "1"))},
            {"x", add_value(module, integer_value(i32, "2"))},
            {"missing", add_value(module, integer_value(i32, "3"))},
        };
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"bad_record", "unit_bad_record", record_type, aggregate_id});
        const auto result = ir::verify_module(module);
        ASSERT_FALSE(result);
        expect_contains_all(result.error().message, {
            "duplicate aggregate field x",
            "unknown aggregate field missing",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}, RecordField {"y", i32}},
        });
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {{"x", add_value(module, integer_value(i32, "1"))}};
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"incomplete", "unit_incomplete", record_type, aggregate_id});
        expect_error_contains(ir::verify_module(module), "aggregate constant does not initialize every field");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "extern_with_body", i32, Linkage::extern_c, AbiCallConv::c);
        FunctionBuilder builder {module, function};
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(result);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "must not have blocks");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "param_mismatch", i32);
        FunctionBuilder builder {module, function};
        Value param;
        param.kind = ValueKind::param;
        param.type = builtin(module, BuiltinType::bool_);
        const ValueId param_id = builder.add(param);
        function.signature_params.push_back(FunctionParam {"x", i32});
        function.param_values.push_back(param_id);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {param_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "parameter type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function target = make_function(module, "target", i32);
        target.signature_params.push_back(FunctionParam {"x", i32});
        module.functions.push_back(target);
        Function caller = make_function(module, "caller_bad_arg", i32);
        FunctionBuilder builder {module, caller};
        const ValueId arg = builder.add(bool_value(module, true));
        Value call;
        call.kind = ValueKind::call;
        call.type = builtin(module, BuiltinType::bool_);
        call.call_target = FunctionId {0};
        call.args.push_back(arg);
        const ValueId call_id = builder.add(call);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        caller.blocks[entry.value].values = {arg, call_id, result};
        caller.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        caller.blocks[entry.value].terminator.value = result;
        module.functions.push_back(caller);
        const auto verify = ir::verify_module(module);
        ASSERT_FALSE(verify);
        expect_contains_all(verify.error().message, {
            "call argument type mismatch",
            "call to @test_target result type mismatch",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}, RecordField {"y", i32}},
        });
        Function function = make_function(module, "bad_aggregate", i32);
        FunctionBuilder builder {module, function};
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {
            {"x", builder.add(integer_value(i32, "1"))},
            {"x", builder.add(integer_value(i32, "2"))},
        };
        const ValueId aggregate_id = builder.add(aggregate);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            aggregate.fields[0].value,
            aggregate.fields[1].value,
            aggregate_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        const auto verify = ir::verify_module(module);
        ASSERT_FALSE(verify);
        expect_contains_all(verify.error().message, {
            "duplicate aggregate field x",
            "aggregate does not initialize every field",
        });
    }
}

TEST(CoreUnit, PassPipelineOptimizesAndReportsVerificationFailures) {
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "local_slot", i32);
        FunctionBuilder builder {module, function};
        Value slot;
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId one = builder.add(integer_value(i32, "1"));
        Value store;
        store.kind = ValueKind::store;
        store.type = builtin(module, BuiltinType::void_);
        store.object = slot_id;
        store.lhs = one;
        const ValueId store_id = builder.add(store);
        Value load;
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = slot_id;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, one, store_id, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        module.functions.push_back(function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        const BasicBlock& block = module.functions[0].blocks[0];
        EXPECT_EQ(block.values.size(), 1U);
        EXPECT_EQ(block.values[0].value, one.value);
        EXPECT_EQ(block.terminator.value.value, one.value);
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::none), "O0");
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::basic), "O1");
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::standard), "O2");
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::aggressive), "O3");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "same_target", i32);
        FunctionBuilder builder {module, function};
        const ValueId condition = builder.add(bool_value(module, true));
        const ValueId result = builder.add(integer_value(i32, "3"));
        const BlockId entry = builder.block("entry");
        const BlockId exit = builder.block("exit");
        function.blocks[entry.value].values = {condition};
        function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
        function.blocks[entry.value].terminator.condition = condition;
        function.blocks[entry.value].terminator.then_target = exit;
        function.blocks[entry.value].terminator.else_target = exit;
        function.blocks[exit.value].values = {result};
        function.blocks[exit.value].terminator.kind = TerminatorKind::return_;
        function.blocks[exit.value].terminator.value = result;
        module.functions.push_back(function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].terminator.kind, TerminatorKind::branch);
        EXPECT_EQ(module.functions[0].blocks[0].terminator.target.value, exit.value);
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.value = invalid_value_id;
        PassPipelineOptions options;
        expect_error_contains(ir::run_pass_pipeline(module, options), "return value value id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        const TypeHandle ptr_record = ptr(module, PointerMutability::mut, record_type);
        module.types.set_record_properties(record_type, false, true);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}},
        });
        Function function = make_function(module, "escape_uses", i32);
        FunctionBuilder builder {module, function};
        Value slot;
        slot.kind = ValueKind::alloca;
        slot.type = ptr_record;
        const ValueId slot_id = builder.add(slot);
        Value field;
        field.kind = ValueKind::field_addr;
        field.type = ptr(module, PointerMutability::mut, i32);
        field.object = slot_id;
        field.name = "x";
        const ValueId field_id = builder.add(field);
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields.push_back({"x", builder.add(integer_value(i32, "1"))});
        const ValueId aggregate_id = builder.add(aggregate);
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = ptr_record;
        cast.target_type = ptr_record;
        cast.cast_kind = CastKind::pointer;
        cast.lhs = slot_id;
        const ValueId cast_id = builder.add(cast);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            slot_id,
            field_id,
            aggregate.fields[0].value,
            aggregate_id,
            cast_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].values.size(), 6U);
    }
    {
        Module module;
        Function function = make_function(module, "empty", builtin(module, BuiltinType::i32), Linkage::extern_c, AbiCallConv::c);
        module.functions.push_back(function);
        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    }
}

TEST(CoreUnit, LlvmBackendCoversConstantsCastsStringsAndNullModule) {
    {
        auto result = backend::emit_llvm_ir({nullptr, "null"});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::internal_error);
        expect_contains(result.error().message, "null IR module");
    }
    {
        Module module;
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const TypeHandle u8 = builtin(module, BuiltinType::u8);
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle u32 = builtin(module, BuiltinType::u32);
        const TypeHandle i64 = builtin(module, BuiltinType::i64);
        const TypeHandle f64 = builtin(module, BuiltinType::f64);
        const TypeHandle str_type = builtin(module, BuiltinType::str);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        const TypeHandle array_i32 = module.types.array(2, i32);
        const TypeHandle pair_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
        module.records.push_back(RecordLayout {
            pair_type,
            "unit.Pair",
            "unit_Pair",
            false,
            {RecordField {"left", i32}, RecordField {"right", i32}},
        });

        const ValueId true_id = add_value(module, bool_value(module, true));
        [[maybe_unused]] const GlobalConstantId flag_constant =
            add_global_constant(module, GlobalConstant {"flag", "unit_flag", bool_type, true_id});
        Value byte;
        byte.kind = ValueKind::byte_literal;
        byte.type = u8;
        byte.text = "b'\\n'";
        const ValueId byte_id = add_value(module, byte);
        [[maybe_unused]] const GlobalConstantId byte_constant =
            add_global_constant(module, GlobalConstant {"byte", "unit_byte", u8, byte_id});
        Value null_value;
        null_value.kind = ValueKind::null_literal;
        null_value.type = ptr_i32;
        const ValueId null_id = add_value(module, null_value);
        [[maybe_unused]] const GlobalConstantId ptr_constant =
            add_global_constant(module, GlobalConstant {"ptr", "unit_ptr", ptr_i32, null_id});
        Value cstr;
        cstr.kind = ValueKind::c_string_literal;
        cstr.type = ptr(module, PointerMutability::const_, u8);
        cstr.text = "c\"abc\"";
        const ValueId cstr_id = add_value(module, cstr);
        [[maybe_unused]] const GlobalConstantId cstr_constant =
            add_global_constant(module, GlobalConstant {"cstr", "unit_cstr", cstr.type, cstr_id});
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = i64;
        cast.target_type = i64;
        cast.cast_kind = CastKind::numeric;
        cast.lhs = add_value(module, integer_value(i32, "9"));
        const ValueId cast_id = add_value(module, cast);
        [[maybe_unused]] const GlobalConstantId wide_constant =
            add_global_constant(module, GlobalConstant {"wide", "unit_wide", i64, cast_id});
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = pair_type;
        aggregate.fields = {
            {"left", add_value(module, integer_value(i32, "1"))},
            {"right", add_value(module, integer_value(i32, "2"))},
        };
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId pair_constant =
            add_global_constant(module, GlobalConstant {"pair", "unit_pair", pair_type, aggregate_id});
        Value sizeof_value;
        sizeof_value.kind = ValueKind::size_of;
        sizeof_value.type = builtin(module, BuiltinType::usize);
        sizeof_value.target_type = array_i32;
        const ValueId size_id = add_value(module, sizeof_value);
        [[maybe_unused]] const GlobalConstantId size_constant =
            add_global_constant(module, GlobalConstant {"size", "unit_size", sizeof_value.type, size_id});
        Value align_value = sizeof_value;
        align_value.kind = ValueKind::align_of;
        align_value.target_type = pair_type;
        const ValueId align_id = add_value(module, align_value);
        [[maybe_unused]] const GlobalConstantId align_constant =
            add_global_constant(module, GlobalConstant {"align", "unit_align", align_value.type, align_id});

        Function external = make_function(module, "external", i32, Linkage::extern_c, AbiCallConv::c);
        external.symbol = "unit_external";
        module.functions.push_back(external);

        Function function = make_function(module, "exercise", i32);
        FunctionBuilder builder {module, function};
        Value lhs_param;
        lhs_param.kind = ValueKind::param;
        lhs_param.type = i32;
        lhs_param.name = "lhs";
        const ValueId lhs = builder.add(lhs_param);
        Value rhs_param = lhs_param;
        rhs_param.name = "rhs";
        const ValueId rhs = builder.add(rhs_param);
        Value u_lhs_param;
        u_lhs_param.kind = ValueKind::param;
        u_lhs_param.type = u32;
        u_lhs_param.name = "u_lhs";
        const ValueId u_lhs = builder.add(u_lhs_param);
        Value u_rhs_param = u_lhs_param;
        u_rhs_param.name = "u_rhs";
        const ValueId u_rhs = builder.add(u_rhs_param);
        function.signature_params = {
            FunctionParam {"lhs", i32},
            FunctionParam {"rhs", i32},
            FunctionParam {"u_lhs", u32},
            FunctionParam {"u_rhs", u32},
        };
        function.param_values = {lhs, rhs, u_lhs, u_rhs};

        std::vector<ValueId> values;
        for (const BinaryOp op : {
                 BinaryOp::div,
                 BinaryOp::shl,
                 BinaryOp::shr,
                 BinaryOp::bit_xor,
                 BinaryOp::bit_or,
             }) {
            Value binary;
            binary.kind = ValueKind::binary;
            binary.type = i32;
            binary.binary_op = op;
            binary.lhs = lhs;
            binary.rhs = rhs;
            values.push_back(builder.add(binary));
        }
        for (const BinaryOp op : {BinaryOp::mod, BinaryOp::shr}) {
            Value binary;
            binary.kind = ValueKind::binary;
            binary.type = u32;
            binary.binary_op = op;
            binary.lhs = u_lhs;
            binary.rhs = u_rhs;
            values.push_back(builder.add(binary));
        }
        Value unsigned_less;
        unsigned_less.kind = ValueKind::binary;
        unsigned_less.type = bool_type;
        unsigned_less.binary_op = BinaryOp::less;
        unsigned_less.lhs = u_lhs;
        unsigned_less.rhs = u_rhs;
        values.push_back(builder.add(unsigned_less));
        Value float_cast;
        float_cast.kind = ValueKind::cast;
        float_cast.type = f64;
        float_cast.target_type = f64;
        float_cast.cast_kind = CastKind::numeric;
        float_cast.lhs = lhs;
        values.push_back(builder.add(float_cast));
        Value runtime_string;
        runtime_string.kind = ValueKind::string_literal;
        runtime_string.type = str_type;
        runtime_string.text = "\"hi\"";
        values.push_back(builder.add(runtime_string));
        Value local_array;
        local_array.kind = ValueKind::alloca;
        local_array.type = ptr(module, PointerMutability::mut, array_i32);
        const ValueId local_array_id = builder.add(local_array);
        values.push_back(local_array_id);
        Value index;
        index.kind = ValueKind::index_addr;
        index.type = ptr_i32;
        index.object = local_array_id;
        index.index = rhs;
        values.push_back(builder.add(index));
        Value call_name;
        call_name.kind = ValueKind::call;
        call_name.type = i32;
        call_name.name = "named_external";
        call_name.call_target = FunctionId {0};
        values.push_back(builder.add(call_name));
        const ValueId result = builder.add(integer_value(i32, "0"));
        values.push_back(result);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = values;
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);

        auto llvm_ir = backend::emit_llvm_ir({&module, "unit_backend"});
        ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
        expect_contains_all(llvm_ir.value().text, {
            "@unit_flag",
            "@unit_byte",
            "@unit_ptr",
            "@unit_pair",
            "@unit_size",
            "@unit_align",
            "%unit_Pair = type",
            "xor",
            "shl",
            "lshr",
            "urem",
            "getelementptr",
            "str.data",
        });
    }
}

TEST(CoreUnit, LlvmBackendUtilityHelpersCoverLiteralVariants) {
    std::uint64_t parsed = 0;
    EXPECT_TRUE(backend::parse_u64("0x2a", parsed));
    EXPECT_EQ(parsed, 42U);
    EXPECT_TRUE(backend::parse_u64("0b1010", parsed));
    EXPECT_EQ(parsed, 10U);
    EXPECT_FALSE(backend::parse_u64("not-a-number", parsed));

    const std::string decoded = backend::decode_string_literal("\"\\0\\r\\t\\\\\\\"x\"", false);
    ASSERT_EQ(decoded.size(), 6U);
    EXPECT_EQ(decoded[0], '\0');
    EXPECT_EQ(decoded[1], '\r');
    EXPECT_EQ(decoded[2], '\t');
    EXPECT_EQ(decoded[3], '\\');
    EXPECT_EQ(decoded[4], '"');
    EXPECT_EQ(decoded[5], 'x');
    EXPECT_EQ(backend::decode_string_literal("c\"raw\"", true), "raw");
    EXPECT_EQ(backend::decode_string_literal("\\q", false), "q");

    EXPECT_EQ(backend::parse_byte_literal("b'\\\\'"), static_cast<std::uint64_t>('\\'));
    EXPECT_EQ(backend::parse_byte_literal("b'\\''"), static_cast<std::uint64_t>('\''));
    EXPECT_EQ(backend::parse_byte_literal("b'\\x'"), static_cast<std::uint64_t>('x'));
    EXPECT_EQ(backend::parse_byte_literal("b''"), 0U);
}

TEST(CoreUnit, NativeToolchainRejectsSupportSourcesForNonExecutableAndReportsMissingClang) {
    driver::NativeCompileRequest unsupported;
    unsupported.emit_kind = driver::EmitKind::object;
    unsupported.input_path = source_root() / "examples" / "hello.ax";
    unsupported.output_path = tmp_root() / "hello.o";
    unsupported.support_source_paths.push_back(source_root() / "std" / "ffi" / "c" / "support" / "host_c.c");
    auto support_result = driver::invoke_clang(unsupported);
    ASSERT_FALSE(support_result);
    EXPECT_EQ(support_result.error().code, ErrorCode::codegen_error);
    expect_contains(support_result.error().message, "support sources are only supported");

    driver::NativeCompileRequest missing;
    missing.clang_path = "/definitely/not/a/real/clang";
    missing.input_path = source_root() / "examples" / "hello.ax";
    missing.output_path = tmp_root() / "missing" / "hello";
    auto missing_result = driver::invoke_clang(missing);
    ASSERT_FALSE(missing_result);
    EXPECT_EQ(missing_result.error().code, ErrorCode::codegen_error);
    expect_contains(missing_result.error().message, "exit code 127");
    EXPECT_TRUE(fs::exists(tmp_root() / "missing"));
}

} // namespace aurex::test

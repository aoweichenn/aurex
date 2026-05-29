#include <aurex/query/query_key.hpp>
#include <aurex/tooling/lsp.hpp>
#include <aurex/tooling/session.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr std::string_view TOOLING_SESSION_SOURCE = "module tooling.session;\n"
                                                    "fn add(a: i32, b: i32) -> i32 {\n"
                                                    "  return a + b;\n"
                                                    "}\n"
                                                    "fn main() -> i32 {\n"
                                                    "  let value = add(1, 2);\n"
                                                    "  return value;\n"
                                                    "}\n";

constexpr std::string_view TOOLING_SESSION_INVALID_SOURCE = "module tooling.session;\n"
                                                            "fn main() -> i32 {\n"
                                                            "  let value: i32 = true;\n"
                                                            "  return value;\n"
                                                            "}\n";

constexpr std::string_view TOOLING_SESSION_NO_MODULE_SOURCE = "module ;\n"
                                                             "fn main() -> i32 {\n"
                                                             "  return 0;\n"
                                                             "}\n";

constexpr std::string_view TOOLING_SESSION_SUGGESTION_SOURCE = "module tooling.suggestion;\n"
                                                               "fn main() -> i32 {\n"
                                                               "  let count: i32 = 1;\n"
                                                               "  return coutn;\n"
                                                               "}\n";

constexpr std::string_view TOOLING_SESSION_FALLBACK_SYMBOL_SOURCE = "module tooling.fallback;\n"
                                                                    "const answer: i32 = 1;\n"
                                                                    "type Count = i32;\n"
                                                                    "opaque struct Handle;\n"
                                                                    "struct Point { x: i32; }\n"
                                                                    "enum Mode { fast }\n"
                                                                    "fn main() -> i32 {\n"
                                                                    "  let value: i32 = true;\n"
                                                                    "  return value;\n"
                                                                    "}\n";

constexpr std::string_view TOOLING_LSP_SYMBOL_SOURCE = "module tooling.symbols;\n"
                                                       "type Count = i32;\n"
                                                       "enum Mode { fast }\n"
                                                       "struct Box[T] { value: T; }\n"
                                                       "fn main() -> i32 { return 0; }\n";

constexpr std::string_view TOOLING_LSP_METHOD_SYMBOL_SOURCE = "module tooling.methods;\n"
                                                              "struct Counter { value: i32; }\n"
                                                              "impl Counter {\n"
                                                              "  fn read(self: &Counter) -> i32 {\n"
                                                              "    return self.value;\n"
                                                              "  }\n"
                                                              "}\n"
                                                              "fn main() -> i32 {\n"
                                                              "  let counter = Counter { value: 1 };\n"
                                                              "  return counter.read();\n"
                                                              "}\n";

constexpr std::string_view TOOLING_SESSION_URI = "file:///workspace/tooling_session.ax";
constexpr std::string_view TOOLING_LSP_URI = "file:///workspace/lsp.ax";
constexpr std::string_view TOOLING_LSP_SYMBOL_URI = "file:///workspace/lsp_symbols.ax";
constexpr std::string_view TOOLING_LSP_FALLBACK_SYMBOL_URI = "file:///workspace/lsp_fallback_symbols.ax";
constexpr std::string_view TOOLING_LSP_METHOD_SYMBOL_URI = "file:///workspace/lsp_method_symbols.ax";
constexpr std::string_view TOOLING_LSP_ESCAPE_URI = "file:///workspace/lsp_escape.ax";
constexpr std::string_view TOOLING_LSP_NO_MODULE_URI = "file:///workspace/lsp_no_module.ax";
constexpr std::string_view TOOLING_SESSION_PACKAGE = "tooling-session-test";
constexpr std::string_view TOOLING_LSP_PACKAGE = "tooling-lsp-test";
constexpr base::i64 TOOLING_VERSION_ONE = 1;
constexpr base::i64 TOOLING_VERSION_TWO = 2;
constexpr base::i64 TOOLING_VERSION_THREE = 3;
constexpr int TOOLING_LSP_ID_INITIALIZE = 1;
constexpr int TOOLING_LSP_ID_HOVER = 2;
constexpr int TOOLING_LSP_ID_DEFINITION = 3;
constexpr int TOOLING_LSP_ID_REFERENCES = 4;
constexpr int TOOLING_LSP_ID_SYMBOLS = 5;
constexpr int TOOLING_LSP_ID_SHUTDOWN = 6;
constexpr int TOOLING_LSP_ID_NO_PARAMS = 7;
constexpr int TOOLING_LSP_ID_INVALID_JSON = 8;
constexpr int TOOLING_LSP_ID_UNKNOWN = 99;
constexpr base::usize TOOLING_TEST_PAST_EOF_OFFSET_DELTA = 10;
constexpr unsigned char TOOLING_TEST_PERCENT_DECODE_BYTE = 0xAFU;
constexpr unsigned char TOOLING_TEST_JSON_CONTROL_LIMIT = 0x20U;
constexpr base::usize TOOLING_TEST_JSON_HEX_NIBBLE_SHIFT = 4;
constexpr unsigned char TOOLING_TEST_JSON_HEX_NIBBLE_MASK = 0x0FU;
constexpr std::string_view TOOLING_TEST_JSON_HEX_DIGITS = "0123456789ABCDEF";

[[nodiscard]] tooling::ToolingProjectConfig tooling_project_config(const std::string_view package)
{
    tooling::ToolingProjectConfig config;
    config.root_path = "/workspace";
    config.package_identity = std::string(package);
    return config;
}

[[nodiscard]] std::string test_json_string(const std::string_view text)
{
    std::string result;
    result.push_back('"');
    for (const unsigned char raw_ch : text) {
        const char ch = static_cast<char>(raw_ch);
        switch (ch) {
            case '"':
                result.append("\\\"");
                break;
            case '\\':
                result.append("\\\\");
                break;
            case '\b':
                result.append("\\b");
                break;
            case '\f':
                result.append("\\f");
                break;
            case '\n':
                result.append("\\n");
                break;
            case '\r':
                result.append("\\r");
                break;
            case '\t':
                result.append("\\t");
                break;
            default:
                if (raw_ch < TOOLING_TEST_JSON_CONTROL_LIMIT) {
                    result.append("\\u00");
                    result.push_back(TOOLING_TEST_JSON_HEX_DIGITS[
                        (raw_ch >> TOOLING_TEST_JSON_HEX_NIBBLE_SHIFT) & TOOLING_TEST_JSON_HEX_NIBBLE_MASK]);
                    result.push_back(TOOLING_TEST_JSON_HEX_DIGITS[raw_ch & TOOLING_TEST_JSON_HEX_NIBBLE_MASK]);
                } else {
                    result.push_back(ch);
                }
                break;
        }
    }
    result.push_back('"');
    return result;
}

[[nodiscard]] std::string lsp_request_json(
    const int id, const std::string_view method, const std::string_view params)
{
    std::string request = "{\"jsonrpc\":\"2.0\",\"id\":";
    request += std::to_string(id);
    request += ",\"method\":";
    request += test_json_string(method);
    request += ",\"params\":";
    request += params;
    request += "}";
    return request;
}

[[nodiscard]] std::string lsp_notification_json(const std::string_view method, const std::string_view params)
{
    std::string notification = "{\"jsonrpc\":\"2.0\",\"method\":";
    notification += test_json_string(method);
    notification += ",\"params\":";
    notification += params;
    notification += "}";
    return notification;
}

[[nodiscard]] std::string text_document_params(const std::string_view uri)
{
    std::string params = "{\"textDocument\":{\"uri\":";
    params += test_json_string(uri);
    params += "}}";
    return params;
}

[[nodiscard]] std::string text_document_position_params(
    const std::string_view uri, const tooling::ToolingSourcePosition position)
{
    std::string params = "{\"textDocument\":{\"uri\":";
    params += test_json_string(uri);
    params += "},\"position\":{\"line\":";
    params += std::to_string(position.line);
    params += ",\"character\":";
    params += std::to_string(position.character);
    params += "}}";
    return params;
}

[[nodiscard]] std::string did_open_params(
    const std::string_view uri, const std::string_view text, const base::i64 version)
{
    std::string params = "{\"textDocument\":{\"uri\":";
    params += test_json_string(uri);
    params += ",\"languageId\":\"aurex\",\"version\":";
    params += std::to_string(version);
    params += ",\"text\":";
    params += test_json_string(text);
    params += "}}";
    return params;
}

[[nodiscard]] std::string did_change_params(
    const std::string_view uri, const std::string_view text, const base::i64 version)
{
    std::string params = "{\"textDocument\":{\"uri\":";
    params += test_json_string(uri);
    params += ",\"version\":";
    params += std::to_string(version);
    params += "},\"contentChanges\":[{\"text\":";
    params += test_json_string(text);
    params += "}]}";
    return params;
}

[[nodiscard]] bool contains_symbol_named(
    const std::vector<tooling::ToolingDocumentSymbol>& symbols, const std::string_view name)
{
    return std::ranges::any_of(symbols, [name](const tooling::ToolingDocumentSymbol& symbol) {
        return symbol.name == name;
    });
}

} // namespace

TEST(CoreUnit, ToolingSessionManagesVersionedDocumentsAndSnapshotCache)
{
    tooling::ToolingSession session(tooling_project_config(TOOLING_SESSION_PACKAGE));
    const tooling::ToolingDocumentId document =
        tooling::tooling_document_id_from_uri(TOOLING_SESSION_URI, session.project_config());

    const base::Result<tooling::ToolingDocumentVersion> opened =
        session.open_document(document, std::string(TOOLING_SESSION_SOURCE), TOOLING_VERSION_ONE);
    ASSERT_TRUE(opened);
    EXPECT_EQ(opened.value().client_version, std::optional<base::i64>(TOOLING_VERSION_ONE));
    EXPECT_TRUE(session.is_open(document));

    base::Result<tooling::ToolingSnapshotHandle> first_snapshot = session.snapshot(document);
    ASSERT_TRUE(first_snapshot);
    base::Result<tooling::ToolingSnapshotHandle> reused_snapshot = session.snapshot(document);
    ASSERT_TRUE(reused_snapshot);
    EXPECT_EQ(first_snapshot.value().snapshot.get(), reused_snapshot.value().snapshot.get());

    constexpr std::array<std::string_view, 1> expected_package{TOOLING_SESSION_PACKAGE};
    EXPECT_EQ(first_snapshot.value().snapshot->query.source_stage.file.package, query::package_key(expected_package));
    EXPECT_FALSE(first_snapshot.value().snapshot->has_errors);

    const base::Result<tooling::ToolingDocumentVersion> stale_change =
        session.change_document(document, std::string(TOOLING_SESSION_INVALID_SOURCE), TOOLING_VERSION_ONE);
    EXPECT_FALSE(stale_change);
    base::Result<tooling::ToolingSnapshotHandle> after_stale_snapshot = session.snapshot(document);
    ASSERT_TRUE(after_stale_snapshot);
    EXPECT_EQ(first_snapshot.value().snapshot.get(), after_stale_snapshot.value().snapshot.get());

    const base::Result<tooling::ToolingDocumentVersion> changed =
        session.change_document(document, std::string(TOOLING_SESSION_INVALID_SOURCE), TOOLING_VERSION_TWO);
    ASSERT_TRUE(changed);
    EXPECT_EQ(changed.value().client_version, std::optional<base::i64>(TOOLING_VERSION_TWO));
    base::Result<tooling::ToolingSnapshotHandle> changed_snapshot = session.snapshot(document);
    ASSERT_TRUE(changed_snapshot);
    EXPECT_NE(first_snapshot.value().snapshot.get(), changed_snapshot.value().snapshot.get());
    EXPECT_TRUE(changed_snapshot.value().snapshot->has_errors);

    base::Result<std::vector<tooling::ToolingDiagnostic>> diagnostics = session.diagnostics(document);
    ASSERT_TRUE(diagnostics);
    EXPECT_TRUE(std::ranges::any_of(diagnostics.value(), [](const tooling::ToolingDiagnostic& diagnostic) {
        return diagnostic.code == base::DiagnosticCode::semantic_type_mismatch && diagnostic.category_name == "type"
            && diagnostic.code_name == "SEM0100";
    }));

    EXPECT_TRUE(session.close_document(document));
    EXPECT_FALSE(session.is_open(document));
    EXPECT_FALSE(session.snapshot(document));
}

TEST(CoreUnit, ToolingSessionProjectsIdeFeaturesWithoutLspTypes)
{
    tooling::ToolingSession session(tooling_project_config(TOOLING_SESSION_PACKAGE));
    const tooling::ToolingDocumentId document =
        tooling::tooling_document_id_from_uri(TOOLING_SESSION_URI, session.project_config());
    ASSERT_TRUE(session.open_document(document, std::string(TOOLING_SESSION_SOURCE), TOOLING_VERSION_ONE));

    const base::usize call_offset = TOOLING_SESSION_SOURCE.find("add(1");
    ASSERT_NE(call_offset, std::string_view::npos);

    base::Result<std::optional<tooling::ToolingDefinition>> definition =
        session.definition_at_offset(document, call_offset);
    ASSERT_TRUE(definition);
    ASSERT_TRUE(definition.value().has_value());
    EXPECT_TRUE(definition.value()->valid);
    EXPECT_EQ(definition.value()->name, "add");
    EXPECT_EQ(definition.value()->kind, "function");
    EXPECT_NE(definition.value()->stable_definition_key.find("DefKey"), std::string::npos);

    base::Result<std::optional<tooling::ToolingHover>> hover = session.hover_at_offset(document, call_offset);
    ASSERT_TRUE(hover);
    ASSERT_TRUE(hover.value().has_value());
    EXPECT_TRUE(hover.value()->valid);
    EXPECT_NE(hover.value()->label.find("identifier `add` -> function"), std::string::npos);
    EXPECT_NE(hover.value()->semantic_fact_key.find("QueryKey"), std::string::npos);
    ASSERT_TRUE(hover.value()->definition.has_value());
    EXPECT_EQ(hover.value()->definition->name, "add");

    base::Result<std::vector<tooling::ToolingReference>> references =
        session.references_at_offset(document, call_offset);
    ASSERT_TRUE(references);
    EXPECT_GE(references.value().size(), 2U);
    EXPECT_TRUE(std::ranges::any_of(references.value(), [](const tooling::ToolingReference& reference) {
        return reference.is_definition;
    }));

    base::Result<std::vector<tooling::ToolingDocumentSymbol>> symbols = session.document_symbols(document);
    ASSERT_TRUE(symbols);
    EXPECT_TRUE(contains_symbol_named(symbols.value(), "add"));
    EXPECT_TRUE(contains_symbol_named(symbols.value(), "main"));
    EXPECT_TRUE(std::ranges::any_of(symbols.value(), [](const tooling::ToolingDocumentSymbol& symbol) {
        return symbol.name == "add" && symbol.kind == "function" && symbol.checked
            && symbol.stable_query_key.find("QueryKey") != std::string::npos;
    }));
}

TEST(CoreUnit, ToolingSessionCoversNormalizationFallbacksAndErrorPaths)
{
    tooling::ToolingProjectConfig config = tooling_project_config("");
    config.package_identity.clear();
    tooling::ToolingSession session(config);

    const std::optional<std::string> localhost_path =
        tooling::tooling_path_from_file_uri("file://localhost/workspace/space%20file.ax");
    ASSERT_TRUE(localhost_path.has_value());
    EXPECT_EQ(*localhost_path, "/workspace/space file.ax");
    EXPECT_FALSE(tooling::tooling_path_from_file_uri("untitled:buffer").has_value());
    EXPECT_NE(tooling::tooling_file_uri_from_path("/workspace/space file.ax").find("%20"), std::string::npos);
    EXPECT_EQ(tooling::tooling_offset_for_position("one\n", tooling::ToolingSourcePosition{4U, 0U}), 4U);
    EXPECT_EQ(tooling::tooling_position_for_offset("one\n", 40U).line, 1U);
    EXPECT_EQ(tooling::tooling_document_id_from_path("relative.ax", session.project_config()).path,
        "/workspace/relative.ax");
    EXPECT_EQ(tooling::tooling_document_id_from_path("/workspace/colon:name.ax", session.project_config()).uri,
        "file:///workspace/colon:name.ax");
    EXPECT_EQ(tooling::tooling_path_from_file_uri("file:///workspace/bad%zz.ax").value(), "/workspace/bad%zz.ax");
    std::string expected_percent_decoded = "/workspace/";
    expected_percent_decoded.push_back(static_cast<char>(TOOLING_TEST_PERCENT_DECODE_BYTE));
    expected_percent_decoded.append(".ax");
    EXPECT_EQ(tooling::tooling_path_from_file_uri("file:///workspace/%AF.ax").value(), expected_percent_decoded);
    EXPECT_EQ(tooling::tooling_path_from_file_uri("file:///workspace/%af.ax").value(), expected_percent_decoded);

    tooling::ToolingDocumentId partial_document;
    partial_document.uri = "file://localhost/workspace/fallback.ax";
    partial_document.package_identity.clear();
    partial_document.virtual_buffer_identity.clear();

    const base::Result<tooling::ToolingDocumentVersion> opened =
        session.open_document(partial_document, std::string(TOOLING_SESSION_SOURCE));
    ASSERT_TRUE(opened);
    EXPECT_FALSE(opened.value().client_version.has_value());
    const std::optional<tooling::ToolingDocumentState> state = session.document_state(partial_document);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->id.package_identity, "ide");
    EXPECT_EQ(state->id.virtual_buffer_identity, "file://localhost/workspace/fallback.ax");
    EXPECT_FALSE(session.open_document(partial_document, std::string(TOOLING_SESSION_SOURCE)));

    ASSERT_TRUE(session.change_document(
        partial_document, std::string(TOOLING_SESSION_FALLBACK_SYMBOL_SOURCE), TOOLING_VERSION_TWO));
    base::Result<std::vector<tooling::ToolingDocumentSymbol>> fallback_symbols =
        session.document_symbols(partial_document);
    ASSERT_TRUE(fallback_symbols);
    EXPECT_TRUE(contains_symbol_named(fallback_symbols.value(), "answer"));
    EXPECT_TRUE(contains_symbol_named(fallback_symbols.value(), "Count"));
    EXPECT_TRUE(contains_symbol_named(fallback_symbols.value(), "Handle"));
    EXPECT_TRUE(contains_symbol_named(fallback_symbols.value(), "Point"));
    EXPECT_TRUE(contains_symbol_named(fallback_symbols.value(), "Mode"));
    EXPECT_TRUE(contains_symbol_named(fallback_symbols.value(), "main"));
    EXPECT_TRUE(std::ranges::any_of(fallback_symbols.value(), [](const tooling::ToolingDocumentSymbol& symbol) {
        return symbol.name == "answer" && symbol.kind == "const" && !symbol.checked;
    }));

    tooling::ToolingDocumentId path_only_document;
    path_only_document.path = "/workspace/path_only.ax";
    ASSERT_TRUE(session.open_document(path_only_document, std::string(TOOLING_SESSION_SOURCE), TOOLING_VERSION_THREE));
    const std::optional<tooling::ToolingDocumentState> path_only_state = session.document_state(path_only_document);
    ASSERT_TRUE(path_only_state.has_value());
    EXPECT_EQ(path_only_state->id.uri, "file:///workspace/path_only.ax");
    EXPECT_EQ(path_only_state->id.virtual_buffer_identity, "file:///workspace/path_only.ax");

    tooling::ToolingSession empty_session(tooling_project_config(TOOLING_SESSION_PACKAGE));
    EXPECT_FALSE(empty_session.document_state(partial_document).has_value());
    EXPECT_FALSE(empty_session.change_document(partial_document, std::string(TOOLING_SESSION_SOURCE)));
    EXPECT_FALSE(empty_session.close_document(partial_document));
    EXPECT_FALSE(empty_session.diagnostics(partial_document));
    EXPECT_FALSE(empty_session.hover_at_offset(partial_document, 0U));
    EXPECT_FALSE(empty_session.hover_at_position(partial_document, tooling::ToolingSourcePosition{}));
    EXPECT_FALSE(empty_session.definition_at_offset(partial_document, 0U));
    EXPECT_FALSE(empty_session.definition_at_position(partial_document, tooling::ToolingSourcePosition{}));
    EXPECT_FALSE(empty_session.references_at_offset(partial_document, 0U));
    EXPECT_FALSE(empty_session.references_at_position(partial_document, tooling::ToolingSourcePosition{}));
    EXPECT_FALSE(empty_session.document_symbols(partial_document));
}

TEST(CoreUnit, ToolingSessionProjectsAbsentIdeFeaturesAndSuggestionDiagnostics)
{
    tooling::ToolingSession session(tooling_project_config(TOOLING_SESSION_PACKAGE));
    const tooling::ToolingDocumentId document =
        tooling::tooling_document_id_from_uri(TOOLING_SESSION_URI, session.project_config());
    ASSERT_TRUE(session.open_document(document, std::string(TOOLING_SESSION_SOURCE), TOOLING_VERSION_ONE));

    const base::usize past_eof_offset = TOOLING_SESSION_SOURCE.size() + TOOLING_TEST_PAST_EOF_OFFSET_DELTA;
    base::Result<std::optional<tooling::ToolingHover>> hover = session.hover_at_offset(document, past_eof_offset);
    ASSERT_TRUE(hover);
    EXPECT_FALSE(hover.value().has_value());

    base::Result<std::optional<tooling::ToolingDefinition>> definition =
        session.definition_at_offset(document, past_eof_offset);
    ASSERT_TRUE(definition);
    EXPECT_FALSE(definition.value().has_value());

    base::Result<std::vector<tooling::ToolingReference>> references =
        session.references_at_offset(document, past_eof_offset);
    ASSERT_TRUE(references);
    EXPECT_TRUE(references.value().empty());

    ASSERT_TRUE(session.change_document(document, std::string(TOOLING_SESSION_SUGGESTION_SOURCE), TOOLING_VERSION_TWO));
    base::Result<std::vector<tooling::ToolingDiagnostic>> diagnostics = session.diagnostics(document);
    ASSERT_TRUE(diagnostics);
    EXPECT_TRUE(std::ranges::any_of(diagnostics.value(), [](const tooling::ToolingDiagnostic& diagnostic) {
        return diagnostic.severity == base::Severity::help && diagnostic.message.find("did you mean `count`")
            != std::string::npos;
    }));
}

TEST(CoreUnit, LspFramingParsesAndWritesDeterministicContentMessages)
{
    const std::string body = lsp_request_json(TOOLING_LSP_ID_INITIALIZE, "initialize", "{}");
    const std::string frame = tooling::write_lsp_content_message(body);
    EXPECT_NE(frame.find("Content-Length: "), std::string::npos);
    EXPECT_NE(frame.find("\r\n\r\n"), std::string::npos);

    const base::Result<std::vector<tooling::LspContentMessage>> parsed =
        tooling::parse_lsp_content_messages(frame + frame);
    ASSERT_TRUE(parsed);
    ASSERT_EQ(parsed.value().size(), 2U);
    EXPECT_EQ(parsed.value()[0].body, body);
    EXPECT_EQ(parsed.value()[1].body, body);

    const base::Result<std::vector<tooling::LspContentMessage>> extra_header =
        tooling::parse_lsp_content_messages("Content-Type: application/vscode-jsonrpc\r\nContent-Length: 2\r\n\r\n{}");
    ASSERT_TRUE(extra_header);
    ASSERT_EQ(extra_header.value().size(), 1U);
    EXPECT_EQ(extra_header.value().front().body, "{}");

    const base::Result<std::vector<tooling::LspContentMessage>> invalid =
        tooling::parse_lsp_content_messages("Content-Length: 12\r\n\r\n{}");
    EXPECT_FALSE(invalid);
    EXPECT_FALSE(tooling::parse_lsp_content_messages("Content-Length\r\n\r\n{}"));
}

TEST(CoreUnit, LspServerCoversErrorPathsFramedDispatchAndEscapedJson)
{
    tooling::LspServer default_server;
    const tooling::LspServer& const_default_server = default_server;
    EXPECT_FALSE(default_server.initialized());
    EXPECT_EQ(&default_server.session(), &const_default_server.session());
    std::vector<std::string> null_id_initialize =
        default_server.handle_json_message("{\"jsonrpc\":\"2.0\",\"method\":\"initialize\"}");
    ASSERT_EQ(null_id_initialize.size(), 1U);
    EXPECT_NE(null_id_initialize.front().find("\"id\":null"), std::string::npos);
    EXPECT_TRUE(default_server.handle_json_message("{\"jsonrpc\":\"2.0\"}").empty());
    EXPECT_TRUE(default_server.handle_json_message("{\"jsonrpc\":\"2.0\",\"method\":\"aurex/unknown\"}").empty());
    EXPECT_TRUE(default_server.handle_json_message("[]").empty());
    EXPECT_TRUE(default_server.handle_json_message("{method:\"initialize\"}").empty());
    EXPECT_TRUE(default_server.handle_json_message("{\"method\" \"initialize\"}").empty());
    EXPECT_TRUE(default_server.handle_json_message("{\"method\":}").empty());
    EXPECT_TRUE(default_server.handle_json_message("{\"method\":\"unterminated}").empty());
    EXPECT_TRUE(default_server.handle_json_message("{\"bad\\q\":1}").empty());
    std::vector<std::string> malformed_method =
        default_server.handle_json_message("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":1}");
    ASSERT_EQ(malformed_method.size(), 1U);
    EXPECT_NE(malformed_method.front().find("-32600"), std::string::npos);

    tooling::LspServer framed_server(tooling_project_config(TOOLING_LSP_PACKAGE));
    const std::string framed_initialize = tooling::write_lsp_content_message(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initializ\\u0065\",\"params\":{}}");
    const std::string framed_initialized =
        tooling::write_lsp_content_message(lsp_notification_json("initialized", "{}"));
    base::Result<std::string> framed = framed_server.handle_framed_messages(framed_initialize + framed_initialized);
    ASSERT_TRUE(framed);
    EXPECT_NE(framed.value().find("Content-Length:"), std::string::npos);
    EXPECT_NE(framed.value().find("\"hoverProvider\":true"), std::string::npos);
    EXPECT_TRUE(framed_server.initialized());
    EXPECT_FALSE(framed_server.handle_framed_messages("Content-Length: nope\r\n\r\n{}"));
    EXPECT_FALSE(tooling::parse_lsp_content_messages("Content-Type: application/vscode-jsonrpc\r\n\r\n{}"));
    EXPECT_FALSE(tooling::parse_lsp_content_messages("Content-Length: 2"));

    tooling::LspServer server(tooling_project_config(TOOLING_LSP_PACKAGE));
    std::vector<std::string> responses = server.handle_json_message("{\"jsonrpc\":\"2.0\",\"id\":99}");
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("-32600"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_UNKNOWN, "aurex/unknown", "{}"));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("-32601"), std::string::npos);
    EXPECT_TRUE(server.handle_json_message(lsp_notification_json("$/cancelRequest", "{}")).empty());
    const std::string invalid_short_escape = "{\"jsonrpc\":\"2.0\",\"id\":"
        + std::to_string(TOOLING_LSP_ID_INVALID_JSON) + ",\"method\":\"bad\\q\"}";
    responses = server.handle_json_message(invalid_short_escape);
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("-32600"), std::string::npos);
    const std::string invalid_unicode_escape = "{\"jsonrpc\":\"2.0\",\"id\":"
        + std::to_string(TOOLING_LSP_ID_INVALID_JSON) + ",\"method\":\"bad\\u00xx\"}";
    responses = server.handle_json_message(invalid_unicode_escape);
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("-32600"), std::string::npos);
    EXPECT_FALSE(server.session()
            .document_state(
                tooling::tooling_document_id_from_uri("file:///workspace/missing.ax", server.session().project_config()))
            .has_value());

    EXPECT_TRUE(server.handle_json_message(lsp_notification_json("textDocument/didOpen", "{}")).empty());
    EXPECT_TRUE(server.handle_json_message(lsp_notification_json("textDocument/didChange", "{}")).empty());
    EXPECT_TRUE(server.handle_json_message(lsp_notification_json("textDocument/didClose", "{}")).empty());
    EXPECT_NE(server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_HOVER, "textDocument/hover", "{}"))
                  .front()
                  .find("\"result\":null"),
        std::string::npos);
    EXPECT_NE(server.handle_json_message(
                        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/definition\",\"params\":{}}")
                  .front()
                  .find("\"result\":null"),
        std::string::npos);
    EXPECT_NE(server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_REFERENCES, "textDocument/references", "{}"))
                  .front()
                  .find("\"result\":[]"),
        std::string::npos);
    EXPECT_NE(
        server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_SYMBOLS, "textDocument/documentSymbol", "{}"))
            .front()
            .find("\"result\":[]"),
        std::string::npos);
    EXPECT_NE(server.handle_json_message(
                        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/hover\"}")
                  .front()
                  .find("\"result\":null"),
        std::string::npos);
    EXPECT_NE(server.handle_json_message(
                        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/definition\"}")
                  .front()
                  .find("\"result\":null"),
        std::string::npos);
    EXPECT_NE(server.handle_json_message(
                        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/references\"}")
                  .front()
                  .find("\"result\":[]"),
        std::string::npos);
    EXPECT_NE(server.handle_json_message(
                        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/documentSymbol\"}")
                  .front()
                  .find("\"result\":[]"),
        std::string::npos);
    EXPECT_TRUE(server.handle_json_message(
                          "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\"}")
                    .empty());
    EXPECT_TRUE(server.handle_json_message(
                          "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\"}")
                    .empty());
    EXPECT_TRUE(server.handle_json_message(
                          "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\"}")
                    .empty());
    EXPECT_TRUE(server.handle_json_message(
                          "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{}}}")
                    .empty());
    EXPECT_TRUE(server.handle_json_message(
                          "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\","
                          "\"params\":{\"textDocument\":{\"uri\":\"file:///workspace/missing.ax\"},"
                          "\"contentChanges\":[]}}")
                    .empty());
    EXPECT_TRUE(server.handle_json_message(
                          "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\","
                          "\"params\":{\"textDocument\":{\"uri\":\"file:///workspace/missing.ax\"},"
                          "\"contentChanges\":[{}]}}")
                    .empty());
    EXPECT_NE(server.handle_json_message(
                        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"textDocument/hover\","
                        "\"params\":{\"textDocument\":{\"uri\":\"file:///workspace/missing.ax\"},"
                        "\"position\":{\"line\":0}}}")
                  .front()
                  .find("\"result\":null"),
        std::string::npos);

    const std::string escaped_open =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"
        + test_json_string(TOOLING_LSP_ESCAPE_URI)
        + ",\"languageId\":\"aurex\",\"version\":1,\"text\":\"module tooling.symbols;\\n"
          "type Count = i32;\\n"
          "enum Mode { fast }\\n"
          "struct Box[T] { value: T; }\\n"
          "fn main() -> i32 { return 0; }\\n"
          "// quote: \\\" slash: \\/ backslash: \\\\ tab:\\t return:\\r form:\\f backspace:\\b unicode:\\u0041\\n\"}}}";
    responses = server.handle_json_message(escaped_open);
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("publishDiagnostics"), std::string::npos);
    EXPECT_TRUE(server.handle_json_message(escaped_open).empty());

    const std::string escaped_output_uri =
        std::string("file:///workspace/lsp_output_\"quote\\slash\n\t\b\f\r") + static_cast<char>(1) + ".ax";
    responses = server.handle_json_message(lsp_notification_json(
        "textDocument/didOpen", did_open_params(escaped_output_uri, TOOLING_SESSION_SOURCE, TOOLING_VERSION_THREE)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\\\"quote\\\\slash\\n\\t\\b\\f\\r\\u0001.ax"), std::string::npos);

    const std::string unicode_open =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"
        "\"file:///workspace/non_ascii_\\u00AF.ax\",\"languageId\":\"aurex\",\"version\":\"bad\",\"text\":"
        "\"module tooling.unicode;\\nfn main() -> i32 { return 0; }\\n\"}}}";
    responses = server.handle_json_message(unicode_open);
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("publishDiagnostics"), std::string::npos);
    const std::string lowercase_unicode_open =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":"
        "\"file:///workspace/non_ascii_lower_\\u00af.ax\",\"languageId\":\"aurex\",\"version\":1,\"text\":"
        "\"module tooling.unicode.lower;\\nfn main() -> i32 { return 0; }\\n\"}}}";
    responses = server.handle_json_message(lowercase_unicode_open);
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("publishDiagnostics"), std::string::npos);

    responses = server.handle_json_message(lsp_notification_json(
        "textDocument/didOpen", did_open_params(TOOLING_LSP_NO_MODULE_URI,
                                    TOOLING_SESSION_NO_MODULE_SOURCE, TOOLING_VERSION_ONE)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"resolved\":false"), std::string::npos);
    EXPECT_NE(responses.front().find("\"valid\":false"), std::string::npos);

    responses = server.handle_json_message(
        lsp_notification_json("textDocument/didOpen", did_open_params(TOOLING_LSP_SYMBOL_URI,
                                                  TOOLING_LSP_SYMBOL_SOURCE, TOOLING_VERSION_ONE)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_TRUE(server.handle_json_message(lsp_notification_json(
                           "textDocument/didChange", did_change_params(TOOLING_LSP_SYMBOL_URI,
                                                       TOOLING_LSP_SYMBOL_SOURCE, TOOLING_VERSION_ONE)))
                    .empty());

    responses = server.handle_json_message(lsp_request_json(
        TOOLING_LSP_ID_SYMBOLS, "textDocument/documentSymbol", text_document_params(TOOLING_LSP_SYMBOL_URI)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"generic_template\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"enum_case\""), std::string::npos);

    responses = server.handle_json_message(
        lsp_notification_json("textDocument/didOpen", did_open_params(TOOLING_LSP_FALLBACK_SYMBOL_URI,
                                                  TOOLING_SESSION_FALLBACK_SYMBOL_SOURCE, TOOLING_VERSION_ONE)));
    ASSERT_EQ(responses.size(), 1U);
    responses = server.handle_json_message(lsp_request_json(
        TOOLING_LSP_ID_SYMBOLS, "textDocument/documentSymbol", text_document_params(TOOLING_LSP_FALLBACK_SYMBOL_URI)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"const\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"struct\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"opaque_struct\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"enum\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"checked\":false"), std::string::npos);

    responses = server.handle_json_message(
        lsp_notification_json("textDocument/didOpen", did_open_params(TOOLING_LSP_METHOD_SYMBOL_URI,
                                                  TOOLING_LSP_METHOD_SYMBOL_SOURCE, TOOLING_VERSION_ONE)));
    ASSERT_EQ(responses.size(), 1U);
    responses = server.handle_json_message(lsp_request_json(
        TOOLING_LSP_ID_SYMBOLS, "textDocument/documentSymbol", text_document_params(TOOLING_LSP_METHOD_SYMBOL_URI)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"method\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"struct\""), std::string::npos);
}

TEST(CoreUnit, LspServerHandlesLifecycleSyncDiagnosticsAndIdeRequests)
{
    tooling::LspServer server(tooling_project_config(TOOLING_LSP_PACKAGE));

    std::vector<std::string> responses =
        server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_INITIALIZE, "initialize", "{}"));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"hoverProvider\":true"), std::string::npos);
    EXPECT_NE(responses.front().find("\"documentSymbolProvider\":true"), std::string::npos);

    EXPECT_TRUE(server.handle_json_message(lsp_notification_json("initialized", "{}")).empty());
    EXPECT_TRUE(server.initialized());

    responses = server.handle_json_message(
        lsp_notification_json("textDocument/didOpen", did_open_params(TOOLING_LSP_URI, TOOLING_SESSION_SOURCE,
                                                  TOOLING_VERSION_ONE)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"method\":\"textDocument/publishDiagnostics\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"diagnostics\":[]"), std::string::npos);

    const base::usize call_offset = TOOLING_SESSION_SOURCE.find("add(1");
    ASSERT_NE(call_offset, std::string_view::npos);
    const tooling::ToolingSourcePosition call_position =
        tooling::tooling_position_for_offset(TOOLING_SESSION_SOURCE, call_offset);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_HOVER, "textDocument/hover",
        text_document_position_params(TOOLING_LSP_URI, call_position)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("identifier `add` -> function"), std::string::npos);
    EXPECT_NE(responses.front().find("semanticFactKey"), std::string::npos);

    const std::string missing_document_position_params =
        text_document_position_params("file:///workspace/not_open.ax", call_position);
    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_HOVER, "textDocument/hover",
        missing_document_position_params));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":null"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_DEFINITION, "textDocument/definition",
        text_document_position_params(TOOLING_LSP_URI, call_position)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"uri\":\"file:///workspace/lsp.ax\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"range\""), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_DEFINITION, "textDocument/definition",
        missing_document_position_params));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":null"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_REFERENCES, "textDocument/references",
        text_document_position_params(TOOLING_LSP_URI, call_position)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":["), std::string::npos);
    EXPECT_NE(responses.front().find("\"uri\":\"file:///workspace/lsp.ax\""), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_REFERENCES, "textDocument/references",
        missing_document_position_params));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":[]"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_NO_PARAMS, "textDocument/hover",
        "{\"textDocument\":{\"uri\":\"file:///workspace/lsp.ax\"},\"position\":{\"line\":-1,\"character\":0}}"));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":null"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(
        TOOLING_LSP_ID_SYMBOLS, "textDocument/documentSymbol", text_document_params(TOOLING_LSP_URI)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"name\":\"add\""), std::string::npos);
    EXPECT_NE(responses.front().find("\"aurexKind\":\"function\""), std::string::npos);

    responses = server.handle_json_message(lsp_notification_json("textDocument/didChange",
        did_change_params(TOOLING_LSP_URI, TOOLING_SESSION_INVALID_SOURCE, TOOLING_VERSION_TWO)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"method\":\"textDocument/publishDiagnostics\""), std::string::npos);
    EXPECT_NE(responses.front().find("SEM0100"), std::string::npos);
    EXPECT_NE(responses.front().find("\"ownerStages\""), std::string::npos);

    responses = server.handle_json_message(lsp_notification_json("textDocument/didChange",
        did_change_params(TOOLING_LSP_URI, TOOLING_SESSION_SUGGESTION_SOURCE, TOOLING_VERSION_THREE)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"severity\":4"), std::string::npos);
    EXPECT_NE(responses.front().find("did you mean `count`"), std::string::npos);

    responses =
        server.handle_json_message(lsp_notification_json("textDocument/didClose", text_document_params(TOOLING_LSP_URI)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"diagnostics\":[]"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_SYMBOLS, "textDocument/documentSymbol",
        text_document_params(TOOLING_LSP_URI)));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":[]"), std::string::npos);

    responses = server.handle_json_message(lsp_request_json(TOOLING_LSP_ID_SHUTDOWN, "shutdown", "{}"));
    ASSERT_EQ(responses.size(), 1U);
    EXPECT_NE(responses.front().find("\"result\":null"), std::string::npos);
    EXPECT_TRUE(server.shutdown_requested());
    EXPECT_TRUE(server.handle_json_message(lsp_notification_json("exit", "{}")).empty());
    EXPECT_TRUE(server.exited());
}

} // namespace aurex::test

#pragma once

#include <aurex/base/result.hpp>
#include <aurex/tooling/session.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::tooling {

struct LspContentMessage {
    std::string body;
};

[[nodiscard]] base::Result<std::vector<LspContentMessage>> parse_lsp_content_messages(std::string_view bytes);
[[nodiscard]] std::string write_lsp_content_message(std::string_view body);

class LspServer {
public:
    LspServer();
    explicit LspServer(ToolingProjectConfig config);

    [[nodiscard]] std::vector<std::string> handle_json_message(std::string_view body);
    [[nodiscard]] base::Result<std::string> handle_framed_messages(std::string_view bytes);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] bool shutdown_requested() const noexcept;
    [[nodiscard]] bool exited() const noexcept;
    [[nodiscard]] ToolingSession& session() noexcept;
    [[nodiscard]] const ToolingSession& session() const noexcept;

private:
    [[nodiscard]] std::vector<std::string> handle_initialize(std::string_view id);
    [[nodiscard]] std::vector<std::string> handle_shutdown(std::string_view id);
    [[nodiscard]] std::vector<std::string> handle_did_open(std::string_view params);
    [[nodiscard]] std::vector<std::string> handle_did_change(std::string_view params);
    [[nodiscard]] std::vector<std::string> handle_did_close(std::string_view params);
    [[nodiscard]] std::vector<std::string> handle_hover(std::string_view id, std::string_view params);
    [[nodiscard]] std::vector<std::string> handle_definition(std::string_view id, std::string_view params);
    [[nodiscard]] std::vector<std::string> handle_references(std::string_view id, std::string_view params);
    [[nodiscard]] std::vector<std::string> handle_document_symbols(std::string_view id, std::string_view params);

    ToolingSession session_;
    bool initialized_ = false;
    bool shutdown_requested_ = false;
    bool exited_ = false;
};

} // namespace aurex::tooling

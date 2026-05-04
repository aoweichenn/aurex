#include "aurex/backend/codegen_backend.hpp"
#include "aurex/backend/llvm_backend.hpp"
#include "aurex/driver/native_toolchain.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace aurex::backend {
namespace {

class LlvmCodeGenBackend final : public CodeGenBackend {
public:
    base::Result<std::string> emit_ir_text(const ir::Module& module) override {
        auto r = emit_llvm_ir(LlvmEmitRequest{&module, "aurex"});
        if (!r) return base::Result<std::string>::fail(r.error());
        return base::Result<std::string>::ok(r.value().text);
    }

    base::Result<std::string> emit_assembly(const ir::Module& module) override {
        auto r = emit_llvm_ir(LlvmEmitRequest{&module, "aurex"});
        if (!r) return base::Result<std::string>::fail(r.error());
        auto temp = write_llvm_temp(r.value().text);
        if (!temp) return base::Result<std::string>::fail(temp.error());
        driver::NativeCompileRequest nc;
        nc.input_path = temp.value();
        nc.emit_kind = driver::EmitKind::assembly;
        nc.input_is_llvm_ir = true;
        auto sp = nc.input_path.string() + ".s";
        nc.output_path = sp;
        auto cres = driver::invoke_clang(nc);
        std::error_code ec;
        std::filesystem::remove(temp.value(), ec);
        if (!cres) return base::Result<std::string>::fail(cres.error());
        auto text = read_file(sp);
        std::filesystem::remove(sp, ec);
        return text;
    }

    base::Result<void> emit_object(const ir::Module& module, const std::string& out) override {
        auto r = emit_llvm_ir(LlvmEmitRequest{&module, "aurex"});
        if (!r) return base::Result<void>::fail(r.error());
        auto temp = write_llvm_temp(r.value().text);
        if (!temp) return base::Result<void>::fail(temp.error());
        driver::NativeCompileRequest nc;
        nc.input_path = temp.value();
        nc.output_path = out;
        nc.emit_kind = driver::EmitKind::object;
        nc.input_is_llvm_ir = true;
        auto cres = driver::invoke_clang(nc);
        std::error_code ec;
        std::filesystem::remove(temp.value(), ec);
        return cres;
    }

private:
    static base::Result<std::filesystem::path> write_llvm_temp(const std::string_view text) {
        auto path = std::filesystem::temp_directory_path() /
            ("aurex_llvm_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ll");
        std::ofstream o(path, std::ios::binary);
        if (!o) return base::Result<std::filesystem::path>::fail({base::ErrorCode::io_error, "temp ll write"});
        o << text;
        if (!o) return base::Result<std::filesystem::path>::fail({base::ErrorCode::io_error, "temp ll write"});
        return base::Result<std::filesystem::path>::ok(path);
    }

    static base::Result<std::string> read_file(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return base::Result<std::string>::fail({base::ErrorCode::io_error, "read file"});
        std::ostringstream buf;
        buf << in.rdbuf();
        return base::Result<std::string>::ok(buf.str());
    }
};

} // namespace

std::unique_ptr<CodeGenBackend> create_llvm_backend() {
    return std::make_unique<LlvmCodeGenBackend>();
}

} // namespace aurex::backend

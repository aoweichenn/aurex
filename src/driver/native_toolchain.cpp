#include <aurex/driver/native_toolchain.hpp>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace aurex::driver {

namespace {

[[nodiscard]] std::string shell_hint(const std::vector<std::string>& args) {
    std::ostringstream out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0) {
            out << ' ';
        }
        out << args[i];
    }
    return out.str();
}

[[nodiscard]] base::Result<void> ensure_output_parent_exists(const std::filesystem::path& output_path) {
    const std::filesystem::path parent = output_path.parent_path();
    if (parent.empty()) {
        return base::Result<void>::ok();
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        return base::Result<void>::fail({
            base::ErrorCode::io_error,
            "failed to create native output directory: " + parent.string()
        });
    }
    return base::Result<void>::ok();
}

} // namespace

base::Result<void> invoke_clang(const NativeCompileRequest& request) {
    if (!request.support_source_paths.empty() && request.emit_kind != EmitKind::executable) {
        return base::Result<void>::fail({base::ErrorCode::codegen_error, "native support sources are only supported for executable output"});
    }
    auto output_parent_result = ensure_output_parent_exists(request.output_path);
    if (!output_parent_result) {
        return output_parent_result;
    }

    std::vector<std::string> args;
    args.push_back(request.clang_path.empty() ? "clang" : request.clang_path);
    if (request.input_is_llvm_ir) {
        args.push_back("-x");
        args.push_back("ir");
    }
    args.push_back(request.input_path.string());
    if (!request.support_source_paths.empty()) {
        if (request.input_is_llvm_ir) {
            args.push_back("-x");
            args.push_back("none");
        }
        for (const std::filesystem::path& support_path : request.support_source_paths) {
            args.push_back(support_path.string());
        }
    }
    args.insert(args.end(), request.clang_args.begin(), request.clang_args.end());
    if (request.emit_kind == EmitKind::assembly) {
        args.push_back("-S");
    } else if (request.emit_kind == EmitKind::object) {
        args.push_back("-c");
    }
    args.push_back("-o");
    args.push_back(request.output_path.string());

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    const pid_t child = fork();
    if (child < 0) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "failed to fork clang process"});
    }
    if (child == 0) {
        execvp(argv[0], argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "failed to wait for clang process"});
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return base::Result<void>::ok();
    }

    std::ostringstream message;
    message << "clang invocation failed";
    if (WIFEXITED(status)) {
        message << " with exit code " << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        message << " from signal " << WTERMSIG(status);
    }
    message << ": " << shell_hint(args);
    return base::Result<void>::fail({base::ErrorCode::codegen_error, message.str()});
}

} // namespace aurex::driver

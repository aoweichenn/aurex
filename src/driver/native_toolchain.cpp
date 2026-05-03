#include "aurex/driver/native_toolchain.hpp"

#include <cerrno>
#include <cstring>
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

} // namespace

base::Result<void> invoke_clang(const NativeCompileRequest& request) {
    std::vector<std::string> args;
    args.push_back(request.clang_path.empty() ? "clang" : request.clang_path);
    args.push_back(request.input_path.string());
    for (const std::filesystem::path& runtime_path : request.runtime_c_paths) {
        args.push_back(runtime_path.string());
    }
    args.insert(args.end(), request.clang_args.begin(), request.clang_args.end());
    if (request.emit_kind == EmitKind::assembly) {
        args.push_back("-S");
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

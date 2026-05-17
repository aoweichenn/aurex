#include <aurex/driver/cli_llvm.hpp>

#include <iostream>
#include <string_view>
#include <vector>

int main(const int argc, char** argv) {
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        arguments.emplace_back(argv[i] == nullptr ? std::string_view {} : std::string_view {argv[i]});
    }
    return aurex::driver::run_cli_with_llvm_backend(arguments, std::cout, std::cerr);
}

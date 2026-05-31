#include <aurex/tooling/lsp_stdio.hpp>

#include <iostream>
#include <string_view>
#include <vector>

int main(const int argc, char** argv)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }
    return aurex::tooling::run_lsp_stdio(arguments, std::cin, std::cout, std::cerr);
}

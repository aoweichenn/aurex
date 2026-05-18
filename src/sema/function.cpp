#include <aurex/sema/function.hpp>

#include <string_view>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_FUNCTION_GENERIC_ARG_LIST_OPEN = "[";
constexpr std::string_view SEMA_FUNCTION_GENERIC_ARG_LIST_CLOSE = "]";
constexpr std::string_view SEMA_FUNCTION_GENERIC_ARG_LIST_SEPARATOR = ",";
constexpr base::usize SEMA_FUNCTION_GENERIC_DISPLAY_ARG_SIZE_ESTIMATE = 16;

} // namespace

std::string function_display_name(const TypeTable& types, const FunctionSignature& signature)
{
    if (signature.generic_args.empty()) {
        return std::string(signature.name.view());
    }

    std::string display;
    display.reserve(signature.name.size() + SEMA_FUNCTION_GENERIC_ARG_LIST_OPEN.size()
        + SEMA_FUNCTION_GENERIC_ARG_LIST_CLOSE.size()
        + signature.generic_args.size() * SEMA_FUNCTION_GENERIC_DISPLAY_ARG_SIZE_ESTIMATE);
    display += signature.name.view();
    display += SEMA_FUNCTION_GENERIC_ARG_LIST_OPEN;
    for (base::usize index = 0; index < signature.generic_args.size(); ++index) {
        if (index != 0) {
            display += SEMA_FUNCTION_GENERIC_ARG_LIST_SEPARATOR;
        }
        display += types.display_name(signature.generic_args[index]);
    }
    display += SEMA_FUNCTION_GENERIC_ARG_LIST_CLOSE;
    return display;
}

} // namespace aurex::sema

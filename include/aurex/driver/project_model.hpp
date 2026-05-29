#pragma once

#include <aurex/driver/invocation.hpp>
#include <aurex/project/project_model.hpp>

namespace aurex::driver {

[[nodiscard]] project::ProjectModel project_model_from_invocation(const CompilerInvocation& invocation);
[[nodiscard]] project::WorkspaceModel workspace_model_from_invocation(const CompilerInvocation& invocation);

} // namespace aurex::driver

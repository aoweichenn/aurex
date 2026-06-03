#pragma once

#include <aurex/application/driver/diagnostic_format.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/base/source.hpp>

#include <iosfwd>
#include <string_view>

namespace aurex::driver {

void render_diagnostics(std::ostream& out, const base::SourceManager& sources, const base::DiagnosticSink& diagnostics,
    DiagnosticOutputFormat format);

void render_driver_error(std::ostream& out, std::string_view message, DiagnosticOutputFormat format);

} // namespace aurex::driver

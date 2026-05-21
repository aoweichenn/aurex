#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/diagnostic_format.hpp>

#include <iosfwd>
#include <string_view>

namespace aurex::driver {

void render_diagnostics(std::ostream& out, const base::SourceManager& sources, const base::DiagnosticSink& diagnostics,
    DiagnosticOutputFormat format);

void render_driver_error(std::ostream& out, std::string_view message, DiagnosticOutputFormat format);

} // namespace aurex::driver

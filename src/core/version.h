// Application version accessor (role: core).
// Gives app and tests a single source for the version string defined in the
// top-level CMakeLists.txt; also the first symbol of the core library.

#pragma once

#include <QString>

namespace garagelog::core {

/**
 * @brief Returns the application version, e.g. "0.1.0".
 * @return Version string as set by project(VERSION ...) in CMake.
 */
QString version();

} // namespace garagelog::core

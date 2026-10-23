// Implementation of the version accessor (role: core).
// GARAGELOG_VERSION is injected by src/core/CMakeLists.txt.

#include "version.h"

namespace garagelog::core {

QString version()
{
    return QStringLiteral(GARAGELOG_VERSION);
}

} // namespace garagelog::core

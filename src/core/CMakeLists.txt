# Build definition for garagelog_core, the GUI-free logic library (role: core).
# Only Qt Core and Qt Sql are allowed here, so the library stays testable
# without a display and without QtQuick/Kirigami.

add_library(garagelog_core STATIC
    version.cpp
    version.h
)

target_include_directories(garagelog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Version string comes from project(), so it is defined in exactly one place.
target_compile_definitions(garagelog_core PRIVATE GARAGELOG_VERSION="${PROJECT_VERSION}")

target_link_libraries(garagelog_core PUBLIC Qt6::Core Qt6::Sql)

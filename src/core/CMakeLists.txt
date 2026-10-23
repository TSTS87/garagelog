# Build definition for garagelog_core, the GUI-free logic library (role: core).
# Only Qt Core and Qt Sql are allowed here, so the library stays testable
# without a display and without QtQuick/Kirigami.

add_library(garagelog_core STATIC
    attachmentstore.cpp
    attachmentstore.h
    categoryrepository.cpp
    categoryrepository.h
    database.cpp
    database.h
    duecalculator.cpp
    duecalculator.h
    dbvalues.cpp
    dbvalues.h
    fuelentry.cpp
    fuelentry.h
    fuelprice.cpp
    fuelprice.h
    fuelrepository.cpp
    fuelrepository.h
    fuelstats.cpp
    fuelstats.h
    intervalrepository.cpp
    intervalrepository.h
    maintenanceinterval.h
    migrationrunner.cpp
    migrationrunner.h
    odometer.cpp
    odometer.h
    serviceentry.cpp
    serviceentry.h
    servicerepository.cpp
    servicerepository.h
    statsservice.cpp
    statsservice.h
    vehicle.cpp
    vehicle.h
    vehiclerepository.cpp
    vehiclerepository.h
    version.cpp
    version.h
)

target_include_directories(garagelog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Version string comes from project(), so it is defined in exactly one place.
target_compile_definitions(garagelog_core PRIVATE GARAGELOG_VERSION="${PROJECT_VERSION}")

target_link_libraries(garagelog_core PUBLIC Qt6::Core Qt6::Sql)

# Migrations are embedded so the runner finds them at :/migrations/NNNN_*.sql
# without installed files. New migrations must be added to this list.
# For a static library Qt links the resource as object library, so no
# Q_INIT_RESOURCE call is needed in app or tests.
qt_add_resources(garagelog_core "migrations"
    PREFIX "/"
    FILES
        migrations/0001_initial.sql
)

# Build definition for the garagelog executable (role: app).
# Links the core library and embeds the QML files from the top-level qml/
# directory as Qt resources, so the binary runs without installed QML files.

# Models and helpers live in a static library so tests can use them without
# a window; only main.cpp and the QML need the GUI modules.
add_library(garagelog_applib STATIC
    datatransfer.cpp
    datatransfer.h
    format.cpp
    format.h
    fuellistmodel.cpp
    fuellistmodel.h
    garagecontroller.cpp
    garagecontroller.h
    intervallistmodel.cpp
    intervallistmodel.h
    servicelistmodel.cpp
    servicelistmodel.h
    statsmodel.cpp
    statsmodel.h
    vehiclelistmodel.cpp
    vehiclelistmodel.h
)
target_include_directories(garagelog_applib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# KI18n (no GUI) translates built-in service categories and formatted texts.
target_link_libraries(garagelog_applib PUBLIC garagelog_core Qt6::Core KF6::I18n)
# i18n() calls in this library use the app's catalogue even when no
# application domain is set (tests), which also silences KI18n's warning.
target_compile_definitions(garagelog_applib PRIVATE TRANSLATION_DOMAIN="garagelog")

add_executable(garagelog
    main.cpp
)

# BASE strips the absolute path, so qml/Main.qml becomes qrc:/qml/Main.qml.
# New QML files must be added to this list.
qt_add_resources(garagelog "qml"
    PREFIX "/"
    BASE ${PROJECT_SOURCE_DIR}
    FILES
        ${PROJECT_SOURCE_DIR}/qml/CsvImportPage.qml
        ${PROJECT_SOURCE_DIR}/qml/DuePage.qml
        ${PROJECT_SOURCE_DIR}/qml/FuelFormPage.qml
        ${PROJECT_SOURCE_DIR}/qml/FuelTab.qml
        ${PROJECT_SOURCE_DIR}/qml/IntervalFormPage.qml
        ${PROJECT_SOURCE_DIR}/qml/IntervalTab.qml
        ${PROJECT_SOURCE_DIR}/qml/Main.qml
        ${PROJECT_SOURCE_DIR}/qml/ServiceFormPage.qml
        ${PROJECT_SOURCE_DIR}/qml/ServiceTab.qml
        ${PROJECT_SOURCE_DIR}/qml/SettingsPage.qml
        ${PROJECT_SOURCE_DIR}/qml/StatsTab.qml
        ${PROJECT_SOURCE_DIR}/qml/StatusDot.qml
        ${PROJECT_SOURCE_DIR}/qml/VehicleDrawer.qml
        ${PROJECT_SOURCE_DIR}/qml/VehicleFormPage.qml
        ${PROJECT_SOURCE_DIR}/qml/VehiclePage.qml
)

target_link_libraries(garagelog PRIVATE
    garagelog_applib
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::I18n
    KF6::I18nQml
    KF6::ConfigCore
)

install(TARGETS garagelog ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

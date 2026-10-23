# Build definition for the garagelog executable (role: app).
# Links the core library and embeds the QML files from the top-level qml/
# directory as Qt resources, so the binary runs without installed QML files.

# Models and helpers live in a static library so tests can use them without
# a window; only main.cpp and the QML need the GUI modules.
add_library(garagelog_applib STATIC
    format.cpp
    format.h
    fuellistmodel.cpp
    fuellistmodel.h
    vehiclelistmodel.cpp
    vehiclelistmodel.h
)
target_include_directories(garagelog_applib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(garagelog_applib PUBLIC garagelog_core Qt6::Core)

add_executable(garagelog
    main.cpp
)

# BASE strips the absolute path, so qml/Main.qml becomes qrc:/qml/Main.qml.
# New QML files must be added to this list.
qt_add_resources(garagelog "qml"
    PREFIX "/"
    BASE ${PROJECT_SOURCE_DIR}
    FILES
        ${PROJECT_SOURCE_DIR}/qml/Main.qml
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
)

install(TARGETS garagelog ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

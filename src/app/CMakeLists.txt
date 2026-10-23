# Build definition for the garagelog executable (role: app).
# Links the core library and embeds the QML files from the top-level qml/
# directory as Qt resources, so the binary runs without installed QML files.

add_executable(garagelog
    main.cpp
)

# BASE strips the absolute path, so qml/Main.qml becomes qrc:/qml/Main.qml.
qt_add_resources(garagelog "qml"
    PREFIX "/"
    BASE ${PROJECT_SOURCE_DIR}
    FILES
        ${PROJECT_SOURCE_DIR}/qml/Main.qml
)

target_link_libraries(garagelog PRIVATE
    garagelog_core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS garagelog ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

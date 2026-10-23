# Top-level build definition for GarageLog.
# Finds Qt 6 / KDE Frameworks 6 and wires up the four parts of the project:
# src/core (static logic library), src/app (QML executable), tools (developer
# helpers such as the test data generator) and tests (QtTest).

cmake_minimum_required(VERSION 3.21)
project(garagelog VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(QT_MIN_VERSION 6.6.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQmlModule)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Kirigami I18n CoreAddons Config)

add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tools)

enable_testing()
add_subdirectory(tests)

# Compiles po/<lang>/garagelog.po to .mo files and installs them to share/locale.
ki18n_install(po)

install(FILES packaging/io.github.TSTS87.GarageLog.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES packaging/io.github.TSTS87.GarageLog.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

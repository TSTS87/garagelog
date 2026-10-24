# Unit tests for garagelog_core (QtTest, no GUI).
# Each test is its own executable registered with ctest.

# One QtTest executable per test file, registered with ctest under its name.
# Extra arguments are the libraries it links besides Qt6::Test.
function(garagelog_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Core library only: proves the logic works without any GUI module.
foreach(test IN ITEMS
        test_database test_vehiclerepository test_odometer test_fuelrepository test_fuelstats test_fuelprice
        test_categoryrepository test_servicerepository test_attachmentstore test_intervalrepository
        test_duecalculator test_statsservice test_jsonexport test_csvfuelimporter)
    garagelog_add_test(${test} garagelog_core)
endforeach()
# Sample files from other apps live in tests/data/.
target_compile_definitions(test_csvfuelimporter PRIVATE GARAGELOG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

# App-layer models: linked against garagelog_applib, still without a GUI.
foreach(test IN ITEMS
        test_vehiclelistmodel test_fuellistmodel test_servicelistmodel test_intervallistmodel test_statsmodel
        test_garagecontroller)
    garagelog_add_test(${test} garagelog_applib)
endforeach()

# M15: 10,000 generated fuel entries; uses the generator from tools/.
garagelog_add_test(test_performance garagelog_applib garagelog_sampledata)

# M16: desktop file, AppStream metadata and translations must stay valid.
# Each check is only registered when its tool is installed (CI installs all of them).
set(PKG ${CMAKE_SOURCE_DIR}/packaging)
find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
if(DESKTOP_FILE_VALIDATE)
    add_test(NAME validate_desktop_file COMMAND ${DESKTOP_FILE_VALIDATE} ${PKG}/io.github.TSTS87.GarageLog.desktop)
endif()
find_program(APPSTREAMCLI appstreamcli)
if(APPSTREAMCLI)
    # --no-net: screenshot URLs only resolve once the release tag is pushed.
    add_test(NAME validate_metainfo COMMAND ${APPSTREAMCLI} validate --no-net ${PKG}/io.github.TSTS87.GarageLog.metainfo.xml)
endif()
find_program(MSGFMT msgfmt)
if(MSGFMT)
    file(GLOB PO_FILES ${CMAKE_SOURCE_DIR}/po/*/garagelog.po)
    foreach(po ${PO_FILES})
        get_filename_component(lang_dir ${po} DIRECTORY)
        get_filename_component(lang ${lang_dir} NAME)
        add_test(NAME validate_po_${lang} COMMAND ${MSGFMT} --check -o /dev/null ${po})
    endforeach()
endif()

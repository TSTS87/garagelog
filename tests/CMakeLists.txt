# Unit tests for garagelog_core (QtTest, no GUI).
# Each test is its own executable registered with ctest.

add_executable(test_smoke test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_smoke COMMAND test_smoke)

add_executable(test_database test_database.cpp)
target_link_libraries(test_database PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_database COMMAND test_database)

add_executable(test_vehiclerepository test_vehiclerepository.cpp)
target_link_libraries(test_vehiclerepository PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_vehiclerepository COMMAND test_vehiclerepository)

add_executable(test_odometer test_odometer.cpp)
target_link_libraries(test_odometer PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_odometer COMMAND test_odometer)

add_executable(test_fuelrepository test_fuelrepository.cpp)
target_link_libraries(test_fuelrepository PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_fuelrepository COMMAND test_fuelrepository)

add_executable(test_fuelstats test_fuelstats.cpp)
target_link_libraries(test_fuelstats PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_fuelstats COMMAND test_fuelstats)

add_executable(test_fuelprice test_fuelprice.cpp)
target_link_libraries(test_fuelprice PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_fuelprice COMMAND test_fuelprice)

add_executable(test_categoryrepository test_categoryrepository.cpp)
target_link_libraries(test_categoryrepository PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_categoryrepository COMMAND test_categoryrepository)

add_executable(test_servicerepository test_servicerepository.cpp)
target_link_libraries(test_servicerepository PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_servicerepository COMMAND test_servicerepository)

add_executable(test_attachmentstore test_attachmentstore.cpp)
target_link_libraries(test_attachmentstore PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_attachmentstore COMMAND test_attachmentstore)

add_executable(test_intervalrepository test_intervalrepository.cpp)
target_link_libraries(test_intervalrepository PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_intervalrepository COMMAND test_intervalrepository)

add_executable(test_duecalculator test_duecalculator.cpp)
target_link_libraries(test_duecalculator PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_duecalculator COMMAND test_duecalculator)

add_executable(test_statsservice test_statsservice.cpp)
target_link_libraries(test_statsservice PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_statsservice COMMAND test_statsservice)

add_executable(test_jsonexport test_jsonexport.cpp)
target_link_libraries(test_jsonexport PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_jsonexport COMMAND test_jsonexport)

# Sample files from other apps live in tests/data/.
add_executable(test_csvfuelimporter test_csvfuelimporter.cpp)
target_link_libraries(test_csvfuelimporter PRIVATE garagelog_core Qt6::Test)
target_compile_definitions(test_csvfuelimporter PRIVATE GARAGELOG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME test_csvfuelimporter COMMAND test_csvfuelimporter)

# App-layer models: linked against garagelog_applib, still without a GUI.
add_executable(test_vehiclelistmodel test_vehiclelistmodel.cpp)
target_link_libraries(test_vehiclelistmodel PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_vehiclelistmodel COMMAND test_vehiclelistmodel)

add_executable(test_fuellistmodel test_fuellistmodel.cpp)
target_link_libraries(test_fuellistmodel PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_fuellistmodel COMMAND test_fuellistmodel)

add_executable(test_servicelistmodel test_servicelistmodel.cpp)
target_link_libraries(test_servicelistmodel PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_servicelistmodel COMMAND test_servicelistmodel)

add_executable(test_intervallistmodel test_intervallistmodel.cpp)
target_link_libraries(test_intervallistmodel PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_intervallistmodel COMMAND test_intervallistmodel)

add_executable(test_statsmodel test_statsmodel.cpp)
target_link_libraries(test_statsmodel PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_statsmodel COMMAND test_statsmodel)

add_executable(test_garagecontroller test_garagecontroller.cpp)
target_link_libraries(test_garagecontroller PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_garagecontroller COMMAND test_garagecontroller)

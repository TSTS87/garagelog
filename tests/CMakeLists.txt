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

# App-layer models: linked against garagelog_applib, still without a GUI.
add_executable(test_vehiclelistmodel test_vehiclelistmodel.cpp)
target_link_libraries(test_vehiclelistmodel PRIVATE garagelog_applib Qt6::Test)
add_test(NAME test_vehiclelistmodel COMMAND test_vehiclelistmodel)

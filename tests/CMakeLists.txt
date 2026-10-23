# Unit tests for garagelog_core (QtTest, no GUI).
# Each test is its own executable registered with ctest.

add_executable(test_smoke test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE garagelog_core Qt6::Test)
add_test(NAME test_smoke COMMAND test_smoke)

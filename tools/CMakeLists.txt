# Developer tools (role: tools). Not installed and not part of the Flatpak.
# garagelog_sampledata is shared by the gendata tool and the performance test.

add_library(garagelog_sampledata STATIC
    sampledata.cpp
    sampledata.h
)
target_include_directories(garagelog_sampledata PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(garagelog_sampledata PUBLIC garagelog_core)

add_executable(garagelog-gendata gendata.cpp)
target_link_libraries(garagelog-gendata PRIVATE garagelog_sampledata)

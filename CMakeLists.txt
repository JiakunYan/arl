cmake_minimum_required(VERSION 3.13)
project(ARL VERSION 0.5.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
enable_testing()

set(ARL_BACKEND gex CACHE STRING "Backend")
set_property(CACHE ARL_BACKEND PROPERTY STRINGS gex)
if(ARL_BACKEND STREQUAL "gex")
    set(ARL_USE_GEX ON)
else()
    message(FATAL_ERROR "Fabric ${LCI_SERVER} not supported")
endif()

option(ARL_DEBUG "Enable debug mode" ON)
option(ARL_INFO "Enable info mode" OFF)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")
include(ARLUtils)

add_library(arl INTERFACE)
target_include_directories(arl INTERFACE arl)
configure_file(arl/config.hpp.in arl/config.hpp @ONLY)
target_include_directories(arl INTERFACE ${CMAKE_BINARY_DIR}/arl)

find_package(PkgConfig REQUIRED)
pkg_check_modules(gasnet REQUIRED IMPORTED_TARGET gasnet-mpi-par)
# fix a cmake bug when encountering multiple "--param <val>" options
string(REPLACE "--param;" "SHELL:--param " gasnet_LDFLAGS_FIX "${gasnet_LDFLAGS}")
set_target_properties(PkgConfig::gasnet PROPERTIES INTERFACE_LINK_OPTIONS "${gasnet_LDFLAGS_FIX}")
target_link_libraries(arl INTERFACE PkgConfig::gasnet)

find_package(MPI REQUIRED)
target_include_directories(arl INTERFACE ${MPI_CXX_INCLUDE_PATH})
target_compile_options(arl INTERFACE ${MPI_CXX_COMPILE_FLAGS})
target_link_libraries(arl INTERFACE ${MPI_CXX_LIBRARIES} ${MPI_CXX_LINK_FLAGS})

#add_subdirectory(examples)
add_subdirectory(tests)
cmake_minimum_required(VERSION 3.13)
project(ARL VERSION 0.5.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
enable_testing()

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")
include(ARLUtils)

add_library(arl INTERFACE)
target_include_directories(arl INTERFACE arl)

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

add_subdirectory(examples)
add_subdirectory(tests)
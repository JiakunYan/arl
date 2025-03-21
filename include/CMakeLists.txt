target_include_directories(arl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(config.hpp.in config.hpp @ONLY)
target_include_directories(arl PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
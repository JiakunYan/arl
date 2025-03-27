configure_file(arl_config.hpp.in arl_config.hpp @ONLY)
target_include_directories(arl
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
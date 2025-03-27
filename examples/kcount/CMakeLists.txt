find_package(ZLIB)

set(COMMON_SOURCE common/hash_funcs.c)

add_arl_executable(kcount_agg src/kcount_agg.cpp ${COMMON_SOURCE})
target_include_directories(kcount_agg PRIVATE include common src/zstr/src)
target_link_libraries(kcount_agg PRIVATE ZLIB::ZLIB)

add_arl_executable(kcount_ff src/kcount_ff.cpp ${COMMON_SOURCE})
target_include_directories(kcount_ff PRIVATE include common src/zstr/src)
target_link_libraries(kcount_ff PRIVATE ZLIB::ZLIB)

add_arl_executable(kcount_ffrd src/kcount_ffrd.cpp ${COMMON_SOURCE})
target_include_directories(kcount_ffrd PRIVATE include common src/zstr/src)
target_link_libraries(kcount_ffrd PRIVATE ZLIB::ZLIB)

add_arl_executable(kcount_aggrd src/kcount_aggrd.cpp ${COMMON_SOURCE})
target_include_directories(kcount_aggrd PRIVATE include common src/zstr/src)
target_link_libraries(kcount_aggrd PRIVATE ZLIB::ZLIB)

if(UPCXX_FOUND)
  add_upcxx_executable(kcount_upcxx src/kcount_upcxx.cpp ${COMMON_SOURCE})
  target_include_directories(kcount_upcxx PRIVATE include_upcxx common src/zstr/src)
  target_link_libraries(kcount_upcxx PRIVATE ZLIB::ZLIB)
endif()

configure_file(src/get_standard_result.sh get_standard_result.sh COPYONLY)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/sample DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

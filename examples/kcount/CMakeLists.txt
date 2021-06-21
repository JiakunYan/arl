find_package(ZLIB)

add_executable(kcount_agg src/kcount_agg.cpp src/kmer_dht_arl.hpp src/config.hpp)
target_include_directories(kcount_agg PRIVATE include src/zstr/src)
target_link_libraries(kcount_agg PRIVATE arl ZLIB::ZLIB)

add_executable(kcount_ff src/kcount_ff.cpp src/kmer_dht_arl.hpp src/config.hpp)
target_include_directories(kcount_ff PRIVATE include src/zstr/src)
target_link_libraries(kcount_ff PRIVATE arl ZLIB::ZLIB)

add_executable(kcount_ffrd src/kcount_ffrd.cpp src/kmer_dht_arl.hpp src/config.hpp)
target_include_directories(kcount_ffrd PRIVATE include src/zstr/src)
target_link_libraries(kcount_ffrd PRIVATE arl ZLIB::ZLIB)

add_executable(kcount_aggrd src/kcount_aggrd.cpp src/kmer_dht_arl.hpp src/config.hpp)
target_include_directories(kcount_aggrd PRIVATE include src/zstr/src)
target_link_libraries(kcount_aggrd PRIVATE arl ZLIB::ZLIB)

add_upcxx_executable(kcount_upcxx src/kcount_upcxx.cpp)
target_include_directories(kcount_upcxx PRIVATE include_upcxx src/zstr/src)
target_link_libraries(kcount_upcxx PRIVATE ZLIB::ZLIB)

configure_file(src/get_standard_result.sh get_standard_result.sh COPYONLY)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/sample DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

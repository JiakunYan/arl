add_arl_executable(kmer_hash_agg kmer_hash_agg.cpp)
add_arl_executable(kmer_hash_rd kmer_hash_rd.cpp)
add_upcxx_executable(kmer_hash_upcxx_rpc kmer_hash_upcxx_rpc.cpp)
add_upcxx_executable(kmer_hash_upcxx_rdma kmer_hash_upcxx_rdma.cpp)

configure_file(test_correct.sh test_correct.sh COPYONLY)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/sample DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
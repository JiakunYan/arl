add_executable(kmer_hash_rd kmer_hash_rd.cpp kmer_t.hpp read_kmers.hpp pkmer_t.hpp)
target_link_libraries(kmer_hash_rd PRIVATE arl)

configure_file(test_correct.sh test_correct.sh COPYONLY)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/sample DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
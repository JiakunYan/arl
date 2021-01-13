set(TEST_EXE mpirun CACHE STRING "exective to be used in ctest")
set(TEST_ARG CACHE STRING "arguments to be used in ctest")

add_all_tests(
        test_agg_buffer
        test_amagg
        test_amaggrd
        test_amff
        test_amffrd
        test_bloom_filter
        test_broadcast
        test_dist_wrapper
        test_hash_map
        test_reduce
        test_rpc_agg
        test_rpc_nonvoid
        test_rpc_void
        test_worker_object
)
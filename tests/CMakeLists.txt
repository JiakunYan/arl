function(add_arl_test name)
    cmake_parse_arguments(ARG "" "" "COMMAND;LABELS;SOURCES" ${ARGN})

    add_arl_executable(${name} ${ARG_SOURCES})
    string(REGEX REPLACE "\\[TARGET\\]" $<TARGET_FILE:${name}> TEST_COMMAND ${ARG_COMMAND})
    string(REPLACE " " ";" TEST_COMMAND ${TEST_COMMAND})
    add_test(NAME ${name}
            COMMAND ${TEST_COMMAND})
    set_property(TEST ${name} PROPERTY LABELS ${ARG_LABELS})
endfunction()

function(add_arl_tests)
    cmake_parse_arguments(ARG "" "" "COMMAND;LABELS;TESTS" ${ARGN})
    foreach(name ${ARG_TESTS})
        string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
        add_arl_test(test-${ARG_LABELS}-${name_without_ext} SOURCES ${name} LABELS ${ARG_LABELS} COMMAND ${ARG_COMMAND})
    endforeach()
endfunction()

option(LCI_BUILD_TESTS "Build tests by default" ON)
if( NOT LCI_BUILD_TESTS )
    set(EXCLUDE_FROM_ALL ON)
endif()
set(SRUN_EXE mpirun CACHE STRING "exective to be used in ctest")
set(SRUN_ARG "-n 2" CACHE STRING "arguments to be used in ctest")

add_arl_tests(
        TESTS
            test_agg_buffer.cpp
            test_amagg.cpp
            test_amaggrd.cpp
            test_amff.cpp
            test_amffrd.cpp
            test_bloom_filter.cpp
            test_broadcast.cpp
            test_dist_wrapper.cpp
            test_hash_map.cpp
            test_reduce.cpp
            test_rpc_agg.cpp
            test_rpc_nonvoid.cpp
            test_rpc_void.cpp
            test_worker_object.cpp
        LABELS all
        COMMAND "${SRUN_EXE} ${SRUN_ARG} [TARGET]"
)
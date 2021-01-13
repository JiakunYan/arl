function(add_example NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE arl)
endfunction()

function(add_all_examples)
    foreach(example ${ARGN})
        add_example(${example})
    endforeach()
endfunction()

function(add_all_tests)
    foreach(example ${ARGN})
        add_example(${example})
        add_test(NAME test_${example}
                COMMAND ${TEST_EXE} ${TEST_ARG} -n 2 $<TARGET_FILE:${example}>)
    endforeach()
endfunction()
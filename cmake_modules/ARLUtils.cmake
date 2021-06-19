function(add_arl_executable EXEC)
    add_executable(${EXEC} ${ARGN})
    target_link_libraries(${EXEC} PRIVATE arl)
endfunction()

function(add_gex_executable EXEC)
    add_executable(${EXEC} ${ARGN})
    target_link_libraries(${EXEC} PRIVATE PkgConfig::gasnet arl)
endfunction()

function(add_upcxx_executable EXEC)
    add_executable(${EXEC} ${ARGN})
    target_link_libraries(${EXEC} PRIVATE UPCXX::upcxx)
endfunction()
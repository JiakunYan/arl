target_sources(arl PRIVATE arl.cpp backend/backend.cpp backend/LCI/backend_lci.cpp)

target_include_directories(arl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
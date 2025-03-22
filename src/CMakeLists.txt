target_sources(
  arl
  PRIVATE arl.cpp
          config_env.cpp
          backend/backend.cpp
          tool/debug.cpp
          am/am.cpp
          am/am_agg.cpp
          am/am_aggrd.cpp
          am/am_ff.cpp
          am/am_ffrd.cpp)

target_include_directories(arl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(ARL_USE_LCI)
  target_sources(arl PRIVATE backend/lci/base.cpp)
endif()

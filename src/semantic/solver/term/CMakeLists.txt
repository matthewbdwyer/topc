add_library(term_solver)
target_sources(term_solver
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/TermUnifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TermUnionFind.cpp)
target_include_directories(term_solver
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(term_solver PRIVATE coverage_config loguru)

# This property communicates the location of the ANTLR4 generated files
get_property(GrammarIncDir GLOBAL PROPERTY GrammarIncludeDir)
# We need to include those files in builds of the unit tests
include_directories(${GrammarIncDir})

add_library(topc_test_includes INTERFACE)
target_include_directories(topc_test_includes INTERFACE
    ${CMAKE_SOURCE_DIR}/src/error
    ${CMAKE_SOURCE_DIR}/src/frontend/
    ${CMAKE_SOURCE_DIR}/src/frontend/ast
    ${CMAKE_SOURCE_DIR}/src/frontend/ast/treetypes
    ${CMAKE_SOURCE_DIR}/src/frontend/prettyprint
    ${CMAKE_SOURCE_DIR}/src/frontend/iterators
    ${CMAKE_SOURCE_DIR}/src/semantic
    ${CMAKE_SOURCE_DIR}/src/semantic/symboltable
    ${CMAKE_SOURCE_DIR}/src/semantic/types
    ${CMAKE_SOURCE_DIR}/src/semantic/types/concrete
    ${CMAKE_SOURCE_DIR}/src/semantic/types/constraints
    ${CMAKE_SOURCE_DIR}/src/semantic/types/solver
    ${CMAKE_SOURCE_DIR}/src/semantic/solver/term
    ${CMAKE_SOURCE_DIR}/src/semantic/solver/cubic
    ${CMAKE_SOURCE_DIR}/src/semantic/cfa
    ${CMAKE_SOURCE_DIR}/src/semantic/weeding
    ${CMAKE_SOURCE_DIR}/test/unit/helpers
    ${CMAKE_SOURCE_DIR}/test/unit/matchers)

add_subdirectory(helpers)
add_subdirectory(codegen)
add_subdirectory(frontend)
add_subdirectory(semantic)
add_subdirectory(solver)

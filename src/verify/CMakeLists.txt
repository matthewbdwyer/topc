# VERUTOP: deductive verification for TOP.  One module per stage; see
# VERUTOP.md, "Architecture".
add_library(verify)
target_sources(
  verify
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Driver.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Encoding.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Feedback.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Gcl.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Lowering.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Obligations.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Outline.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Passivize.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/SmtEncode.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Specs.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Term.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/TermPrint.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Termination.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/VcStrategy.cpp)
target_include_directories(
  verify
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_SOURCE_DIR}/src/error
          ${CMAKE_SOURCE_DIR}/src/frontend
          ${CMAKE_SOURCE_DIR}/src/frontend/ast
          ${CMAKE_SOURCE_DIR}/src/frontend/ast/treetypes
          ${CMAKE_SOURCE_DIR}/src/frontend/prettyprint
          ${CMAKE_SOURCE_DIR}/src/semantic
          ${CMAKE_SOURCE_DIR}/src/semantic/ownership
          ${CMAKE_SOURCE_DIR}/src/semantic/symboltable
          ${CMAKE_SOURCE_DIR}/src/semantic/cfa
          ${CMAKE_SOURCE_DIR}/src/semantic/solver/cubic
          ${CMAKE_SOURCE_DIR}/src/semantic/types
          ${CMAKE_SOURCE_DIR}/src/semantic/types/concrete
          ${CMAKE_SOURCE_DIR}/src/semantic/types/constraints
          ${CMAKE_SOURCE_DIR}/src/semantic/types/solver
          ${CMAKE_SOURCE_DIR}/src/semantic/solver/term
          ${CMAKE_SOURCE_DIR}/src/semantic/weeding)
target_link_libraries(verify PRIVATE semantic error coverage_config loguru)

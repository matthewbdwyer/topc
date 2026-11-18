# VERUTOP: deductive verification for TOP.  One library; the directories
# follow the pipeline, and each uses only those listed before it:
#
#   logic/   formulas and queries (nothing of TOP)
#   ir/      guarded commands, obligations, the passive form
#   lower/   AST to IR: the encoding, lowering, spec functions, termination
#   vc/      IR to verification conditions to queries
#   solver/  the back end
#   report/  answers to results to text
#
# Driver.cpp wires them.  See VERUTOP.md, "Architecture".
add_library(verify)
target_sources(
  verify
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Driver.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/logic/Query.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/logic/Term.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/logic/TermPrint.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/ir/Gcl.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/ir/Obligations.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/ir/Passivize.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/Encoding.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/LowerCall.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/LowerExpr.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/LowerStmt.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/Lowering.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/Specs.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/lower/Termination.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/vc/Outline.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/vc/SmtEncode.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/vc/VcStrategy.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/solver/Solver.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/report/Feedback.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/report/Report.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/report/Trust.cpp)
target_include_directories(
  verify
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
         ${CMAKE_CURRENT_SOURCE_DIR}/logic
         ${CMAKE_CURRENT_SOURCE_DIR}/ir
         ${CMAKE_CURRENT_SOURCE_DIR}/lower
         ${CMAKE_CURRENT_SOURCE_DIR}/vc
         ${CMAKE_CURRENT_SOURCE_DIR}/solver
         ${CMAKE_CURRENT_SOURCE_DIR}/report
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

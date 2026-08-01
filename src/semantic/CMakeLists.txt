# Build the different semantic analysis passes
add_subdirectory(weeding)
add_subdirectory(symboltable)
add_subdirectory(solver)
add_subdirectory(types)
add_subdirectory(cfa)

# Define a library for all semantic analyses including the underlying passes
add_library(semantic)
target_sources(
  semantic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SemanticAnalysis.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/SemanticAnalysis.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/BorrowChecker.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/BorrowChecker.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGTypes.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/BasicBlock.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/BasicBlock.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/ControlFlowGraph.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/ControlFlowGraph.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/IntraproceduralCFGs.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/IntraproceduralCFGs.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGBuilder.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGBuilder.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGRenderer.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGRenderer.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/DestructionPass.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/DestructionPass.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/OwnershipClassifier.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/OwnershipClassifier.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/FunctionEffectSummaries.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/FunctionEffectSummaries.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/MoveAnalysis.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/ownership/MoveAnalysis.cpp)
target_include_directories(
  semantic
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/ownership
          ${CMAKE_CURRENT_SOURCE_DIR}/symboltable
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg
          ${CMAKE_CURRENT_SOURCE_DIR}/cfa
          ${CMAKE_CURRENT_SOURCE_DIR}/types
          ${CMAKE_CURRENT_SOURCE_DIR}/types/concrete
          ${CMAKE_CURRENT_SOURCE_DIR}/types/constraints
          ${CMAKE_CURRENT_SOURCE_DIR}/types/solver
          ${CMAKE_SOURCE_DIR}/src/semantic/solver/term
          ${CMAKE_CURRENT_SOURCE_DIR}/weeding
          ${CMAKE_SOURCE_DIR}/src/frontend/ast
          ${CMAKE_SOURCE_DIR}/src/frontend/ast/treetypes)
target_link_libraries(
  semantic
  PRIVATE cfa
          ast
          weeding
          symboltable
          types
          coverage_config
          loguru)

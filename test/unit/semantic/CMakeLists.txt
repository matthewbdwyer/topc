add_subdirectory(types)
add_subdirectory(cfa)

add_executable(semantic_unit_tests)
target_sources(
  semantic_unit_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SymbolTableTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/LocalNameCollectorTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/CheckAssignableTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/SumTypeWeedingTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/OwnershipClassifierTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/MoveAnalysisTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/BorrowCheckerTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/SemanticAnalysisCFGIntegrationTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg/BasicBlockTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg/ControlFlowGraphTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGBuilderTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/cfg/CFGRendererTest.cpp)
target_link_libraries(
  semantic_unit_tests
  PRIVATE ast
          topc_codegen
          symboltable
          weeding
          error
          types
          semantic
          test_helpers
          topc_test_includes
          cfa
          coverage_config
          Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(semantic_unit_tests)

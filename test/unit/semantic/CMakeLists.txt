add_subdirectory(types)
add_subdirectory(cfa)

add_executable(semantic_unit_tests)
target_sources(
  semantic_unit_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SymbolTableTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/LocalNameCollectorTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/CheckAssignableTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/Phase4WeedingTest.cpp)
target_link_libraries(
  semantic_unit_tests
  PRIVATE ast
          codegen
          symboltable
          weeding
          error
          test_helpers
          tipc_test_includes
          cfa
          coverage_config
          Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(semantic_unit_tests)

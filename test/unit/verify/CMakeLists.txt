# Unit tests for VERUTOP.  None of them runs Z3.
add_executable(verify_unit_tests)
target_sources(
  verify_unit_tests
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/TermTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/TermPrintTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/DriverTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/EncodingTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/GclTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/LoweringTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/ObligationsTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/PassivizeTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/QueryTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/SolverTest.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/VcStrategyTest.cpp)
target_link_libraries(
  verify_unit_tests
  PRIVATE verify
          ast
          symboltable
          weeding
          error
          types
          semantic
          test_helpers
          topc_test_includes
          coverage_config
          Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(verify_unit_tests TEST_PREFIX "verify.")

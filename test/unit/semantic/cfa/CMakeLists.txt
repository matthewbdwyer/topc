add_executable(call_graph_unit_tests)
target_sources(call_graph_unit_tests
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CallGraphTest.cpp)
target_link_libraries(
  call_graph_unit_tests
  PRIVATE antlr4_static
          ${llvm_libs}
          ast
          types
          symboltable
          frontend
          semantic
          codegen
          optimizer
          error
          test_helpers
          tipc_test_includes
          coverage_config
          cfa
          Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(call_graph_unit_tests)

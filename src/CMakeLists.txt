# compiler binary target
add_executable(topc)
target_sources(topc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tipc.cpp)

# Build the phase specific libraries
add_subdirectory(error)
add_subdirectory(frontend)
add_subdirectory(semantic)
add_subdirectory(codegen)
add_subdirectory(optimizer)

target_link_libraries(
  topc
  PRIVATE error
          frontend
          semantic
          codegen
          optimizer
          antlr4_static
          ${llvm_libs}
          coverage_config
          loguru)
target_include_directories(
  topc
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/error
          ${CMAKE_CURRENT_SOURCE_DIR}/frontend
          ${CMAKE_CURRENT_SOURCE_DIR}/frontend/ast
          ${CMAKE_CURRENT_SOURCE_DIR}/frontend/ast/treetypes
          ${CMAKE_CURRENT_SOURCE_DIR}/frontend/prettyprint
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/cfa
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/solver/cubic
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/symboltable
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/types
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/types/concrete
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/types/constraints
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/types/solver
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/solver/term
          ${CMAKE_CURRENT_SOURCE_DIR}/semantic/weeding
          ${CMAKE_CURRENT_SOURCE_DIR}/codegen
          ${CMAKE_CURRENT_SOURCE_DIR}/optimizer)

# Create a 'tipc' symlink alongside the 'topc' binary for backward compatibility.
add_custom_command(
  TARGET topc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          $<TARGET_FILE_NAME:topc>
          $<TARGET_FILE_DIR:topc>/tipc
  COMMENT "Creating tipc -> topc symlink")

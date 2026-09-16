add_library(error)
target_sources(
  error
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Error.h
          ${CMAKE_CURRENT_SOURCE_DIR}/Error.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/ParseError.h
          ${CMAKE_CURRENT_SOURCE_DIR}/SemanticError.h
          ${CMAKE_CURRENT_SOURCE_DIR}/InternalError.h
          ${CMAKE_CURRENT_SOURCE_DIR}/RuleToggles.h
          ${CMAKE_CURRENT_SOURCE_DIR}/RuleToggles.cpp)
target_include_directories(error PRIVATE)
target_link_libraries(error PRIVATE coverage_config)

add_library(weeding)
target_sources(weeding PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CheckAssignable.cpp
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckAssignable.h
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckBorrowPositions.cpp
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckBorrowPositions.h
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckCaseCompleteness.cpp
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckCaseCompleteness.h
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckSumTypeNames.cpp
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckSumTypeNames.h
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckPatternTypes.cpp
                               ${CMAKE_CURRENT_SOURCE_DIR}/CheckPatternTypes.h)
target_include_directories(
  weeding
  PRIVATE ${CMAKE_SOURCE_DIR}/src/frontend/ast
          ${CMAKE_SOURCE_DIR}/src/frontend/ast/treetypes
          ${CMAKE_SOURCE_DIR}/src/frontend/prettyprint
          ${CMAKE_SOURCE_DIR}/src/error)
target_link_libraries(weeding PRIVATE coverage_config)

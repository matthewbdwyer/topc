set(RTLIB_BC ${CMAKE_CURRENT_BINARY_DIR}/top_rtlib.bc)

add_custom_command(
  OUTPUT  ${RTLIB_BC}
  COMMAND ${LLVM_TOOLS_BINARY_DIR}/clang -c -emit-llvm
          -o ${RTLIB_BC}
          ${CMAKE_CURRENT_SOURCE_DIR}/top_rtlib.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/top_rtlib.c
  COMMENT "Compiling TOP runtime library to bitcode")

add_custom_target(top_rtlib ALL DEPENDS ${RTLIB_BC})

set(RTLIB_BC ${CMAKE_CURRENT_BINARY_DIR}/top_rtlib.bc)

# On macOS, Homebrew's clang does not probe xcrun/SDKROOT at runtime; it uses a
# built-in default SDK path that can vanish after an OS update, breaking this
# raw clang invocation. Pass the same sysroot CMake selected for the rest of
# the build so the runtime-library bitcode compile does not rely on that
# default.
set(RTLIB_SYSROOT_FLAGS)
if(APPLE)
  set(RTLIB_MACOS_SDK "${CMAKE_OSX_SYSROOT}")
  if(NOT RTLIB_MACOS_SDK)
    execute_process(
      COMMAND xcrun --show-sdk-path
      OUTPUT_VARIABLE RTLIB_MACOS_SDK
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  if(RTLIB_MACOS_SDK)
    set(RTLIB_SYSROOT_FLAGS -isysroot ${RTLIB_MACOS_SDK})
  endif()
endif()

add_custom_command(
  OUTPUT  ${RTLIB_BC}
  COMMAND ${LLVM_TOOLS_BINARY_DIR}/clang -c -emit-llvm
          ${RTLIB_SYSROOT_FLAGS}
          -o ${RTLIB_BC}
          ${CMAKE_CURRENT_SOURCE_DIR}/top_rtlib.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/top_rtlib.c
  COMMENT "Compiling TOP runtime library to bitcode")

add_custom_target(top_rtlib ALL DEPENDS ${RTLIB_BC})

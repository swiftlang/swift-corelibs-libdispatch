include_guard()

# Returns the architecture name in a variable
#
# Usage:
#   get_swift_host_arch(result_var_name)
#
# Sets ${result_var_name} with the converted architecture name derived from
# CMAKE_SYSTEM_PROCESSOR or CMAKE_HOST_SYSTEM_PROCESSOR.
function(get_swift_host_arch result_var_name)
  if(CMAKE_SYSTEM_PROCESSOR)
    set(cmake_arch ${CMAKE_SYSTEM_PROCESSOR})
  else()
    set(cmake_arch ${CMAKE_HOST_SYSTEM_PROCESSOR})
  endif()
  if("${cmake_arch}" STREQUAL "x86_64")
    set("${result_var_name}" "x86_64" PARENT_SCOPE)
  elseif(cmake_arch MATCHES "aarch64|ARM64|arm64")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR
        "${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
      set("${result_var_name}" "aarch64" PARENT_SCOPE)
    else()
      set("${result_var_name}" "arm64" PARENT_SCOPE)
    endif()
  elseif("${cmake_arch}" STREQUAL "ppc64")
    set("${result_var_name}" "powerpc64" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "ppc64le")
    set("${result_var_name}" "powerpc64le" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "s390x")
    set("${result_var_name}" "s390x" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "armv6l")
    set("${result_var_name}" "armv6" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "armv7-a")
    set("${result_var_name}" "armv7" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "armv7l")
    set("${result_var_name}" "armv7" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "amd64")
    set("${result_var_name}" "amd64" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "AMD64")
    set("${result_var_name}" "x86_64" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "IA64")
    set("${result_var_name}" "itanium" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "x86")
    set("${result_var_name}" "i686" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "i686")
    set("${result_var_name}" "i686" PARENT_SCOPE)
  elseif("${cmake_arch}" STREQUAL "riscv64")
    set("${result_var_name}" "riscv64" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Unrecognized architecture: ${cmake_arch}")
  endif()
endfunction()

if(NOT dispatch_MODULE_TRIPLE)
  set(module_triple_command "${CMAKE_Swift_COMPILER}" -print-target-info)
  if(CMAKE_Swift_COMPILER_TARGET)
    list(APPEND module_triple_command -target ${CMAKE_Swift_COMPILER_TARGET})
  endif()
  execute_process(COMMAND ${module_triple_command} OUTPUT_VARIABLE target_info_json)

  string(JSON module_triple GET "${target_info_json}" "target" "moduleTriple")
  set(dispatch_MODULE_TRIPLE "${module_triple}" CACHE STRING "Triple used to install swiftmodule files")
  mark_as_advanced(dispatch_MODULE_TRIPLE)

  message(CONFIGURE_LOG "Swift module triple: ${module_triple}")
endif()

function(install_swift_module target)
  get_target_property(module ${target} Swift_MODULE_NAME)
  if(NOT module)
    set(module ${target})
  endif()

  if(NOT SWIFT_SYSTEM_NAME)
    if(APPLE)
      set(SWIFT_SYSTEM_NAME macosx)
    else()
      set(SWIFT_SYSTEM_NAME "$<LOWER_CASE:${CMAKE_SYSTEM_NAME}>")
    endif()
  endif()
  set(INSTALL_SWIFT_MODULE_DIR "${CMAKE_INSTALL_LIBDIR}/swift$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:_static>/${SWIFT_SYSTEM_NAME}" CACHE PATH "Path where the swift modules will be installed")

  install(
    FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module}.swiftdoc
    DESTINATION ${INSTALL_SWIFT_MODULE_DIR}/${module}.swiftmodule
    RENAME ${dispatch_MODULE_TRIPLE}.swiftdoc)
  install(
    FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module}.swiftmodule
    DESTINATION ${INSTALL_SWIFT_MODULE_DIR}/${module}.swiftmodule
    RENAME ${dispatch_MODULE_TRIPLE}.swiftmodule)
endfunction()

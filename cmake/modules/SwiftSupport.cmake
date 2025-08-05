include_guard()

if(NOT dispatch_Swift_MODULE_TRIPLE OR NOT dispatch_Swift_ARCH OR NOT dispatch_Swift_PLATFORM)
  # Get the target information from the Swift compiler.
  set(module_triple_command "${CMAKE_Swift_COMPILER}" -print-target-info)
  if(CMAKE_Swift_COMPILER_TARGET)
    list(APPEND module_triple_command -target ${CMAKE_Swift_COMPILER_TARGET})
  endif()
  execute_process(COMMAND ${module_triple_command} OUTPUT_VARIABLE target_info_json)
endif()

if(NOT dispatch_Swift_MODULE_TRIPLE)
  string(JSON module_triple GET "${target_info_json}" "target" "moduleTriple")
  set(dispatch_Swift_MODULE_TRIPLE "${module_triple}" CACHE STRING "Triple used to install swiftmodule files")
  mark_as_advanced(dispatch_Swift_MODULE_TRIPLE)
  message(CONFIGURE_LOG "Swift module triple: ${module_triple}")
endif()

if(NOT dispatch_Swift_ARCH)
  if(CMAKE_Swift_COMPILER_VERSION VERSION_EQUAL 0.0.0 OR CMAKE_Swift_COMPILER_VERSION VERSION_GREATER_EQUAL 6.2)
    # For newer compilers, we can use the -print-target-info command to get the architecture.
    string(JSON module_arch GET "${target_info_json}" "target" "arch")
  else()
    # For older compilers, extract the value from `dispatch_Swift_MODULE_TRIPLE`.
    string(REGEX MATCH "^[^-]+" module_arch "${dispatch_Swift_MODULE_TRIPLE}")
  endif()

  set(dispatch_Swift_ARCH "${module_arch}" CACHE STRING "Arch folder name used to install libraries")
  mark_as_advanced(dispatch_Swift_ARCH)
  message(CONFIGURE_LOG "Swift arch: ${dispatch_Swift_ARCH}")
endif()

if(NOT dispatch_Swift_PLATFORM)
  if(CMAKE_Swift_COMPILER_VERSION VERSION_EQUAL 0.0.0 OR CMAKE_Swift_COMPILER_VERSION VERSION_GREATER_EQUAL 6.2)
    # For newer compilers, we can use the -print-target-info command to get the platform.
    string(JSON swift_platform GET "${target_info_json}" "target" "platform")
  else()
    # For older compilers, compile the value from `CMAKE_SYSTEM_NAME`.
    if(APPLE)
      set(swift_platform macosx)
    else()
      set(swift_platform "$<LOWER_CASE:${CMAKE_SYSTEM_NAME}>")
    endif()
  endif()

  set(dispatch_Swift_PLATFORM "${swift_platform}" CACHE STRING "Platform folder name used to install libraries")
  mark_as_advanced(dispatch_Swift_PLATFORM)
  message(CONFIGURE_LOG "Swift platform: ${dispatch_Swift_PLATFORM}")
endif()

function(install_swift_module target)
  get_target_property(module ${target} Swift_MODULE_NAME)
  if(NOT module)
    set(module ${target})
  endif()

  set(INSTALL_SWIFT_MODULE_DIR "${CMAKE_INSTALL_LIBDIR}/swift$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:_static>/${dispatch_Swift_PLATFORM}" CACHE PATH "Path where the swift modules will be installed")

  install(
    FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module}.swiftdoc
    DESTINATION ${INSTALL_SWIFT_MODULE_DIR}/${module}.swiftmodule
    RENAME ${dispatch_Swift_MODULE_TRIPLE}.swiftdoc)
  install(
    FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module}.swiftmodule
    DESTINATION ${INSTALL_SWIFT_MODULE_DIR}/${module}.swiftmodule
    RENAME ${dispatch_Swift_MODULE_TRIPLE}.swiftmodule)
endfunction()

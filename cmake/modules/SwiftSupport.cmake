include_guard()

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
  install(
    FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module}.swiftdoc
    DESTINATION ${INSTALL_TARGET_DIR}/${module}.swiftmodule
    RENAME ${dispatch_MODULE_TRIPLE}.swiftdoc)
  install(
    FILES $<TARGET_PROPERTY:${target},Swift_MODULE_DIRECTORY>/${module}.swiftmodule
    DESTINATION ${INSTALL_TARGET_DIR}/${module}.swiftmodule
    RENAME ${dispatch_MODULE_TRIPLE}.swiftmodule)
endfunction()


set(DISPATCH_EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/dispatchExports.cmake)
configure_file(dispatchConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/dispatchConfig.cmake
  @ONLY)

if(LibRT_FOUND)
  file(COPY FindLibRT.cmake DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

get_property(DISPATCH_EXPORTS GLOBAL PROPERTY DISPATCH_EXPORTS)
export(TARGETS ${DISPATCH_EXPORTS} FILE ${DISPATCH_EXPORTS_FILE})

if(NOT ENABLE_SWIFT)
  install(EXPORT dispatchExports DESTINATION ${INSTALL_TARGET_DIR}/cmake/dispatch)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dispatchConfig.cmake
          DESTINATION ${INSTALL_TARGET_DIR}/cmake/dispatch)

  if(LibRT_FOUND)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FindLibRT.cmake
            DESTINATION ${INSTALL_TARGET_DIR}/cmake/dispatch)
  endif()
endif()


if(APPLE)
  set(DISPATCH_MODULE_MAP ${PROJECT_SOURCE_DIR}/dispatch/darwin/module.modulemap)
else()
  set(DISPATCH_MODULE_MAP ${PROJECT_SOURCE_DIR}/dispatch/generic/module.modulemap)
endif()
configure_file(dispatch-vfs.yaml.in
  ${CMAKE_BINARY_DIR}/dispatch-vfs-overlay.yaml
  @ONLY)

install(FILES
          base.h
          block.h
          data.h
          dispatch.h
          group.h
          introspection.h
          io.h
          object.h
          once.h
          queue.h
          semaphore.h
          source.h
          time.h
        DESTINATION
          "${INSTALL_DISPATCH_HEADERS_DIR}")
if(ENABLE_SWIFT)
  install(FILES
            ${DISPATCH_MODULE_MAP}
          DESTINATION
            "${INSTALL_DISPATCH_HEADERS_DIR}")
endif()


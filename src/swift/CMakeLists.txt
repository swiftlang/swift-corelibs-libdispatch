include(SwiftSupport)

if(HAVE_OBJC)
  add_library(DispatchStubs STATIC
    DispatchStubs.m)
  target_include_directories(DispatchStubs PRIVATE
    ${PROJECT_SOURCE_DIR})
endif()

add_library(swiftDispatch
  Block.swift
  Data.swift
  Dispatch.swift
  IO.swift
  Private.swift
  Queue.swift
  Source.swift
  Time.swift
  Wrapper.swift)
target_compile_options(swiftDispatch PRIVATE
  "SHELL:-Xcc -fblocks"
  "SHELL:-Xcc -fmodule-map-file=${PROJECT_SOURCE_DIR}/dispatch/module.modulemap"
  "SHELL:-Xcc -I${PROJECT_SOURCE_DIR}"
  "SHELL:-Xcc -I${PROJECT_SOURCE_DIR}/src/swift/shims")
target_compile_options(swiftDispatch PUBLIC
  "SHELL:-vfsoverlay ${CMAKE_BINARY_DIR}/dispatch-vfs-overlay.yaml")
set_target_properties(swiftDispatch PROPERTIES
  Swift_MODULE_NAME Dispatch
  Swift_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swift
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/swift)
target_link_libraries(swiftDispatch PRIVATE
  $<$<BOOL:${HAVE_OBJC}>:DispatchStubs>
  BlocksRuntime::BlocksRuntime)
target_link_libraries(swiftDispatch PUBLIC
  dispatch)
if(NOT APPLE AND NOT WIN32)
  target_link_options(swiftDispatch PRIVATE "SHELL:-no-toolchain-stdlib-rpath")
  set_target_properties(swiftDispatch PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

install_swift_module(swiftDispatch)
set_property(GLOBAL APPEND PROPERTY DISPATCH_EXPORTS swiftDispatch)
install(TARGETS swiftDispatch
  EXPORT dispatchExports
  ARCHIVE DESTINATION ${INSTALL_TARGET_DIR}
  LIBRARY DESTINATION ${INSTALL_TARGET_DIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(HAVE_OBJC AND NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY DISPATCH_EXPORTS DispatchStubs)
  install(TARGETS DispatchStubs
    EXPORT dispatchExports
    DESTINATION ${INSTALL_TARGET_DIR})
endif()

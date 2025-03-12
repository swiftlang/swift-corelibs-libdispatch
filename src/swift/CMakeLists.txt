add_library(DispatchStubs STATIC
  DispatchStubs.cc)
target_include_directories(DispatchStubs PRIVATE
  ${PROJECT_SOURCE_DIR})
set_target_properties(DispatchStubs PROPERTIES
  POSITION_INDEPENDENT_CODE YES)

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
  DispatchStubs
  BlocksRuntime::BlocksRuntime)
target_link_libraries(swiftDispatch PUBLIC
  dispatch)
if(NOT DARWIN AND NOT WIN32)
  target_link_options(swiftDispatch PRIVATE "SHELL:-no-toolchain-stdlib-rpath")
  set_target_properties(swiftDispatch PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

install(TARGETS swiftDispatch
  EXPORT dispatchExports
  ARCHIVE DESTINATION "${Dispatch_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${Dispatch_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
INSTALL(FILES $<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_DIRECTORY>/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftdoc
  DESTINATION ${Dispatch_INSTALL_SWIFTMODULEDIR}/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftmodule
  RENAME ${Dispatch_MODULE_TRIPLE}.swiftdoc)
# INSTALL(FILES $<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_DIRECTORY>/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftinterface
#   DESTINATION ${Dispatch_INSTALL_SWIFTMODULEDIR}/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftmodule
#   RENAME ${Dispach_MODULE_TRIPLE}.swiftinterface)
INSTALL(FILES $<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_DIRECTORY>/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftmodule
  DESTINATION ${Dispatch_INSTALL_SWIFTMODULEDIR}/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftmodule
  RENAME ${Dispatch_MODULE_TRIPLE}.swiftmodule)
INSTALL(FILES $<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_DIRECTORY>/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftsourceinfo
  DESTINATION ${Dispatch_INSTALL_SWIFTMODULEDIR}/$<TARGET_PROPERTY:swiftDispatch,Swift_MODULE_NAME>.swiftmodule
  RENAME ${Dispatch_MODULE_TRIPLE}.swiftsourceinfo)
if(NOT BUILD_SHARED_LIBS)
  set_property(GLOBAL APPEND PROPERTY DISPATCH_EXPORTS DispatchStubs)
  install(TARGETS DispatchStubs
    EXPORT dispatchExports
    DESTINATION ${Dispatch_INSTALL_LIBDIR})
endif()

set_property(GLOBAL APPEND PROPERTY DISPATCH_EXPORTS swiftDispatch)

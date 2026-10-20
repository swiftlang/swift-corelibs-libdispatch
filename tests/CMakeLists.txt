if(WASI)
  # The command that runs the .wasm test binaries. It can be a CMake list,
  # for example "wasmtime;--some-flag". The runner must return the exit code
  # of the guest. The default is the WasmKit that ships with the Swift
  # toolchain.
  get_filename_component(_wasi_toolchain_bin "${CMAKE_C_COMPILER}" DIRECTORY)
  find_program(WASMKIT_EXECUTABLE wasmkit HINTS "${_wasi_toolchain_bin}")
  if(WASMKIT_EXECUTABLE)
    set(_wasi_default_runner "${WASMKIT_EXECUTABLE};run")
  else()
    set(_wasi_default_runner "wasmkit;run")
  endif()
  set(WASI_TEST_RUNNER "${_wasi_default_runner}" CACHE STRING
      "Command (semicolon-separated list) used to execute WASI test binaries")
  list(GET WASI_TEST_RUNNER 0 _wasi_test_runner_program)
  # Not cached, so a changed WASI_TEST_RUNNER is looked up again.
  find_program(_wasi_test_runner_executable "${_wasi_test_runner_program}"
               NO_CACHE)
  if(NOT _wasi_test_runner_executable)
    message(STATUS
      "WASI test runner '${_wasi_test_runner_program}' not found; "
      "WASI unit tests will be registered but disabled")
  endif()
endif()

if(WIN32)
    execute_process(COMMAND
                      "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/private"
                      "${CMAKE_CURRENT_BINARY_DIR}/dispatch")
    execute_process(COMMAND
                      "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/leaks-wrapper.sh"
                      "${CMAKE_CURRENT_BINARY_DIR}/leaks-wrapper")
else()
    execute_process(COMMAND
                      "${CMAKE_COMMAND}" -E create_symlink "${PROJECT_SOURCE_DIR}/private"
                      "${CMAKE_CURRENT_BINARY_DIR}/dispatch")
    execute_process(COMMAND
                      "${CMAKE_COMMAND}" -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/leaks-wrapper.sh"
                      "${CMAKE_CURRENT_BINARY_DIR}/leaks-wrapper")
endif()

if(LINUX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lrt")
endif()

add_library(bsdtests
            STATIC
              bsdtests.c
              dispatch_test.c)
target_link_libraries(bsdtests PUBLIC
  dispatch)
if(WASI)
  # The harness logs the process id. libdispatch links the getpid emulation.
  target_compile_definitions(bsdtests PRIVATE _WASI_EMULATED_GETPID)
endif()
target_include_directories(bsdtests
                           PRIVATE
                             ${CMAKE_CURRENT_BINARY_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}
                             ${PROJECT_SOURCE_DIR}
                           PUBLIC
                             # bsdtests.h needs config_ac.h
                             ${PROJECT_BINARY_DIR})
if (WIN32)
  target_sources(bsdtests
                 PRIVATE
                   generic_win_port.c)
  target_compile_definitions(bsdtests
                             PUBLIC
                               _CRT_NONSTDC_NO_WARNINGS
                               _CRT_SECURE_NO_WARNINGS
                               _USE_MATH_DEFINES)
  target_link_libraries(bsdtests
                        PUBLIC
                          bcrypt)
endif ()

# bsdtestharness needs posix_spawn, which WASI does not have. So
# ${WASI_TEST_RUNNER} runs the WASI unit tests directly.
if(NOT WASI)
  add_executable(bsdtestharness
                 bsdtestharness.c)
  target_include_directories(bsdtestharness
                             PRIVATE
                               ${CMAKE_CURRENT_BINARY_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}
                               ${PROJECT_SOURCE_DIR})
  target_link_libraries(bsdtestharness
                        PRIVATE
                          bsdtests
                          dispatch)
endif()

function(add_unit_test name)
  set(options DISABLED_TEST)
  set(single_value_args)
  set(multiple_value_args SOURCES)
  cmake_parse_arguments(AUT "${options}" "${single_value_args}" "${multiple_value_args}" ${ARGN})

  if(AUT_DISABLED_TEST)
    return()
  endif()

  add_executable(${name} ${AUT_SOURCES})
  target_include_directories(${name}
                             PRIVATE
                               ${CMAKE_CURRENT_BINARY_DIR}
                               ${CMAKE_CURRENT_SOURCE_DIR}
                               ${PROJECT_SOURCE_DIR})
  if(ENABLE_SWIFT)
    # For testing in swift.org CI system; make deadlines lenient by default
    # to reduce probability of test failures due to machine load.
    target_compile_options(${name} PRIVATE -DLENIENT_DEADLINES=1)
  endif()
  target_include_directories(${name}
                             SYSTEM BEFORE PRIVATE
                               "${BlocksRuntime_INCLUDE_DIR}")
  if("${CMAKE_C_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
    target_compile_options(${name} PRIVATE -Xclang -fblocks)
    target_compile_options(${name} PRIVATE /W3 -Wno-deprecated-declarations)
  else()
    target_compile_options(${name} PRIVATE -fblocks)
    target_compile_options(${name} PRIVATE -Wall -Wno-deprecated-declarations)
  endif()
  # Without this flag, cross-compiling static test executables for Android armv7
  # fails with the multiple definition errors seen in android/ndk#176, so I
  # pulled in this workaround noted there. The tests build and run with this
  # flag applied.
  if(NOT BUILD_SHARED_LIBS AND ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL armv7-a)
    target_link_options(${name} PRIVATE "LINKER:--allow-multiple-definition")
  endif()
  target_link_libraries(${name}
                        PRIVATE
                          dispatch
                          Threads::Threads
                          BlocksRuntime::BlocksRuntime)
  target_link_libraries(${name} PRIVATE bsdtests)
  if(WASI)
    add_test(NAME ${name}
             COMMAND ${WASI_TEST_RUNNER} $<TARGET_FILE:${name}>)
    set_tests_properties(${name}
                         PROPERTIES
                           TIMEOUT 120
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    if(NOT _wasi_test_runner_executable)
      set_tests_properties(${name} PROPERTIES DISABLED TRUE)
    endif()
  else()
    add_test(NAME ${name}
             COMMAND bsdtestharness $<TARGET_FILE:${name}>)
    set_tests_properties(${name}
                         PROPERTIES
                           TIMEOUT 120
                           DEPENDS bsdtestharness
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  if(NOT leaks_EXECUTABLE)
    set_tests_properties(${name}
                         PROPERTIES
                           ENVIRONMENT NOLEAKS=1)
  endif()
endfunction()

if(WASI)
  # The subset of the default C test suite that runs on single-threaded
  # WASI. The other default tests need more than one thread:
  #   apply: keeps worker threads spinning while dispatch_apply runs
  #   overcommit: spins (while(1)) in every work item but the last, so it
  #       needs concurrent worker threads to reach the item that stops it
  #   context_for_key, sema, timer_short: block in a wait for work that a
  #       worker thread would run. On one thread a wait runs no work, so the
  #       port rejects that as a deadlock.
  #   timer_timeout: blocks the sole thread in sleep() and expects timers
  #       to fire on worker threads meanwhile
  #   io_muxed, io_net, io_pipe, io_pipe_close, select: single-threaded
  #       WASI does not support file-descriptor dispatch sources
  # The wasi_* tests cover what single-threaded WASI adds: a wait that needs
  # queued work crashes, dispatch_async never runs its block inline,
  # dispatch_main() runs work and crashes when idle.
  set(DISPATCH_C_TESTS
      api
      debug
      queue_finalizer
      after
      timer
      timer_bit31
      timer_bit63
      timer_set_time
      data
      wasi_blocking_waits
      wasi_nested_wait
      wasi_timed_wait
      wasi_dispatch_main_work
      wasi_dispatch_main_idle
      wasi_deferred_submission
      wasi_wall_timer
      wasi_serial_lane_fairness
      wasi_wait_diagnostics)
else()
# Tests that reliably pass on all platforms
set(DISPATCH_C_TESTS
    apply
    api
    debug
    queue_finalizer
    overcommit
    context_for_key
    after
    timer
    timer_short
    timer_timeout
    sema
    timer_bit31
    timer_bit63
    timer_set_time
    data
    io_muxed
    io_net
    io_pipe
    io_pipe_close
    select)
endif()

if(UNIX)
  list(APPEND DISPATCH_C_TESTS timer_idle)
endif()

# Tests that usually pass, but occasionally fail.
# Excluded by default for purposes of Swift CI
# The extended tests need worker threads, pipes, or sysctl, so WASI skips them.
if(EXTENDED_TEST_SUITE AND NOT WASI)
  # When dispatch_group is reenabled here, also remove the if(EXTENDED_TEST_SUITE) condition below
  list(APPEND DISPATCH_C_TESTS
       priority
       concur
       group
       read
       read2
       starfish
       suspend_timer
       pingpong
       drift
       readsync
       cascade
       io)
  # an oddball; dispatch_priority.c compiled with -DUSE_SET_TARGET_QUEUE=1
  add_unit_test(dispatch_priority2 SOURCES dispatch_priority.c)
  target_compile_options(dispatch_priority2 PRIVATE -DUSE_SET_TARGET_QUEUE=1)
endif()

# add C tests for platform-specific functionality when applicable
if(UNIX AND (HAVE_PTHREAD_SETNAME_NP OR HAVE_PTHREAD_SET_NAME_NP) AND (HAVE_PTHREAD_GETNAME_NP OR HAVE_PTHREAD_GET_NAME_NP))
  list(APPEND DISPATCH_C_TESTS
       thread_name)
endif()
# EPOLLHUP handling (SR-9033); kevent platforms get this via EV_EOF
if(LINUX)
  list(APPEND DISPATCH_C_TESTS
       read_hup)
endif()
if(APPLE)
  list(APPEND DISPATCH_C_TESTS
       deadname
       proc
       vm
       vnode)
endif()

foreach(test ${DISPATCH_C_TESTS})
  add_unit_test(dispatch_${test}
                SOURCES
                  dispatch_${test}.c)
endforeach()

if(WASI)
  # These tests pass when the guest crashes with the expected diagnostic.
  # The exit status is ignored and the output must contain the message.
  set_tests_properties(dispatch_wasi_nested_wait PROPERTIES
    PASS_REGULAR_EXPRESSION "Deadlock: semaphore wait on single-threaded WASI")
  set_tests_properties(dispatch_wasi_dispatch_main_idle PROPERTIES
    PASS_REGULAR_EXPRESSION "dispatch_main\\(\\) with no work or timer")
  set_tests_properties(dispatch_wasi_wait_diagnostics PROPERTIES
    PASS_REGULAR_EXPRESSION
      "Deadlock: dispatch_sync or dispatch_async_and_wait on single-threaded WASI")
  add_unit_test(dispatch_wasi_wait_diagnostics_top_level
                SOURCES
                  dispatch_wasi_wait_diagnostics.c)
  target_compile_definitions(dispatch_wasi_wait_diagnostics_top_level PRIVATE
    WAIT_DIAGNOSTICS_TOP_LEVEL=1)
  set_tests_properties(dispatch_wasi_wait_diagnostics_top_level PROPERTIES
    PASS_REGULAR_EXPRESSION
      "Deadlock: semaphore wait on single-threaded WASI")
endif()

if(NOT WASI)
  set_tests_properties(dispatch_io_pipe PROPERTIES TIMEOUT 15)
  set_tests_properties(dispatch_io_pipe_close PROPERTIES TIMEOUT 5)
endif()
if(LINUX)
  set_tests_properties(dispatch_read_hup PROPERTIES TIMEOUT 60)
endif()

# test dispatch API for various C/CXX language variants
add_unit_test(dispatch_c99 SOURCES dispatch_c99.c)
add_unit_test(dispatch_plusplus SOURCES dispatch_plusplus.cpp)

if (DISPATCH_USE_INTERNAL_WORKQUEUE AND NOT WASI)
  add_unit_test(dispatch_workqueue
                SOURCES
                  dispatch_workqueue.c)
endif()

# test-specific link options
if(WIN32)
  target_link_libraries(dispatch_io_muxed PRIVATE WS2_32)
  target_link_libraries(dispatch_io_net PRIVATE WS2_32)
elseif(NOT WASI)
  # When dispatch_group is reenabled above, remove this
  if(EXTENDED_TEST_SUITE)
    target_link_libraries(dispatch_group PRIVATE m)
  endif()
  target_link_libraries(dispatch_timer_short PRIVATE m)
endif()

# test-specific compile options
set_target_properties(dispatch_c99 PROPERTIES C_STANDARD 99)

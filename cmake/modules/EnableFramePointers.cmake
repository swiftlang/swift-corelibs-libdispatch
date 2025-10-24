#
# Including this file enables frame pointers, if we know how.
#

include(CheckCompilerFlag)

# Check if the compiler supports -fno-omit-frame-pointer
check_compiler_flag(C -fno-omit-frame-pointer SUPPORTS_NO_OMIT_FP)

# If it does, use it
if (SUPPORTS_NO_OMIT_FP)
  add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-fno-omit-frame-pointer>)
endif()

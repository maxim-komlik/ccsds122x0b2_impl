# configuring by compiler id doesn't work really
# 
# set(CMAKE_CXX_COMPILER_ID GNU)
# set(CMAKE_C_COMPILER_ID GNU)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_C_COMPILER gcc)

# set(COMPILER_BIGOBJ_FLAG "-mbig-obj") # flag is invalid for GCC as is, should be passed to assembly compiler?
# and yet not clear if big objects flag is even needed for gcc

set(COMPILER_BIGOBJ_FLAG "")

add_compile_options("-Wno-error=template-body" "-Wno-error=narrowing" "-Wno-error=changes-meaning")

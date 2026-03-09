# configuring by compiler id doesn't work really
# 
# set(CMAKE_CXX_COMPILER_ID Clang)
# set(CMAKE_C_COMPILER_ID Clang)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

set(COMPILER_BIGOBJ_FLAG "") # clang enables big object files silently?

add_compile_options(-stdlib=libc++ -pedantic-errors)
add_compile_options(-Wno-error=c++11-narrowing -Wno-c++11-narrowing) # disable warnings temporarily

add_link_options(-stdlib=libc++ -static-libstdc++)

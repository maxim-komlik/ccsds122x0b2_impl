# configuring by compiler id doesn't work really
# 
# set(CMAKE_CXX_COMPILER_ID MSVC)
# set(CMAKE_C_COMPILER_ID MSVC)

set(CMAKE_CXX_COMPILER cl)
set(CMAKE_C_COMPILER cl)

set(COMPILER_BIGOBJ_FLAG "/bigobj")

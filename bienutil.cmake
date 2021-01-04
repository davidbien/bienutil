# bienutil.cmake
# Standard cmake setup for modules using bienutil, etc.

# Code must run on C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_definitions( 
    $<$<BOOL:WIN32>:_CRT_SECURE_NO_WARNINGS>
)

#setup the root directory for the development environment under Windows.
if (WIN32)
    set(DEVENV_ROOT_DIRECTORY "c:/devenv" )
    message(STATUS "Dev root dir ${DEVENV_ROOT_DIRECTORY}")
endif (WIN32)

#setup our development enviroment include directories which may be platform dependent.
include_directories( BEFORE SYSTEM
if (WIN32)
  ${DEVENV_ROOT_DIRECTORY}/icu4c/include
endif (WIN32)
)

# Libraries that almost all users of bienutil need:
link_libraries(Rpcrt4)

# bienutil.cmake
# Standard cmake setup for modules using bienutil, etc.

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# Code must run on C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (WIN32)
  add_compile_definitions( 
      _CRT_SECURE_NO_WARNINGS
  )

  set(DEVENV_ROOT_DIRECTORY "c:/devenv" )
  message(STATUS "Dev root dir ${DEVENV_ROOT_DIRECTORY}")

  include_directories( BEFORE SYSTEM
    ${DEVENV_ROOT_DIRECTORY}/icu4c/include
  )
  link_libraries(
    Rpcrt4
  )
endif (WIN32)

if (LINUX)
link_libraries(
  uuid
)
endif(LINUX)

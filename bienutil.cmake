# bienutil.cmake
# Standard cmake setup for modules using bienutil, etc.

if(MSVC)
set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELWITHDEBINFO
        )
foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    set(${CompilerFlag} "${${CompilerFlag}} /bigobj" CACHE STRING "msvc compiler flags" FORCE)
    message("MSVC flags: ${CompilerFlag}:${${CompilerFlag}}")
endforeach()
endif(MSVC)

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
    ${DEVENV_ROOT_DIRECTORY}/icu4c/lib64/icuuc.lib
  )
endif (WIN32)

if (LINUX)
link_libraries(
  uuid
  icuuc
)
endif(LINUX)

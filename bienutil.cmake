# bienutil.cmake
# Standard cmake setup for modules using bienutil, etc.

#          Copyright David Lawrence Bien 1997 - 2021.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          https://www.boost.org/LICENSE_1_0.txt).


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(COMPILER_CLANG TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_GCC TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  set(COMPILER_INTEL TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(COMPILER_MSVC TRUE)
endif()

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
if ( MOD_USE_WINDOWS_UCRT_DLL EQUAL 1 )
    string(REPLACE "/MT" "/MD" ${CompilerFlag} "${${CompilerFlag}}")
else()
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endif()
    string(REPLACE "/bigobj" "" ${CompilerFlag} "${${CompilerFlag}}")
    set(${CompilerFlag} "${${CompilerFlag}} /bigobj" CACHE STRING "msvc compiler flags" FORCE)
    message("MSVC flags: ${CompilerFlag}:${${CompilerFlag}}")
endforeach()
endif(MSVC)

if (UNIX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
if (NOT APPLE)
  message("Compiling under LINUX")
  set(LINUX TRUE)
else()
  message("Compiling under MacOS")
  endif()
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

  link_libraries(
    Rpcrt4
  )
# ICU4C support:
if ( MOD_USE_ICU4C EQUAL 1 )
  set(DEVENV_ICU4C_VERSION "70" )
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(DEVENV_ICU4C_DIRECTORY ${DEVENV_ROOT_DIRECTORY}/icu4c64 )
  set(DEVENV_ICU4C_BIN_DIRECTORY ${DEVENV_ICU4C_DIRECTORY}/bin64 )
  include_directories( BEFORE SYSTEM
    ${DEVENV_ICU4C_DIRECTORY}/include
  )
  link_libraries(
    ${DEVENV_ICU4C_DIRECTORY}/lib64/icuuc.lib
  )
else()
  set(DEVENV_ICU4C_DIRECTORY ${DEVENV_ROOT_DIRECTORY}/icu4c32 )
  set(DEVENV_ICU4C_BIN_DIRECTORY ${DEVENV_ICU4C_DIRECTORY}/bin )
  include_directories( BEFORE SYSTEM
    ${DEVENV_ICU4C_DIRECTORY}/include
  )
  link_libraries(
    ${DEVENV_ICU4C_DIRECTORY}/lib/icuuc.lib
  )
endif()
set(DEVENV_ICU4C_SRC_DLLS icudt${DEVENV_ICU4C_VERSION}.dll icuuc${DEVENV_ICU4C_VERSION}.dll )
# since we are using ICU4C for these unit tests then copy the appropriate DLL(s) to the output directory:
foreach( DEVENV_ICU4C_DLL ${DEVENV_ICU4C_SRC_DLLS} )
message(STATUS "Copying file ${DEVENV_ICU4C_BIN_DIRECTORY}/${DEVENV_ICU4C_DLL} to ${DEVENV_ICU4C_DLL}")
configure_file( ${DEVENV_ICU4C_BIN_DIRECTORY}/${DEVENV_ICU4C_DLL} ${DEVENV_ICU4C_DLL} COPYONLY )
endforeach()
endif()
endif (WIN32)

if (APPLE)
# I used "brew install icu4c" which doesn't link the libraries into /usr/local directly since it might stomp on MacOS Xcode libraries somehow.
  set(MACOS_LOCAL_OPT "/usr/local/opt")
  include_directories( BEFORE SYSTEM
    ${MACOS_LOCAL_OPT}/icu4c/include
  )
  link_directories( 
    BEFORE ${MACOS_LOCAL_OPT}/icu4c/lib
  )
endif(APPLE)

if (UNIX)
# Shared between Linux and MacOS.
link_libraries(
  icuuc
)
endif(UNIX)

if (LINUX)
# Linux only
link_libraries(
  uuid
)
endif(LINUX)

if ( MOD_USE_VULKAN EQUAL 1 )

#if (WIN32)
#set( Vulkan_LIBRARY C:/devenv/VulkanSDK/CurrentVersion)
#set( Vulkan_INCLUDE C:/devenv/VulkanSDK/CurrentVersion/vulkan)
#endif (WIN32)
#if (LINUX)
#set( Vulkan_INCLUDE /usr/include/vulkan)
#endif (LINUX)
find_package(Vulkan COMPONENTS glslc REQUIRED)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

# If we have a vk_layer_settings.txt in the source directory then copy it to the build directory for usage.
if ( EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vk_layer_settings.txt )
message( "vk_layer_settings.txt exists")
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/vk_layer_settings.txt vk_layer_settings.txt COPYONLY )
endif ()

function(compile_shader target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "ENV;FORMAT" "SOURCES")
    foreach(source ${arg_SOURCES})
        add_custom_command(
            OUTPUT ${source}.${arg_FORMAT}
            DEPENDS ${source}
            DEPFILE ${source}.d
            COMMAND
                ${glslc_executable}
                $<$<BOOL:${arg_ENV}>:--target-env=${arg_ENV}>
                $<$<BOOL:${arg_FORMAT}>:-mfmt=${arg_FORMAT}>
                -MD -MF ${source}.d
                -o ${source}.${arg_FORMAT}
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${source}.${arg_FORMAT})
    endforeach()
endfunction()

function( fn_compile_model target model_compiler )
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "EXTENSION" "SOURCES")
    message( "arg_EXTENSION: ${arg_EXTENSION}" )
    message( "arg_SOURCES: ${arg_SOURCES}" )
    foreach(source ${arg_SOURCES})
        add_custom_command(
            OUTPUT ${source}.${arg_EXTENSION}
            DEPENDS ${source} ${CMAKE_CURRENT_BINARY_DIR}/${model_compiler}
            COMMAND
                ${model_compiler}
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
                ${source}.${arg_EXTENSION}
        )
        target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${source}.${arg_EXTENSION})
    endforeach()
endfunction()
endif()

if ( MOD_USE_GLFW EQUAL 1 )
if (WIN32)
# cmake can't seem to automatically find GLFW:
set ( glfw3_DIR "C:/Program Files (x86)/GLFW/lib/cmake/glfw3" )
endif(WIN32)
find_package( glfw3 3.3 REQUIRED )
endif()
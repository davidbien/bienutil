# bienutil.cmake
# Standard cmake setup for modules using bienutil, etc.

#          Copyright David Lawrence Bien 1997 - 2021.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          https://www.boost.org/LICENSE_1_0.txt).


if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(COMPILER_CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(COMPILER_GCC TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  set(COMPILER_INTEL TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
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
    if(MOD_USE_WINDOWS_UCRT_DLL EQUAL 1)
      string(REPLACE "/MT" "/MD" ${CompilerFlag} "${${CompilerFlag}}")
    else()
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endif()
    string(REPLACE "/bigobj" "" ${CompilerFlag} "${${CompilerFlag}}")
    string(REPLACE "/Zm2000" "" ${CompilerFlag} "${${CompilerFlag}}")
    #string(REPLACE "/Zm2" "" ${CompilerFlag} "${${CompilerFlag}}")
    set(${CompilerFlag} "${${CompilerFlag}} /bigobj" CACHE STRING "msvc compiler flags" FORCE)
    set(${CompilerFlag} "${${CompilerFlag}} /Zm2000" CACHE STRING "msvc compiler flags" FORCE)
    message("MSVC flags: ${CompilerFlag}:${${CompilerFlag}}")
  endforeach()
endif(MSVC)

if(UNIX)
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  if(NOT APPLE)
    message("Compiling under LINUX")
    set(LINUX TRUE)
    if(MOD_USE_ICU4C EQUAL 1)
      if(MOD_USE_VCPKG EQUAL 1)
        message("Using UNICODE ICU Library through VCPKG")
        find_package(ICU REQUIRED COMPONENTS dt uc)
      endif(MOD_USE_VCPKG EQUAL 1)
    endif(MOD_USE_ICU4C EQUAL 1)
  else()
    message("Compiling under MacOS")
  endif()
endif()

# Code must run on C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(WIN32)
  message("Compiling under Windows")
  add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
  )

  set(DEVENV_ROOT_DIRECTORY "c:/devenv")
  message(STATUS "Dev root dir ${DEVENV_ROOT_DIRECTORY}")

  link_libraries(
    Rpcrt4
  )
  # ICU4C support:
  if(MOD_USE_ICU4C EQUAL 1)
    if(MOD_USE_VCPKG EQUAL 1)
      message("Using UNICODE ICU Library through VCPKG")
      find_package(ICU REQUIRED COMPONENTS dt uc)
    else(MOD_USE_VCPKG)
      message("Using UNICODE ICU Library through DEVENV directory - the old way.")
      set(DEVENV_ICU4C_VERSION "69")
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DEVENV_ICU4C_DIRECTORY ${DEVENV_ROOT_DIRECTORY}/icu4c64)
        set(DEVENV_ICU4C_BIN_DIRECTORY ${DEVENV_ICU4C_DIRECTORY}/bin64)
        include_directories(BEFORE SYSTEM
          ${DEVENV_ICU4C_DIRECTORY}/include
        )
        link_libraries(
          ${DEVENV_ICU4C_DIRECTORY}/lib64/icuuc.lib
        )
      else(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DEVENV_ICU4C_DIRECTORY ${DEVENV_ROOT_DIRECTORY}/icu4c32)
        set(DEVENV_ICU4C_BIN_DIRECTORY ${DEVENV_ICU4C_DIRECTORY}/bin)
        include_directories(BEFORE SYSTEM
          ${DEVENV_ICU4C_DIRECTORY}/include
        )
        link_libraries(
          ${DEVENV_ICU4C_DIRECTORY}/lib/icuuc.lib
        )
      endif(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(DEVENV_ICU4C_SRC_DLLS icudt${DEVENV_ICU4C_VERSION}.dll icuuc${DEVENV_ICU4C_VERSION}.dll)
      # since we are using ICU4C for these unit tests then copy the appropriate DLL(s) to the output directory:
      foreach(DEVENV_ICU4C_DLL ${DEVENV_ICU4C_SRC_DLLS})
        message(STATUS "Copying file ${DEVENV_ICU4C_BIN_DIRECTORY}/${DEVENV_ICU4C_DLL} to ${DEVENV_ICU4C_DLL}")
        configure_file(${DEVENV_ICU4C_BIN_DIRECTORY}/${DEVENV_ICU4C_DLL} ${DEVENV_ICU4C_DLL} COPYONLY)
      endforeach()
    endif(MOD_USE_VCPKG EQUAL 1)
  endif(MOD_USE_ICU4C EQUAL 1)
endif(WIN32)

if(0)
  # I used "brew install icu4c" which doesn't link the libraries into /usr/local directly since it might stomp on MacOS Xcode libraries somehow.
  set(MACOS_LOCAL_OPT "/usr/local/opt")
  include_directories(BEFORE SYSTEM
    ${MACOS_LOCAL_OPT}/icu4c/include
  )
  link_directories(
    BEFORE ${MACOS_LOCAL_OPT}/icu4c/lib
  )
  link_libraries(
    icuuc
  )
endif(0)

if(LINUX)
  # Linux only
  link_libraries(
    uuid
  )
endif(LINUX)

if(MOD_USE_VULKAN EQUAL 1)

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
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vk_layer_settings.txt)
    message("vk_layer_settings.txt exists")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/vk_layer_settings.txt vk_layer_settings.txt COPYONLY)
  endif()

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

  function(fn_compile_model target model_compiler)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "EXTENSION" "SOURCES")
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

if(MOD_USE_GLFW EQUAL 1)
  if(WIN32)
    # cmake can't seem to automatically find GLFW:
    set(glfw3_DIR "C:/Program Files (x86)/GLFW/lib/cmake/glfw3")
  endif(WIN32)
  find_package(glfw3 3.3 REQUIRED)
endif()

# google test stuff:
function(fetch_googletest version)
  if(NOT version)
    set(version "main")
  else()
    set(version "v${version}")
  endif()

  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ${version})
  FetchContent_MakeAvailable(googletest)
  include_directories("${gtest_SOURCE_DIR}/include")
  message("Google Test version: ${version}")
endfunction()

# For these copy targets we need to make sure we don't create the same target multiple times.
# So make a global counter that we can use to make sure we don't create the same target multiple times by
#  appending the counter to the target name.
set(MOD_COPY_COUNTER_TARGET_VAL 1 CACHE INTERNAL "Number of times the copy methods have been called")

# create a target to copy a directory and a set of wildcard files to the output directory:
function(copy_directory_to_build DIRNAME WILDCARD)
  math(EXPR NEW_VAL "${MOD_COPY_COUNTER_TARGET_VAL}+1")
  set(MOD_COPY_COUNTER_TARGET_VAL ${NEW_VAL} PARENT_SCOPE) # post increment
  # Get all files in the source directory
  file(GLOB SRC_FILES "${DIRNAME}/${WILDCARD}")

  # Create a custom target that depends on the copied files
  add_custom_target(copy_directory_to_build-${DIRNAME}-${MOD_COPY_COUNTER_TARGET_VAL} ALL)

  # Create the directory in the output directory
  add_custom_command(
    TARGET copy_directory_to_build-${DIRNAME}-${MOD_COPY_COUNTER_TARGET_VAL}
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${DIRNAME}"
    COMMENT "Creating ${DIRNAME} directory in ${CMAKE_BINARY_DIR}"
  )

  # For each file, create a custom command and a custom target that copies the file to the output directory
  foreach(FILE ${SRC_FILES})
    get_filename_component(FILENAME ${FILE} NAME)
    set(OUTPUT "${CMAKE_BINARY_DIR}/${DIRNAME}/${FILENAME}")

    add_custom_command(
      OUTPUT "${OUTPUT}"
      COMMAND ${CMAKE_COMMAND} -E copy "${FILE}" "${OUTPUT}"
      DEPENDS "${FILE}"
      COMMENT "Copying ${FILE} to ${OUTPUT}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_custom_target(copy_${DIRNAME}_${FILENAME}-${MOD_COPY_COUNTER_TARGET_VAL} ALL DEPENDS "${OUTPUT}")
    add_dependencies(copy_directory_to_build-${DIRNAME}-${MOD_COPY_COUNTER_TARGET_VAL} copy_${DIRNAME}_${FILENAME}-${MOD_COPY_COUNTER_TARGET_VAL})
  endforeach()
endfunction()

function(copy_files_to_build FILES_TO_COPY)
  math(EXPR NEW_VAL "${MOD_COPY_COUNTER_TARGET_VAL}+1")
  set(MOD_COPY_COUNTER_TARGET_VAL ${NEW_VAL} PARENT_SCOPE) # post increment
  # Create a custom target that depends on the copied files
  add_custom_target(copy_files_to_build-${MOD_COPY_COUNTER_TARGET_VAL} ALL)

  # For each file, create a custom command and a custom target that copies the file to the output directory
  foreach(FILE ${FILES_TO_COPY})
    get_filename_component(FILENAME ${FILE} NAME)
    get_filename_component(DIRNAME ${FILE} DIRECTORY)
    set(OUTPUT "${CMAKE_BINARY_DIR}/${DIRNAME}")

    # Create the directory in the output directory only if DIRNAME is not empty
    if(NOT "${DIRNAME}" STREQUAL "")
      add_custom_command(
        TARGET copy_files_to_build-${MOD_COPY_COUNTER_TARGET_VAL}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT}"
        COMMENT "Creating ${OUTPUT} directory in ${CMAKE_BINARY_DIR}"
      )
    endif()

    # Copy the file to the output directory
    add_custom_command(
      OUTPUT "${OUTPUT}/${FILENAME}"
      COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${FILE}" "${OUTPUT}/${FILENAME}"
      DEPENDS "${CMAKE_SOURCE_DIR}/${FILE}"
      COMMENT "Copying ${FILE} to ${OUTPUT}"
    )

    add_custom_target(copy_${FILENAME}-${MOD_COPY_COUNTER_TARGET_VAL} ALL DEPENDS "${OUTPUT}/${FILENAME}")
    add_dependencies(copy_files_to_build-${MOD_COPY_COUNTER_TARGET_VAL} copy_${FILENAME}-${MOD_COPY_COUNTER_TARGET_VAL})
  endforeach()
endfunction()

function(copy_files_to_build_dir FILES_TO_COPY DEST_DIR)
  math(EXPR NEW_VAL "${MOD_COPY_COUNTER_TARGET_VAL}+1")
  set(MOD_COPY_COUNTER_TARGET_VAL ${NEW_VAL} PARENT_SCOPE) # post increment

  # Create a custom target that depends on the copied files
  add_custom_target(copy_files_to_build_dir-${MOD_COPY_COUNTER_TARGET_VAL} ALL)

  # For each file pattern, create a custom command and a custom target that copies the file to the output directory
  foreach(FILE_PATTERN ${FILES_TO_COPY})
    file(GLOB MATCHING_FILES RELATIVE "${CMAKE_SOURCE_DIR}" "${FILE_PATTERN}")
    foreach(FILE ${MATCHING_FILES})
      get_filename_component(FILENAME ${FILE} NAME)
      set(OUTPUT "${CMAKE_BINARY_DIR}/${DEST_DIR}")

      # Create the directory in the output directory
      add_custom_command(
        TARGET copy_files_to_build_dir-${MOD_COPY_COUNTER_TARGET_VAL}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT}"
        COMMENT "Creating ${OUTPUT} directory in ${CMAKE_BINARY_DIR}"
      )

      # Copy the file to the output directory
      add_custom_command(
        OUTPUT "${OUTPUT}/${FILENAME}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${FILE}" "${OUTPUT}/${FILENAME}"
        DEPENDS "${CMAKE_SOURCE_DIR}/${FILE}"
        COMMENT "Copying ${FILE} to ${OUTPUT}"
      )

      add_custom_target(copy_${FILENAME}-${MOD_COPY_COUNTER_TARGET_VAL} ALL DEPENDS "${OUTPUT}/${FILENAME}")
      add_dependencies(copy_files_to_build_dir-${MOD_COPY_COUNTER_TARGET_VAL} copy_${FILENAME}-${MOD_COPY_COUNTER_TARGET_VAL})
    endforeach()
  endforeach()
endfunction()

function(copy_files_to_source_dir FILES_TO_COPY DEST_DIR)
  math(EXPR NEW_VAL "${MOD_COPY_COUNTER_TARGET_VAL}+1")
  set(MOD_COPY_COUNTER_TARGET_VAL ${NEW_VAL} PARENT_SCOPE) # post increment

  # Create a custom target that depends on the copied files
  add_custom_target(copy_files_to_source_dir-${MOD_COPY_COUNTER_TARGET_VAL} ALL)

  # For each file, create a custom command and a custom target that copies the file to the destination directory
  foreach(FILE ${FILES_TO_COPY})
    get_filename_component(FILENAME ${FILE} NAME)

    # Create a safe version of the full file path
    string(REPLACE "/" "_" FILE_SAFE ${FILE})

    # Copy the file to the destination directory
    add_custom_command(
      OUTPUT "${CMAKE_SOURCE_DIR}/${DEST_DIR}/${FILENAME}"
      COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${FILE}" "${CMAKE_SOURCE_DIR}/${DEST_DIR}/${FILENAME}"
      DEPENDS "${CMAKE_SOURCE_DIR}/${FILE}"
      COMMENT "Copying ${FILE} to destination ${CMAKE_SOURCE_DIR}/${DEST_DIR}"
    )

    # Include FILE_SAFE and MOD_COPY_COUNTER_TARGET_VAL in the custom target name
    add_custom_target(copy_${FILE_SAFE}-${MOD_COPY_COUNTER_TARGET_VAL} ALL DEPENDS "${CMAKE_SOURCE_DIR}/${DEST_DIR}/${FILENAME}")
    add_dependencies(copy_files_to_source_dir-${MOD_COPY_COUNTER_TARGET_VAL} copy_${FILE_SAFE}-${MOD_COPY_COUNTER_TARGET_VAL})
  endforeach()
endfunction()

function(copy_dir_to_source_dir SOURCE_DIR WILDCARD DEST_DIR)
  math(EXPR NEW_VAL "${MOD_COPY_COUNTER_TARGET_VAL}+1")
  set(MOD_COPY_COUNTER_TARGET_VAL ${NEW_VAL} PARENT_SCOPE) # post increment

  # Create a custom target that depends on the copied files
  add_custom_target(copy_dir_to_source_dir-${MOD_COPY_COUNTER_TARGET_VAL} ALL)

  # Get a list of files in the source directory that match the wildcard
  file(GLOB FILES_TO_COPY "${CMAKE_SOURCE_DIR}/${SOURCE_DIR}/${WILDCARD}")

  # For each file, create a custom command and a custom target that copies the file to the destination directory
  foreach(FILE ${FILES_TO_COPY})
    get_filename_component(FILENAME ${FILE} NAME)

    # Create a safe version of the file name
    string(REPLACE "/" "_" FILE_SAFE ${FILENAME})

    # Copy the file to the destination directory
    add_custom_command(
      OUTPUT "${CMAKE_SOURCE_DIR}/${DEST_DIR}/${FILENAME}"
      COMMAND ${CMAKE_COMMAND} -E copy "${FILE}" "${CMAKE_SOURCE_DIR}/${DEST_DIR}/${FILENAME}"
      DEPENDS "${FILE}"
      COMMENT "Copying ${FILE} to ${CMAKE_SOURCE_DIR}/${DEST_DIR}"
    )

    # Include FILE_SAFE and MOD_COPY_COUNTER_TARGET_VAL in the custom target name
    add_custom_target(copy_${FILE_SAFE}-${MOD_COPY_COUNTER_TARGET_VAL} ALL DEPENDS "${CMAKE_SOURCE_DIR}/${DEST_DIR}/${FILENAME}")
    add_dependencies(copy_dir_to_source_dir-${MOD_COPY_COUNTER_TARGET_VAL} copy_${FILE_SAFE}-${MOD_COPY_COUNTER_TARGET_VAL})
  endforeach()
endfunction()

function(copy_url_to_build_dir DOWNLOAD_URL DEST_DIR BIENUTIL_DIR)
  math(EXPR NEW_VAL "${MOD_COPY_COUNTER_TARGET_VAL}+1")
  set(MOD_COPY_COUNTER_TARGET_VAL ${NEW_VAL} PARENT_SCOPE) # post increment

  # Create a custom target that depends on the downloaded file
  add_custom_target(copy_url_to_build_dir-${MOD_COPY_COUNTER_TARGET_VAL} ALL)

  get_filename_component(FILENAME ${DOWNLOAD_URL} NAME)

  # Create a safe version of the file name
  string(REPLACE "/" "_" FILE_SAFE ${FILENAME})

  # Set the destination file path
  set(DEST_FILE "${CMAKE_BINARY_DIR}/${DEST_DIR}/${FILENAME}")

  # Download the file to the destination directory
  add_custom_command(
    OUTPUT ${DEST_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env --unset=LD_LIBRARY_PATH
    ${CMAKE_COMMAND} -D "DOWNLOAD_URL=${DOWNLOAD_URL}" -D "DEST_FILE=${DEST_FILE}" -P "${BIENUTIL_DIR}/DownloadFile.cmake"
    COMMENT "Downloading ${DOWNLOAD_URL} to ${DEST_FILE}"
  )
  # Include FILE_SAFE and MOD_COPY_COUNTER_TARGET_VAL in the custom target name
  add_custom_target(download_${FILE_SAFE}-${MOD_COPY_COUNTER_TARGET_VAL} ALL DEPENDS "${DEST_FILE}")
  add_dependencies(copy_url_to_build_dir-${MOD_COPY_COUNTER_TARGET_VAL} download_${FILE_SAFE}-${MOD_COPY_COUNTER_TARGET_VAL})
endfunction()

function(copy_url_to_build_dir_during_config DOWNLOAD_URL DEST_DIR BIENUTIL_DIR)
  get_filename_component(FILENAME ${DOWNLOAD_URL} NAME)
  set(DEST_FILE "${CMAKE_BINARY_DIR}/${DEST_DIR}/${FILENAME}")

  if(NOT EXISTS ${DEST_FILE})
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${DEST_DIR}")
    file(DOWNLOAD ${DOWNLOAD_URL} ${DEST_FILE}
      STATUS status
      LOG log
    )
    list(GET status 0 status_code)
    list(GET status 1 status_string)
    if(NOT status_code EQUAL 0)
      message(FATAL_ERROR "Error: Download failed! Status: ${status_string} Log: ${log}")
    endif()
  endif()

  set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES ${DEST_FILE})
endfunction()

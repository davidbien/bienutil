# CMakeLists.txt for bienutil.
# bienutil is header-only: this defines an interface library for it and builds its unit tests.
# It may be built standalone or added to another project with add_subdirectory().
#
# BIENUTIL_BUILD_TESTS: build the unit tests ( test/ ) - ON when standalone, OFF within another project unless that
#   project sets it.

cmake_minimum_required (VERSION 3.14)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(BIENUTIL_STANDALONE TRUE)
  project ("bienutil" CXX)
else()
  set(BIENUTIL_STANDALONE FALSE)
endif()

if(BIENUTIL_STANDALONE)
  # When part of another project we expect it to have set things up - bienutil.cmake modifies global compiler flags.
  include ( "${CMAKE_CURRENT_SOURCE_DIR}/bienutil.cmake" )
  enable_testing()
  bienutil_ctest_log_dir( "${CMAKE_BINARY_DIR}/testlogs" ) # The test programs' logs go here - cleared each ctest run.
endif()

if(NOT TARGET bienutil)
  add_library(bienutil INTERFACE)
  target_include_directories(bienutil INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
  target_compile_features(bienutil INTERFACE cxx_std_20)
endif()

option(BIENUTIL_BUILD_TESTS "Build the bienutil unit tests." ${BIENUTIL_STANDALONE})
if(BIENUTIL_BUILD_TESTS)
  add_subdirectory(test)
endif()

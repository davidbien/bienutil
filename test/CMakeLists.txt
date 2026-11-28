# CMakeLists.txt for the bienutil unit tests.
#
# jsontest: Google test based unit tests of the JSON reader and writer - jsonstrm.h - and of the JSON values built on
#   them - jsonobjs.h. See jsontest.cpp.
# strwrsvtest: Google test based unit tests of StrWRsv - strwrsv.h - the string every JSON string is kept in.
# strutiltest: Google test based unit tests of _strutil.h and utfconvert.h - UTF conversions, encodings, C strings,
#   printf to a string, reading numbers.
# containertest: Google test based unit tests of _bitutil.h, _simple_bitvec ( _simpbv.h ), LogArray ( _logarray.h ) and
#   UniquePtrSList ( upslist.h ).
# segarraytest: Google test based unit tests of SegArray, SegArrayRotatingBuffer and SegArrayView ( segarray.h ).
# iotest: Google test based unit tests of MemFile and MemStream ( memfile.h ), the file functions of _compat.h and
#   _compat.inl, FileObj and FileMappingObj ( _fdobjs.h ), and FdReadRotating ( fdrotbuf.h ).

include(GoogleTest)

if(NOT TARGET gtest)
  include(FetchContent)
  set(GTEST_VERSION 1.14.0 CACHE STRING "Google test version")
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v${GTEST_VERSION})
  # Use the same runtime library settings as we do under MSVC.
  set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

foreach(BIENUTILTEST jsontest strwrsvtest strutiltest containertest segarraytest iotest)
  add_executable (${BIENUTILTEST} ${BIENUTILTEST}.cpp bienutiltest.h)
  target_link_libraries(${BIENUTILTEST} bienutil gtest gtest_main)
  target_compile_definitions( ${BIENUTILTEST}
    PUBLIC __NDEBUG_THROW
    PUBLIC TRACESENABLED=0
  )
  gtest_discover_tests(${BIENUTILTEST})
endforeach()

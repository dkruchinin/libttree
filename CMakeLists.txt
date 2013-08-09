cmake_minimum_required(VERSION 2.6, FATAL_ERROR)
project(ttree C)

set(DEFAULT_CFLAGS "-O3")
set(GCOV_CFLAGS "-g -fprofile-arcs -ftest-coverage")
option(WITH_GCOV "Use GCOV" OFF)

if(WITH_GCOV)
  set(CMAKE_C_FLAGS "${GCOV_CFLAGS}")
else()
  set(CMAKE_C_FLAGS "${DEFAULT_CFLAGS}")
endif()

include_directories(${ttree_source_dir})
ADD_LIBRARY(ttree STATIC ttree.c)
add_subdirectory(tests EXCLUDE_FROM_ALL)

set(DOXYGEN_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(DOXYGEN_OUTPUT_DIR docs)
string(REGEX REPLACE ";" " " CMAKE_DOXYGEN_INPUT_LIST "${DOXYGEN_SOURCE_DIR}")
configure_file(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
set(DOCS_TARGET "docs")
add_custom_target(${DOCS_TARGET} doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)


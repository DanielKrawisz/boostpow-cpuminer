cmake_minimum_required (VERSION 3.16)

# Back compatibility for VERSION range
if (${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy (VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif ()

include (GoogleTest)

add_executable (unit_tests
  test_detect_boost.cpp
  test_program_options.cpp
)

target_include_directories (unit_tests PUBLIC ../include)

target_link_libraries (unit_tests
  PRIVATE bm gtest_main gmock)
gtest_discover_tests (unit_tests)



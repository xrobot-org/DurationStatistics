# CMakeLists.txt for DurationStatistics

# DurationStatistics is a header-only dependency module. It is consumed by the
# parent LibXR build and does not define a standalone executable; its test is in tests/.
target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

get_filename_component(_MODULE_DIRNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(_DEPS_TARGET "${_MODULE_DIRNAME}_deps")
add_library(${_DEPS_TARGET} INTERFACE)
set_property(GLOBAL APPEND PROPERTY XR_MODULE_DEPS ${_DEPS_TARGET})

if(BUILD_TESTING)
  include(CTest)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tests
                   ${CMAKE_CURRENT_BINARY_DIR}/DurationStatistics_tests)
endif()

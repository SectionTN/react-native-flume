cmake_minimum_required(VERSION 3.16)
project(flume_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED REACT_NATIVE_DIR OR NOT DEFINED WORKLETS_DIR)
  message(FATAL_ERROR "pass -DREACT_NATIVE_DIR and -DWORKLETS_DIR, see scripts/test-cpp.sh")
endif()

# React Native's own Yoga, so the host tests lay out with the engine the app ships.
add_subdirectory("${REACT_NATIVE_DIR}/ReactCommon/yoga/yoga" yoga)

file(GLOB_RECURSE FLUME_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/../core/*.cpp")
add_library(flume_core STATIC ${FLUME_CORE_SOURCES})
target_include_directories(flume_core PUBLIC "${CMAKE_SOURCE_DIR}/..")
target_link_libraries(flume_core PUBLIC yogacore)

# Compile check only: the JSI and worklets symbols resolve on the device.
file(GLOB_RECURSE FLUME_BINDINGS_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/../bindings/*.cpp")
if(FLUME_BINDINGS_SOURCES)
  add_library(flume_bindings STATIC ${FLUME_BINDINGS_SOURCES})
  target_include_directories(flume_bindings PRIVATE "${REACT_NATIVE_DIR}/ReactCommon/jsi" "${REACT_NATIVE_DIR}/ReactCommon/callinvoker" "${WORKLETS_DIR}/Common/cpp")
  target_link_libraries(flume_bindings PRIVATE flume_core)
endif()

# The platform-neutral half of the in-process adapters (iOS).
file(GLOB_RECURSE FLUME_ADAPTER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/../adapter/*.cpp")
add_library(flume_adapter STATIC ${FLUME_ADAPTER_SOURCES})
target_link_libraries(flume_adapter PUBLIC flume_core)

file(GLOB FLUME_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*_test.cpp")
add_executable(flume_tests ${FLUME_TEST_SOURCES} "${CMAKE_SOURCE_DIR}/main.cpp")
target_include_directories(flume_tests PRIVATE "${CMAKE_SOURCE_DIR}/vendor" "${CMAKE_SOURCE_DIR}")
target_link_libraries(flume_tests PRIVATE flume_core flume_adapter)

# The bindings suite runs on a Hermes runtime linked from React Native's macOS framework.
if(DEFINED HERMES_DIR)
  set(HERMES_FRAMEWORKS "${HERMES_DIR}/destroot/Library/Frameworks/macosx")
  file(GLOB FLUME_JSI_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/jsi/*_test.cpp")
  add_executable(flume_jsi_tests ${FLUME_JSI_TEST_SOURCES} "${CMAKE_SOURCE_DIR}/jsi/main.cpp"
                 "${CMAKE_SOURCE_DIR}/../bindings/JsiConverter.cpp" "${CMAKE_SOURCE_DIR}/../bindings/ItemCache.cpp")
  target_include_directories(flume_jsi_tests BEFORE PRIVATE "${HERMES_DIR}/destroot/include")
  target_include_directories(flume_jsi_tests PRIVATE "${CMAKE_SOURCE_DIR}/vendor" "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/jsi")
  target_link_libraries(flume_jsi_tests PRIVATE flume_core "-F${HERMES_FRAMEWORKS}" "-framework hermesvm")
  set_target_properties(flume_jsi_tests PROPERTIES BUILD_RPATH "${HERMES_FRAMEWORKS}")
endif()

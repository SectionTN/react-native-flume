cmake_minimum_required(VERSION 3.16)
project(flume)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fbjni REQUIRED CONFIG)
find_package(ReactAndroid REQUIRED CONFIG)
find_package(react-native-worklets REQUIRED CONFIG)

include("${REACT_NATIVE_DIR}/ReactAndroid/cmake-utils/folly-flags.cmake")
add_compile_options(${folly_FLAGS})

file(GLOB_RECURSE FLUME_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/../cpp/core/*.cpp"
     "${CMAKE_SOURCE_DIR}/../cpp/bindings/*.cpp"
     "${CMAKE_SOURCE_DIR}/src/main/cpp/*.cpp")
add_library(flume SHARED ${FLUME_SOURCES})

include("${REACT_NATIVE_DIR}/ReactCommon/cmake-utils/react-native-flags.cmake")
target_compile_reactnative_options(flume PRIVATE)

target_include_directories(flume PRIVATE "${CMAKE_SOURCE_DIR}/../cpp" "${CMAKE_SOURCE_DIR}/src/main/cpp")
target_link_libraries(flume log android ReactAndroid::reactnative ReactAndroid::jsi fbjni::fbjni react-native-worklets::worklets)

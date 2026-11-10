cmake_minimum_required(VERSION 3.13)

# Replaces the generated codegen target, so the app links Flume's own shadow nodes.
set(FLUME_CODEGEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../build/generated/source/codegen/jni)
set(FLUME_FABRIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/fabric)

file(GLOB react_codegen_SRCS CONFIGURE_DEPENDS
  ${FLUME_CODEGEN_DIR}/*.cpp
  ${FLUME_CODEGEN_DIR}/react/renderer/components/FlumeSpec/*.cpp
  ${FLUME_FABRIC_DIR}/*.cpp)

add_library(react_codegen_FlumeSpec OBJECT ${react_codegen_SRCS})

# This folder first, so autolinking's <FlumeSpec.h> finds the header that declares the descriptors.
target_include_directories(react_codegen_FlumeSpec PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${FLUME_FABRIC_DIR}
  ${FLUME_CODEGEN_DIR}
  ${FLUME_CODEGEN_DIR}/react/renderer/components/FlumeSpec)

target_link_libraries(react_codegen_FlumeSpec fbjni jsi reactnative)

target_compile_reactnative_options(react_codegen_FlumeSpec PRIVATE)

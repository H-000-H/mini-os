#mini-os static library cmake file
#author: H-OOO-H
#copyright: SPDX-License-Identifier: Apache-2.0
#file: CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Temporary set the cross-compiler to arm-none-eabi-gcc for testing
set(CMAKE_C_COMPILER arm-none-eabi-gcc)

set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_PROJECT_NAME mini-os)

project(${CMAKE_PROJECT_NAME} C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MINI_OS_INC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

set(MINI_OS_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/list.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/task.c
)

add_library(${CMAKE_PROJECT_NAME}
    STATIC ${MINI_OS_SRC}
)

target_include_directories(
  ${CMAKE_PROJECT_NAME} PRIVATE ${MINI_OS_INC}
)

target_sources(
  ${CMAKE_PROJECT_NAME} PRIVATE ${MINI_OS_SRC}
)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE
)

#mini-os static library cmake file
#author: H-OOO-H
#copyright: SPDX-License-Identifier: Apache-2.0
#file: CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

set(CMAKE_PROJECT_NAME mini-os)

project(${CMAKE_PROJECT_NAME} C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MINI_OS_MCU cortex-m3 CACHE STRING "ARM Cortex-M core (cortex-m0/m0+/m3/m4/m7)")

set(MINI_OS_INC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

set(MINI_OS_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mem.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/thread.c
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

target_compile_options(
  ${CMAKE_PROJECT_NAME} PRIVATE
  -mcpu=${MINI_OS_MCU}
  -mthumb
)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME} PRIVATE
)

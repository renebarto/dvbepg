cmake_minimum_required(VERSION 3.6)
project(dvbepg)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake CACHE STRING
    "CMake module path." FORCE)

find_package(DVBPSI REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

set(BUILD_SHARED_LIBS ON)
set(BUILD_TESTS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)

set(CMAKE_CXX_STANDARD 11)

set(DVBEPG_LIBS
    ${DVBPSI_LIBRARIES})

file(GLOB SOURCE_FILES main.cpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${DVBEPG_LIBS})

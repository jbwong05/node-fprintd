cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_CXX_STANDARD 11)

project(node-fprintd)
include_directories(${CMAKE_JS_INC} src)
set(SOURCE_FILES src/main.c src/node-fprintd.c)

find_library(SYSTEMD NAMES systemd REQUIRED)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} ${SYSTEMD})

# Define NAPI_VERSION
add_definitions(-DNAPI_VERSION=3)
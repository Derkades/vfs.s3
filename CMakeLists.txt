cmake_minimum_required(VERSION 3.10)
project(vfs.s3)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(aws-cpp-sdk-core REQUIRED)
find_package(aws-cpp-sdk-s3 REQUIRED)

include_directories(${KODI_INCLUDE_DIR})

set(DEPLIBS aws-cpp-sdk-core aws-cpp-sdk-s3)
set(S3_SOURCES src/main.cpp)
set(S3_HEADERS src/main.h)

build_addon(vfs.s3 S3 DEPLIBS)

include(CPack)

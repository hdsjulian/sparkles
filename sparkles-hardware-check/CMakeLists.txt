cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sparkles-hardware-check)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
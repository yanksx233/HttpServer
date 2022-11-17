cmake_minimum_required(VERSION 3.5)
project(HttpServer)

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_FLAGS
    ${CMAKE_CXX_FLAGS}
    -Wall
    -std=c++14
)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

message(STATUS "CMAKE_CXX_FLAGS = " ${CMAKE_CXX_FLAGS})
message(STATUS "CMAKE_CXX_FLAGS_DEBUG = " ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "CMAKE_CXX_FLAGS_RELEASE = " ${CMAKE_CXX_FLAGS_RELEASE})
message(STATUS "CMAKE_BUILD_TYPE = " ${CMAKE_BUILD_TYPE})


add_subdirectory(base)

add_executable(${PROJECT_NAME} HttpServer.cc HttpConnection.cc)
target_link_libraries(${PROJECT_NAME} base)

add_executable(test test.cc)
target_link_libraries(test base)
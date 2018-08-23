cmake_minimum_required(VERSION 2.8)

file(GLOB sourceFiles src/*.cpp src/*.h)

add_executable(websocket_to_posix_proxy ${sourceFiles})

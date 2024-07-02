cmake_minimum_required(VERSION 3.28)
project(rotatingDonut C)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

if(NOT SDL2_ttf_FOUND)
    set(SDL2_TTF_INCLUDE_DIRS "/usr/include/SDL2")
    set(SDL2_TTF_LIBRARIES "/usr/lib/x86_64-linux-gnu/libSDL2_ttf.so")
else()
    find_package(SDL2_ttf REQUIRED)
endif()

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

add_executable(rotatingDonut main.c)

target_link_libraries(rotatingDonut ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES} m)
cmake_minimum_required(VERSION 3.18)
project(canis)
set(CMAKE_CXX_STANDARD 20)

#find_package(OpenGL REQUIRED)
find_package(Python REQUIRED)

add_subdirectory(external/GL)
#add_subdirectory(external/glad)
#add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/SDL)
add_subdirectory(external/stb)
add_subdirectory(external/freetype2)
include_directories("${PROJECT_SOURCE_DIR}/external/freetype2/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

file(GLOB_RECURSE SRC_SOURCES src/*.c*)
file(GLOB_RECURSE SRC_HEADERS include/Canis/*.h*)

add_library(${PROJECT_NAME} ${SRC_SOURCES} ${SRC_HEADERS})

#file(GLOB_RECURSE SRC_SOURCES src/*.c*)
#file(GLOB_RECURSE SRC_HEADERS src/*.h*)

#add_executable(${PROJECT_NAME} ${SRC_SOURCES} ${SRC_HEADERS})

#glfw
#glad 
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    glm
    stb
    libglew_static
    freetype
    SDL2main
    SDL2-static
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
#PRIVATE
#glfw
#glad
target_include_directories(${PROJECT_NAME}
    PUBLIC
        glm
        stb
        freetype
        SDL2main
        SDL2-static
        "${PROJECT_SOURCE_DIR}/include"
)

# This command will copy your assets folder to your running directory, in order to have access to your shaders, textures, etc


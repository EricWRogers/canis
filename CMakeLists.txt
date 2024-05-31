cmake_minimum_required(VERSION 3.18)
project(canis)

set(CMAKE_CXX_STANDARD 20)

set(BUILD_TESTING                   FALSE)

set(SDL_TEST_ENABLED_BY_DEFAULT     FALSE)
set(SDL_SHARED_ENABLED_BY_DEFAULT   FALSE)

set(BUILD_SHARED_LIBS               FALSE)

set(YAML_CPP_BUILD_TOOLS            FALSE)
set(YAML_CPP_BUILD_TESTS            FALSE)
set(YAML_CPP_BUILD_TOOLS            FALSE)
set(YAML_BUILD_SHARED_LIBS          FALSE)

set(FT_DISABLE_PNG                  TRUE)
set(FT_DISABLE_HARFBUZZ             TRUE)

set(SDL2MIXER_VENDORED              TRUE)
set(SDL2MIXER_WAVE                  FALSE)
set(SDL2MIXER_WAVPACK               FALSE)
set(SDL2MIXER_OGG                   TRUE)
set(SDL2MIXER_MP3                   FALSE)
set(SDL2MIXER_OPUS                  FALSE)
set(SDL2MIXER_FLAC_LIBFLAC          FALSE)
set(SDL2MIXER_MOD_XMP               FALSE)
set(SDL2MIXER_MOD                   FALSE)
set(SDL2MIXER_MIDI_FLUIDSYNTH       FALSE)

#add_subdirectory(external/GL)
add_subdirectory(external/glm)
add_subdirectory(external/yaml-cpp)
add_subdirectory(external/SDL)
add_subdirectory(external/stb)
add_subdirectory(external/freetype)
add_subdirectory(external/SDL_mixer)

# Set where the ImGui files are stored
set(IMGUI_PATH  "external/imgui")
    
# Compile as static library 
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp) 
file(GLOB IMGUI_HEADERS ${IMGUI_PATH}/*.h ${IMGUI_PATH}/backends/imgui_impl_opengl3.h ${IMGUI_PATH}/backends/imgui_impl_sdl2.h ${IMGUI_PATH}/misc/cpp/imgui_stdlib.h) 
add_library("imgui" STATIC ${IMGUI_SOURCES})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH} ${IMGUI_PATH}/backends/ ${IMGUI_PATH}/misc/cpp/)
target_link_libraries("imgui" PRIVATE SDL2main SDL2-static)

include_directories("${PROJECT_SOURCE_DIR}/external/freetype/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

file(GLOB_RECURSE GLAD_SOURCES external/glad/*.c)
file(GLOB_RECURSE GLAD_HEADERS external/glad/*.h)

add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

file(GLOB_RECURSE SRC_SOURCES src/*.c*)
file(GLOB_RECURSE SRC_HEADERS include/Canis/*.h*)

add_library(${PROJECT_NAME} STATIC ${SRC_SOURCES} ${SRC_HEADERS})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    $<INSTALL_INTERFACE:include>
    glm
    stb
    freetype
    yaml-cpp
    SDL2-static
    SDL2main
    SDL2_mixer
    imgui
    glad
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        glm
        stb
        freetype
        yaml-cpp
        SDL2-static
        SDL2main
        SDL2_mixer
        imgui
        "${PROJECT_SOURCE_DIR}/include"
)

#target_precompile_headers(${PROJECT_NAME}
#    PUBLIC
#    <vector>
#    <unordered_map>
#    <string>
#    <functional>
#    <map>
#    <fstream>
#    <cstdint>
#    <memory>
#    <string.h>
#    <chrono>
#    <stdio.h>
#    <cstring>
#    <iostream>
#)

# This command will copy your assets folder to your running directory, in order to have access to your shaders, textures, etc


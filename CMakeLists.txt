cmake_minimum_required(VERSION 3.18)
project(canis)

set(CMAKE_CXX_STANDARD 20)

set(BUILD_TESTING                   FALSE)
set(glew-cmake_BUILD_SHARED         FALSE)
set(SDL_TEST_ENABLED_BY_DEFAULT     FALSE)
set(SDL_SHARED_ENABLED_BY_DEFAULT   FALSE)
set(YAML_CPP_BUILD_TESTS            FALSE)
set(YAML_CPP_BUILD_TOOLS            OFF)
set(YAML_BUILD_SHARED_LIBS          FALSE)
set(BUILD_SHARED_LIBS               FALSE)
set(SDL2MIXER_VENDORED              TRUE)
set(SDL2MIXER_WAVE                  TRUE)
set(SDL2MIXER_OGG                   TRUE)
set(SDL2MIXER_MP3                   TRUE)
set(SDL2MIXER_OPUS                  FALSE)
set(SDL2MIXER_FLAC_LIBFLAC          FALSE)
set(SDL2MIXER_MOD_XMP               FALSE)
set(SDL2MIXER_MOD                   FALSE)
set(SDL2MIXER_MIDI_FLUIDSYNTH       FALSE)
set(FT_DISABLE_PNG                  TRUE)
set(FT_DISABLE_HARFBUZZ             TRUE)


add_subdirectory(external/GL)
add_subdirectory(external/glm)
add_subdirectory(external/yaml-cpp)
add_subdirectory(external/SDL)
add_subdirectory(external/stb)
add_subdirectory(external/freetype2)
add_subdirectory(external/SDL_mixer)

set(YAML_CPP_BUILD_TOOLS            OFF)
set(YAML_CPP_BUILD_TESTS            FALSE)
set(YAML_CPP_BUILD_TOOLS            OFF)
set(YAML_BUILD_SHARED_LIBS          FALSE)

include_directories("${PROJECT_SOURCE_DIR}/external/freetype2/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

file(GLOB_RECURSE SRC_SOURCES src/*.c*)
file(GLOB_RECURSE SRC_HEADERS include/Canis/*.h*)

add_library(${PROJECT_NAME} STATIC ${SRC_SOURCES} ${SRC_HEADERS})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    $<INSTALL_INTERFACE:include>
    glm
    stb
    libglew_static
    freetype
    yaml-cpp
    SDL2-static
    SDL2main
    SDL2_mixer
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
        "${PROJECT_SOURCE_DIR}/include"
)

# This command will copy your assets folder to your running directory, in order to have access to your shaders, textures, etc


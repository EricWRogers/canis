cmake_minimum_required(VERSION 3.18)
project(canis)

add_definitions(
    -fPIC
)

set(CMAKE_CXX_STANDARD 20)

message(STATUS "HERE")

set(SDL3_mixer::SDL3_mixer_FOUND                TRUE)
set(SDL3_mixer::SDL3MIXER_VENDORED              FALSE)
set(SDL3_mixer::SDL3MIXER_CMD                   FALSE)
set(SDL3_mixer::SDL3MIXER_FLAC_LIBFLAC          FALSE)
set(SDL3_mixer::SDL3MIXER_FLAC_DRFLAC           TRUE)
set(SDL3_mixer::SDL3MIXER_GME                   FALSE)
set(SDL3_mixer::SDL3MIXER_MOD                   TRUE)
set(SDL3_mixer::SDL3MIXER_MOD_MODPLUG           TRUE)
set(SDL3_mixer::SDL3MIXER_MOD_XMP               FALSE)
set(SDL3_mixer::SDL3MIXER_MOD_XMP_LITE          FALSE)
set(SDL3_mixer::SDL3MIXER_MOD_XMP_SHARED        FALSE)
set(SDL3_mixer::SDL3MIXER_MP3                   TRUE)
set(SDL3_mixer::SDL3MIXER_MP3_DRMP3             TRUE)
set(SDL3_mixer::SDL3MIXER_MP3_MPG123            FALSE)
set(SDL3_mixer::SDL3MIXER_MIDI                  TRUE)
set(SDL3_mixer::SDL3MIXER_MIDI_FLUIDSYNTH       FALSE)
set(SDL3_mixer::SDL3MIXER_MIDI_NATIVE           TRUE)
set(SDL3_mixer::SDL3MIXER_MIDI_TIMIDITY         TRUE)
set(SDL3_mixer::SDL3MIXER_OPUS                  TRUE)
set(SDL3_mixer::SDL3MIXER_VORBIS                STB)
set(SDL3_mixer::SDL3MIXER_VORBIS_STB            TRUE)
set(SDL3_mixer::SDL3MIXER_VORBIS_TREMOR         FALSE)
set(SDL3_mixer::SDL3MIXER_VORBIS_VORBISFILE     FALSE)
set(SDL3_mixer::SDL3MIXER_WAVE                  TRUE)

set(SDL3_mixer::SDL3_SHARED FALSE)
set(SDL3::INTERFACE_SDL3_SHARED FALSE)
set(SDL3_mixer::SDL3MIXER_BUILD_SHARED_LIBS FALSE)
set(SDL3MIXER_BUILD_SHARED_LIBS FALSE)

#find_package(OpenGL REQUIRED)
#find_package(Python REQUIRED)

add_subdirectory(external/GL)
#add_subdirectory(external/glad)
#add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/SDL)
add_subdirectory(external/SDL_mixer)
add_subdirectory(external/stb)
add_subdirectory(external/freetype2)
include_directories("${PROJECT_SOURCE_DIR}/external/freetype2/include")
include_directories("${PROJECT_SOURCE_DIR}/include")

#set(SDL3_mixer::INTERFACE_SDL3_SHARED FALSE)
#set(SDL3::INTERFACE_SDL3_SHARED FALSE)
#set(SDL3_mixer::SDL3MIXER_BUILD_SHARED_LIBS FALSE)
#set(SDL3MIXER_BUILD_SHARED_LIBS FALSE)
#option(SDL3_mixer::SDL3MIXER_BUILD_SHARED_LIBS FALSE)
#option(SDL3_mixer::BUILD_SHARED_LIBS FALSE)

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
    glm
    stb
    libglew_static
    freetype
    SDL3_mixer
    SDL3
    
)
#PRIVATE
#glfw
#glad
target_include_directories(${PROJECT_NAME}
    PUBLIC
        glm
        stb
        freetype
        SDL3_mixer
        SDL3
        "${PROJECT_SOURCE_DIR}/include"
)

# This command will copy your assets folder to your running directory, in order to have access to your shaders, textures, etc


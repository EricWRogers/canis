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

add_subdirectory(external/GL)
add_subdirectory(external/glm)
add_subdirectory(external/yaml-cpp)
add_subdirectory(external/SDL)
add_subdirectory(external/stb)
add_subdirectory(external/freetype2)

set(YAML_CPP_BUILD_TOOLS            OFF)

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
    glm
    stb
    libglew_static
    freetype
    yaml-cpp
    SDL2-static
    SDL2main
)
#PRIVATE
#glfw
#glad
target_include_directories(${PROJECT_NAME}
    PUBLIC
        glm
        stb
        freetype
        yaml-cpp
        SDL2-static
        SDL2main
        "${PROJECT_SOURCE_DIR}/include"
)

# This command will copy your assets folder to your running directory, in order to have access to your shaders, textures, etc


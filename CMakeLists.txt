cmake_minimum_required(VERSION 3.18)
project(learnopengl)

#find_package(OpenGL REQUIRED)
find_package(Python REQUIRED)

add_subdirectory(external/GL)
add_subdirectory(external/glad)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/SDL)
add_subdirectory(external/stb)

file(GLOB_RECURSE SRC_SOURCES src/*.c*)
file(GLOB_RECURSE SRC_HEADERS src/*.h*)

add_executable(${PROJECT_NAME} ${SRC_SOURCES} ${SRC_HEADERS})


target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        glm
        stb
        libglew_static
        SDL2main
        SDL2-static
	glad
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        glfw
        glm
        stb
        SDL2main
        SDL2-static
	glad
)

# This command will copy your assets folder to your running directory, in order to have access to your shaders, textures, etc
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/assets/ $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets)

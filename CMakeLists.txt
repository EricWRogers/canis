set(CANIS_ENGINE_LIB CanisEngine)

set(CANISENGINE_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")
message(STATUS "CANISENGINE_INCLUDE_DIRS ${CANISENGINE_INCLUDE_DIRS}")


file(GLOB_RECURSE CANIS_SOURCES src/*.c*)
file(GLOB_RECURSE CANIS_HEADERS include/Canis/*.h*)

if(CANIS_ENABLE_EDITOR)
    list(REMOVE_ITEM CANIS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/EditorStub.cpp")
else()
    list(REMOVE_ITEM CANIS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Editor.cpp")
endif()

set(CANIS_ENGINE_LIBRARY_TYPE SHARED)
if(CANIS_PLATFORM_WEB)
    set(CANIS_ENGINE_LIBRARY_TYPE STATIC)
endif()

add_library(${CANIS_ENGINE_LIB} ${CANIS_ENGINE_LIBRARY_TYPE} ${CANIS_HEADERS} ${CANIS_SOURCES})
set_target_properties(${CANIS_ENGINE_LIB} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(${CANIS_ENGINE_LIB}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<TARGET_PROPERTY:yaml-cpp,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(${CANIS_ENGINE_LIB}
    PUBLIC
    glm
    imgui
    PRIVATE
    yaml-cpp
    SDL3::SDL3
    stb
    Jolt
)

target_compile_definitions(${CANIS_ENGINE_LIB}
    PUBLIC
    CANIS_EDITOR=${CANIS_EDITOR_DEFINE}
)

if(CANIS_ENABLE_EDITOR)
    target_link_libraries(${CANIS_ENGINE_LIB}
        PRIVATE
        imguizmo
    )
endif()

if(NOT CANIS_PLATFORM_WEB)
    target_link_libraries(${CANIS_ENGINE_LIB}
        PRIVATE
        libglew_static
    )
endif()

if(NOT TARGET Freetype::Freetype)
    message(FATAL_ERROR
        "CanisEngine requires the vendored FreeType submodule target. "
        "Run: git submodule update --init --recursive external/freetype")
endif()

target_link_libraries(${CANIS_ENGINE_LIB} PRIVATE Freetype::Freetype)
target_compile_definitions(${CANIS_ENGINE_LIB} PRIVATE CANIS_HAS_FREETYPE)
message(STATUS "CanisEngine: using vendored FreeType submodule")

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    glm
    yaml-cpp
    imgui
    ${CANIS_ENGINE_LIB}
    PRIVATE
    SDL3::SDL3
    stb
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    CANIS_EDITOR=${CANIS_EDITOR_DEFINE}
)

if(CANIS_ENABLE_EDITOR)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        imguizmo
    )
endif()

if(NOT CANIS_PLATFORM_WEB)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        libglew_static
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(CANIS_PLATFORM_WEB)
    set(CANIS_WEB_STACK_SIZE "5242880" CACHE STRING "Stack size in bytes for web exports")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME index
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${CANIS_WEB_EXPORT_DIRECTORY}"
    )

    target_link_options(${PROJECT_NAME}
        PRIVATE
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sDECLARE_ASM_MODULE_EXPORTS=0"
        "SHELL:-sFORCE_FILESYSTEM=1"
        "SHELL:-sNO_EXIT_RUNTIME=1"
        "SHELL:-sSTACK_SIZE=${CANIS_WEB_STACK_SIZE}"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/project/assets@/assets"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/project/project.canis@/project.canis"
    )
endif()

target_precompile_headers(${PROJECT_NAME}
    PUBLIC
    <string>
    <vector>
)

#add_custom_command(TARGET CanisEngine POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        $<TARGET_FILE:${PROJECT_NAME}>
#        $<TARGET_FILE_DIR:CanisEngine>
#)

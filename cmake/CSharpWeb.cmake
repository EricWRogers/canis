find_program(CANIS_DOTNET_EXECUTABLE dotnet REQUIRED)
set(CANIS_WEB_MANAGED_INPUT "" CACHE PATH "Desktop-exported managed directory containing Game.Runtime.dll and generated bindings")
if(NOT EXISTS "${CANIS_WEB_MANAGED_INPUT}/Game.Runtime.dll" OR NOT EXISTS "${CANIS_WEB_MANAGED_INPUT}/Canis.Native.generated.cs")
    message(FATAL_ERROR "C# web builds require CANIS_WEB_MANAGED_INPUT; use scripts/build-csharp-web.sh.")
endif()
set(_canis_web_host "${CMAKE_CURRENT_SOURCE_DIR}/managed/Canis.WebHost")
set(CANIS_WEB_START_SCENE "" CACHE STRING "Optional assets/...scene path for the browser player")
if(CANIS_WEB_START_SCENE AND NOT CANIS_WEB_START_SCENE MATCHES "^assets/[A-Za-z0-9_./ -]+\\.scene$")
    message(FATAL_ERROR "CANIS_WEB_START_SCENE must be an assets/...scene path using letters, digits, spaces, dots, dashes or underscores.")
endif()
configure_file("${_canis_web_host}/launch.js.in" "${CMAKE_BINARY_DIR}/managed/launch.js" @ONLY)
file(GLOB_RECURSE _canis_web_sources CONFIGURE_DEPENDS
    "${_canis_web_host}/*.cs" "${_canis_web_host}/*.csproj" "${_canis_web_host}/*.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/managed/Canis.Core/*.cs" "${CMAKE_CURRENT_SOURCE_DIR}/managed/Canis.Core/*.csproj")
list(FILTER _canis_web_sources EXCLUDE REGEX "/(obj|bin)/")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/managed/web.stamp"
    COMMAND "${CANIS_DOTNET_EXECUTABLE}" publish "${_canis_web_host}/Canis.WebHost.csproj" -c Release
        "-p:CanisGameAssembly=${CANIS_WEB_MANAGED_INPUT}/Game.Runtime.dll"
        --artifacts-path "${CMAKE_BINARY_DIR}/managed/artifacts"
        --output "${CMAKE_BINARY_DIR}/managed/publish" --nologo
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CANIS_WEB_EXPORT_DIRECTORY}/managed"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_BINARY_DIR}/managed/publish/wwwroot" "${CANIS_WEB_EXPORT_DIRECTORY}/managed"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_canis_web_host}/main.js" "${CANIS_WEB_EXPORT_DIRECTORY}/managed/main.js"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CANIS_WEB_MANAGED_INPUT}/Canis.Native.generated.cs" "${CANIS_WEB_EXPORT_DIRECTORY}/managed/Canis.Native.generated.cs"
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_BINARY_DIR}/managed/web.stamp"
    DEPENDS ${_canis_web_sources} "${CMAKE_CURRENT_LIST_FILE}" "${CANIS_WEB_MANAGED_INPUT}/Game.Runtime.dll" "${CANIS_WEB_MANAGED_INPUT}/Canis.Native.generated.cs"
    VERBATIM)
add_custom_target(CanisWebHost DEPENDS "${CMAKE_BINARY_DIR}/managed/web.stamp")
add_dependencies(${PROJECT_NAME} CanisWebHost)
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_BINARY_DIR}/managed/launch.js" "${_canis_web_host}/pre.js"
    "${CANIS_WEB_MANAGED_INPUT}/Game.Runtime.dll" "${CANIS_PROJECT_CONFIG}")
target_compile_options(${CANIS_ENGINE_LIB} PUBLIC -fexceptions)
target_link_options(${PROJECT_NAME} PRIVATE
    -fexceptions
    "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall"
    "SHELL:--pre-js ${CMAKE_BINARY_DIR}/managed/launch.js"
    "SHELL:--pre-js ${_canis_web_host}/pre.js")

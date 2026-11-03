# Keep each source's generated editor bindings in sync for native builds,
# including edits made outside the built-in script editor.
function(canis_generate_editor_exports target include_root source_root)
    if(NOT TARGET CanisScriptExports)
        return()
    endif()
    get_target_property(target_sources ${target} SOURCES)
    foreach(source IN LISTS target_sources)
        if(NOT source MATCHES "\\.cpp$")
            continue()
        endif()
        get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(RELATIVE_PATH relative "${source_root}" "${source}")
        string(REGEX REPLACE "\\.cpp$" ".hpp" header "${include_root}/${relative}")
        if(NOT EXISTS "${header}")
            string(REGEX REPLACE "\\.cpp$" ".h" header "${include_root}/${relative}")
        endif()
        if(NOT EXISTS "${header}")
            continue()
        endif()
        string(SHA256 export_id "${header}")
        set(stamp "${CMAKE_CURRENT_BINARY_DIR}/generated/editor-exports/${export_id}.stamp")
        add_custom_command(OUTPUT "${stamp}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/editor-exports"
            COMMAND $<TARGET_FILE:CanisScriptExports> "${header}" "${source}"
            COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
            DEPENDS CanisScriptExports "${header}" "${source}"
            VERBATIM)
        # A stamp makes the generated edits visible to Ninja before compilation.
        # Authored sources are deliberately not BYPRODUCTS: clean must retain them.
        set_property(SOURCE "${source}" APPEND PROPERTY OBJECT_DEPENDS "${stamp}")
    endforeach()
endfunction()

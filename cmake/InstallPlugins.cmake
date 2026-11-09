# Copies built VST3 / AU bundles into the user plug-in folders so DAWs
# pick up the latest build without a manual drag. Invoked as a POST_BUILD
# step with -DPLUGIN_SRC=... -DPLUGIN_KIND=VST3|AU -DPRODUCT_NAME=...

if(NOT DEFINED PLUGIN_SRC OR NOT DEFINED PLUGIN_KIND OR NOT DEFINED PRODUCT_NAME)
    message(FATAL_ERROR "InstallPlugins.cmake requires PLUGIN_SRC, PLUGIN_KIND, PRODUCT_NAME")
endif()

if(NOT EXISTS "${PLUGIN_SRC}")
    message(WARNING "InstallPlugins: source missing: ${PLUGIN_SRC}")
    return()
endif()

if(APPLE)
    # PLUGIN_DEST (full bundle path) or PLUGIN_DEST_DIR (folder) override the
    # scanned plug-in folder, e.g. for the private FX copy the Max device loads.
    if(DEFINED PLUGIN_DEST AND NOT PLUGIN_DEST STREQUAL "")
        set(_dest "${PLUGIN_DEST}")
    elseif(DEFINED PLUGIN_DEST_DIR AND NOT PLUGIN_DEST_DIR STREQUAL "")
        get_filename_component(_src_name "${PLUGIN_SRC}" NAME)
        set(_dest "${PLUGIN_DEST_DIR}/${_src_name}")
    elseif(PLUGIN_KIND STREQUAL "VST3")
        set(_dest "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/${PRODUCT_NAME}.vst3")
    elseif(PLUGIN_KIND STREQUAL "AU")
        set(_dest "$ENV{HOME}/Library/Audio/Plug-Ins/Components/${PRODUCT_NAME}.component")
    else()
        message(STATUS "InstallPlugins: skipping unsupported kind ${PLUGIN_KIND}")
        return()
    endif()

    get_filename_component(_dest_dir "${_dest}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dest_dir}")

    # Replace any previous install atomically-ish: remove then ditto (preserves bundle bits).
    if(EXISTS "${_dest}")
        file(REMOVE_RECURSE "${_dest}")
    endif()

    execute_process(
        COMMAND ditto "${PLUGIN_SRC}" "${_dest}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "InstallPlugins: failed to replace ${PLUGIN_KIND} at ${_dest} (${_rc}): ${_err}")
    endif()
    if(NOT EXISTS "${_dest}")
        message(FATAL_ERROR "InstallPlugins: ${PLUGIN_KIND} missing after copy: ${_dest}")
    endif()
    # JUCE may rewrite moduleinfo.json after the initial sign; ditto also
    # invalidates the seal. Re-sign so hosts like Live don't skip the bundle.
    execute_process(
        COMMAND codesign --force --deep --sign - "${_dest}"
        RESULT_VARIABLE _sign_rc
        ERROR_VARIABLE _sign_err)
    if(NOT _sign_rc EQUAL 0)
        message(WARNING "InstallPlugins: codesign failed for ${_dest} (${_sign_rc}): ${_sign_err}")
    endif()
    message(STATUS "Replaced ${PLUGIN_KIND} -> ${_dest}")
elseif(WIN32)
    if(PLUGIN_KIND STREQUAL "VST3")
        set(_dest "$ENV{LOCALAPPDATA}/Programs/Common/VST3/${PRODUCT_NAME}.vst3")
        get_filename_component(_dest_dir "${_dest}" DIRECTORY)
        file(MAKE_DIRECTORY "${_dest_dir}")
        if(EXISTS "${_dest}")
            file(REMOVE_RECURSE "${_dest}")
        endif()
        file(COPY "${PLUGIN_SRC}" DESTINATION "${_dest_dir}")
        message(STATUS "Replaced VST3 -> ${_dest}")
    endif()
endif()

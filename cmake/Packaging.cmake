# Staging the deliverable. Everything a version folder needs, and nothing else.
#
# The order matters: deploy the Qt runtime, prune what it over-collects, add the
# app-local runtime, sign, then hash. Hashing before signing would hash the
# unsigned bytes.

include_guard(GLOBAL)

if(NOT WIN32)
    return()
endif()

set(SQUIFLOW_STAGE_DIR "${CMAKE_BINARY_DIR}/stage/${SQUIFLOW_VERSION}")

find_program(SQUIFLOW_WINDEPLOYQT windeployqt HINTS "${QT_ROOT}/bin")

add_custom_target(squiflow_stage
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${SQUIFLOW_STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SQUIFLOW_STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:squiflow> "${SQUIFLOW_STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:squiflow_updater> "${SQUIFLOW_STAGE_DIR}"
    COMMAND ${SQUIFLOW_WINDEPLOYQT}
            --release --no-translations --no-system-d3d-compiler
            --no-opengl-sw --qmldir "${CMAKE_SOURCE_DIR}/src/ui"
            "${SQUIFLOW_STAGE_DIR}/squiflow.exe"
    COMMAND ${CMAKE_COMMAND}
            -DSTAGE_DIR=${SQUIFLOW_STAGE_DIR}
            -DPRUNE_LIST=${CMAKE_SOURCE_DIR}/packaging/prune.txt
            -P ${CMAKE_SOURCE_DIR}/packaging/prune.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/packaging/licenses" "${SQUIFLOW_STAGE_DIR}/licenses"
    DEPENDS squiflow squiflow_updater
    COMMENT "Staging ${SQUIFLOW_VERSION}")

# The manifest is written after signing, so the hashes cover the signed bytes.
add_custom_target(squiflow_manifest
    COMMAND ${CMAKE_COMMAND} -E env python3
            "${CMAKE_SOURCE_DIR}/packaging/manifest.py"
            --stage "${SQUIFLOW_STAGE_DIR}"
            --version "${SQUIFLOW_VERSION}"
            --out "${SQUIFLOW_STAGE_DIR}/manifest.json"
    DEPENDS squiflow_stage
    COMMENT "Writing the update manifest")

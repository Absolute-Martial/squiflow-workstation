include_guard(GLOBAL)
include(GNUInstallDirs)

install(TARGETS squiflow_workstation RUNTIME DESTINATION . COMPONENT Runtime)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/packaging/licenses/" DESTINATION licenses COMPONENT Runtime)

if(NOT WIN32)
    return()
endif()

set(SQUIFLOW_STAGE_DIR "${CMAKE_BINARY_DIR}/stage/SquiFlow-${SQUIFLOW_VERSION}-windows-x64")
set(SQUIFLOW_ARCHIVE_DIR "${CMAKE_BINARY_DIR}/artifacts")
set(SQUIFLOW_ARCHIVE "${SQUIFLOW_ARCHIVE_DIR}/SquiFlow-${SQUIFLOW_VERSION}-windows-x64.zip")

if(SQUIFLOW_WITH_QT)
    find_program(SQUIFLOW_WINDEPLOYQT NAMES windeployqt windeployqt6 HINTS "${QT_ROOT}/bin" REQUIRED)
else()
    set(SQUIFLOW_WINDEPLOYQT "")
endif()

add_custom_target(squiflow_stage
    COMMAND "${CMAKE_COMMAND}"
        -DEXECUTABLE=$<TARGET_FILE:squiflow_workstation>
        -DSTAGE_DIR=${SQUIFLOW_STAGE_DIR}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DWITH_QT=${SQUIFLOW_WITH_QT}
        -DWINDEPLOYQT=${SQUIFLOW_WINDEPLOYQT}
        -P "${CMAKE_SOURCE_DIR}/packaging/stage_windows.cmake"
    DEPENDS squiflow_workstation
    VERBATIM
    COMMENT "Staging the Windows runtime bundle")

add_custom_target(squiflow_archive
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SQUIFLOW_ARCHIVE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${SQUIFLOW_ARCHIVE}"
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${SQUIFLOW_ARCHIVE}" --format=zip -- .
    WORKING_DIRECTORY "${SQUIFLOW_STAGE_DIR}"
    DEPENDS squiflow_stage
    VERBATIM
    COMMENT "Creating ${SQUIFLOW_ARCHIVE}")

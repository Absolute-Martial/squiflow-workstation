# Derives the version from the git tag. One source of truth for the header,
# the installer and the update manifest.

find_package(Git QUIET)

set(SQUIFLOW_VERSION "0.0.0")
set(SQUIFLOW_VERSION_TAG "untagged")
set(SQUIFLOW_VERSION_COMMIT "unknown")

if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE SQUIFLOW_VERSION_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE SQUIFLOW_VERSION_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _m "${SQUIFLOW_VERSION_TAG}")
    if(_m)
        set(SQUIFLOW_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
    endif()
endif()

message(STATUS "SquiFlow version ${SQUIFLOW_VERSION} (${SQUIFLOW_VERSION_TAG})")

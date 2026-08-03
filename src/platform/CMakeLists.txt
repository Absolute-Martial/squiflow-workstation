# The platform boundary: the only place that is allowed to know which operating
# system this is. Everything above it compiles with no knowledge of Windows.
#
# Discovery of the two roots is the one part that differs per lane. The Qt
# implementation ships; the standard-library implementation exists so the
# verification machine, which has neither Qt nor Windows, can run the real
# resolver against real directories.

add_library(squiflow_platform STATIC
    paths.cpp
    local_directory_probe.cpp
    log_record.cpp
    log_clock.cpp
    log_formatter.cpp
    log_level_policy.cpp
    log_throttle.cpp
    log_backtrace.cpp
    async_log_sink.cpp
    local_log_storage.cpp
    rotating_log_file.cpp
    logger.cpp
    crash_breadcrumb.cpp)

add_library(squiflow::platform ALIAS squiflow_platform)

target_include_directories(squiflow_platform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# The logging dispatcher. Private and SYSTEM: no target above the platform
# boundary sees spdlog, and its headers never produce our warnings.
target_include_directories(squiflow_platform SYSTEM
    PRIVATE ${PROJECT_SOURCE_DIR}/external/spdlog/include
            ${PROJECT_SOURCE_DIR}/external/crashcatch/include)

if(WIN32)
    target_sources(squiflow_platform PRIVATE crash_handler_win.cpp)
    target_link_libraries(squiflow_platform PRIVATE DbgHelp User32)
else()
    target_sources(squiflow_platform PRIVATE crash_handler_posix.cpp)
endif()

if(SQUIFLOW_WITH_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    target_sources(squiflow_platform PRIVATE path_environment_qt.cpp)
    target_link_libraries(squiflow_platform PRIVATE Qt6::Core)
else()
    target_sources(squiflow_platform PRIVATE path_environment_posix.cpp)
    message(STATUS
        "SQUIFLOW_WITH_QT=OFF: platform discovery uses the standard-library "
        "implementation. This is the verification lane, not a shippable build.")
endif()

target_link_libraries(squiflow_platform
    PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)

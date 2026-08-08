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
    crash_breadcrumb.cpp
    single_instance_name.cpp
    secret_buffer.cpp
    secret_identity.cpp
    secret_envelope.cpp
    network_state.cpp
    network_monitor.cpp
    background_service.cpp
    background_executor.cpp
    background_supervisor.cpp
    password_hash.cpp)

add_library(squiflow::platform ALIAS squiflow_platform)

find_package(unofficial-sodium CONFIG QUIET)
if(TARGET unofficial-sodium::sodium)
    set(SQUIFLOW_SODIUM_TARGET unofficial-sodium::sodium)
elseif(SQUIFLOW_SODIUM_ROOT)
    add_library(squiflow_sodium STATIC IMPORTED GLOBAL)
    set_target_properties(squiflow_sodium PROPERTIES
        IMPORTED_LOCATION "${SQUIFLOW_SODIUM_ROOT}/lib/libsodium.a"
        INTERFACE_INCLUDE_DIRECTORIES "${SQUIFLOW_SODIUM_ROOT}/include")
    set(SQUIFLOW_SODIUM_TARGET squiflow_sodium)
else()
    message(FATAL_ERROR
        "libsodium 1.0.22 is required for guarded secret memory. "
        "Install it through vcpkg or set SQUIFLOW_SODIUM_ROOT.")
endif()

target_include_directories(squiflow_platform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# The logging dispatcher. Private and SYSTEM: no target above the platform
# boundary sees spdlog, and its headers never produce our warnings.
target_include_directories(squiflow_platform SYSTEM
    PRIVATE ${SQUIFLOW_ROOT}/external/spdlog/include
            ${SQUIFLOW_ROOT}/external/crashcatch/include)

if(WIN32)
    target_sources(squiflow_platform PRIVATE crash_handler_win.cpp single_instance_win.cpp secrets_dpapi.cpp)
    target_link_libraries(squiflow_platform PRIVATE Advapi32 Crypt32 DbgHelp User32)
else()
    target_sources(squiflow_platform PRIVATE crash_handler_posix.cpp single_instance_posix.cpp)
endif()

if(SQUIFLOW_WITH_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core Network)
    target_sources(squiflow_platform PRIVATE path_environment_qt.cpp network_state_qt.cpp)
    target_link_libraries(squiflow_platform PRIVATE Qt6::Core Qt6::Network)
else()
    target_sources(squiflow_platform PRIVATE path_environment_posix.cpp)
    message(STATUS
        "SQUIFLOW_WITH_QT=OFF: platform discovery uses the standard-library "
        "implementation. This is the verification lane, not a shippable build.")
endif()

target_link_libraries(squiflow_platform
    PRIVATE ${SQUIFLOW_SODIUM_TARGET}
            squiflow::warnings squiflow::hardening squiflow::sanitizers)

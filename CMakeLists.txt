cmake_minimum_required(VERSION 3.26)

project(SquiFlow
    VERSION 0.1.0
    DESCRIPTION "SquiFlow workstation application"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Global language and warning policy
#
# The whole project is held to C++23. Note: on the reference build machine
# used during initial development (GCC 11.5.0), the compiler accepts
# -std=c++23 but its standard library only implements a subset of the
# feature set (no <expected>, <format>, <print>, <stacktrace>, <flat_map>,
# <generator>). See docs/plan/language-and-verification.md. The integrity
# gate enforces the exclusion list so both build lanes stay compatible with
# whichever compiler is actually present.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

include(cmake/Version.cmake)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(SQUIFLOW_WITH_SQLITE "Build the SQLite-backed storage implementation" ON)
option(SQUIFLOW_WITH_QT "Build the Qt/QML interface layer" ON)
option(SQUIFLOW_WARNINGS_AS_ERRORS "Treat all warnings as errors" ON)
option(SQUIFLOW_ENABLE_SANITIZERS "Build with address+undefined sanitizers" OFF)
option(SQUIFLOW_BUILD_TESTS "Build the test suites" ON)

if(SQUIFLOW_WITH_QT)
    # Qt imported targets (Qt6::Core, Qt6::Qml, ...) are only visible in the
    # directory that called find_package and its subdirectories. The
    # workstation executable is created in src/app while the QML module is
    # registered from src/ui, so the find must live here at the top so every
    # subdirectory can reference the targets at generate time.
    find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.11.1)
endif()

# ---------------------------------------------------------------------------
# Shared compile options, applied via an INTERFACE target so every module
# picks them up the same way instead of copy-pasting compiler flags.
# ---------------------------------------------------------------------------

add_library(squiflow_compiler_options INTERFACE)

if(MSVC)
    target_compile_options(squiflow_compiler_options INTERFACE
        /W4
        $<$<BOOL:${SQUIFLOW_WARNINGS_AS_ERRORS}>:/WX>
    )
else()
    target_compile_options(squiflow_compiler_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wold-style-cast
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        $<$<BOOL:${SQUIFLOW_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

if(SQUIFLOW_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(squiflow_compiler_options INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(squiflow_compiler_options INTERFACE
        -fsanitize=address,undefined
    )
endif()

# ---------------------------------------------------------------------------
# Module declaration helper and dependency-graph checks.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Testing
# ---------------------------------------------------------------------------

if(SQUIFLOW_BUILD_TESTS)
    enable_testing()
    include(CTest)

    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 3 not found via find_package; "
                        "tests that need it will be skipped in this configure. "
                        "On the shop machine, install Catch2 3 (e.g. via vcpkg) "
                        "before configuring.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Subdirectories
#
# Each is added only once its own sub-phase has actually written it, so a
# fresh checkout at any commit always configures cleanly for whatever has
# been completed so far.
# ---------------------------------------------------------------------------

add_subdirectory(src)

if(SQUIFLOW_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Deployment and release targets are defined only after the executable exists.
include(cmake/Packaging.cmake)

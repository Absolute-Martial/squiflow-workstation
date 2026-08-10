# Build-time acceleration. Neither of these changes what gets built, only how
# fast repeated builds get there, so both default on and either can be turned
# off with one cache variable if it ever misbehaves on a given machine.
#
# Measured on the Qt-off/SQLite-off verification lane (GCC 13.3, 1 core):
# see docs/plan/build-performance.md for the before/after numbers. The
# headers below were not guessed - they are the <system> headers that show
# up in more than ten translation units under src/, found by counting
# #include lines project-wide (`grep -rh '^#include <' src | sort | uniq -c`).

option(SQUIFLOW_USE_CCACHE "Wrap the compiler with ccache/sccache if one is on PATH" ON)
option(SQUIFLOW_USE_PCH "Precompile the standard-library headers most translation units pull in" ON)

if(SQUIFLOW_USE_CCACHE)
    find_program(SQUIFLOW_CCACHE_PROGRAM NAMES sccache ccache)
    if(SQUIFLOW_CCACHE_PROGRAM)
        # A normal (non-cache) variable is enough: every add_library/
        # add_executable below this point in src/CMakeLists.txt picks it up
        # as each target is created, which is all of them.
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SQUIFLOW_CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${SQUIFLOW_CCACHE_PROGRAM}")
        message(STATUS "Compiler cache: ${SQUIFLOW_CCACHE_PROGRAM}")
    else()
        message(STATUS "SQUIFLOW_USE_CCACHE=ON but neither sccache nor ccache "
                        "was found on PATH; compiling without a cache")
    endif()
endif()

set(SQUIFLOW_COMMON_PCH_HEADERS
    <string>
    <string_view>
    <vector>
    <memory>
    <functional>
    <optional>
    <utility>
    <algorithm>
    <stdexcept>
    <cstdint>
    <cstddef>
    <array>
)

# The Qt-facing headers a squiflow_shell/squiflow_workstation translation
# unit typically pays for: QString/QObject appear in far fewer files than
# <string> does, but each one is far more expensive to parse, so they are
# kept as a separate list applied only where SQUIFLOW_WITH_QT is on.
#
# NOTE: this list has not been build-verified in this pass - the sandbox this
# optimization pass ran in has no Qt 6.11 installation and no display server,
# so only the STL list above has an actual measured before/after. Confirm the
# clean-build delta on a real Qt/MSVC or Qt/Linux machine before trusting the
# number for this half.
set(SQUIFLOW_QT_PCH_HEADERS
    <QString>
    <QObject>
    <QCoreApplication>
)

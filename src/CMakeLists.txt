# Complete source-tree aggregation. The ordering is architectural: protocol is
# below the engine, the platform boundary depends on nothing of ours at all,
# modules sit above engine and protocol, and workflows compose modules.

set(SQUIFLOW_ROOT "${CMAKE_SOURCE_DIR}")

include("${SQUIFLOW_ROOT}/cmake/Warnings.cmake")
include("${SQUIFLOW_ROOT}/cmake/Hardening.cmake")
include("${SQUIFLOW_ROOT}/cmake/Sanitizers.cmake")
include("${SQUIFLOW_ROOT}/cmake/SquiflowModule.cmake")
include("${SQUIFLOW_ROOT}/cmake/ModuleGraph.cmake")

add_subdirectory("${SQUIFLOW_ROOT}/external/protocol"
                 "${CMAKE_BINARY_DIR}/external/protocol")
add_subdirectory(platform)
add_subdirectory(engine)
add_subdirectory(modules)
squiflow_check_module_graph()
add_subdirectory(workflows)
add_subdirectory(app)
add_subdirectory(shell)

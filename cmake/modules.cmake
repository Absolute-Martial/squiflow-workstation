# ---------------------------------------------------------------------------
# squiflow_add_module
#
# Declares one module as a static library target named
# squiflow_module_<name>, and records its position in the module dependency
# graph so squiflow_check_module_graph() can verify the whole graph later.
#
# Usage:
#   squiflow_add_module(
#       NAME <name>
#       SOURCES <file> [<file> ...]
#       INCLUDE_DIRS <dir> [<dir> ...]
#       [DEPENDS <other-module-name> [<other-module-name> ...]]
#   )
#
# NAME must match the module's directory name under src/modules/ and must
# also be the exact spelling used in src/protocol/module_id.def, so a module
# can never exist in the build graph under a different name than the one the
# protocol layer knows about.
#
# DEPENDS lists other modules (by NAME) this module's *code* directly
# includes headers from or calls into. It does not need to include
# transitive dependencies; the graph check computes closure itself. Omitting
# a real dependency here does not fail the build (CMake will still link
# transitively through squiflow_module_framework in most cases), but it will
# fail squiflow_check_module_graph() once that dependency shows up in actual
# #include lines checked by the integrity gate, so declare it honestly.
# ---------------------------------------------------------------------------

set_property(GLOBAL PROPERTY SQUIFLOW_MODULE_NAMES "")

function(squiflow_add_module)
    set(options "")
    set(one_value_args NAME)
    set(multi_value_args SOURCES INCLUDE_DIRS DEPENDS)
    cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "squiflow_add_module: NAME is required")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "squiflow_add_module(${ARG_NAME}): SOURCES is required and must not be empty")
    endif()

    set(target_name "squiflow_module_${ARG_NAME}")

    add_library(${target_name} STATIC ${ARG_SOURCES})

    target_link_libraries(${target_name} PUBLIC squiflow_compiler_options)

    if(ARG_INCLUDE_DIRS)
        target_include_directories(${target_name} PUBLIC ${ARG_INCLUDE_DIRS})
    endif()

    foreach(dep IN LISTS ARG_DEPENDS)
        target_link_libraries(${target_name} PUBLIC "squiflow_module_${dep}")
    endforeach()

    set_property(TARGET ${target_name} PROPERTY SQUIFLOW_MODULE_NAME "${ARG_NAME}")
    set_property(TARGET ${target_name} PROPERTY SQUIFLOW_MODULE_DEPENDS "${ARG_DEPENDS}")

    get_property(existing GLOBAL PROPERTY SQUIFLOW_MODULE_NAMES)
    list(APPEND existing "${ARG_NAME}")
    set_property(GLOBAL PROPERTY SQUIFLOW_MODULE_NAMES "${existing}")
endfunction()

# ---------------------------------------------------------------------------
# squiflow_check_module_graph
#
# Walks every module registered via squiflow_add_module and fails the
# configure step if:
#   - any module lists a DEPENDS entry that was never itself declared with
#     squiflow_add_module (a dependency on a module that does not exist), or
#   - the dependency graph contains a cycle.
#
# This is a configure-time check (it runs while CMake is processing the
# build files, not at build or test time), so a cyclic or dangling module
# graph is caught before a single source file is compiled.
# ---------------------------------------------------------------------------

function(squiflow_check_module_graph)
    get_property(all_modules GLOBAL PROPERTY SQUIFLOW_MODULE_NAMES)

    # Dangling dependency check.
    foreach(mod IN LISTS all_modules)
        get_property(deps TARGET "squiflow_module_${mod}" PROPERTY SQUIFLOW_MODULE_DEPENDS)
        foreach(dep IN LISTS deps)
            if(NOT dep IN_LIST all_modules)
                message(FATAL_ERROR
                    "Module graph error: '${mod}' depends on '${dep}', "
                    "which was never declared with squiflow_add_module.")
            endif()
        endforeach()
    endforeach()

    # Cycle check: iterative DFS per module with an explicit stack, since
    # CMake script functions cannot recurse into themselves cleanly.
    foreach(start_mod IN LISTS all_modules)
        set(visiting "${start_mod}")
        set(stack "${start_mod}")

        while(stack)
            list(GET stack -1 current)
            list(REMOVE_AT stack -1)

            get_property(deps TARGET "squiflow_module_${current}" PROPERTY SQUIFLOW_MODULE_DEPENDS)
            foreach(dep IN LISTS deps)
                if(dep STREQUAL start_mod)
                    message(FATAL_ERROR
                        "Module graph error: cycle detected involving '${start_mod}' "
                        "(reached back from '${current}').")
                endif()
                list(APPEND stack "${dep}")
            endforeach()
        endwhile()
    endforeach()

    list(LENGTH all_modules module_count)
    message(STATUS "Module graph check passed: ${module_count} module(s), acyclic.")
endfunction()

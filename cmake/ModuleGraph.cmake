# The checks that make the architecture real. These fail the build. A warning
# about a dependency cycle is a dependency cycle that ships.

include_guard(GLOBAL)

function(_squiflow_visit MODULE_NAME PATH_SO_FAR)
    get_property(_state GLOBAL PROPERTY SQUIFLOW_VISIT_${MODULE_NAME})
    if(_state STREQUAL "done")
        return()
    endif()
    if(_state STREQUAL "visiting")
        message(FATAL_ERROR
            "module dependency cycle: ${PATH_SO_FAR} -> ${MODULE_NAME}")
    endif()
    set_property(GLOBAL PROPERTY SQUIFLOW_VISIT_${MODULE_NAME} "visiting")
    get_property(_deps GLOBAL PROPERTY SQUIFLOW_MODULE_REQUIRES_${MODULE_NAME})
    foreach(dep IN LISTS _deps)
        _squiflow_visit(${dep} "${PATH_SO_FAR} -> ${MODULE_NAME}")
    endforeach()
    set_property(GLOBAL PROPERTY SQUIFLOW_VISIT_${MODULE_NAME} "done")
endfunction()

function(squiflow_check_module_graph)
    get_property(_modules GLOBAL PROPERTY SQUIFLOW_MODULE_LIST)

    if(NOT _modules)
        message(FATAL_ERROR "no modules were declared")
    endif()

    # 1. Every declared requirement must be a module that exists.
    foreach(m IN LISTS _modules)
        get_property(_deps GLOBAL PROPERTY SQUIFLOW_MODULE_REQUIRES_${m})
        foreach(dep IN LISTS _deps)
            if(NOT dep IN_LIST _modules)
                message(FATAL_ERROR "module ${m} requires ${dep}, which does not exist")
            endif()
        endforeach()
    endforeach()

    # 2. Core must be closed under dependency: a core module may never require
    #    an extra, because switching that extra off would break something that
    #    cannot be switched off.
    foreach(m IN LISTS _modules)
        get_property(_tier GLOBAL PROPERTY SQUIFLOW_MODULE_TIER_${m})
        if(_tier STREQUAL "core")
            get_property(_deps GLOBAL PROPERTY SQUIFLOW_MODULE_REQUIRES_${m})
            foreach(dep IN LISTS _deps)
                get_property(_dep_tier GLOBAL PROPERTY SQUIFLOW_MODULE_TIER_${dep})
                if(_dep_tier STREQUAL "extra")
                    message(FATAL_ERROR
                        "core module ${m} requires extra module ${dep}; "
                        "core must be closed under dependency")
                endif()
            endforeach()
        endif()
    endforeach()

    # 3. The graph must be acyclic.
    foreach(m IN LISTS _modules)
        _squiflow_visit(${m} "")
    endforeach()

    # 4. Every module directory on disk must be a declared module. Catches the
    #    module someone wrote and forgot to register.
    if(NOT SQUIFLOW_ROOT)
        message(FATAL_ERROR "SQUIFLOW_ROOT is not set; the top-level "
                            "CMakeLists.txt must set it before this runs")
    endif()
    file(GLOB _dirs RELATIVE "${SQUIFLOW_ROOT}/src/modules"
         "${SQUIFLOW_ROOT}/src/modules/*")
    foreach(d IN LISTS _dirs)
        if(IS_DIRECTORY "${SQUIFLOW_ROOT}/src/modules/${d}")
            if(NOT d IN_LIST _modules)
                message(FATAL_ERROR
                    "src/modules/${d} exists on disk but is not a declared module")
            endif()
        endif()
    endforeach()

    list(LENGTH _modules _count)
    message(STATUS "Module graph verified: ${_count} modules, acyclic, core closed")
endfunction()

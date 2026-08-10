# The only way a module is declared. A module cannot grant itself a dependency
# anywhere else, which is what makes the dependency rules checkable.
#
#   squiflow_add_module(orders
#       TIER core
#       REQUIRES catalog pricing parties
#       SOURCES module.cpp domain/order.cpp ...
#       UI      view/OrderListView.qml ...)

include_guard(GLOBAL)

set_property(GLOBAL PROPERTY SQUIFLOW_MODULE_LIST "")

function(squiflow_add_module MODULE_NAME)
    set(options "")
    set(one_value TIER)
    set(multi_value REQUIRES SOURCES UI TEST_SOURCES)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT ARG_TIER)
        message(FATAL_ERROR "module ${MODULE_NAME}: TIER is required (core or extra)")
    endif()
    if(NOT ARG_TIER STREQUAL "core" AND NOT ARG_TIER STREQUAL "extra")
        message(FATAL_ERROR "module ${MODULE_NAME}: TIER must be core or extra")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "module ${MODULE_NAME}: SOURCES is required")
    endif()

    set(target squiflow_module_${MODULE_NAME})
    add_library(${target} STATIC ${ARG_SOURCES})
    add_library(squiflow::module::${MODULE_NAME} ALIAS ${target})

    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

    # A module sees the protocol, the engine, and the modules it declared.
    # It cannot see the shell, the workflows, or any undeclared module.
    target_link_libraries(${target}
        PUBLIC  squiflow::protocol squiflow::engine
        PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)

    foreach(dep IN LISTS ARG_REQUIRES)
        target_link_libraries(${target} PUBLIC squiflow::module::${dep})
    endforeach()

    # One choke point, same as every other cross-cutting concern in this
    # function: every module gets the shared PCH without having to ask.
    if(SQUIFLOW_USE_PCH AND SQUIFLOW_COMMON_PCH_HEADERS)
        target_precompile_headers(${target} PRIVATE ${SQUIFLOW_COMMON_PCH_HEADERS})
    endif()

    set_target_properties(${target} PROPERTIES
        FOLDER "modules"
        SQUIFLOW_MODULE_TIER "${ARG_TIER}"
        SQUIFLOW_MODULE_REQUIRES "${ARG_REQUIRES}")

    get_property(_list GLOBAL PROPERTY SQUIFLOW_MODULE_LIST)
    list(APPEND _list ${MODULE_NAME})
    set_property(GLOBAL PROPERTY SQUIFLOW_MODULE_LIST "${_list}")
    set_property(GLOBAL PROPERTY SQUIFLOW_MODULE_TIER_${MODULE_NAME} "${ARG_TIER}")
    set_property(GLOBAL PROPERTY SQUIFLOW_MODULE_REQUIRES_${MODULE_NAME} "${ARG_REQUIRES}")

    if(SQUIFLOW_BUILD_TESTS AND ARG_TEST_SOURCES)
        set(test_target squiflow_module_${MODULE_NAME}_test)
        add_executable(${test_target} ${ARG_TEST_SOURCES})
        target_link_libraries(${test_target}
            PRIVATE ${target} squiflow::testing squiflow::warnings squiflow::sanitizers)
        set_target_properties(${test_target} PROPERTIES FOLDER "tests/modules")
        add_test(NAME module.${MODULE_NAME} COMMAND ${test_target})
    endif()
endfunction()

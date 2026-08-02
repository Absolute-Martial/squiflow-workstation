# Sanitizer lane. Used by a CI job and by anyone chasing a memory bug.

set(SQUIFLOW_SANITIZE "" CACHE STRING "Semicolon list: address, undefined, thread")

add_library(squiflow_sanitizers INTERFACE)
add_library(squiflow::sanitizers ALIAS squiflow_sanitizers)

if(SQUIFLOW_SANITIZE)
    string(REPLACE ";" "," _sanitize_list "${SQUIFLOW_SANITIZE}")
    if(MSVC)
        if("address" IN_LIST SQUIFLOW_SANITIZE)
            target_compile_options(squiflow_sanitizers INTERFACE /fsanitize=address)
        endif()
    else()
        target_compile_options(squiflow_sanitizers INTERFACE
            -fsanitize=${_sanitize_list} -fno-omit-frame-pointer -g)
        target_link_options(squiflow_sanitizers INTERFACE -fsanitize=${_sanitize_list})
    endif()
    message(STATUS "Sanitizers enabled: ${_sanitize_list}")
endif()

# Hardening applied to every target. Cheap at run time, and the cost is paid
# once here rather than argued about per target.

add_library(squiflow_hardening INTERFACE)
add_library(squiflow::hardening ALIAS squiflow_hardening)

if(MSVC)
    target_compile_options(squiflow_hardening INTERFACE
        /sdl /GS /guard:cf
        $<$<CONFIG:Debug>:/RTC1>)
    target_link_options(squiflow_hardening INTERFACE
        /GUARD:CF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
else()
    target_compile_options(squiflow_hardening INTERFACE
        -fstack-protector-strong
        $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
endif()

# Standard library assertions in debug builds. Catches the out-of-range access
# that would otherwise be undefined behaviour.
target_compile_definitions(squiflow_hardening INTERFACE
    $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=1>)

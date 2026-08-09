# One warning set, applied to every SquiFlow target. Nothing here is optional.

add_library(squiflow_warnings INTERFACE)
add_library(squiflow::warnings ALIAS squiflow_warnings)

if(MSVC)
    target_compile_options(squiflow_warnings INTERFACE
        /W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor
        /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296
        /w14311 /w14545 /w14546 /w14547 /w14549 /w14555 /w14619
        /w14640 /w14826 /w14905 /w14906 /w14928)
else()
    target_compile_options(squiflow_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2)
    # GCC's -Wnull-dereference is an optimizer pass: at -O2 it misreports
    # std::get_if/std::vector::size inside a std::variant as possibly null
    # (GCC 13, variants with string/vector alternatives). The verification
    # lane keeps the check enabled but not fatal, exactly like
    # tools/sandbox/Makefile does, so release builds stay green.
    target_compile_options(squiflow_warnings INTERFACE
        -Wno-error=null-dereference)
endif()

option(SQUIFLOW_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
if(SQUIFLOW_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(squiflow_warnings INTERFACE /WX)
    else()
        target_compile_options(squiflow_warnings INTERFACE -Werror)
    endif()
endif()

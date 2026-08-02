// tools/sandbox/smoke_test.cpp
//
// Proves the sandbox toolchain itself: the compiler is present, the C++23
// flag is accepted, and a binary produced by this Makefile actually links
// and runs. This is the entire done-condition for sub-phase 1.1 -- nothing
// about the real project is tested here yet. That comes with the module
// graph in 1.2 onward.
//
// Deliberately exercises std::span, which is part of the subset of the
// C++23 standard library actually available with this compiler (see
// docs/plan/language-and-verification.md for the full list of what is and
// is not available).

#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace {

int sum(std::span<const int> values) {
    int total = 0;
    for (const int v : values) total += v;
    return total;
}

}  // namespace

int main() {
    std::cout << "compiler standard: __cplusplus = " << __cplusplus << '\n';

    const std::vector<int> numbers{1, 2, 3, 4, 5};
    const int total = sum(numbers);

    std::cout << "sum(1..5) via std::span = " << total << '\n';

    if (total != 15) {
        std::cerr << "smoke test failed: expected 15, got " << total << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "smoke test passed\n";
    return EXIT_SUCCESS;
}

#pragma once

// A test harness small enough to read in one sitting.
//
// No framework on purpose: this layer must compile and run on any machine with
// a compiler, including one where nothing has been installed. The real build
// uses a proper framework for the module tests; these checks are the ones that
// have to keep working even when the toolchain does not cooperate.

#include <iostream>
#include <string>

namespace squiflow::testing {

inline int g_failures = 0;
inline int g_checks = 0;

inline void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        std::cout << "  FAIL  " << what << "\n";
        ++g_failures;
    }
}

inline void section(const std::string& name) {
    std::cout << name << "\n";
}

inline int report() {
    std::cout << "\n" << g_checks << " checks, " << g_failures << " failed\n";
    return g_failures == 0 ? 0 : 1;
}

}  // namespace squiflow::testing

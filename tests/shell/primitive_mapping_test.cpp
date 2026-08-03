#include "shell/primitive_mapping.hpp"
#include "support/check.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
int main(){namespace t=squiflow::testing;using squiflow::shell::format_minor_units;t::check(format_minor_units(12345,"USD")=="123.45 USD","positive money exact");t::check(format_minor_units(-5,"USD")=="-0.05 USD","negative fraction exact");t::check(format_minor_units(0,"",0)=="0","zero scale exact");t::check(format_minor_units(std::numeric_limits<std::int64_t>::min(),"",2)=="-92233720368547758.08","minimum integer safe");bool rejected=false;try{(void)format_minor_units(1,"",7);}catch(const std::invalid_argument&){rejected=true;}t::check(rejected,"unbounded scale rejected");return t::report();}

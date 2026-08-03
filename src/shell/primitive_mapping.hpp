#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
namespace squiflow::shell {
inline std::string format_minor_units(std::int64_t amount,std::string_view currency,std::uint8_t scale=2){if(scale>6)throw std::invalid_argument("money scale exceeds six digits");std::uint64_t magnitude=amount<0?static_cast<std::uint64_t>(-(amount+1))+1U:static_cast<std::uint64_t>(amount);std::uint64_t divisor=1;for(std::uint8_t i=0;i<scale;++i)divisor*=10U;std::string number=std::to_string(magnitude/divisor);if(scale>0){auto fraction=std::to_string(magnitude%divisor);number.push_back('.');number.append(static_cast<std::size_t>(scale)-fraction.size(),'0');number+=fraction;}if(amount<0)number.insert(number.begin(),'-');if(!currency.empty()){number.push_back(' ');number.append(currency);}return number;}
}

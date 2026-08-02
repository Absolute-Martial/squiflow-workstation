#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::files {
inline constexpr std::int64_t kMaxScanItems=500;
inline constexpr std::int64_t kMaxSearchResults=500;
class FilesService {
public:
 using Clock=std::function<std::int64_t()>;
 explicit FilesService(Clock clock):clock_{std::move(clock)}{}
 void index_scan(engine::Transaction&,const Call&)const;
 std::vector<engine::Row> search(const engine::Store&,const Call&)const;
 void link(engine::Transaction&,const Call&)const;
 void forget(engine::Transaction&,const Call&)const;
private: Clock clock_;
};
}

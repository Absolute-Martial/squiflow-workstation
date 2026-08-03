#pragma once
#include <cstdint>
#include <thread>
namespace squiflow::shell {
class CompletionGate final {public:CompletionGate():owner_(std::this_thread::get_id()){}std::uint64_t begin()noexcept{return ++generation_;}void cancel()noexcept{++generation_;}bool accepts(std::uint64_t generation)const noexcept{return generation==generation_;}bool on_owner_thread()const noexcept{return std::this_thread::get_id()==owner_;}private:std::thread::id owner_;std::uint64_t generation_{0};};
}

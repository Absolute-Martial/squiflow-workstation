#pragma once
#include "platform/background_service.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>
#include <mutex>
#include <string>
namespace squiflow::platform {
using BackgroundCompletion=std::function<void(BackgroundOutcome,std::string)>;
class BackgroundExecutor final {
public:
 explicit BackgroundExecutor(std::size_t capacity=kMaxBackgroundQueue);
 BackgroundExecutor(const BackgroundExecutor&)=delete; BackgroundExecutor& operator=(const BackgroundExecutor&)=delete;
 ~BackgroundExecutor();
 SubmissionResult submit(BackgroundLane lane,std::string id,BackgroundTask task,BackgroundCompletion completion);
 ShutdownResult shutdown(BackgroundClock::duration timeout);
 std::size_t queued() const; bool stopping() const;
private:
 struct Work {std::string id;BackgroundTask task;BackgroundCompletion completion;};
 void worker(BackgroundLane lane,std::stop_token token) noexcept;
 std::deque<Work>& queue(BackgroundLane lane) noexcept;
 mutable std::mutex mutex_;std::condition_variable cv_;std::condition_variable exited_cv_;std::deque<Work> sync_;std::deque<Work> shared_;std::size_t capacity_;bool stopping_{false};std::size_t active_{0};std::size_t exited_{0};std::jthread sync_worker_;std::jthread shared_worker_;
};
}

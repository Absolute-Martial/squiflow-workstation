#include "engine/storage/writer.hpp"

#include <algorithm>

namespace squiflow::engine {

Writer::Writer(Store& store) : store_(store) {}

void Writer::skip_abandoned() {
    // Called with queue_mutex_ held. A ticket whose owner gave up must not
    // hold the queue for everyone behind it.
    while (abandoned_.erase(serving_) > 0) {
        ++serving_;
    }
}

void Writer::finish_turn() {
    {
        const std::lock_guard<std::mutex> lock(queue_mutex_);
        ++serving_;
        skip_abandoned();
    }
    turn_changed_.notify_all();
}

void Writer::write(const Work& work) {
    if (!work) {
        throw StoreError("a write needs something to do");
    }

    std::uint64_t ticket = 0;
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        ticket = next_ticket_++;
        ++waiting_;
        statistics_.peak_waiting = std::max(statistics_.peak_waiting, waiting_);
        turn_changed_.wait(lock, [this, ticket] { return serving_ == ticket; });
        --waiting_;
        ++statistics_.served;
    }

    // Whatever happens below, the next writer gets its turn. An early return
    // or a thrown exception must never leave the queue stopped.
    struct TurnGuard {
        Writer& writer;
        ~TurnGuard() {
            writer.finish_turn();
        }
    } guard{*this};

    try {
        const std::unique_lock<std::shared_mutex> data_lock(data_mutex_);
        auto transaction = store_.begin();
        work(*transaction);
        transaction->commit();
    } catch (...) {
        const std::lock_guard<std::mutex> lock(queue_mutex_);
        ++statistics_.failed;
        throw;
    }

    const std::lock_guard<std::mutex> lock(queue_mutex_);
    ++statistics_.completed;
}

bool Writer::write_within(const Work& work, std::chrono::milliseconds patience) {
    if (!work) {
        throw StoreError("a write needs something to do");
    }

    std::uint64_t ticket = 0;
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        ticket = next_ticket_++;
        ++waiting_;
        statistics_.peak_waiting = std::max(statistics_.peak_waiting, waiting_);

        const bool our_turn = turn_changed_.wait_for(
            lock, patience, [this, ticket] { return serving_ == ticket; });

        --waiting_;
        if (!our_turn) {
            // Give the ticket up rather than hold the queue. If the queue has
            // already reached it, skip past it immediately.
            abandoned_.insert(ticket);
            skip_abandoned();
            ++statistics_.abandoned;
            lock.unlock();
            turn_changed_.notify_all();
            return false;
        }
        ++statistics_.served;
    }

    struct TurnGuard {
        Writer& writer;
        ~TurnGuard() {
            writer.finish_turn();
        }
    } guard{*this};

    try {
        const std::unique_lock<std::shared_mutex> data_lock(data_mutex_);
        auto transaction = store_.begin();
        work(*transaction);
        transaction->commit();
    } catch (...) {
        const std::lock_guard<std::mutex> lock(queue_mutex_);
        ++statistics_.failed;
        throw;
    }

    const std::lock_guard<std::mutex> lock(queue_mutex_);
    ++statistics_.completed;
    return true;
}

void Writer::read(const Read& reader) const {
    if (!reader) {
        throw StoreError("a read needs something to do");
    }
    const std::shared_lock<std::shared_mutex> data_lock(data_mutex_);
    reader(store_);
}

std::uint64_t Writer::waiting() const {
    const std::lock_guard<std::mutex> lock(queue_mutex_);
    return waiting_;
}

Writer::Statistics Writer::statistics() const {
    const std::lock_guard<std::mutex> lock(queue_mutex_);
    return statistics_;
}

}  // namespace squiflow::engine

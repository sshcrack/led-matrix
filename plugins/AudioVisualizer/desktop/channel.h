#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template<typename T>
class Channel {
public:
    void send(T value) {
        std::unique_lock lock(mutex_);
        queue_.push(std::move(value));
        cond_.notify_one();
    }

    std::optional<T> receive() {
        std::unique_lock lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }


    std::optional<T> try_receive() {
        std::unique_lock lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    void close() {
        std::unique_lock lock(mutex_);
        closed_ = true;
        cond_.notify_all();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool closed_ = false;
};

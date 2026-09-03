#pragma once
#include <atomic>
#include <string>

class YouTubeSearcher {
public:
  static std::string search(const std::string& query, const std::atomic<bool>* running = nullptr);
};

#pragma once
#include <atomic>
#include <string>
#include <vector>

namespace VideoSearch {

struct Result {
  std::string id;
  std::string url;
  std::string title;
  std::string channel;
  int duration_sec = 0; // 0 = unknown / live
  std::string thumbnail;
};

/// Searches YouTube through the configured yt-dlp. Blocking; run it on a worker
/// thread. Clearing *running aborts the child process. On failure `error` is
/// set and the result is empty.
std::vector<Result> search(const std::string &query, int count,
                           const std::atomic<bool> *running, std::string &error);

/// Parses yt-dlp output lines formatted as `id###title###channel###duration`.
/// Lines that do not match (yt-dlp warnings, partial lines) are skipped.
std::vector<Result> parse_output(const std::string &output);

/// "3:07", "1:02:30", or "" for unknown.
std::string format_duration(int seconds);

} // namespace VideoSearch

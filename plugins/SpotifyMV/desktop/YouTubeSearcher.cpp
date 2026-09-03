#include "YouTubeSearcher.h"
#include "shared/desktop/utils.h"
#include <spdlog/spdlog.h>

namespace {
std::string sanitize_query(const std::string& raw) {
  std::string out;
  out.reserve(raw.size());
  for (char c : raw) {
    switch (c) {
    case '"': case '\'': case '`': case '$': case ';':
    case '|': case '\\': case '\n': case '\r':
      continue;
    default:
      out += c;
    }
  }
  return out;
}
} // anonymous namespace

std::string YouTubeSearcher::search(const std::string& query, const std::atomic<bool>* running) {
  std::string safe = sanitize_query(query);
  if (safe.empty()) {
    spdlog::warn("YouTubeSearcher: query empty after sanitization");
    return "";
  }
  std::string cmd = get_ytdlp_network_command() + " \"ytsearch1:" + safe + "\" "
                    "--flat-playlist --print webpage_url --no-warnings";
  spdlog::info("YouTubeSearcher: searching YouTube for '{}'", query);
  const auto command = run_command_capture(cmd, running);
  if (command.exit_code == -2) {
    spdlog::info("YouTubeSearcher: search cancelled");
    return "";
  }
  if (command.exit_code != 0) {
    spdlog::warn("YouTubeSearcher: yt-dlp search failed (exit {}): {}",
                 command.exit_code, command.output);
    return "";
  }
  std::string result = command.output;
  if (result.empty()) {
    spdlog::warn("YouTubeSearcher: no results for '{}'", query);
    return "";
  }
  result.erase(result.find_last_not_of(" \n\r\t") + 1);
  result.erase(0, result.find_first_not_of(" \n\r\t"));
  if (result.empty()) {
    spdlog::warn("YouTubeSearcher: no results for '{}'", query);
  } else {
    spdlog::info("YouTubeSearcher: found URL: {}", result);
  }
  return result;
}

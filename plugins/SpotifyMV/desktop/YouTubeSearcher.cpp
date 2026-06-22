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

std::string YouTubeSearcher::search(const std::string& query) {
  std::string safe = sanitize_query(query);
  if (safe.empty()) {
    spdlog::warn("YouTubeSearcher: query empty after sanitization");
    return "";
  }
  std::string cmd = "yt-dlp \"ytsearch1:" + safe + "\" "
                    "--flat-playlist --print webpage_url --no-warnings 2>"
#ifdef _WIN32
                    "nul"
#else
                    "/dev/null"
#endif
                    ;
  spdlog::info("YouTubeSearcher: running {}", cmd);
  std::string result = run_command_and_get_output(cmd);
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

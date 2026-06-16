#include "YouTubeSearcher.h"
#include <cstdio>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

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
#ifdef _WIN32
  std::string cmd = "yt-dlp \"ytsearch1:" + safe + "\" "
                    "--flat-playlist --print webpage_url --no-warnings 2>nul";
#else
  std::string cmd = "yt-dlp \"ytsearch1:" + safe + "\" "
                    "--flat-playlist --print webpage_url --no-warnings 2>/dev/null";
#endif
  spdlog::info("YouTubeSearcher: running {}", cmd);
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    spdlog::error("YouTubeSearcher: popen failed");
    return "";
  }
  char buf[512];
  std::string result;
  while (fgets(buf, sizeof(buf), pipe)) result += buf;
  int status = pclose(pipe);
  if (status != 0) {
    spdlog::warn("YouTubeSearcher: yt-dlp exited with code {}", status);
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

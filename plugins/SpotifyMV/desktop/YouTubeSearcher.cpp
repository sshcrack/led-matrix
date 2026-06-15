#include "YouTubeSearcher.h"
#include <cstdio>
#include <spdlog/spdlog.h>

std::string YouTubeSearcher::search(const std::string& query) {
  std::string cmd = "yt-dlp \"ytsearch1:" + query + "\" "
                    "--flat-playlist --print webpage_url --no-warnings 2>/dev/null";
  spdlog::info("YouTubeSearcher: running {}", cmd);
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    spdlog::error("YouTubeSearcher: popen failed");
    return "";
  }
  char buf[512];
  std::string result;
  while (fgets(buf, sizeof(buf), pipe)) result += buf;
  pclose(pipe);
  result.erase(result.find_last_not_of(" \n\r\t") + 1);
  result.erase(0, result.find_first_not_of(" \n\r\t"));
  if (result.empty()) {
    spdlog::warn("YouTubeSearcher: no results for '{}'", query);
  } else {
    spdlog::info("YouTubeSearcher: found URL: {}", result);
  }
  return result;
}

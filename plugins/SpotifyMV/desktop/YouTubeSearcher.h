#pragma once
#include <string>

class YouTubeSearcher {
public:
  static std::string search(const std::string& query,
                            const std::string& ytdlp_path = "");
};

#include "VideoSearch.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace VideoSearch {
namespace {
constexpr const char *kSep = "###";

// The query ends up inside a double-quoted shell argument (cmd.exe and sh).
std::string sanitize_query(const std::string &raw) {
  std::string out;
  for (unsigned char c : raw) {
    if (c < 0x20) continue;
    switch (c) {
    case '"': case '\'': case '`': case '$': case ';': case '|':
    case '\\': case '&': case '<': case '>': case '%': case '^':
      continue;
    default:
      out += static_cast<char>(c);
    }
  }
  return out;
}

bool plausible_id(const std::string &id) {
  return id.size() == 11 &&
         std::all_of(id.begin(), id.end(), [](unsigned char c) {
           return std::isalnum(c) || c == '-' || c == '_';
         });
}
} // namespace

std::vector<Result> parse_output(const std::string &output) {
  std::vector<Result> results;
  std::istringstream in(output);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<std::string> parts;
    size_t pos = 0, next;
    while ((next = line.find(kSep, pos)) != std::string::npos) {
      parts.push_back(line.substr(pos, next - pos));
      pos = next + 3;
    }
    parts.push_back(line.substr(pos));
    if (parts.size() != 4 || !plausible_id(parts[0])) continue;

    Result r;
    r.id = parts[0];
    r.url = "https://www.youtube.com/watch?v=" + r.id;
    r.title = parts[1];
    r.channel = parts[2] == "NA" ? "" : parts[2];
    try {
      r.duration_sec = parts[3] == "NA" ? 0 : static_cast<int>(std::stod(parts[3]));
    } catch (const std::exception &) {
      r.duration_sec = 0;
    }
    r.thumbnail = fmt::format("https://i.ytimg.com/vi/{}/mqdefault.jpg", r.id);
    results.push_back(std::move(r));
  }
  return results;
}

std::vector<Result> search(const std::string &query, int count,
                           const std::atomic<bool> *running, std::string &error) {
  error.clear();
  const auto safe = sanitize_query(query);
  if (safe.empty()) {
    error = "Empty search query";
    return {};
  }
  count = std::clamp(count, 1, 25);
  const auto cmd = fmt::format(
      "{} \"ytsearch{}:{}\" --flat-playlist --no-warnings "
      "--print \"%(id)s###%(title)s###%(channel)s###%(duration)s\"",
      get_ytdlp_network_command(), count, safe);
  spdlog::info("VideoSearch: searching YouTube for '{}'", safe);
  const auto res = run_command_capture(cmd, running, 256 * 1024);
  if (res.exit_code == -2) { // cancelled by a newer search
    error = "cancelled";
    return {};
  }
  auto results = parse_output(res.output);
  if (res.exit_code != 0 && results.empty()) {
    spdlog::warn("VideoSearch: yt-dlp failed (exit {}): {}", res.exit_code, res.output);
    error = "yt-dlp search failed. Check the yt-dlp path and your connection.";
  } else if (results.empty()) {
    error = "No results";
  }
  return results;
}

std::string format_duration(int s) {
  if (s <= 0) return "";
  if (s >= 3600) return fmt::format("{}:{:02}:{:02}", s / 3600, s / 60 % 60, s % 60);
  return fmt::format("{}:{:02}", s / 60, s % 60);
}

} // namespace VideoSearch

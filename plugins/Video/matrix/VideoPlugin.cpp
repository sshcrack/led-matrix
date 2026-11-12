#include "VideoPlugin.h"
#include "scenes/VideoScene.h"
#include "shared/matrix/server/server_utils.h"
#include <restinio/uri_helpers.hpp>
#include <spdlog/spdlog.h>

using namespace Plugins;

REGISTER_PLUGIN(Video, VideoPlugin)

vector<std::unique_ptr<SceneWrapper>>
VideoPlugin::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> created;
    created.push_back(std::make_unique<Scenes::VideoSceneWrapper>());
    return created;
}

vector<std::unique_ptr<ImageProviderWrapper>>
VideoPlugin::create_image_providers() {
  return {};
}

vector<
    std::unique_ptr<ShaderProviderWrapper>>
VideoPlugin::create_shader_providers() {
  return {};
}

bool VideoPlugin::on_udp_packet(const uint8_t pluginId,
                                const uint8_t *packetData, const size_t size) {
  if (pluginId != 0x03) return false;
  
  std::lock_guard<std::mutex> lock(dataMutex);
  data.assign(packetData, packetData + size);
  return true;
}

std::optional<std::vector<std::string>> VideoPlugin::on_websocket_open() {
  std::lock_guard lock(streamMutex);
  std::vector<std::string> messages{"video"};
  if (streamOwner && !streamUrl.empty()) {
    messages.push_back("stream:start");
    messages.push_back("url:" + streamUrl);
  }
  return messages;
}

void VideoPlugin::start_stream(const void* owner) {
  std::lock_guard lock(streamMutex);
  streamOwner = owner;
  streamUrl.clear();
  send_msg_to_desktop("stream:start");
}

void VideoPlugin::set_stream_url(const void* owner, const std::string& url) {
  std::lock_guard lock(streamMutex);
  if (streamOwner != owner) return;
  streamUrl = url;
  flush_status();
  {
    std::lock_guard dataLock(dataMutex);
    data.clear();
  }
  send_msg_to_desktop("url:" + url);
}

void VideoPlugin::stop_stream(const void* owner) {
  std::lock_guard lock(streamMutex);
  if (streamOwner != owner) return;
  streamOwner = nullptr;
  streamUrl.clear();
  send_msg_to_desktop("stream:stop");
  flush_status();
}

namespace {
// The desktop app needs a few seconds for yt-dlp; beyond this it is either
// not connected or yt-dlp hung, so tell the UI instead of spinning forever.
constexpr auto kSearchTimeout = std::chrono::seconds(30);
constexpr size_t kMaxQueryLength = 200;

std::string clean_query(std::string q) {
  std::string out;
  for (char c : q)
    if (c != '\n' && c != '\r' && c != '\0')
      out += c;
  const auto b = out.find_first_not_of(" \t");
  if (b == std::string::npos) return {};
  out = out.substr(b, out.find_last_not_of(" \t") - b + 1);
  if (out.size() > kMaxQueryLength) {
    // Cut on a UTF-8 character boundary (continuation bytes are 10xxxxxx), otherwise the
    // query is invalid UTF-8 and breaks JSON/WebSocket text frames.
    size_t cut = kMaxQueryLength;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
    out.resize(cut);
  }
  return out;
}

bool is_http_url(const std::string &u) {
  return u.starts_with("https://") || u.starts_with("http://");
}
} // namespace

nlohmann::json VideoPlugin::search_snapshot() {
  std::lock_guard<std::mutex> lock(searchMutex);
  if (searchState == SearchState::Searching &&
      std::chrono::steady_clock::now() - searchStarted > kSearchTimeout) {
    searchState = SearchState::Error;
    searchError = "Search timed out. Is the desktop app running?";
  }
  const char *state = "idle";
  switch (searchState) {
  case SearchState::Idle: break;
  case SearchState::Searching: state = "searching"; break;
  case SearchState::Done: state = "done"; break;
  case SearchState::Error: state = "error"; break;
  }
  return {{"state", state}, {"query", searchQuery},
          {"error", searchError}, {"items", searchItems}};
}

std::unique_ptr<router_t>
VideoPlugin::register_routes(std::unique_ptr<router_t> router) {
  // Starts a search on the desktop app (yt-dlp runs there, never on the Pi).
  router->http_get("/video/search", [this](const auto &req, auto) {
    const auto qp = restinio::parse_query(req->header().query());
    const std::string query =
        qp.has("q") ? clean_query(std::string{qp["q"]}) : std::string{};
    if (query.empty())
      return Server::reply_with_error(req, "Missing search query");
    if (!Server::is_desktop_connected())
      return Server::reply_with_error(
          req, "Desktop app is not connected. Start it to search YouTube.",
          restinio::status_service_unavailable());
    {
      std::lock_guard<std::mutex> lock(searchMutex);
      searchState = SearchState::Searching;
      searchQuery = query;
      searchError.clear();
      searchItems = nlohmann::json::array();
      searchStarted = std::chrono::steady_clock::now();
    }
    send_msg_to_desktop("search:" + query);
    return Server::reply_with_json(req, search_snapshot());
  });

  router->http_get("/video/search_results", [this](const auto &req, auto) {
    return Server::reply_with_json(req, search_snapshot());
  });

  // Skips the playlist rotation and plays this URL right now (if the video
  // scene is currently showing). Does not modify the saved playlist.
  router->http_post("/video/play", [this](const auto &req, auto) {
    const auto qp = restinio::parse_query(req->header().query());
    const std::string url =
        qp.has("url") ? std::string{qp["url"]} : std::string{};
    if (!is_http_url(url))
      return Server::reply_with_error(req, "Missing or invalid url");
    set_play_request(url);
    return Server::reply_success(req);
  });

  return router;
}

void VideoPlugin::on_websocket_message(const std::string &message) {
  if (message.starts_with("play:")) {
    if (auto url = message.substr(5); is_http_url(url))
      set_play_request(std::move(url));
    return;
  }
  if (message.starts_with("results:")) {
    try {
      auto j = nlohmann::json::parse(message.substr(8));
      std::lock_guard<std::mutex> lock(searchMutex);
      // Ignore replies to a superseded search.
      if (j.value("query", "") != searchQuery) return;
      if (j.contains("error") && j["error"].is_string() &&
          !j["error"].get<std::string>().empty()) {
        searchState = SearchState::Error;
        searchError = j["error"].get<std::string>();
      } else {
        searchState = SearchState::Done;
        searchItems = j.value("items", nlohmann::json::array());
      }
    } catch (const std::exception &e) {
      spdlog::warn("VideoPlugin: bad search results message: {}", e.what());
    }
    return;
  }
  if (message.starts_with("status:")) {
    std::lock_guard<std::mutex> lock(statusMutex);
    status = message.substr(7);
    spdlog::info("VideoPlugin status update: {}", status);
  }
}

#include "VideoDesktop.h"
#include "VideoPacket.h"
#include "shared/desktop/utils.h"
#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

extern "C" PLUGIN_EXPORT VideoDesktop *createVideo() {
  return new VideoDesktop();
}

extern "C" PLUGIN_EXPORT void destroyVideo(VideoDesktop *c) { delete c; }

VideoDesktop::~VideoDesktop() {
  stop_audio();
}

void VideoDesktop::post_init() {
  auto cacheRoot = get_data_dir() / "cache" / "video";
  std::filesystem::create_directories(cacheRoot);
  engine_ = std::make_unique<Shared::VideoStreamEngine>(cacheRoot, matrix_width, matrix_height, fps);
  auto err = engine_->check_tools();
  tools_available = err.empty();
  if (!err.empty()) {
    tools_error_msg = err;
    spdlog::error(tools_error_msg);
  }
  engine_->on_status_change = [this](const std::string& s) {
    send_websocket_message("status:" + s);
    if (s == "playing")      state = State::Playing;
    else if (s == "downloading") state = State::Downloading;
    else if (s == "error")   state = State::Error;
    else if (s == "idle")    state = State::Idle;
  };
}

void VideoDesktop::load_config(std::optional<const nlohmann::json> config) {
  if (config.has_value()) {
    const auto &cfg = config.value();
    if (cfg.contains("enable_audio"))
      enable_audio = cfg["enable_audio"].get<bool>();
  }
}

void VideoDesktop::save_config(nlohmann::json &config) const {
  config["enable_audio"] = enable_audio;
}

void VideoDesktop::initialize_imgui(ImGuiContext *im_gui_context,
                                    ImGuiMemAllocFunc *alloc_fn,
                                    ImGuiMemFreeFunc *free_fn,
                                    void **user_data) {
  ImGui::SetCurrentContext(im_gui_context);
  ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void VideoDesktop::render() {
  if (!tools_available) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", tools_error_msg.c_str());
    return;
  }
  render_status_ui();
}

void VideoDesktop::render_status_ui() {
  ImGui::Text("URL: %s", current_url.empty() ? "None" : current_url.c_str());

  if (ImGui::Checkbox("Enable Audio", &enable_audio)) {
    if (!enable_audio) stop_audio();
  }

  const char *stateStr = "Unknown";
  ImVec4 stateColor = {1, 1, 1, 1};
  switch (state.load()) {
  case State::Idle:        stateStr = "Idle";        break;
  case State::Downloading: stateStr = "Downloading"; stateColor = {1, 1, 0, 1}; break;
  case State::Playing:     stateStr = "Playing";     stateColor = {0, 1, 0, 1}; break;
  case State::Error:       stateStr = "Error";       stateColor = {1, 0, 0, 1}; break;
  }
  ImGui::TextColored(stateColor, "Status: %s", stateStr);
  if (state.load() == State::Error)
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error: %s", last_error.c_str());
}

void VideoDesktop::pre_new_frame() {}

std::optional<std::unique_ptr<UdpPacket>>
VideoDesktop::compute_next_packet(const std::string sceneName) {
  if (sceneName != "video" || !tools_available || !allow_sending_packets.load())
    return std::nullopt;
  if (engine_->get_state() != Shared::VideoStreamEngine::State::Playing)
    return std::nullopt;
  if (!engine_->tick())
    return std::nullopt;

  auto frame = engine_->get_current_frame();
  if (frame.empty()) return std::nullopt;

  return std::unique_ptr<UdpPacket>(
      new VideoPacket(std::move(frame)),
      [](UdpPacket *p) { delete dynamic_cast<VideoPacket *>(p); });
}

void VideoDesktop::on_websocket_message(const std::string message) {
  if (message == "stream:stop") {
    allow_sending_packets = false;
    engine_->stop();
    return;
  }
  if (message == "stream:start") {
    allow_sending_packets = true;
    return;
  }
  if (message.starts_with("url:")) {
    if (!tools_available) {
      send_websocket_message("status:error");
      return;
    }
    std::string newUrl = message.substr(4);
    current_url = newUrl;
    engine_->start(newUrl);
    return;
  }
  if (message.starts_with("size:")) {
    std::string sizeStr = message.substr(5);
    auto xPos = sizeStr.find('x');
    matrix_width  = std::stoi(sizeStr.substr(0, xPos));
    matrix_height = std::stoi(sizeStr.substr(xPos + 1));
  }
}

// ─── Audio ───────────────────────────────────────────────────────────────────

void VideoDesktop::start_audio(const std::string &path) {
  stop_audio();
#ifdef _WIN32
  std::string cmd = fmt::format("ffplay -nodisp -autoexit -loglevel error \"{}\"", path);
  STARTUPINFOA si{};
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  std::vector<char> cmdline(cmd.begin(), cmd.end());
  cmdline.push_back('\0');
  if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    spdlog::error("Failed to start ffplay (error {})", GetLastError());
    return;
  }
  audio_process_info = pi;
#else
  audio_pid = fork();
  if (audio_pid == 0) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "error",
           path.c_str(), nullptr);
    _exit(1);
  } else if (audio_pid < 0) {
    spdlog::error("Failed to fork for audio playback");
    audio_pid = -1;
  }
#endif
}

void VideoDesktop::stop_audio() {
#ifdef _WIN32
  if (audio_process_info.hProcess) {
    if (WaitForSingleObject(audio_process_info.hProcess, 0) == WAIT_TIMEOUT)
      TerminateProcess(audio_process_info.hProcess, 0);
    CloseHandle(audio_process_info.hProcess);
    CloseHandle(audio_process_info.hThread);
    audio_process_info = {};
  }
#else
  if (audio_pid > 0) {
    kill(audio_pid, SIGTERM);
    int status;
    if (waitpid(audio_pid, &status, WNOHANG) == 0) {
      kill(audio_pid, SIGKILL);
      waitpid(audio_pid, &status, 0);
    }
    audio_pid = -1;
  }
  if (audio_pipe) {
    pclose(audio_pipe);
    audio_pipe = nullptr;
  }
#endif
}

#pragma once

#include <memory>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

#include "shared/matrix/server/common.h"

namespace rgb_matrix { class RGBMatrixBase; }
class PostProcessor;
class TransitionManager;
namespace Config { class MainConfig; }
namespace Update { class UpdateManager; }
class UdpServer;
namespace Scenes { class Scene; }

using server_t = restinio::http_server_t<Server::traits_t>;

class Daemon {
public:
    Daemon(int argc, char* argv[]);
    ~Daemon();

    int run();

private:
#ifdef ENABLE_EMULATOR
    std::string pinned_scene_name_;
    nlohmann::json prop_overrides_;
    std::shared_ptr<Scenes::Scene> pinned_scene_;

    void parse_emulator_options(int argc, char* argv[]);
    std::shared_ptr<Scenes::Scene> build_pinned_scene();
#endif

    bool is_debugging_ = false;
    uint16_t port_ = 8080;

    std::unique_ptr<rgb_matrix::RGBMatrixBase> matrix_;

    std::unique_ptr<PostProcessor> post_processor_;
    std::unique_ptr<TransitionManager> transition_manager_;
    std::unique_ptr<Config::MainConfig> config_;
    std::shared_ptr<Update::UpdateManager> update_manager_;

    std::unique_ptr<server_t> http_server_;
    std::thread control_thread_;

    std::unique_ptr<UdpServer> udp_server_;
};

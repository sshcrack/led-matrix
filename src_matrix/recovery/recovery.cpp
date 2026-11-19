#include "recovery.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include <graphics.h>
#include <led-matrix.h>
#include <matrix-factory.h>
#include <nlohmann/json.hpp>
#include <restinio/core.hpp>
#include <spdlog/spdlog.h>

#include "recovery_page.h"
#include "recovery_state.h"
#include "server/request_guard.h"
#include "shared/common/Version.h"
#include "shared/common/utils/durable_file.h"
#include "shared/common/utils/utils.h"
#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/server/common.h"
#include "shared/matrix/update/UpdateManager.h"

#ifndef LED_MATRIX_DATA_DIR
#define LED_MATRIX_DATA_DIR "."
#endif
#ifndef LED_MATRIX_SHARE_DIR
#define LED_MATRIX_SHARE_DIR "."
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace Recovery {
    namespace {
        using server_t = restinio::http_server_t<Server::traits_t>;

        fs::path run_dir()
        {
            const char *dir = getenv("LED_MATRIX_RECOVERY_DIR");
            return dir && *dir ? fs::path(dir) : fs::path("/run/led-matrix-recovery");
        }
        const fs::path data_dir{LED_MATRIX_DATA_DIR};

        /// First IPv4 address of an interface that is up (not loopback, not a
        /// virtual bridge such as docker0 or virbr0).
        std::string local_ip()
        {
            auto is_virtual = [](std::string_view name) {
                for (std::string_view prefix : {"docker", "virbr", "br-", "veth", "lxc", "tun", "wg"})
                    if (name.starts_with(prefix))
                        return true;
                return false;
            };
            ifaddrs *list = nullptr;
            if (getifaddrs(&list) != 0)
                return {};
            std::string found;
            for (auto *entry = list; entry && found.empty(); entry = entry->ifa_next) {
                if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET || (entry->ifa_flags & IFF_LOOPBACK) ||
                    !(entry->ifa_flags & IFF_UP) || is_virtual(entry->ifa_name))
                    continue;
                std::array<char, INET_ADDRSTRLEN> text{};
                const auto *address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
                if (inet_ntop(AF_INET, &address->sin_addr, text.data(), text.size()))
                    found = text.data();
            }
            freeifaddrs(list);
            return found;
        }

        std::optional<ActionState> read_action_state()
        {
            std::ifstream in(run_dir() / "state.json");
            if (!in)
                return std::nullopt;
            std::stringstream text;
            text << in.rdbuf();
            return parse_action_state(text.str());
        }

        bool storage_writable()
        {
            const auto probe = data_dir / ".recovery-probe";
            std::error_code ec;
            {
                std::ofstream out(probe);
                if (!out)
                    return false;
            }
            fs::remove(probe, ec);
            return true;
        }

        /// The data directory is on its own mount (the panel card's data partition
        /// is bind-mounted there); on the RAM overlay it would not be.
        bool storage_persistent()
        {
            std::ifstream mounts("/proc/self/mounts");
            std::string device, mount_point, type, rest;
            const auto target = fs::weakly_canonical(data_dir).string();
            std::string best_type;
            std::size_t best_length = 0;
            while (mounts >> device >> mount_point >> type && std::getline(mounts, rest)) {
                if ((target == mount_point || target.starts_with(mount_point == "/" ? "/" : mount_point + "/")) &&
                    mount_point.size() >= best_length) {
                    best_length = mount_point.size();
                    best_type = type;
                }
            }
            return !best_type.empty() && best_type != "overlay" && best_type != "tmpfs";
        }

        bool is_panel_card()
        {
            std::ifstream fstab("/etc/fstab");
            std::string line;
            while (std::getline(fstab, line))
                if (line.find("LABEL=LEDDATA") != std::string::npos && !line.starts_with("#"))
                    return true;
            return false;
        }

        std::string run_command(const char *command)
        {
            std::string out;
            if (FILE *pipe = popen(command, "r")) {
                std::array<char, 4096> buffer{};
                std::size_t n;
                while ((n = fread(buffer.data(), 1, buffer.size(), pipe)) > 0 && out.size() < 4 * 1024 * 1024)
                    out.append(buffer.data(), n);
                pclose(pipe);
            }
            return out;
        }

        restinio::request_handling_status_t reply(const restinio::request_handle_t &req, const json &body,
                                                  restinio::http_status_line_t status = restinio::status_ok())
        {
            return req->create_response(status)
                .append_header(restinio::http_field::content_type, "application/json")
                .append_header(restinio::http_field::cache_control, "no-store")
                .set_body(body.dump())
                .done();
        }

        /// Download and hand over to the updater, as the normal daemon does. Needs
        /// working storage: the package and the update request live there.
        class Installer {
        public:
            bool running() const { return running_.load(); }
            std::string message()
            {
                std::lock_guard lock(mutex_);
                return message_;
            }

            void start()
            {
                if (running_.exchange(true))
                    return;
                set_message("Looking for the latest release…");
                std::thread([this] {
                    try {
                        config_ = std::make_unique<Config::MainConfig>((data_dir / "config.json").string(),
                                                                       Config::MainConfig::LoadMode::Recover);
                        manager_ = std::make_shared<Update::UpdateManager>(config_.get());
                        auto info = manager_->get_update_info();
                        if (!info)
                            return fail("Could not find the latest release: " + info.error());
                        set_message("Downloading " + info->version.toString() + "…");
                        // Sets interrupt_received once the package is ready; run() then
                        // hands it to the updater (UpdateManager::stop) and exits.
                        manager_->begin_manual_install();
                        auto result = manager_->manual_download_and_install(*info);
                        if (!result)
                            return fail(result.error());
                        set_message("Installing " + info->version.toString() + "; the panel restarts");
                    } catch (const std::exception &e) {
                        fail(e.what());
                    }
                }).detach();
            }

            /// After the main loop ends: queue the update request (no-op without one).
            void hand_over()
            {
                if (manager_)
                    manager_->launch_pending_update();
            }

        private:
            void fail(const std::string &message)
            {
                spdlog::error("Recovery install failed: {}", message);
                set_message("Install failed: " + message);
                running_.store(false);
            }
            void set_message(std::string message)
            {
                std::lock_guard lock(mutex_);
                message_ = std::move(message);
            }

            std::atomic<bool> running_{false};
            std::mutex mutex_;
            std::string message_;
            std::unique_ptr<Config::MainConfig> config_;
            std::shared_ptr<Update::UpdateManager> manager_;
        };

        std::unique_ptr<Server::router_t> make_router(Reason reason, uint16_t port, Installer &installer)
        {
            auto router = std::make_unique<Server::router_t>();
            auto page = [](auto req, auto) {
                return req->create_response()
                    .append_header(restinio::http_field::content_type, "text/html; charset=utf-8")
                    .append_header(restinio::http_field::cache_control, "no-store")
                    .set_body(page_html)
                    .done();
            };
            router->http_get("/", page);
            router->http_get("/web", page);
            router->http_get("/web/", page);

            router->http_get("/api/recovery/status", [reason, port, &installer](auto req, auto) {
                const bool ok = storage_persistent() && storage_writable();
                json body{{"reason", reason_id(reason)},
                          {"description", describe(reason)},
                          {"version", Common::Version::getCurrentVersion().toString()},
                          {"ip", local_ip()},
                          {"port", port},
                          {"storage", {{"ok", ok}, {"panel_card", is_panel_card()}}},
                          {"installing", installer.running()},
                          {"install_message", installer.message()},
                          {"action", nullptr}};
                if (auto action = read_action_state())
                    body["action"] = {{"action", action->action},
                                      {"state", action->state},
                                      {"message", action->message},
                                      {"progress", action->progress}};
                return reply(req, body);
            });
            // The normal API's version endpoint, so update health checks and the
            // desktop app see which version answers (and that it is recovery).
            router->http_get("/api/update/status", [](auto req, auto) {
                return reply(req, {{"current_version", Common::Version::getCurrentVersion().toString()},
                                   {"recovery", true}});
            });

            router->http_post("/api/recovery/action", [](auto req, auto) {
                const auto query = restinio::parse_query(req->header().query());
                const std::string name = query.has("name") ? std::string(query["name"]) : "";
                if (!is_helper_action(name))
                    return reply(req, {{"error", "Unknown action"}}, restinio::status_bad_request());
                if (auto action = read_action_state(); action && action->running())
                    return reply(req, {{"error", "Another action is running"}}, restinio::status_conflict());
                std::string error;
                if (!DurableFile::replace(run_dir() / "request", name + "\n", &error))
                    return reply(req, {{"error", "Could not queue the action: " + error}},
                                 restinio::status_internal_server_error());
                spdlog::info("Recovery: requested {}", name);
                return reply(req, {{"status", "started"}}, restinio::status_accepted());
            });

            router->http_post("/api/recovery/install", [&installer](auto req, auto) {
                if (!(storage_persistent() && storage_writable()))
                    return reply(req, {{"error", "Repair or reset the storage first"}}, restinio::status_conflict());
                installer.start();
                return reply(req, {{"status", "started"}}, restinio::status_accepted());
            });

            router->http_post("/api/recovery/config", [](auto req, auto) {
                if (!(storage_persistent() && storage_writable()))
                    return reply(req, {{"error", "Repair or reset the storage first"}}, restinio::status_conflict());
                const auto config = json::parse(req->body(), nullptr, false);
                if (!config.is_object())
                    return reply(req, {{"error", "That is not a config.json (expected a JSON object)"}},
                                 restinio::status_bad_request());
                std::string error;
                if (!DurableFile::replace(data_dir / "config.json", config.dump(4), &error))
                    return reply(req, {{"error", "Could not save the config: " + error}},
                                 restinio::status_internal_server_error());
                spdlog::info("Recovery: config.json replaced from an upload");
                return reply(req, {{"status", "saved"}});
            });

            router->http_get("/api/recovery/logs", [](auto req, auto) {
                std::string text = "== journal (this boot) ==\n";
                text += run_command("journalctl -b --no-pager -n 3000 -u led-matrix -u led-matrix-update "
                                    "-u led-matrix-recovery -u led-matrix-deploy 2>&1");
                text += "\n== /var/log/led-matrix-update.log ==\n";
                text += run_command("tail -n 300 /var/log/led-matrix-update.log 2>&1");
                text += "\n== mounts ==\n" + run_command("findmnt -l 2>&1");
                return req->create_response()
                    .append_header(restinio::http_field::content_type, "text/plain; charset=utf-8")
                    .append_header(restinio::http_field::content_disposition,
                                   "attachment; filename=\"led-matrix-logs.txt\"")
                    .set_body(std::move(text))
                    .done();
            });

            router->non_matched_request_handler([](auto req) {
                return reply(req, {{"error", "Recovery mode: only the recovery page is available"}},
                             restinio::status_not_found());
            });
            return router;
        }

        void draw_icon(rgb_matrix::Canvas *canvas, Icon icon, Color color, int x0, int y0)
        {
            auto px = [&](int x, int y) { canvas->SetPixel(x0 + x, y0 + y, color.r, color.g, color.b); };
            switch (icon) {
                case Icon::Disk:  // a drive: box with a slot and a light
                    for (int x = 0; x < 16; ++x)
                        for (int y = 3; y < 13; ++y)
                            if (y == 3 || y == 12 || x == 0 || x == 15 || (y == 9 && x > 2 && x < 10))
                                px(x, y);
                    px(12, 9), px(13, 9), px(12, 10), px(13, 10);
                    break;
                case Icon::Crash:  // an X
                    for (int i = 1; i < 15; ++i)
                        px(i, i), px(i + 1, i), px(15 - i, i), px(14 - i, i);
                    break;
                case Icon::Wrench:  // a gear-like ring
                    for (int x = 0; x < 16; ++x)
                        for (int y = 0; y < 16; ++y) {
                            const int dx = 2 * x - 15, dy = 2 * y - 15, d = dx * dx + dy * dy;
                            if ((d >= 64 && d <= 144) || (d > 144 && d <= 225 && ((x + y) % 4 == 0)))
                                px(x, y);
                        }
                    break;
                case Icon::Wifi:  // arcs above a dot, struck through
                    for (int x = 0; x < 16; ++x)
                        for (int y = 0; y < 16; ++y) {
                            const int dx = 2 * x - 15, dy = 2 * y - 28, d = dx * dx + dy * dy;
                            if (y < 13 && ((d >= 300 && d <= 380) || (d >= 120 && d <= 180)))
                                px(x, y);
                        }
                    px(7, 13), px(8, 13), px(7, 14), px(8, 14);
                    for (int i = 0; i < 16; ++i)
                        canvas->SetPixel(x0 + i, y0 + 15 - i, 230, 30, 30);
                    break;
            }
        }

        void draw(rgb_matrix::FrameCanvas *canvas, const rgb_matrix::Font &font, const Screen &screen)
        {
            canvas->Clear();
            const int width = canvas->width(), height = canvas->height();
            const int line_height = font.height() + 1;
            const int lines = 1 + static_cast<int>(screen.lines.size()) + (screen.progress >= 0 ? 1 : 0);
            const int block = 16 + 3 + lines * line_height;
            int y = std::max(0, (height - block) / 2);
            draw_icon(canvas, screen.icon, screen.color, (width - 16) / 2, y);
            y += 19;
            auto centered = [&](const std::string &text, rgb_matrix::Color color) {
                const int text_width = static_cast<int>(text.size()) * font.CharacterWidth('0');
                rgb_matrix::DrawText(canvas, font, std::max(0, (width - text_width) / 2), y + font.baseline(), color,
                                     text.c_str());
                y += line_height;
            };
            centered(screen.word, {screen.color.r, screen.color.g, screen.color.b});
            for (const auto &line : screen.lines)
                centered(line, {200, 200, 200});
            if (screen.progress >= 0) {
                const int bar_width = std::max(8, width - 16), filled = bar_width * screen.progress / 100;
                for (int x = 0; x < bar_width; ++x)
                    for (int dy = 2; dy < 6; ++dy) {
                        const bool on = x < filled;
                        canvas->SetPixel(8 + x, y + dy, on ? 40 : 30, on ? 120 : 30, on ? 255 : 30);
                    }
            }
        }
    }  // namespace

    std::optional<Reason> reason_from_args(int &argc, char **argv)
    {
        std::optional<Reason> reason;
        int out = 1;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg.starts_with("--recovery=")) {
                reason = parse_reason(arg.substr(11));
                if (!reason)
                    reason = Reason::Crash;
                continue;
            }
            argv[out++] = argv[i];
        }
        argv[out] = nullptr;
        argc = out;
        return reason;
    }

    int run(int argc, char **argv, Reason reason)
    {
        spdlog::warn("Recovery mode: {}", reason_id(reason));
        // Hold nothing open on the data partition, so a repair can unmount it.
        if (chdir("/") != 0)
            spdlog::warn("Could not leave the working directory");

        rgb_matrix::MatrixFactory::Options options;
        options.runtime_options.drop_priv_user = getenv("SUDO_UID");
        options.runtime_options.drop_priv_group = getenv("SUDO_GID");
        if (!rgb_matrix::MatrixFactory::ParseOptionsFromFlags(&argc, &argv, &options)) {
            rgb_matrix::MatrixFactory::PrintMatrixFactoryFlags(stderr);
            return 1;
        }
        std::unique_ptr<rgb_matrix::RGBMatrixBase> matrix(rgb_matrix::MatrixFactory::CreateMatrix(options));
        if (!matrix) {
            spdlog::error("Recovery mode could not open the matrix");
            return 1;
        }

        struct sigaction action{};
        action.sa_handler = InterruptHandler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, nullptr);
        sigaction(SIGINT, &action, nullptr);

        rgb_matrix::Font font;
        const auto font_path = get_share_dir("led-matrix", LED_MATRIX_SHARE_DIR) / "7x13.bdf";
        if (!font.LoadFont(font_path.c_str()))
            spdlog::error("Recovery mode could not load {}", font_path.string());

        uint16_t port = 8080;
        if (const char *value = getenv("PORT"))
            try {
                port = static_cast<uint16_t>(std::stoi(value));
            } catch (...) {}

        Installer installer;
        auto policy = Server::RequestGuard::policy_from_env();
        std::shared_ptr<Server::router_t> router = make_router(reason, port, installer);
        auto server = std::make_unique<server_t>(restinio::own_io_context(), [&](auto &settings) {
            settings.port(port);
            settings.address("0.0.0.0");
            settings.max_pipelined_requests(4);
            settings.request_handler([router, policy](auto req) {
                if (auto rejected = Server::RequestGuard::reject_if_needed(policy, req))
                    return *rejected;
                return (*router)(std::move(req));
            });
        });
        std::thread http([&] {
            spdlog::info("Recovery page on http://0.0.0.0:{}/", port);
            try {
                restinio::run(restinio::on_thread_pool(1, restinio::skip_break_signal_handling(), *server));
            } catch (const std::exception &e) {
                spdlog::error("Recovery web server stopped: {}", e.what());
            }
        });

        auto *canvas = matrix->CreateFrameCanvas();
        const std::size_t max_chars = std::max(1, canvas->width() / std::max(1, font.CharacterWidth('0')));
        while (!interrupt_received.load()) {
            const auto screen = screen_for(reason, local_ip(), port, read_action_state(), max_chars);
            draw(canvas, font, screen);
            canvas = matrix->SwapOnVSync(canvas);
#ifdef ENABLE_EMULATOR
            if (options.use_emulator)
                static_cast<rgb_matrix::EmulatorMatrix *>(matrix.get())->Render();
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        spdlog::info("Recovery mode stopping");
        initiate_shutdown(*server);
        http.join();
        installer.hand_over();
        matrix->Clear();
        return 0;
    }
}  // namespace Recovery

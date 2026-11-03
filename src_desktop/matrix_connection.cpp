#include "app_state.h"
#include "matrix_connection.h"

#include <shared/desktop/config.h>

#include <cpr/cpr.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace desktop_app::detail {
    std::shared_ptr<WebsocketClient> ws_sp;
    WebsocketClient *ws = nullptr;
    UpdateChecker::UpdateManager updateManager;
    MatrixVersionChecker::MatrixVersionManager matrixVersionManager;

    // ----- Function to change matrix status (synchronous with timeout) ----
    void change_matrix_status(const std::string &hostname, uint16_t port, bool turnOn, bool waitForCompletion) {
        auto task = [hostname, port, turnOn]() {
            try {
                auto url = fmt::format("http://{}:{}/set_enabled", hostname, port);

                cpr::Header headers;
                if (const auto auth = Config::matrixAuthorizationHeader(); !auth.empty())
                    headers["Authorization"] = auth;
                cpr::Response response = cpr::Post(cpr::Url(url), headers, cpr::Payload{{"enabled", turnOn ? "true" : "false"}}, cpr::Timeout{5000L});
                if (response.error) {
                    spdlog::error("Failed to change matrix status: {}", response.error.message);
                } else {
                    spdlog::info("Matrix turned {} successfully.", turnOn ? "on" : "off");
                }
            } catch (const std::exception &e) {
                spdlog::error("Exception while changing matrix status: {}", e.what());
            }
        };

        if (waitForCompletion) {
            // Synchronous call for shutdown scenarios
            task();
        } else {
            // Asynchronous call for normal operation (self-contained lambda, safe to detach)
            std::thread(task).detach();
        }
    }

    void init_websocket() {
        ws_sp = WebsocketClient::create();
        ws = ws_sp.get();
        WebsocketClient::setInstance(ws);
    }
}

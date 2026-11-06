#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Plugins {
    class BasicPlugin;

    /// Plugin lifecycle hooks run by every host (daemon, scene worker,
    /// preview generator). Running them through this module guarantees each
    /// host sees the same result for a hook that reports an error or throws.
    enum class LifecycleStage {
        BeforeServerInit,
        AfterServerInit,
        PreExit,
    };

    [[nodiscard]] std::string_view to_string(LifecycleStage stage);

    struct LifecycleFailure {
        std::string plugin;
        LifecycleStage stage;
        std::string message;

        /// "Plugin 'X' failed during before_server_init: <message>"
        [[nodiscard]] std::string describe() const;
    };

    /// Runs one hook on one plugin. Both a returned error string and a thrown
    /// exception are reported as a failure.
    [[nodiscard]] std::optional<LifecycleFailure> run_lifecycle_stage(BasicPlugin *plugin, LifecycleStage stage);

    /// Runs init hooks on every plugin and throws std::runtime_error with the
    /// first failure's description. Use for BeforeServerInit/AfterServerInit:
    /// a plugin that failed to initialise must not keep serving scenes.
    void run_lifecycle_stage_or_throw(const std::vector<BasicPlugin *> &plugins, LifecycleStage stage);

    /// Runs a hook on every plugin, continuing past failures, and returns all
    /// of them. Use for PreExit so one plugin cannot block another's teardown.
    [[nodiscard]] std::vector<LifecycleFailure> run_lifecycle_stage_all(
        const std::vector<BasicPlugin *> &plugins, LifecycleStage stage);
}

#include "shared/matrix/plugin/lifecycle.h"
#include "shared/matrix/plugin/main.h"

#include <stdexcept>

#include <fmt/format.h>

namespace Plugins {
    std::string_view to_string(const LifecycleStage stage) {
        switch (stage) {
            case LifecycleStage::BeforeServerInit: return "before_server_init";
            case LifecycleStage::AfterServerInit: return "after_server_init";
            case LifecycleStage::PreExit: return "pre_exit";
        }
        return "unknown";
    }

    std::string LifecycleFailure::describe() const {
        return fmt::format("Plugin '{}' failed during {}: {}", plugin, to_string(stage), message);
    }

    std::optional<LifecycleFailure> run_lifecycle_stage(BasicPlugin *plugin, const LifecycleStage stage) {
        if (!plugin) return std::nullopt;

        std::string name;
        try { name = plugin->get_plugin_name(); }
        catch (...) { name = "<unknown>"; }

        try {
            std::optional<std::string> err;
            switch (stage) {
                case LifecycleStage::BeforeServerInit: err = plugin->before_server_init(); break;
                case LifecycleStage::AfterServerInit: err = plugin->after_server_init(); break;
                case LifecycleStage::PreExit: err = plugin->pre_exit(); break;
            }
            if (err.has_value())
                return LifecycleFailure{std::move(name), stage, std::move(*err)};
        } catch (const std::exception &e) {
            return LifecycleFailure{std::move(name), stage, e.what()};
        } catch (...) {
            return LifecycleFailure{std::move(name), stage, "unknown exception"};
        }
        return std::nullopt;
    }

    void run_lifecycle_stage_or_throw(const std::vector<BasicPlugin *> &plugins, const LifecycleStage stage) {
        for (auto *plugin : plugins) {
            if (auto failure = run_lifecycle_stage(plugin, stage))
                throw std::runtime_error(failure->describe());
        }
    }

    std::vector<LifecycleFailure> run_lifecycle_stage_all(
        const std::vector<BasicPlugin *> &plugins, const LifecycleStage stage) {
        std::vector<LifecycleFailure> failures;
        for (auto *plugin : plugins) {
            if (auto failure = run_lifecycle_stage(plugin, stage))
                failures.push_back(std::move(*failure));
        }
        return failures;
    }
}

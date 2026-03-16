#include "shared/matrix/transition_manager.h"
#include <spdlog/spdlog.h>

void TransitionManager::register_transition(
    std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)> effect) {
    if (!effect) return;
    std::string name = effect->get_name();
    spdlog::debug("Registered transition effect: {}", name);
    registered_transitions.emplace(name, std::move(effect));
}

TransitionEffect *TransitionManager::get_transition(const std::string &name) const {
    auto it = registered_transitions.find(name);
    if (it == registered_transitions.end()) return nullptr;
    return it->second.get();
}

std::vector<std::string> TransitionManager::get_registered_transitions() const {
    std::vector<std::string> names;
    names.reserve(registered_transitions.size());
    for (const auto &pair : registered_transitions)
        names.push_back(pair.first);
    return names;
}

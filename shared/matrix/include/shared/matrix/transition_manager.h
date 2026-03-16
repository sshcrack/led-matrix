#pragma once

#include "transition_effect.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/// Holds all plugin-registered transition effects and provides lookup by name.
class TransitionManager {
    std::unordered_map<std::string,
                       std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)>>
        registered_transitions;

public:
    TransitionManager() = default;
    ~TransitionManager() = default;

    void register_transition(
        std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)> effect);

    /// Returns the effect for 'name', or nullptr if not found.
    TransitionEffect *get_transition(const std::string &name) const;

    std::vector<std::string> get_registered_transitions() const;
};

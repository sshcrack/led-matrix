#pragma once

#include <memory>
#include <string>

#include "shared/matrix/Scene.h"
#include "shared/matrix/audio_state.h"

struct TransitionPlan {
    tmillis_t duration_ms = 750;
    tmillis_t start_delay_ms = 0;
    std::string name = "blend";
    bool beat_synchronized = false;
    std::string reason;
};

class TransitionPlanner {
public:
    [[nodiscard]] TransitionPlan plan(
        const std::shared_ptr<Scenes::Scene> &from,
        const std::shared_ptr<Scenes::Scene> &to,
        tmillis_t fallback_duration,
        const std::string &fallback_name,
        const AudioState::Snapshot &audio) const;
};

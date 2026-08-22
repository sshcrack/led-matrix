#include <iostream>

#include "matrix_control/TransitionPlanner.h"

namespace {
class TestScene final : public Scenes::Scene {
public:
    TestScene(std::string name, std::string family, float intensity, float motion)
        : name_(std::move(name)), family_(std::move(family)), intensity_(intensity), motion_(motion)
    {
        update_default_properties();
        register_properties();
        load_properties(nlohmann::json::object());
    }
    bool render(rgb_matrix::FrameCanvas*) override { return true; }
    void register_properties() override {}
    std::string get_name() const override { return name_; }
    Scenes::SceneDescriptor get_descriptor() const override
    {
        auto d = Scene::get_descriptor();
        d.family = family_;
        d.intensity = intensity_;
        d.motion = motion_;
        return d;
    }
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }

private:
    std::string name_, family_;
    float intensity_, motion_;
};
AudioState::Snapshot music(float phase, bool drop = false)
{
    AudioState::Snapshot a;
    a.available = true;
    a.age_seconds = 0.01f;
    a.set(AudioProtocol::Feature::Bpm, 120.f);
    a.set(AudioProtocol::Feature::BeatPhase, phase);
    a.set(AudioProtocol::Feature::BeatConfidence, .9f);
    a.set(AudioProtocol::Feature::TempoStability, .9f);
    if (drop)
        a.set(AudioProtocol::Feature::Drop, 1.f);
    return a;
}
}  // namespace
int main()
{
    TransitionPlanner planner;
    auto calm = std::make_shared<TestScene>("calm", "organic", .3f, .3f);
    auto peak = std::make_shared<TestScene>("peak", "tunnel", .9f, .9f);
    auto plan = planner.plan(calm, peak, 750, "blend", music(.5f));
    if (!plan.beat_synchronized || plan.start_delay_ms < 200 || plan.start_delay_ms > 300 || plan.duration_ms < 450 ||
        plan.duration_ms > 550) {
        std::cerr << "beat plan was not quantized around 120 BPM\n";
        return 1;
    }
    auto drop = planner.plan(calm, peak, 750, "blend", music(.2f, true));
    if (drop.name != "radial_reveal" || drop.start_delay_ms != 0 || drop.duration_ms < 300 || drop.duration_ms > 450) {
        std::cerr << "drop transition was not immediate but visually smooth\n";
        return 2;
    }
    AudioState::Snapshot stale;
    auto fallback = planner.plan(calm, peak, 900, "blend", stale);
    if (fallback.beat_synchronized || fallback.duration_ms != 900) {
        std::cerr << "stale audio should preserve configured timing\n";
        return 3;
    }
    std::cout << "transition planner adapts style and beat timing without requiring user configuration\n";
}

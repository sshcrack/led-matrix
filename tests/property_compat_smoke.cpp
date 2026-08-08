#include <shared/matrix/plugin/property.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/audio_state.h>

#include <cmath>
#include <iostream>

namespace {
class ModulationTestScene final : public Scenes::Scene {
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.0f, 4.0f);

public:
    float observed = 0.0f;

    void register_properties() override { add_property(speed_); }
    bool render(rgb_matrix::FrameCanvas *) override { observed = speed_->get(); return true; }
    std::string get_name() const override { return "modulation_test"; }
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};
}

int main()
{
    Plugins::Property<int> property("num_particles", 40, false, 1, 12000);
    property.legacy_name("numParticles");

    property.load_from_json(nlohmann::json{{"numParticles", 77}});
    if (property.get() != 77) {
        std::cerr << "legacy property name was not loaded\n";
        return 1;
    }

    nlohmann::json serialized = nlohmann::json::object();
    property.dump_to_json(serialized);
    if (serialized.value("num_particles", 0) != 77 || serialized.contains("numParticles")) {
        std::cerr << "property did not serialize with canonical name only\n";
        return 1;
    }

    property.load_from_json(nlohmann::json{{"numParticles", 55}, {"num_particles", 99}});
    if (property.get() != 99) {
        std::cerr << "canonical property name did not take precedence\n";
        return 1;
    }

    if (!property.supports_runtime_numeric() || !property.runtime_numeric_value().has_value()) {
        std::cerr << "numeric property did not expose runtime modulation access\n";
        return 1;
    }
    if (!property.set_runtime_numeric_value(20000.0) || property.get() != 12000) {
        std::cerr << "runtime numeric setter did not clamp to schema maximum\n";
        return 1;
    }

    ModulationTestScene scene;
    scene.update_default_properties();
    scene.register_properties();
    scene.load_properties(nlohmann::json{
        {"speed", 1.0},
        {"audio_modulations", nlohmann::json::array({
            {
                {"property", "speed"},
                {"feature", "bass"},
                {"min", 0.5},
                {"max", 3.0},
                {"smoothing", 0.0}
            }
        })}
    });
    scene.initialize(8, 8);

    AudioProtocol::Frame audio;
    audio.set(AudioProtocol::Feature::Bass, 1.0f);
    AudioState::update(audio);
    scene.render_frame(nullptr, 1.0 / 60.0, true);
    if (std::abs(scene.observed - 3.0f) > 0.001f) {
        std::cerr << "audio modulation did not map bass to the configured high value: "
                  << scene.observed << "\n";
        return 1;
    }

    scene.load_properties(nlohmann::json{{"audio_modulations", nlohmann::json::array()}});
    scene.render_frame(nullptr, 1.0 / 60.0, true);
    if (std::abs(scene.observed - 1.0f) > 0.001f) {
        std::cerr << "removing audio modulation did not restore the configured base value: "
                  << scene.observed << "\n";
        return 1;
    }

    return 0;
}

#include <shared/matrix/Scene.h>
#include <shared/matrix/scene_descriptor.h>

#include <cmath>
#include <iostream>

namespace {
class VariantScene final : public Scenes::Scene {
    PropertyPointer<float> level_ = MAKE_PROPERTY_MINMAX("level", float, 0.25f, 0.0f, 1.0f);
public:
    bool render(rgb_matrix::FrameCanvas *) override { return true; }
    void register_properties() override { add_property(level_); }
    std::string get_name() const override { return "variant_test"; }
    Scenes::SceneDescriptor get_descriptor() const override {
        auto d = Scene::get_descriptor();
        d.family = "test";
        d.intensity = 0.2f;
        d.motion = 0.3f;
        d.music_affinity = 0.1f;
        d.performance_cost = 0.4f;
        d.tags = {"base"};
        d.variants = {{
            "boost", "Boost", "Test override", {{"level", 0.8f}}, {"boosted"},
            0.9f, std::nullopt, 0.7f, std::nullopt
        }};
        return d;
    }
protected:
    tmillis_t get_default_duration() override { return 1000; }
    int get_default_weight() override { return 1; }
};
}

int main() {
    VariantScene scene;
    scene.update_default_properties();
    scene.register_properties();
    scene.apply_variant("boost");
    if (scene.get_variant_id() != "boost") {
        std::cerr << "variant id was not retained\n";
        return 1;
    }
    const auto values = scene.to_json();
    if (!values.contains("level") || std::abs(values["level"].get<float>() - 0.8f) > 0.001f) {
        std::cerr << "variant property override was not applied\n";
        return 2;
    }

    const auto descriptor = scene.get_descriptor();
    const auto *variant = Scenes::find_variant(descriptor, "boost");
    if (!variant) {
        std::cerr << "variant lookup failed\n";
        return 3;
    }
    const auto profile = Scenes::effective_profile(descriptor, variant);
    if (std::abs(profile.intensity - 0.9f) > 0.001f ||
        std::abs(profile.motion - 0.3f) > 0.001f ||
        std::abs(profile.music_affinity - 0.7f) > 0.001f ||
        profile.tags.size() != 2) {
        std::cerr << "variant effective profile did not merge descriptor metadata\n";
        return 4;
    }
    const auto json = Scenes::descriptor_to_json(descriptor);
    if (json.value("family", "") != "test" || json["variants"].size() != 1 ||
        json["variants"][0].value("id", "") != "boost") {
        std::cerr << "descriptor JSON changed unexpectedly\n";
        return 5;
    }

    bool rejected = false;
    try { scene.apply_variant("missing"); }
    catch (const std::exception &) { rejected = true; }
    if (!rejected) {
        std::cerr << "unknown variant was accepted\n";
        return 6;
    }
    return 0;
}

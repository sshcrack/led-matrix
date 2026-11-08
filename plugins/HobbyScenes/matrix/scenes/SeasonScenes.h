#pragma once
// Seasonal landscapes. Both follow the real time of day, weather and season
// (or the overrides) and are designed to change visibly through the year:
//
//   season_forest_path - a path winding into a broadleaf wood framed by two big
//                        trunks. Blossom in spring, deep green in summer, gold
//                        leaves drifting down in autumn, bare snowy branches in
//                        winter; fireflies on warm nights, a deer now and then.
//   season_cottage     - a cottage on a hill with smoking chimney, lit windows
//                        at dusk, a garden tree and fence. Decorates itself for
//                        occasions: lights and a wreath at Christmas, pumpkins
//                        at Halloween, eggs at Easter, fireworks on New Year's.

#include <array>
#include <vector>

#include "HobbyScene.h"

namespace Hobbies {

class ForestPathScene final : public HobbySceneBase {
    struct Tree {
        float x, base, height;
        uint32_t seed;
        bool pine;
    };
    std::vector<Tree> trees_;      // mid-ground, far to near
    std::vector<float> backdrop_;  // sky, hills, treeline, ground, path
    std::array<float, 12> backdropKey_{};
    Environment::SeasonalDrift drift_;
    float horizon_ = 0.0f;
    // Deer: wanders in, grazes, looks up, leaves.
    float deerX_ = -100.0f, deerTimer_ = 6.0f, deerPhase_ = 0.0f, deerGraze_ = 0.0f, deerDir_ = 1.0f;
    int deerState_ = 0;  // 0 away, 1 walking in, 2 grazing, 3 walking out
    float path_center(float y) const;
    float path_half(float y) const;
    void build_backdrop();

public:
    ForestPathScene();
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void set_random_seed(uint32_t seed) override
    {
        HobbySceneBase::set_random_seed(seed);
        drift_.set_seed(seed + 4);
    }
    [[nodiscard]] std::string get_name() const override { return "season_forest_path"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

enum class CottageDecorations { AUTO, NONE, CHRISTMAS, HALLOWEEN, EASTER, NEW_YEAR };

class CottageScene final : public HobbySceneBase {
    struct Puff {
        float x, y, r, life;
    };
    struct Spark {
        float x, y, vx, vy, life;
        int hue;
    };
    struct Burst {
        float x, y, life;
        int hue;
    };
    std::vector<Burst> flashes_;
    std::vector<Puff> smoke_;
    std::vector<Spark> sparks_;
    std::vector<float> backdrop_;
    std::array<float, 12> backdropKey_{};
    Environment::SeasonalDrift drift_;
    float horizon_ = 0.0f, smokeCarry_ = 0.0f, nextFirework_ = 3.0f;
    PropertyPointer<Plugins::EnumProperty<CottageDecorations>> decorations_ =
        MAKE_ENUM_PROPERTY("decorations", CottageDecorations, CottageDecorations::AUTO);
    void build_backdrop();
    [[nodiscard]] bool decorated(std::string_view occasion) const;

public:
    CottageScene();
    void register_properties() override;
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void set_random_seed(uint32_t seed) override
    {
        HobbySceneBase::set_random_seed(seed);
        drift_.set_seed(seed + 4);
    }
    [[nodiscard]] std::string get_name() const override { return "season_cottage"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies

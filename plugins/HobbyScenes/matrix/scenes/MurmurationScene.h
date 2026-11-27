#pragma once

#include "HobbyScene.h"

namespace Hobbies {

// Starlings over a wetland at dusk. Flocks fly in from off-screen, wheel
// over the water as a murmuration for a while and then leave or pour down
// into the reeds to roost; between flocks the sky is empty except for the
// odd small group or single bird passing across. Below: a far shore with a
// lone tree, still water that mirrors the sky and reeds in the foreground.
//
// The flock is simulated: every bird steers by its seven nearest neighbours
// (found through a grid, so it stays cheap on the Pi), keeps a nearly
// constant speed and turns smoothly. Turns start at one bird and run through
// the flock as a wave; banking birds show more wing and look darker, so the
// waves roll across the flock as dark bands. Far birds are 1 px specks and
// the flock's shape comes from their density; a few nearer birds show
// wingbeats.
//
// The scene id stays "boids" (it replaced the old abstract flocking demo), so
// saved presets keep working.
class MurmurationScene final : public HobbySceneBase {
    // A starling in the flock (design units; z is depth, 0 = the flock's plane).
    struct Bird {
        float x, y, z, vx, vy, vz;
        float cruise;  // preferred speed (design px/s)
        float bank;    // 0..1, how hard it is turning: banked wings look darker
        float turn;    // turn still to make (radians), from a wave passing through
        float wander;  // phase of its small individual drift
        int wave;      // last turning wave it joined
    };
    // A bird flying past on its own: a far speck, a near bird with visible
    // wingbeats, or the falcon.
    struct Flier {
        float x, y, vx, vy;
        float span;   // wingspan in design px; up to 1.5 is drawn as a speck
        float phase;  // wingbeat phase (radians)
        float mode;   // seconds left in the current flap burst or glide
        float beat;   // wingbeats per second
        bool flapping;
        bool falcon;
    };
    enum class Phase { Empty, Perform, Depart, Roost };

    std::vector<Bird> birds_, next_;
    std::vector<Flier> fliers_;
    std::vector<int> cellStart_, cellFill_, cellBirds_, cellOf_;
    Phase phase_ = Phase::Empty;
    float phaseLeft_ = 0.0f;  // seconds left in Empty / Perform; time spent leaving
    float stragglerWait_ = 0.0f;
    float falconWait_ = 0.0f;
    float waveWait_ = 0.0f;
    int waveId_ = 0;
    float waveAngle_ = 0.0f;
    float ax_ = 64.0f, ay_ = 40.0f, az_ = 0.0f;  // where the flock is drawn to
    float pathPhase_[4]{};
    float clock_ = 0.0f;
    float simCarry_ = 0.0f;
    float roostX_ = 16.0f, exitX_ = -200.0f, exitY_ = 20.0f;
    float opening_ = 0.0f;     // seconds left of the opening flock's tighter wheeling
    bool descending_ = false;  // roosting flock is over the reeds and pouring down

    std::vector<float> backdrop_, previous_;  // cached sky, shore, water and reeds
    float fade_ = 0.0f;
    std::vector<uint8_t> sky_, water_;  // open sky / open water masks
    std::vector<float> reedTop_;        // per pixel column: top row of the reeds, or the canvas height
    std::vector<float> ink_;            // bird darkness per pixel this frame
    std::array<float, 12> backdropKey_{};

    PropertyPointer<int> birdCount_ = MAKE_PROPERTY_MINMAX("birds", int, 700, 150, 1200);
    PropertyPointer<bool> falconOn_ = MAKE_PROPERTY("falcon", bool, true);

    void build_backdrop();
    void start_flock(bool in_view = false);
    void spawn_stragglers(bool near_only);
    void spawn_falcon();
    void update_phase(float dt);
    void simulate(float h);
    void update_fliers(float dt);
    void draw_birds();
    void draw_flier(const Flier& f, Rgb color);
    [[nodiscard]] float reed_top(float x) const;  // design row of the reed tops at design x (128 = none)

public:
    MurmurationScene();
    void initialize(int width, int height) override;
    void register_properties() override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "boids"; }
    [[nodiscard]] std::string get_category() const override { return "Ambient"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
    // 1: the abstract flocking demo. 2: the murmuration (different properties).
    [[nodiscard]] int remote_contract_version() const override { return 2; }
    tmillis_t get_default_duration() override { return 90000; }
};

}  // namespace Hobbies

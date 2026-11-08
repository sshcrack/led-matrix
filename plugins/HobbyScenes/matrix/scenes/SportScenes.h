#pragma once
// Ball sports, watched like a passer-by rather than a broadcast: no scores,
// no clocks, no text.
//
//   basketball_court - streetball at dusk; a player dribbles, rises for a jump
//                      shot and swishes it (or rattles the rim), then fetches
//                      the rebound. With music the dribble lands on the beat,
//                      floodlights pump with the bass and a drop sets the next
//                      shot on fire.
//   soccer_pitch     - a floodlit night match from above; two small teams pass
//                      the ball around, shoot, the keeper saves or the net
//                      ripples and the stands flash. With music passes go on
//                      beats, a wave runs round the crowd and drops bring goals.

#include <array>
#include <vector>

#include "FigureDraw.h"
#include "HobbyScene.h"

namespace Hobbies {

class BasketballScene final : public AudioHobbyScene {
    enum class State { Dribble, Gather, Flight, Loose };
    State state_ = State::Dribble;
    std::vector<float> background_;
    float stateT_ = 0.0f;
    float px_ = 0.0f, pTarget_ = 0.0f, facing_ = 1.0f, walk_ = 0.0f;
    float dribble_ = 0.0f;      // bounce clock; integer part counts bounces
    int bouncesLeft_ = 5;
    float bx_ = 0, by_ = 0, bvx_ = 0, bvy_ = 0, spin_ = 0;
    float flightT_ = 0, flightDur_ = 1;
    float shotX0_ = 0, shotY0_ = 0, shotVx0_ = 0, shotVy0_ = 0;  // release state of the shot
    bool make_ = true;
    float swish_ = 0.0f, rattle_ = 0.0f;
    bool onFire_ = false;
    std::array<Point, 10> trail_{};
    int trailHead_ = 0;
    void build_background();
    void release(float handX, float handY);

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "basketball_court"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

class SoccerScene final : public AudioHobbyScene {
    struct Player {
        Point pos, home;
        int team;  // 0 attacks right, 1 attacks left
        bool keeper;
        float stride;
    };
    enum class Ball { Held, Pass, Shot, Goal };
    std::vector<Player> players_;
    std::vector<float> background_;
    Ball ball_ = Ball::Held;
    int holder_ = 0, receiver_ = 0;
    Point ballPos_{}, from_{}, to_{};
    float ballT_ = 0.0f, ballDur_ = 1.0f, holdT_ = 0.0f, holdMin_ = 1.5f;
    bool goal_ = false, forceShot_ = false;
    float celebrate_ = 0.0f, netRipple_ = 0.0f, flashBoost_ = 0.0f, wave_ = 0.0f;
    int netSide_ = 1;
    struct Flash {
        int x, y;
        float life;
    };
    std::vector<Flash> flashes_;
    std::vector<float> waveWeight_;  // crowd wave weight per column (this frame)
    // Pitch rectangle in pixels.
    float x0_ = 0, x1_ = 0, y0_ = 0, y1_ = 0;
    void build_background();
    void kickoff(int team);
    void decide();
    [[nodiscard]] Point goal_center(int attackingTeam) const;

public:
    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    [[nodiscard]] std::string get_name() const override { return "soccer_pitch"; }
    [[nodiscard]] Scenes::SceneDescriptor get_descriptor() const override;
};

}  // namespace Hobbies

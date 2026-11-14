#include "shared/matrix/transition_composer.h"

#include <algorithm>

float TransitionComposer::ease(float linear_progress)
{
    return linear_progress * linear_progress * (3.0f - 2.0f * linear_progress);
}

bool TransitionComposer::begin(FrameCanvas* from, const std::string& name, tmillis_t duration_ms, int width, int height, tmillis_t start_ms)
{
    effect_ = nullptr;
    if (effects_) {
        effect_ = effects_->get_transition(name);
        if (!effect_)
            effect_ = effects_->get_transition("blend");
    }
    from_ = from;
    start_ms_ = start_ms;
    duration_ms_ = std::max<tmillis_t>(1, duration_ms);
    width_ = width;
    height_ = height;
    have_incoming_ = false;
    finished_ = false;
    return effect_ != nullptr;
}

void TransitionComposer::end()
{
    from_ = nullptr;
    effect_ = nullptr;
    have_incoming_ = false;
    finished_ = false;
}

bool TransitionComposer::refresh_outgoing(const FrameCanvas& latest)
{
    if (policy_ != OutgoingPolicy::KeepMoving || !from_ || finished_)
        return false;
    from_->CopyFrom(latest);
    return true;
}

float TransitionComposer::linear_progress(tmillis_t now_ms) const
{
    return std::clamp(static_cast<float>(now_ms - start_ms_) / static_cast<float>(duration_ms_), 0.0f, 1.0f);
}

float TransitionComposer::eased_progress(tmillis_t now_ms) const
{
    return ease(linear_progress(now_ms));
}

TransitionComposer::Frame TransitionComposer::frame(FrameCanvas* out, FrameCanvas* to, bool to_has_new_pixels, tmillis_t now_ms)
{
    have_incoming_ = have_incoming_ || to_has_new_pixels;
    if (!have_incoming_)
        to->CopyFrom(*from_);

    const float alpha = linear_progress(now_ms);
    Frame result;
    result.eased = ease(alpha);
    result.done = alpha >= 1.0f;
    if (effect_)
        effect_->apply(out, from_, to, result.eased, width_, height_);
    else
        out->CopyFrom(*to);
    if (result.done)
        finished_ = true;
    return result;
}

#pragma once

#include <string>
#include <string_view>

#include "shared/matrix/preview.h"

class AudioPreviewProvider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return Previews::Inputs::Audio; }
    void begin(const Previews::RunContext &context) override;
    void update(const Scenes::SceneFrameContext &frame) override;
    void end() noexcept override;

private:
    int fps_ = 15;
    float bpm_ = 120.0f;
    std::string profile_ = "balanced";
};

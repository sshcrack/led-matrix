#pragma once

#include "ShaderPreviewRenderer.h"

namespace ShadertoyPreview {

class Provider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return InputId; }
    void begin(const Previews::RunContext& context) override;
    void update(const Scenes::SceneFrameContext&) override {}
    void end() noexcept override;
};

}  // namespace ShadertoyPreview

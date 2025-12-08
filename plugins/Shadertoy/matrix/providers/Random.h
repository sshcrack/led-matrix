#pragma once

#include "shared/matrix/config/shader_providers/general.h"
#include "shared/matrix/wrappers.h"

namespace ShaderProviders {
    class Random final : public General {
        PropertyPointer<int> min_page = MAKE_PROPERTY("min_page", int, 0);
        PropertyPointer<int> max_page = MAKE_PROPERTY("max_page", int, 100);

    public:
        std::expected<std::string, std::string> get_next_shader() override;

        ~Random() override = default;

        void flush() override;

        void tick() override;

        [[nodiscard]] string get_name() const override;

        explicit Random();

        void register_properties() override;
    };

    class RandomWrapper final : public Plugins::ShaderProviderWrapper {
        std::unique_ptr<General, void (*)(General *)> create() override;
    };
}

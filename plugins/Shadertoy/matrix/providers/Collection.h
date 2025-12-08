#pragma once

#include "shared/matrix/config/shader_providers/general.h"
#include "shared/matrix/wrappers.h"

namespace ShaderProviders {
    class Collection final : public General {
        vector<std::string> available_urls;
        vector<std::string> already_shown;

        PropertyPointer<std::vector<string>> urls = MAKE_PROPERTY("urls", std::vector<string>, {});
    public:
        std::expected<std::string, std::string> get_next_shader() override;

        ~Collection() override = default;

        void flush() override;

        [[nodiscard]] string get_name() const override;

        explicit Collection();

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;
    };

    class CollectionWrapper final : public Plugins::ShaderProviderWrapper {
        std::unique_ptr<General, void (*)(General *)> create() override;
    };
}

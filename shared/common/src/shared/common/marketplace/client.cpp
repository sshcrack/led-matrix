#include "shared/common/marketplace/client.h"
#include "shared/common/marketplace/client_impl.h"

namespace Plugins {
namespace Marketplace {

    std::unique_ptr<MarketplaceClient> create_marketplace_client(const std::string& plugin_dir) {
        return std::make_unique<MarketplaceClientImpl>(plugin_dir);
    }

} // namespace Marketplace
} // namespace Plugins
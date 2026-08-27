#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include "shared/matrix/utils/consts.h"
#include "shared/matrix/update/UpdateManager.h"

namespace Constants {
    std::shared_ptr<Update::UpdateManager> global_update_manager;
}
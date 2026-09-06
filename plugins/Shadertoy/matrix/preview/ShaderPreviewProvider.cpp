#include "ShaderPreviewProvider.h"

namespace ShadertoyPreview {

void Provider::begin(const Previews::RunContext& context)
{
    ShadertoyPreview::begin(context);
}

void Provider::end() noexcept
{
    ShadertoyPreview::end();
}

}  // namespace ShadertoyPreview

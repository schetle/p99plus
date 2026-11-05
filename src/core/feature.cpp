#include "core/feature.h"
#include "features/camera/camera.h"
#include "features/limiter/limiter.h"
#include "features/mouse/mouse.h"
#include "game/eqgame.h"
#include "overlay/overlay.h"

namespace feature {

const std::vector<const Feature*>& All()
{
    // Menu order. Per-frame work also runs in this order.
    static const std::vector<const Feature*> all = {
        &mouse::feature,
        &camera::feature,
        &limiter::feature,
        &overlay::feature,
    };
    return all;
}

void OnFrame()
{
    game::Check();   // features may rely on knowing whether eqgame.exe is the expected build
    for (const Feature* f : All())
        if (f->onFrame) f->onFrame();
}

} // namespace feature

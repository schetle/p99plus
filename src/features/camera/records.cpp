#include "features/camera/internal.h"
#include "core/host.h"
#include "util/small_map.h"
#include <type_traits>

namespace camera {

namespace {

using Records = SmallMap<void*, CameraRecord, 8>;
static_assert(std::is_trivially_copyable_v<Records>);

// What we wrote into each engine camera, so the values are recognized as ours. Kept across a
// core reload: the cameras still hold the offset eye, orbit angles and FOV written by the old
// core, and without the records the new one would take those for the game's own and add its
// offset on top.
constexpr const char* kStashKey = "camera.records";

Records& All()
{
    static Records records = [] {
        Records r;
        stash::GetValue(kStashKey, r);
        stash::Clear(kStashKey);
        return r;
    }();
    return records;
}

} // namespace

CameraRecord& Record(void* cam)
{
    return All().Claim(cam);
}

void StashRecords()
{
    stash::PutValue(kStashKey, All());
}

} // namespace camera

#include "features/camera/internal.h"
#include "util/small_map.h"

namespace camera {

CameraRecord& Record(void* cam)
{
    static SmallMap<void*, CameraRecord, 8> records;
    return records.Claim(cam);
}

} // namespace camera

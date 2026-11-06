#include "util/mem.h"
#include <vector>

namespace mem {

namespace {

struct Original {
    unsigned char* addr;
    std::vector<unsigned char> bytes;
};
std::vector<Original> s_originals;

void Record(unsigned char* dst, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        bool known = false;
        for (const Original& o : s_originals)
            if (dst + i >= o.addr && dst + i < o.addr + o.bytes.size()) { known = true; break; }
        if (!known) s_originals.push_back({ dst + i, { dst[i] } });
    }
}

} // namespace

bool PatchCode(void* dst, const void* src, size_t n)
{
    Record((unsigned char*)dst, n);
    return Patch(dst, src, n);
}

void RestoreCode()
{
    for (const Original& o : s_originals) Patch(o.addr, o.bytes.data(), o.bytes.size());
    s_originals.clear();
}

} // namespace mem

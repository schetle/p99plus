#include "game/spells.h"
#include "core/app.h"
#include "core/log.h"
#include "util/timing.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace game::spells {

namespace {

// Field numbers in a spells_us.txt line.
constexpr int kFieldId = 0, kFieldName = 1, kFieldFormula = 16, kFieldDuration = 17;
constexpr int kFieldGoodEffect = 83, kFieldIcon = 144;
constexpr int kMaxId = 65535;

std::vector<Spell> s_spells;   // by id
bool s_loaded = false;
int  s_count = 0;

int ToInt(const char* begin, const char* end)
{
    std::string s(begin, end);
    return atoi(s.c_str());
}

void ParseLine(const char* p, const char* end)
{
    const char* field[kFieldIcon + 2] = {};
    int n = 0;
    field[n++] = p;
    for (const char* c = p; c < end && n <= kFieldIcon + 1; ++c)
        if (*c == '^') field[n++] = c + 1;
    if (n <= kFieldIcon + 1) return;                  // too short: not a spell line
    auto fieldEnd = [&](int i) { return field[i + 1] - 1; };

    const int id = ToInt(field[kFieldId], fieldEnd(kFieldId));
    if (id <= 0 || id > kMaxId) return;
    if ((int)s_spells.size() <= id) s_spells.resize(id + 1);
    Spell& sp = s_spells[id];
    sp.name.assign(field[kFieldName], fieldEnd(kFieldName));
    sp.formula = ToInt(field[kFieldFormula], fieldEnd(kFieldFormula));
    sp.duration = ToInt(field[kFieldDuration], fieldEnd(kFieldDuration));
    sp.detrimental = ToInt(field[kFieldGoodEffect], fieldEnd(kFieldGoodEffect)) == 0;
    sp.icon = ToInt(field[kFieldIcon], fieldEnd(kFieldIcon));
}

void Load()
{
    s_loaded = true;
    const LONGLONG t0 = timing::Ticks();
    const std::string path = g_gameDir + "spells_us.txt";
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { Log("spells: can't open %s", path.c_str()); return; }
    std::vector<char> data;
    char buf[65536];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) data.insert(data.end(), buf, buf + got);
    fclose(f);

    const char* p = data.data();
    const char* end = p + data.size();
    while (p < end) {
        const char* eol = p;
        while (eol < end && *eol != '\n') ++eol;
        const char* lineEnd = (eol > p && eol[-1] == '\r') ? eol - 1 : eol;
        ParseLine(p, lineEnd);
        p = eol + 1;
    }
    for (const Spell& s : s_spells) if (!s.name.empty()) ++s_count;
    Log("spells: %d spells from %s (%.0f ms)", s_count, path.c_str(),
        (timing::Ticks() - t0) * 1000.0 / timing::Frequency());
}

} // namespace

const Spell* Find(int id)
{
    if (!s_loaded) Load();
    if (id <= 0 || id >= (int)s_spells.size() || s_spells[id].name.empty()) return nullptr;
    return &s_spells[id];
}

bool Ready()
{
    if (!s_loaded) Load();
    return s_count > 0;
}

// EQEmu's CalcBuffDuration_formula (which follows the client's).
int DurationTicks(int formula, int duration, int level)
{
    int t;
    switch (formula) {
    case 0:  return 0;
    case 1:  t = level > 3 ? level / 2 : 1; break;
    case 2:  t = level > 3 ? level / 2 + 5 : 6; break;
    case 3:  t = 30 * level; break;
    case 4:  t = 50; break;
    case 5:  t = 2; break;
    case 6:  t = level / 2 + 2; break;
    case 7:  t = level; break;
    case 8:  t = level + 10; break;
    case 9:  t = 2 * level + 10; break;
    case 10: t = 3 * level + 10; break;
    case 11: t = 30 * (level + 3); break;
    case 12: t = level > 7 ? level / 4 : 1; break;
    case 13: t = 4 * level + 10; break;
    case 14: t = 5 * (level + 2); break;
    case 15: t = 10 * (level + 10); break;
    case 50:
    case 51: return -1;
    default:
        if (formula < 200) return 0;
        t = formula;
        break;
    }
    if (duration && duration < t) t = duration;
    return t;
}

} // namespace game::spells

#include "core/config.h"
#include "core/app.h"
#include "core/feature.h"
#include "core/log.h"
#include "util/math.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

std::vector<std::string> s_defaults;   // each setting's value before the first load, as ini text
std::vector<std::string> s_saved;      // as of the last load/save, for the unsaved-changes marker

const std::vector<const Setting*>& AllSettings()
{
    static const std::vector<const Setting*> all = [] {
        std::vector<const Setting*> v;
        for (const Feature* f : feature::All())
            for (size_t i = 0; i < f->numSettings; ++i) v.push_back(&f->settings[i]);
        return v;
    }();
    return all;
}

std::string FormatFloat(float v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", v);
    if (strchr(buf, '.')) {                        // 3.200 -> 3.2, 53.000 -> 53
        char* end = buf + strlen(buf);
        while (end[-1] == '0') *--end = 0;
        if (end[-1] == '.') *--end = 0;
    }
    return strcmp(buf, "-0") == 0 ? "0" : buf;
}

std::string Format(const Setting& s)
{
    switch (s.type) {
    case SettingType::Bool:  return *(bool*)s.value ? "true" : "false";
    case SettingType::Int:   return std::to_string(*(int*)s.value);
    case SettingType::Float: return FormatFloat(*(float*)s.value);
    case SettingType::Key:   return keybind::Format(*(KeyBinding*)s.value);
    }
    return {};
}

void ClampToRange(const Setting& s)
{
    if (s.type == SettingType::Int)
        *(int*)s.value = (int)math::Clamp((float)*(int*)s.value, s.lo, s.hi);
    else if (s.type == SettingType::Float)
        *(float*)s.value = math::Clamp(*(float*)s.value, s.lo, s.hi);
}

// Stores `text` in the setting. Returns false and leaves the setting alone if it isn't valid.
bool Parse(const Setting& s, const std::string& text)
{
    switch (s.type) {
    case SettingType::Bool: {
        std::string t = text;
        for (char& c : t) c = (char)tolower((unsigned char)c);
        if (t == "true" || t == "1" || t == "on" || t == "yes")       *(bool*)s.value = true;
        else if (t == "false" || t == "0" || t == "off" || t == "no") *(bool*)s.value = false;
        else return false;
        return true;
    }
    case SettingType::Int: {
        char* end = nullptr;
        long v = strtol(text.c_str(), &end, 0);
        if (end == text.c_str()) return false;
        *(int*)s.value = (int)v;
        ClampToRange(s);
        return true;
    }
    case SettingType::Float: {
        char* end = nullptr;
        float v = strtof(text.c_str(), &end);
        if (end == text.c_str()) return false;
        *(float*)s.value = v;
        ClampToRange(s);
        return true;
    }
    case SettingType::Key:
        return keybind::Parse(text, *(KeyBinding*)s.value);
    }
    return false;
}

std::vector<std::string> Snapshot()
{
    std::vector<std::string> v;
    for (const Setting* s : AllSettings()) v.push_back(Format(*s));
    return v;
}

} // namespace

void ConfigLoad()
{
    const auto& all = AllSettings();
    const std::string path = AppFile(".ini");
    if (s_defaults.empty()) s_defaults = Snapshot();   // the values the structs were built with

    for (size_t i = 0; i < all.size(); ++i) {
        char buf[256];
        GetPrivateProfileStringA(all[i]->section, all[i]->key, "", buf, sizeof(buf), path.c_str());
        if (!buf[0] || !Parse(*all[i], buf)) Parse(*all[i], s_defaults[i]);
    }
    s_saved = Snapshot();

    std::string line, section;
    for (size_t i = 0; i < all.size(); ++i) {
        if (section != all[i]->section) {
            if (!line.empty()) Log("config: [%s]%s", section.c_str(), line.c_str());
            section = all[i]->section;
            line.clear();
        }
        line += std::string(" ") + all[i]->key + "=" + s_saved[i];
    }
    if (!line.empty()) Log("config: [%s]%s", section.c_str(), line.c_str());

    // First run: write every setting out so there is a complete file to edit.
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ConfigSave();
        Log("config: wrote default %s", path.c_str());
    }
}

void ConfigSave()
{
    const std::string path = AppFile(".ini");
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) { Log("config: cannot write %s", tmp.c_str()); return; }

    fprintf(f, "; %s settings. The in-game menu rewrites this file when you click Save.\n", kAppName);
    const char* section = nullptr;
    for (const Setting* s : AllSettings()) {
        if (!section || strcmp(section, s->section) != 0) {
            fprintf(f, "\n[%s]\n", s->section);
            section = s->section;
        }
        if (s->help && *s->help) fprintf(f, "; %s\n", s->help);
        fprintf(f, "%s=%s\n", s->key, Format(*s).c_str());
    }
    fclose(f);

    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        Log("config: cannot replace %s (error %lu)", path.c_str(), GetLastError());
        DeleteFileA(tmp.c_str());
        return;
    }
    s_saved = Snapshot();
    Log("config saved");
}

void ConfigClamp()
{
    for (const Setting* s : AllSettings()) ClampToRange(*s);
}

bool ConfigDirty()
{
    const auto& all = AllSettings();
    for (size_t i = 0; i < all.size() && i < s_saved.size(); ++i)
        if (Format(*all[i]) != s_saved[i]) return true;
    return false;
}

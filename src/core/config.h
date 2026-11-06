#pragma once
// Settings are declared as tables next to the feature that owns them (see core/feature.h).
// This file turns those tables into the ini file: load, save, and the unsaved-changes check.
#include "util/keybinding.h"

enum class SettingType { Bool, Int, Float, Key };

struct Setting {
    const char* section;
    const char* key;
    SettingType type;
    void*       value;     // bool*, int*, float* or KeyBinding*
    float       lo, hi;    // Int / Float: values are clamped to this range on load
    const char* help;      // written as a comment line above the key
};

constexpr Setting BoolSetting(const char* section, const char* key, bool* v, const char* help)
{ return { section, key, SettingType::Bool, v, 0, 0, help }; }

constexpr Setting IntSetting(const char* section, const char* key, int* v, int lo, int hi, const char* help)
{ return { section, key, SettingType::Int, v, (float)lo, (float)hi, help }; }

constexpr Setting FloatSetting(const char* section, const char* key, float* v, float lo, float hi, const char* help)
{ return { section, key, SettingType::Float, v, lo, hi, help }; }

constexpr Setting KeySetting(const char* section, const char* key, KeyBinding* v, const char* help)
{ return { section, key, SettingType::Key, v, 0, 0, help }; }

void ConfigLoad();          // missing or invalid keys fall back to the built-in defaults
void ConfigSave();          // rewrites the whole file
bool ConfigDirty();         // true when settings differ from what was last loaded/saved
void ConfigClamp();         // pulls Int/Float settings back into range (Ctrl+click typing isn't clamped)

// Across a core reload: ConfigStash keeps every setting's current value and its last loaded/saved
// value; ConfigUnstash (after ConfigLoad) puts both back, so unsaved edits and the unsaved-changes
// marker survive. Settings the new build doesn't have are dropped, new ones keep their ini value.
void ConfigStash();
void ConfigUnstash();

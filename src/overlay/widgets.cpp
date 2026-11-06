#include "overlay/widgets.h"
#include "game/ui/custom_window.h"
#include <imgui.h>
#include <cstdarg>
#include <cstdio>

namespace ui {

namespace {
float LabelColumn()
{
    // Wide enough for the longest label in the menu.
    return ImGui::CalcTextSize("Full offset beyond distance").x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
}
} // namespace

void RowLabel(const char* label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(LabelColumn());
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool SliderF(const char* label, float* v, float lo, float hi, const char* fmt)
{
    RowLabel(label);
    ImGui::PushID(label);
    bool r = ImGui::SliderFloat("##v", v, lo, hi, fmt);
    ImGui::PopID();
    return r;
}

bool SliderI(const char* label, int* v, int lo, int hi)
{
    RowLabel(label);
    ImGui::PushID(label);
    bool r = ImGui::SliderInt("##v", v, lo, hi);
    ImGui::PopID();
    return r;
}

bool WindowStatusHint(const game::wnd::CustomWindow& w)
{
    using game::wnd::Status;
    switch (w.GetStatus()) {
    case Status::Unavailable:
        Hint("Unavailable: the game's window code wasn't found.");
        return true;
    case Status::Rejected:
        Hint("The game rejected the window; off until restart (see the log).");
        return true;
    case Status::NotLoaded:
        Hint("Not in the loaded UI; run /loadskin.");
        return true;
    case Status::Stale:
        Hint("Layout changed; run /loadskin to update the window.");
        return true;
    case Status::NotInWorld:
        Hint("Appears once you're in the world.");
        return true;
    case Status::Shown:
        break;
    }
    return false;
}

void Hint(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", buf);
    ImGui::PopStyleColor();
}

void BeginSection(const char* title, const char* description, const char* enableLabel, bool* enabled)
{
    ImGui::SeparatorText(title);
    if (description) { Hint("%s", description); ImGui::Spacing(); }
    if (enabled) ImGui::Checkbox(enableLabel, enabled);
    ImGui::BeginDisabled(enabled && !*enabled);
}

void EndSection() { ImGui::EndDisabled(); }

} // namespace ui

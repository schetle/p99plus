#include "overlay/menu.h"
#include "core/app.h"
#include "core/config.h"
#include "core/feature.h"
#include "core/host.h"
#include "overlay/overlay.h"
#include "overlay/widgets.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace overlay {

namespace {

constexpr float kPageMinWidth = 320.0f;   // the menu never gets narrower than sidebar + this
constexpr float kMinHeight = 300.0f;

// The selected page survives a core reload by title (the feature list may change between builds).
struct PageStash { char title[32]; };
constexpr const char* kPageKey = "overlay.menu.page";
const Feature* s_page = nullptr;

bool Listed(const Feature* f) { return f->title && f->drawMenu; }
bool Off(const Feature* f) { return f->enabled && !*f->enabled; }

void SelectPage(const Feature* f)
{
    s_page = f;
    PageStash st = {};
    snprintf(st.title, sizeof(st.title), "%s", f->title);
    stash::PutValue(kPageKey, st);
}

// After a reload: the page that was open, else the first one.
const Feature* CurrentPage()
{
    if (s_page) return s_page;
    PageStash st = {};
    const bool had = stash::GetValue(kPageKey, st);
    for (const Feature* f : feature::All()) {
        if (!Listed(f)) continue;
        if (!s_page) s_page = f;
        if (had && strcmp(f->title, st.title) == 0) { s_page = f; break; }
    }
    return s_page;
}

float SidebarWidth()
{
    const ImGuiStyle& st = ImGui::GetStyle();
    float w = 0.0f;
    for (const Feature* f : feature::All())
        if (Listed(f)) w = std::max(w, ImGui::CalcTextSize(f->title).x);
    return w + (st.WindowPadding.x + st.FramePadding.x) * 2.0f;
}

// The feature list: switched-off features are greyed so their state shows at a glance.
void DrawSidebar(float width)
{
    ImGui::BeginChild("##sidebar", ImVec2(width, 0), ImGuiChildFlags_Borders);
    for (const Feature* f : feature::All()) {
        if (!Listed(f)) continue;
        if (Off(f)) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(f->title, f == s_page) && f != s_page) SelectPage(f);
        if (Off(f)) ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

// One feature's page: description, the enable checkbox, then its controls (greyed out while disabled).
void DrawFeaturePage(const Feature& f)
{
    if (f.description) {
        ui::Hint("%s", f.description);
        ImGui::Spacing();
    }
    if (f.enabled) {
        ImGui::Checkbox(f.enableLabel, f.enabled);
        ImGui::Separator();
    }
    ImGui::BeginDisabled(f.enabled && !*f.enabled);
    f.drawMenu();
    ImGui::EndDisabled();
}

} // namespace

void DrawMenu(bool* open)
{
    const float sidebarW = SidebarWidth();
    ImGui::SetNextWindowSize(ImVec2(sidebarW + 420.0f, 460.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(sidebarW + kPageMinWidth, kMinHeight), ImVec2(FLT_MAX, FLT_MAX));
    // "###<name>" keeps the window ID stable while the title text changes.
    const bool dirty = ConfigDirty();
    char title[96];
    snprintf(title, sizeof(title), "%s v%s%s###%s", kAppName, kAppVersion, dirty ? " *" : "", kAppName);
    if (!ImGui::Begin(title, open)) { ImGui::End(); return; }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%.0f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);

    // Save / Reload, right-aligned on the FPS row above the pages.
    {
        const ImGuiStyle& st = ImGui::GetStyle();
        const float saveW = ImGui::CalcTextSize("Save").x + st.FramePadding.x * 2.0f;
        const float reloadW = ImGui::CalcTextSize("Reload").x + st.FramePadding.x * 2.0f;
        const float unsavedW = dirty ? ImGui::CalcTextSize("* unsaved").x + st.ItemSpacing.x : 0.0f;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - saveW - reloadW - unsavedW - st.ItemSpacing.x);
        if (dirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "* unsaved");
            ImGui::SameLine();
        }
        if (dirty) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.10f, 1.0f));
        if (ImGui::Button("Save")) ConfigSave();
        if (dirty) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save to %s", AppFileName(".ini").c_str());
        ImGui::SameLine();
        if (ImGui::Button("Reload")) ConfigLoad();   // discards unsaved edits
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Discard unsaved changes and re-read %s", AppFileName(".ini").c_str());
    }

    if (const Feature* page = CurrentPage()) {
        DrawSidebar(sidebarW);
        ImGui::SameLine();
        ImGui::BeginChild("##page", ImVec2(0, 0));
        DrawFeaturePage(*page);
        ImGui::EndChild();
    }
    // Ctrl+click lets a slider take any typed value; keep each setting inside its ini range
    // (e.g. an FPS cap of 0). Runs before the frame limiter in the same Present.
    ConfigClamp();

    ImGui::End();
}

void DrawFps()
{
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::Begin("##fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBackground);   // text only: no fill, no border
    char fps[16];
    snprintf(fps, sizeof(fps), "%.0f", ImGui::GetIO().Framerate);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + 1.0f, p.y + 1.0f), IM_COL32(0, 0, 0, 220), fps);   // drop shadow
    ImGui::TextUnformatted(fps);
    ImGui::End();
}

// Startup banner: shown at the top of the screen for a few seconds after the overlay loads.
void DrawBanner()
{
    constexpr double kShow = 10.0, kFade = 1.0;
    static const double start = ImGui::GetTime();   // first call = first frame after init
    const double t = ImGui::GetTime() - start;
    if (t >= kShow) return;

    const float alpha = t > kShow - kFade ? (float)((kShow - t) / kFade) : 1.0f;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 12.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::Begin("##banner", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (g_host->loadCount > 1)
        ImGui::Text("%s v%s reloaded (#%d).", kAppName, kAppVersion, g_host->loadCount);
    else
        ImGui::Text("%s v%s loaded!  Press %s to open the menu.", kAppName, kAppVersion, keybind::Name(cfg.menu_key).c_str());
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace overlay

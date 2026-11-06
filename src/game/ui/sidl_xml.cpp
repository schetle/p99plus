#include "game/ui/sidl_xml.h"

namespace game::wnd::sidl {

namespace {

std::string Tabs(int n) { return std::string((size_t)n, '\t'); }
std::string Elem(int n, const char* name, const std::string& v)
{
    return Tabs(n) + "<" + name + ">" + v + "</" + name + ">\n";
}
std::string Elem(int n, const char* name, int v) { return Elem(n, name, std::to_string(v)); }
std::string Elem(int n, const char* name, bool v) { return Elem(n, name, std::string(v ? "true" : "false")); }
std::string Open(int n, const char* name) { return Tabs(n) + "<" + name + ">\n"; }
std::string Close(int n, const char* name) { return Tabs(n) + "</" + name + ">\n"; }
std::string Item(const char* kind, const std::string& item) { return std::string("\t<") + kind + " item = \"" + item + "\">\n"; }

std::string Animation(const std::string& item, const std::string& texture, int x, int size)
{
    return Item("Ui2DAnimation", item) +
           Elem(2, "Cycle", true) +
           Open(2, "Frames") +
           Elem(3, "Texture", texture) +
           Open(3, "Location") + Elem(4, "X", x) + Elem(4, "Y", 0) + Close(3, "Location") +
           Open(3, "Size") + Elem(4, "CX", size) + Elem(4, "CY", size) + Close(3, "Size") +
           Open(3, "Hotspot") + Elem(4, "X", 0) + Elem(4, "Y", 0) + Close(3, "Hotspot") +
           Elem(3, "Duration", 1000) +
           Close(2, "Frames") +
           "\t</Ui2DAnimation>\n";
}

// The part every stretching control shares.
std::string Placement(const std::string& screenId)
{
    return Elem(2, "ScreenID", screenId) +
           Elem(2, "RelativePosition", true);
}
std::string AnchorElems(const Anchors& a)
{
    return Elem(2, "TopAnchorToTop", true) +
           Elem(2, "BottomAnchorToTop", true) +
           Elem(2, "LeftAnchorToLeft", a.leftFromLeft) +
           Elem(2, "RightAnchorToLeft", false) +
           Elem(2, "TopAnchorOffset", a.top) +
           Elem(2, "BottomAnchorOffset", a.bottom) +
           Elem(2, "LeftAnchorOffset", a.left) +
           Elem(2, "RightAnchorOffset", a.right);
}

} // namespace

std::string Document(const std::vector<std::string>& parts)
{
    std::string out =
        "<?xml version = \"1.0\"?>\n"
        "<XML ID = \"EQInterfaceDefinitionLanguage\">\n"
        "\t<Schema xmlns = \"EverQuestData\" xmlns:dt = \"EverQuestDataTypes\"/>\n\n";
    for (const std::string& p : parts) out += p + "\n";
    return out + "</XML>\n";
}

std::string Texture(const std::string& file, int cx, int cy)
{
    return Item("TextureInfo", file) +
           Open(2, "Size") + Elem(3, "CX", cx) + Elem(3, "CY", cy) + Close(2, "Size") +
           "\t</TextureInfo>\n";
}

std::string ButtonFrames(const std::string& prefix, const std::string& texture, int size)
{
    return Animation(prefix + "Normal", texture, 0, size) + "\n" +
           Animation(prefix + "Flyby", texture, size, size) + "\n" +
           Animation(prefix + "Pressed", texture, 2 * size, size) + "\n" +
           Animation(prefix + "PressedFlyby", texture, 3 * size, size) + "\n" +
           Animation(prefix + "Disabled", texture, 0, size);
}

std::string Label(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& text)
{
    return Item("Label", item) +
           Placement(screenId) +
           Elem(2, "Font", 3) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           Elem(2, "Text", text) +
           Open(2, "TextColor") + Elem(3, "R", 255) + Elem(3, "G", 255) + Elem(3, "B", 255) + Close(2, "TextColor") +
           Elem(2, "NoWrap", true) +
           Elem(2, "AlignCenter", true) +
           Elem(2, "AlignRight", false) +
           "\t</Label>\n";
}

std::string CheckButton(const std::string& item, const std::string& screenId, const Anchors& a,
                        const std::string& animPrefix)
{
    return Item("Button", item) +
           Placement(screenId) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           Elem(2, "Style_Checkbox", true) +
           Open(2, "ButtonDrawTemplate") +
           Elem(3, "Normal", animPrefix + "Normal") +
           Elem(3, "Pressed", animPrefix + "Pressed") +
           Elem(3, "Flyby", animPrefix + "Flyby") +
           Elem(3, "Disabled", animPrefix + "Disabled") +
           Elem(3, "PressedFlyby", animPrefix + "PressedFlyby") +
           Close(2, "ButtonDrawTemplate") +
           "\t</Button>\n";
}

std::string BorderedScreen(const Screen& s)
{
    std::string out = Item("Screen", s.item) +
           Elem(2, "RelativePosition", false) +
           Open(2, "Location") + Elem(3, "X", s.x) + Elem(3, "Y", s.y) + Close(2, "Location") +
           Open(2, "Size") + Elem(3, "CX", s.cx) + Elem(3, "CY", s.cy) + Close(2, "Size") +
           Elem(2, "Style_VScroll", false) +
           Elem(2, "Style_HScroll", false) +
           Elem(2, "Style_Transparent", false) +
           Elem(2, "DrawTemplate", std::string("WDT_RoundedNoTitle")) +
           Elem(2, "Style_Titlebar", false) +
           Elem(2, "Style_Closebox", false) +
           Elem(2, "Style_Minimizebox", false) +
           Elem(2, "Style_Border", true) +
           Elem(2, "Style_Sizable", true);
    for (const std::string& p : s.pieces) out += Elem(2, "Pieces", p);
    return out + "\t</Screen>\n";
}

} // namespace game::wnd::sidl

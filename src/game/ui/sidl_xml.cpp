#include "game/ui/sidl_xml.h"

namespace game::wnd::sidl {

namespace {

std::string Tabs(int n) { return std::string((size_t)n, '\t'); }
std::string Elem(int n, const char* name, const std::string& v)
{
    return Tabs(n) + "<" + name + ">" + v + "</" + name + ">\n";
}
std::string Elem(int n, const char* name, const char* v) { return Elem(n, name, std::string(v)); }   // not the bool one
std::string Elem(int n, const char* name, int v) { return Elem(n, name, std::to_string(v)); }
std::string Elem(int n, const char* name, bool v) { return Elem(n, name, std::string(v ? "true" : "false")); }
std::string Open(int n, const char* name) { return Tabs(n) + "<" + name + ">\n"; }
std::string Close(int n, const char* name) { return Tabs(n) + "</" + name + ">\n"; }
std::string Item(const char* kind, const std::string& item) { return std::string("\t<") + kind + " item = \"" + item + "\">\n"; }

// White text, as every control of ours has.
std::string WhiteText(bool withAlpha = false)
{
    return Open(2, "TextColor") + (withAlpha ? Elem(3, "Alpha", 255) : std::string()) +
           Elem(3, "R", 255) + Elem(3, "G", 255) + Elem(3, "B", 255) + Close(2, "TextColor");
}

// The part every stretching control shares.
std::string Placement(const std::string& screenId)
{
    return Elem(2, "ScreenID", screenId) +
           Elem(2, "RelativePosition", true);
}
std::string AnchorElems(const Anchors& a)
{
    return Elem(2, "TopAnchorToTop", a.topFromTop) +
           Elem(2, "BottomAnchorToTop", a.bottomFromTop) +
           Elem(2, "LeftAnchorToLeft", a.leftFromLeft) +
           Elem(2, "RightAnchorToLeft", a.rightFromLeft) +
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

std::string Label(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& text, bool center)
{
    return Item("Label", item) +
           Placement(screenId) +
           Elem(2, "Font", 3) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           Elem(2, "Text", text) +
           WhiteText() +
           Elem(2, "NoWrap", true) +
           Elem(2, "AlignCenter", center) +
           Elem(2, "AlignRight", false) +
           "\t</Label>\n";
}

std::string CheckButton(const std::string& item, const std::string& screenId, const Anchors& a)
{
    return Item("Button", item) +
           Placement(screenId) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           Elem(2, "Style_Checkbox", true) +
           Open(2, "ButtonDrawTemplate") +
           Elem(3, "Normal", "A_CheckBoxNormal") +
           Elem(3, "Pressed", "A_CheckBoxPressed") +
           Close(2, "ButtonDrawTemplate") +
           "\t</Button>\n";
}

std::string PushButton(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& text)
{
    return Item("Button", item) +
           Placement(screenId) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           Elem(2, "Text", text) +
           WhiteText() +
           Open(2, "ButtonDrawTemplate") +
           Elem(3, "Normal", "A_BtnNormal") +
           Elem(3, "Pressed", "A_BtnPressed") +
           Elem(3, "Flyby", "A_BtnFlyby") +
           Elem(3, "Disabled", "A_BtnDisabled") +
           Elem(3, "PressedFlyby", "A_BtnPressedFlyby") +
           Close(2, "ButtonDrawTemplate") +
           "\t</Button>\n";
}

std::string Editbox(const std::string& item, const std::string& screenId, const Anchors& a)
{
    return Item("Editbox", item) +
           Placement(screenId) +
           Elem(2, "DrawTemplate", "WDT_Inner") +
           Elem(2, "Style_Transparent", false) +
           Elem(2, "Style_VScroll", true) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           WhiteText() +
           Elem(2, "Style_Multiline", true) +
           "\t</Editbox>\n";
}

std::string Listbox(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& heading)
{
    return Item("Listbox", item) +
           Placement(screenId) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           WhiteText(true) +
           Elem(2, "Style_VScroll", true) +
           Elem(2, "Style_HScroll", false) +
           Elem(2, "Style_Transparent", false) +
           Elem(2, "Style_Border", true) +
           Elem(2, "DrawTemplate", "WDT_Inner") +
           Open(2, "Columns") + Elem(3, "Heading", heading) + Elem(3, "Width", 400) + Elem(3, "Sortable", false) + Close(2, "Columns") +
           Elem(2, "OwnerDraw", false) +
           "\t</Listbox>\n";
}

std::string StmlBox(const std::string& item, const std::string& screenId, const Anchors& a)
{
    return Item("STMLbox", item) +
           Placement(screenId) +
           Elem(2, "DrawTemplate", "WDT_Inner") +
           Elem(2, "Style_VScroll", true) +
           Elem(2, "AutoStretch", true) +
           AnchorElems(a) +
           Elem(2, "Style_Border", true) +
           Elem(2, "Style_Transparent", false) +
           "\t</STMLbox>\n";
}

std::string BorrowedButton(const std::string& item, const std::string& screenId, int x, int y, int cx, int cy,
                           const std::string& from)
{
    const std::string box = std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(cx) + " " + std::to_string(cy);
    return Item("Button", item) +
           Elem(2, "ScreenID", screenId) +
           Elem(2, "RelativePosition", true) +
           Tabs(2) + "<!--" + kBorrowPlace + " from=\"" + from + "\" box=\"" + box + "\"-->\n" +
           Elem(2, "Style_Transparent", false) +
           Elem(2, "Style_Checkbox", true) +
           Tabs(2) + "<!--" + kBorrowLook + " from=\"" + from + "\"-->\n" +
           "\t</Button>\n";
}

bool IsSizable(const std::string& xml)
{
    return xml.find(Elem(2, "Style_Sizable", true)) != std::string::npos;
}

std::string BorderedScreen(const Screen& s)
{
    const bool titled = !s.title.empty();
    std::string out = Item("Screen", s.item) +
           Elem(2, "RelativePosition", false) +
           Open(2, "Location") + Elem(3, "X", s.x) + Elem(3, "Y", s.y) + Close(2, "Location") +
           Open(2, "Size") + Elem(3, "CX", s.cx) + Elem(3, "CY", s.cy) + Close(2, "Size") +
           Elem(2, "Style_VScroll", false) +
           Elem(2, "Style_HScroll", false) +
           Elem(2, "Style_Transparent", false) +
           Elem(2, "DrawTemplate", std::string(titled ? kTitledFrame : "WDT_RoundedNoTitle")) +
           Elem(2, "Style_Titlebar", titled) +
           Elem(2, "Style_Closebox", titled) +
           Elem(2, "Style_Minimizebox", false) +
           Elem(2, "Style_Border", s.border) +
           Elem(2, "Style_Sizable", s.sizable);
    if (titled) out += Elem(2, "Text", s.title);
    for (const std::string& p : s.pieces) out += Elem(2, "Pieces", p);
    return out + "\t</Screen>\n";
}

} // namespace game::wnd::sidl

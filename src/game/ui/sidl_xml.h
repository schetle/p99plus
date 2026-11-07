#pragma once
// Builds the XML of a custom window's screen (see custom_window.h): small writers for the pieces
// the game's skins are made of. Only what our windows use; each call returns one element,
// ready to be passed to Document().
#include <string>
#include <vector>

namespace game::wnd::sidl {

// The whole file: a full <XML> document holding `parts` in order.
std::string Document(const std::vector<std::string>& parts);

// Where a control sits in its window: each edge is an offset from one of the window's edges
// (measured inwards), so a control anchored to both sides stretches with the window. `top` is from
// the top unless !topFromTop, `bottom` from the top unless !bottomFromTop, `left` from the left
// unless !leftFromLeft, `right` from the right unless rightFromLeft.
//   Field order: { top, bottom, left, right, leftFromLeft, topFromTop, bottomFromTop, rightFromLeft }
struct Anchors {
    int  top, bottom, left, right;
    bool leftFromLeft;
    bool topFromTop = true;
    bool bottomFromTop = true;
    bool rightFromLeft = false;
};
// A single-line, white label in the skin's font; centred, or left-aligned.
std::string Label(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& text, bool center = true);
// A checkbox button drawn with the skin's own checkbox art (A_CheckBoxNormal / A_CheckBoxPressed,
// 11x11 in every skin), so it always matches the loaded skin. Size it 12x12 with the anchors.
std::string CheckButton(const std::string& item, const std::string& screenId, const Anchors& a);
// A text button in the skin's own button art; CustomWindow::Clicked sees its clicks.
std::string PushButton(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& text);
// A multi-line text box with a scroll bar, for typing in.
std::string Editbox(const std::string& item, const std::string& screenId, const Anchors& a);
// A list with one text column and a scroll bar.
std::string Listbox(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& heading);
// A scrolling box showing STML, the game's small HTML (<br>, <c "#rrggbb">, <b>, <u>).
std::string StmlBox(const std::string& item, const std::string& screenId, const Anchors& a);

// A button that looks like one of the skin's own buttons: `from` is that button's item name (e.g.
// "CSPW_SpellBook", the spell gem window's book). Its art (normal, hover, pressed) and size are
// taken from the loaded skin when the XML is written into it (inject.cpp; the Default skin when the
// skin hasn't got that button), centred in the box x, y, cx, cy. A checkbox button: it shows its
// pressed art while checked (CustomWindow::SetCheck), like the spellbook button while the book is open.
std::string BorrowedButton(const std::string& item, const std::string& screenId, int x, int y, int cx, int cy,
                           const std::string& from);
// The markers BorrowedButton leaves for inject.cpp: XML comments, so the file is valid either way.
constexpr char kBorrowPlace[] = "P99Plus_BorrowPlace";   // <!--P99Plus_BorrowPlace from="item" box="x y cx cy"-->
constexpr char kBorrowLook[]  = "P99Plus_BorrowLook";    // <!--P99Plus_BorrowLook from="item"-->

// A window: the skin's frame (WDT_RoundedNoTitle), or with a title text a titled frame (title bar
// and close box), whose pieces are the items named in `pieces`. Skins don't agree on the titled
// frame (Default's titled windows use WDT_Rounded, velious's WDT_RoundedNoTitle, whose WDT_Rounded
// draws no border around the title), so a titled screen names kTitledFrame, and the XML is given
// the skin's own choice when it is written into the skin (inject.cpp).
constexpr char kTitledFrame[] = "P99Plus_TitledFrame";
struct Screen {
    std::string              item;
    int                      x, y, cx, cy;   // default position and size
    std::vector<std::string> pieces;
    bool                     border = true;
    bool                     sizable = true;
    std::string              title;          // non-empty: a title bar and close box
};
std::string BorderedScreen(const Screen& s);
// Whether a window's XML (a Document holding a BorderedScreen) lets the player resize it.
bool IsSizable(const std::string& xml);

} // namespace game::wnd::sidl

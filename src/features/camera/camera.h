#pragma once
// Camera control: eye offset, middle-drag orbit and FOV (one tab, three switchable sections).
//
// The offset eye (and orbit angles) are written into the engine's own camera object, so name
// tags, spell effects and mouse picking all agree with what is drawn. The FOV is rewritten in
// the same object. If the engine hooks can't be installed, a Direct3D-level fallback offsets
// the view matrix instead (d3d_fallback.cpp).
#include "core/feature.h"
#include "util/math.h"

namespace camera {

// One set per camera mode. Offsets are in world units: up along world up, forward/right along
// the camera's horizontal facing.
struct ModeSettings {
    float up = 0, forward = 0, right = 0;
};

struct OrbitSettings {
    bool enabled = true;              // middle-button drag rotates the camera around the character
    bool return_on_release = false;   // swing back behind the character when the button is released
    bool return_on_move = false;      // swing back when the character moves or turns (right-click always does)
};

struct OffsetSettings {
    bool  enabled = true;                    // off = the eye stays where the game puts it
    bool  zoom_fade = true;                  // shrink the third-person offset as the camera nears the head
    float zoom_fade_distance = 53.0f;        // distance from the head at which the full offset applies
    ModeSettings first_person;
    ModeSettings third_person = { 3.2f, 0.0f, 0.0f };
};

struct FovSettings {
    bool  enabled = true;            // off = the game's own FOV (fixed horizontal, so it crops widescreen)
    float first_person = 0;          // vertical FOV in degrees; 0 = default (Hor+: widens to fit the screen)
    float third_person = 0;
};

// The camera is three independent systems, each with its own switch.
struct Settings {
    OffsetSettings offset;
    OrbitSettings  orbit;
    FovSettings    fov;
};
extern Settings cfg;

extern const Feature feature;      // the "Camera" tab

// What the camera is doing right now, for the menu.
struct Status { bool firstPerson; bool fadeActive; float distance; float percent; };
Status GetStatus();

const char* FovProblem();          // nullptr when the FOV hook is working or still starting up

// Where a world point shows on screen, in game screen pixels (with the offset, orbit and FOV the
// world was drawn with). False when it is behind the camera or the world camera isn't known yet.
bool ProjectToScreen(math::Vec3 world, float& x, float& y);

namespace orbit {
bool Enabled();                    // the setting
bool Dragging();                   // enabled, middle button held, menu closed
void AddMouse(long dx, long dy);   // mouse movement diverted from the game while dragging
} // namespace orbit

} // namespace camera

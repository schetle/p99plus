#pragma once
// The game's string table (eqstr_*.txt): texts the client prints by id.
namespace game {

// A text from the string table, or nullptr when the game isn't ready or the id is empty.
const char* String(int id);

// Ids used by the features. Sense Heading is decided by the client (roll against skill 40) and
// printed with these; the server only gets an empty "sensed" packet on success, for the skill-up.
namespace str {
inline constexpr int kSenseHeading = 12435;   // "You think you are heading %1."
inline constexpr int kSenseFail    = 12426;   // "You have no idea what direction you are facing."
inline constexpr int kLocation     = 13177;   // "Your Location is %1, %2, %3" (/loc)
} // namespace str

} // namespace game

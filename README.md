# p99plus

A `dinput8.dll` proxy for the EverQuest **Titanium** client that adds an in-game menu with:

- camera offset (first and third person) and middle-drag camera orbit
- FOV control, with ultrawide support
- FPS-independent horizontal mouse turning and separate X/Y sensitivity
- removal of the mouselook axis dead-zone
- a small game window with the zone you are in, the result of your last Sense Heading and your last `/loc`
- floating combat text: damage, misses and heals shown over the one they happen to, in the game's own font
- an optional frame limiter

> Check your server's rules before using. Injected DLLs are typically treated like MacroQuest.

## Compatibility

- **Client:** EverQuest Titanium only (`eqgame.exe` built Oct 31 2005, 32-bit). Other clients use different memory addresses, so the patches won't apply.
- **Linux:** tested with Proton (DXVK), including under gamescope. Plain Wine is untested but should work the same way.
- **Windows:** not tested. It should only need the DLL in the EverQuest folder, with no override.

## Install

Download the zip from the [Releases](https://github.com/schetle/p99plus/releases) page, or build it yourself (see [Building](#building)).

Copy `dinput8.dll` and `p99plus_core.dll` into the EverQuest folder, next to `eqgame.exe`. To uninstall, delete both, plus any `p99plus_core.live.*.dll` (a copy the game uses while running); `p99plus.ini`, `p99plus_imgui.ini` and `p99plus.log` can stay or go.

### Wine / Proton only: enable the override

Wine prefers its builtin dinput8 unless told otherwise. Use either method:

- **winecfg** (run it against your game's prefix): on the *Libraries* tab add `dinput8` and set it to *Native then Builtin*.
- **Environment variable:** launch with `WINEDLLOVERRIDES="dinput8=n,b"`.

### Check it worked

A "loaded" banner appears at the top of the screen for about 10 seconds after the game starts, and `p99plus.log` appears in the EverQuest folder.

## Use

Press **Insert** to open the menu. While it's open, the game gets no mouse or keyboard input. The key can be changed under *Overlay → Menu toggle key*. Ctrl+click any slider to type an exact value.

The menu lists the features down the left side; click one to open its page, which starts with a short description. Features that are switched off are greyed in the list. Mouse and Limiter have an **Enable …** checkbox at the top, Zone info and Combat text a **Show …** one, and Nameplates a **Change nameplates** one; switching it off greys out the page and turns the feature off. Camera has a switch per section instead, and Overlay has none.

### Mouse

- `X sensitivity` / `Y sensitivity`: multiply the game's horizontal and vertical mouse input separately. *Reset mouse* sets both back to 1.0.
- `Frame-rate independent mouselook` (on by default): without it, horizontal turning gets weaker as FPS rises, and the usual workaround is capping FPS at about 60. With it on, turning feels the same at any FPS, matching the game at 60 FPS. If you raised X sensitivity to compensate for the bug, set it back to about 1.0.
- `Remove mouselook axis dead-zone` (on by default): the game ignores the smaller axis of a mostly-horizontal or mostly-vertical mouse movement, so diagonal mouselook jumps. With this on, both axes always count.

The last two options only appear when `eqgame.exe` is the expected Titanium build.

### Camera

Three independent systems, each in its own section with its own checkbox. Switching one off greys out that section only.

#### FOV

By default the FOV uses Hor+ scaling: the game's 4:3 vertical FOV is kept and the horizontal FOV widens to fit your screen, so ultrawide works with nothing to configure. With `Enable FOV control` off, the game's own FOV is used, which crops the top and bottom on wide screens.

- `First-person FOV` / `Third-person FOV`: move a slider to set an exact vertical FOV in degrees instead. The reset button below each returns it to default.

#### Offset

- `Enable camera offset`: off leaves the eye where the game puts it.
- *First person* and *Third person*: separate `Up`, `Forward` and `Right` offsets (negative values go the other way), each set with its own reset button.
- `Shrink offset as you zoom in` (on by default): fades the third-person offset out as the camera nears your head. `Full offset beyond distance` sets the zoom distance at which the full offset applies.
- Camera collision ignores the offset, so a large offset can put the camera inside walls.

#### Orbit

Hold the middle mouse button and drag to rotate the camera around your character without turning them. In first person this works as free look. While it's on, the game no longer sees the middle button.

- `Middle mouse drag rotates the camera` (on by default): enables the orbit.
- Right-click always swings the camera back.
- `Return camera when you release`: the camera swings back as soon as you let go of the button.
- `Return camera when you move`: the camera also swings back when you move or turn. Greyed out while the option above is on.

#### Fixes

- `Fix camera jump facing north` (on by default): the game points the camera a fraction of a degree short of where your character faces, and the gap closes all at once as you turn through north, so the view jumps a little there. This removes the jump.

### Zone info

A small window in the game's own style, with a border like the game's other windows and up to three lines. Each line has its own checkbox on the Zone info page (*Zone name*, *Heading*, *Last Location*); the lines that are on are shown from the top with no gaps, and with all three off the window goes away.

- the long name of the zone you are in, for example "Butcherblock Mountains";
- the result of your last Sense Heading and how long ago it was, for example `Heading: North (12s ago)`. A failed attempt shows `Heading: unsure`, and before your first attempt it shows `Heading: ?`. Zoning clears it;
- the result of your last `/loc` and how long ago it was, for example `Last Location: 120.50, -340.25, 3.75 (40s ago)`, with the numbers as the game printed them. Before your first `/loc` in a zone it shows `Last Location: ?`, and zoning clears it. The line has a pin checkbox at its right end (the skin's own checkbox, so it matches whatever skin you use): tick it to keep the location shown, through new `/loc` results and zoning. Untick it to unpin; the line then shows your latest `/loc` in the current zone.

The heading and the location come from the messages the game prints when you use Sense Heading (from its ability button or a hotkey) or type `/loc` yourself. p99plus never uses the skill or the command for you and doesn't work out either on its own; it only shows what the game told you.

Untick *Enable Pin Location* (under *Last Location* on the page) to remove the checkbox; that also unpins. Esc doesn't close the window, like the target and chat windows.

Drag the window to move it and drag its edges to resize it. Its position and size are saved with your other windows in the character's UI ini (`UI_<character>_<server>.ini`). Untick *Show the zone info window* to remove it.

The first time you enter the world, p99plus adds two small files to your skin folder (`UIFiles\<skin>`): `EQUI_P99PlusZoneInfoWnd.xml` and `EQUI_p99plus.xml`. If you switch skins, they are added to the new one. They are safe to delete; they come back on the next load. If the game ever rejects them, your skin loads as usual and the window stays off until you restart the game.

### Combat text

Numbers and words over the one a hit or heal happens to, in the game's own font, starting just above the name label: red for damage, grey for a miss (`Miss`, `Block`, `Parry`, `Riposte`, `Dodge`, `Invulnerable`, `Absorbed`), green for a heal. Each one stays for a moment, fades out and drifts up a little. Texts on the same target stack: a new one starts at the bottom and pushes the older ones up a line, and with more than five, the oldest goes.

- `Show combat text` (on by default): turns it all on or off.
- `Attacks on you`: hits and misses on you, over your head (in first person, just above the middle of the screen).
- `Attacks on your pet`: hits and misses on your pet, over it.
- `Attacks on enemies`: your hits and misses, and your pet's, over the one attacked.
- `Attacks from other players` (off by default): other players' hits and misses, and their pets', over the one attacked. Only fights close enough to show in your chat window.
- `Other fights` (off by default): NPCs' hits and misses on anyone but you and your pet, such as guards fighting monsters, over the one attacked. Only fights close enough to show in your chat window.
- `Heals on you`: your HP going up, shown where hits on you are, as one `+N` per heal. This includes regen; `Smallest heal` hides gains below it, so most regen ticks stay out.
- `Show time` and `Fade time`: how long a text stays at full strength, then how long it takes to fade.
- `Rise`: how far, in pixels, a text drifts up while it is shown.
- `Start height`: moves where texts start, in pixels above the name label (negative goes lower).
- `Font size`: one of the game's font sizes, as in `/chatfontsize`.

The hits shown are the ones the game prints in your chat window. Heals on others aren't shown: the game only knows their health as a percentage.

### Nameplates

The name labels the game shows over spawns.

- `Change nameplates` (on by default): turns it all on or off.
- `Hide your name`: your own name isn't shown over your character.
- `Your name`, `NPC names`, `Other players`: the size of those names, in percent of the game's size (50–250). Pets count as NPCs; corpses keep the game's size. Names still shrink with distance as they normally do, and very large ones stop growing at about a seventh of the screen.
- `Target health status`: a line under your target's name that says how hurt it is: **Uninjured** (green, 99 % health and up), **Barely Injured** (yellow-green, 67–98 %), **Injured** (yellow, 34–66 %) or **Badly Injured** (red, 1–33 %). It is shown for NPCs that aren't pets, and goes when they die. Only your target gets it: the server only tells you an NPC's health while you have it targeted. It grows, shrinks and fades out with the name and, like the name, is hidden by anything in front of it.
- `Your health status`: the same line under your own name, whether or not you have yourself targeted. With `Hide your name` on, it takes your name's place.
  - `Hide when Uninjured`: your line only shows once you're below 99 % health (Barely Injured or worse).
- `Health text size`: the health status text's size, in percent of the name's (30–100).

**Debuff icons.** Small spell icons in a row under an NPC's name, one per debuff on it, like the icons in your buff window. They only show while the NPC's name does, and like the name they are hidden by anything in front of it: walls, scenery, your own character.

- `Mine`: your own debuffs (snares, slows, DoTs, roots, mezzes and so on). The icon goes when the game tells you the spell wore off, when the NPC dies or leaves, or when you zone.
- `My pet's`: the same for your pet's debuffs.
- `Icon size`: in pixels.

Only spells that last a while get an icon; a nuke doesn't. Casting a spell again on the same NPC keeps one icon for it.

### Limiter

- `Enable frame limiter` (off by default) and `FPS cap`: 10–240, a wider range than the in-game setting offers.

If you use it, set the game's own cap (`MaxFPS` in `eqclient.ini`) at least as high as the overlay cap, or the game's limit applies first.

### Overlay

- `Show FPS when menu hidden`: an FPS counter in the top-left corner while the menu is closed.
- `Cursor speed`: how fast the menu cursor moves. Under gamescope with `--force-grab-cursor`, the menu cursor is driven by raw mouse input, so adjust this if it feels too fast or slow.
- `Menu toggle key`: click it, then press the new key. Hold Ctrl, Shift or Alt for a combo.
- `Reload core`: loads `p99plus_core.dll` again without restarting the game (see [Hot reload](#hot-reload)).

### Settings

Changes take effect immediately but aren't saved until you click **Save** (top right of the menu). The file, `p99plus.ini`, lives in the EverQuest folder and is created with the defaults on first launch; `eqclient.ini` is never touched.
A `*` in the menu title (and a tinted Save button, plus an "unsaved" note beside it) means unsaved changes; **Reload** discards them and re-reads the file.

## Building

Both builds produce two 32-bit DLLs, `build/dinput8.dll` and `build/p99plus_core.dll`, which you then install as above. MinGW is required (MSVC won't work).

### Linux

```sh
sudo apt install g++-mingw-w64-i686 cmake   # once
./build.sh
cp build/dinput8.dll build/p99plus_core.dll "/path/to/EverQuest/"
```

### Windows (untested)

`build.sh` is Linux-only, so build with CMake directly:

1. Install [MSYS2](https://www.msys2.org) and open the **MSYS2 MINGW32** shell (it must be the 32-bit one).
2. `pacman -S --needed mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-ninja`
3. In the project folder:
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```
4. Copy `build/dinput8.dll` and `build/p99plus_core.dll` into the EverQuest folder.

### Hot reload

`dinput8.dll` stays loaded while the game runs, and everything else lives in `p99plus_core.dll`, which the game reloads by itself within about a second of the file changing. Replace it with a rename, not by writing over it in place (on Linux: copy it next to the old one, then `mv` it over), so the game never reads a half-written file. Settings carry over, unsaved ones included, and p99plus's own game windows keep the position and size you dragged them to; **Reload core** on the Overlay page reloads it by hand. Changes to `dinput8.dll` need a game restart. If a new build fails to load, the game keeps running without the mod until the file changes again; `p99plus.log` says why.

## More

How it works internally: [docs/how-it-works.md](docs/how-it-works.md).

## License

MIT, see [LICENSE](LICENSE). p99plus uses [Dear ImGui](https://github.com/ocornut/imgui) (MIT) and [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause); their licenses are in [third_party/](third_party/) and in `THIRD_PARTY_LICENSES.txt` in each release zip.

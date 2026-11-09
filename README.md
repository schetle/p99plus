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

Copy `dinput8.dll` and `p99plus_core.dll` into the EverQuest folder, next to `eqgame.exe`. To uninstall, delete both, plus any `p99plus_core.live.*.dll`; the other `p99plus*` files can stay or go.

### Wine / Proton only: enable the override

Wine prefers its builtin dinput8 unless told otherwise. Use either method:

- **winecfg** (run it against your game's prefix): on the *Libraries* tab add `dinput8` and set it to *Native then Builtin*.
- **Environment variable:** launch with `WINEDLLOVERRIDES="dinput8=n,b"`.

### Check it worked

A "loaded" banner appears at the top of the screen for about 10 seconds after the game starts, and `p99plus.log` appears in the EverQuest folder.

## Use

Press **Insert** to open the menu (the key can be changed on the Overlay page). While it's open, the game gets no mouse or keyboard input. Ctrl+click a slider to type an exact value.

The features are listed down the left side; click one to open its page. The switch at the top of a page turns that feature off.

### Mouse

- `X sensitivity` / `Y sensitivity`: multiply the game's horizontal and vertical mouse input separately.
- `Frame-rate independent mouselook` (on by default): without it, horizontal turning gets weaker as FPS rises (the usual workaround is capping FPS at about 60). With it on, turning feels like the game at 60 FPS, whatever your FPS. If you raised X sensitivity to make up for this, set it back to about 1.0.
- `Remove mouselook axis dead-zone` (on by default): the game ignores the smaller axis of a mostly-horizontal or mostly-vertical mouse movement, so diagonal mouselook jumps. With this on, both axes always count.

The last two options only appear when `eqgame.exe` is the expected Titanium build.

### Camera

Three independent sections, each with its own switch.

#### FOV

By default the FOV uses Hor+ scaling: the game's 4:3 vertical FOV is kept and the horizontal FOV widens to fit your screen, so ultrawide works with nothing to configure. With FOV control off, the game's own FOV is used, which crops the top and bottom on wide screens.

- `First-person FOV` / `Third-person FOV`: set an exact vertical FOV in degrees instead.

#### Offset

- *First person* and *Third person*: separate `Up`, `Forward` and `Right` offsets.
- `Shrink offset as you zoom in` (on by default): fades the third-person offset out as the camera nears your head. `Full offset beyond distance` sets the zoom distance at which the full offset applies.
- Camera collision ignores the offset, so a large offset can put the camera inside walls.

#### Orbit

Hold the middle mouse button and drag to rotate the camera around your character without turning them (on by default). In first person this works as free look. While it's on, the game no longer sees the middle button.

- Right-click always swings the camera back.
- `Return camera when you release`: the camera swings back as soon as you let go of the button.
- `Return camera when you move`: the camera also swings back when you move or turn.

#### Fixes

- `Fix camera jump facing north` (on by default): the game points the camera a fraction of a degree short of where your character faces, and the gap closes all at once as you turn through north, so the view jumps a little there. This removes the jump.

### Zone info

A small game window with up to three lines, each with its own checkbox on the page:

- the long name of the zone you are in, for example "Butcherblock Mountains";
- the result of your last Sense Heading and how long ago it was, for example `Heading: North (12s ago)`, or `Heading: unsure` after a failed attempt;
- the result of your last `/loc` and how long ago it was, for example `Last Location: 120.50, -340.25, 3.75 (40s ago)`. Tick the pin checkbox at the end of the line to keep that location shown through new `/loc` results and zoning.

The heading and the location come from the messages the game prints when you use Sense Heading or type `/loc` yourself. p99plus never uses the skill or the command for you; it only shows what the game told you.

The first time you enter the world, p99plus adds two small files to your skin folder (`UIFiles\<skin>`): `EQUI_P99PlusZoneInfoWnd.xml` and `EQUI_p99plus.xml`. They are safe to delete; they come back on the next load. If the game ever rejects them, your skin loads as usual and the window stays off until you restart the game.

### Combat text

Numbers and words over the one a hit or heal happens to, just above the name label: red for melee damage, purple for spell damage (nukes and DoT ticks), grey for a miss (`Miss`, `Block`, `Parry`, ...), green for a heal. Each one fades out as it drifts up, and texts on the same target stack.

- `Attacks on you`: over your head (in first person, just above the middle of the screen).
- `Attacks on your pet`: over your pet.
- `Attacks on enemies`: your and your pet's hits and misses, including your DoT ticks.
- `Attacks from other players` (off by default): other players' and their pets' hits and misses.
- `Other fights` (off by default): NPCs' hits and misses on anyone but you and your pet, such as guards fighting monsters.
- `Heals on you`: one `+N` per heal, regen included; `Smallest heal` hides gains below it, so most regen ticks stay out.
- `Show time`, `Fade time`, `Rise`, `Start height` and `Font size` set how the texts look and move.

The hits shown are the ones the game prints in your chat window, so other people's fights only show when they're close enough. Heals on others aren't shown: the game only knows their health as a percentage.

### Nameplates

The name labels the game shows over spawns.

- `Hide your name`: your own name isn't shown over your character.
- `Your name`, `NPC names`, `Other players`: the size of those names, in percent of the game's size. Pets count as NPCs; corpses keep the game's size.
- `Target health status`: a line under your target's name that says how hurt it is: **Uninjured**, **Barely Injured**, **Injured** or **Badly Injured**, from green to red. Only for NPCs that aren't pets, and only your target: the server only tells you an NPC's health while you have it targeted.
- `Your health status`: the same line under your own name, whether or not you have yourself targeted. With `Hide your name` on, it takes your name's place.
  - `Hide when Uninjured`: your line only shows once you're hurt.

**Debuff icons.** Small spell icons in a row under an NPC's name, one per debuff on it, like the icons in your buff window.

- `Mine`: your own debuffs (snares, slows, DoTs, roots, mezzes and so on). The icon goes when the game tells you the spell wore off, when the NPC dies or leaves, or when you zone.
- `My pet's`: the same for your pet's debuffs.

Only spells that last a while get an icon; a nuke doesn't.

### Notepad

A small book icon (your skin's spellbook button) opens and closes the notepad, and so does **Ctrl+Shift+N**. The key can be changed, and the icon hidden, on the Notepad page.

On the left is the list of your notes, newest first. On the right is the selected note: a header with when it was written, the zone, the position you were at (the numbers `/loc` would show) and the character, then the text.

- **New** starts a note, stamped with the time, zone, position and character name.
- **Edit / View** switches between typing in the note and reading it formatted.
- While typing, the usual Ctrl shortcuts work: copy, cut and paste (through the Windows clipboard), select all, undo and redo.
- While the text box has the keyboard it has a gold ring, and no keys reach the game.
- All your characters share one notebook.

Notes are saved as Markdown files, one per note, in a `notes` folder inside the EverQuest folder. You can open them in any editor; a file you add or change outside the game shows up the next time the notepad opens.

The formatted view shows headings (`#`), bullet and numbered lists, `**bold**`, `*italic*`, `` `code` ``, `> quotes`, `---` rules and `[links](url)` (the link text only), using colours rather than styles.

Like the zone info window, the notepad is added to your skin the first time you enter the world; after you update p99plus, type `/loadskin <your skin>` once in the game.

### Models

- `Enable newer player models` (off by default): players show the newer, more detailed models (the ones introduced with Shadows of Luclin) instead of the 1999 ones.
- One `Male` and one `Female` checkbox per race, for each of the 12 classic races and Iksar, so you can keep the old look for some.

Changes apply the next time you enter the world. Only you see them: other players' clients decide for themselves.

### Limiter

- `Enable frame limiter` (off by default) and `FPS cap`: a wider range than the in-game setting offers.

If you use it, set the game's own cap (`MaxFPS` in `eqclient.ini`) at least as high as the overlay cap, or the game's limit applies first.

### Overlay

- `Show FPS when menu hidden`: an FPS counter in the top-left corner while the menu is closed.
- `Cursor speed`: how fast the menu cursor moves. Under gamescope with `--force-grab-cursor`, the menu cursor is driven by raw mouse input, so adjust this if it feels too fast or slow.
- `Menu toggle key`: click it, then press the new key (Ctrl, Shift or Alt combos work).
- `Reload core`: loads `p99plus_core.dll` again without restarting the game (see [Hot reload](#hot-reload)).

### Settings

Changes take effect immediately but aren't saved until you click **Save** (top right of the menu). The file, `p99plus.ini`, lives in the EverQuest folder; `eqclient.ini` is never touched. A `*` in the menu title means unsaved changes; **Reload** discards them.

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

`dinput8.dll` stays loaded while the game runs, and everything else lives in `p99plus_core.dll`, which the game reloads by itself within about a second of the file changing. Replace it with a rename, not by writing over it in place (on Linux: copy it next to the old one, then `mv` it over), so the game never reads a half-written file. Settings carry over, unsaved ones included. Changes to `dinput8.dll` need a game restart. If a new build fails to load, the game keeps running without the mod until the file changes again; `p99plus.log` says why.

## More

How it works internally: [docs/how-it-works.md](docs/how-it-works.md).

## License

MIT, see [LICENSE](LICENSE). p99plus uses [Dear ImGui](https://github.com/ocornut/imgui) (MIT) and [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause); their licenses are in [third_party/](third_party/) and in `THIRD_PARTY_LICENSES.txt` in each release zip.

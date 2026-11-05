# p99plus

A `dinput8.dll` proxy for the EverQuest **Titanium** client that adds an in-game menu with:

- camera offset (first and third person) and middle-drag camera orbit
- FOV control, with ultrawide support
- FPS-independent horizontal mouse turning and separate X/Y sensitivity
- removal of the mouselook axis dead-zone
- an optional frame limiter

> Check your server's rules before using. Injected DLLs are typically treated like MacroQuest.

## Compatibility

- **Client:** EverQuest Titanium only (`eqgame.exe` built Oct 31 2005, 32-bit). Other clients use different memory addresses, so the patches won't apply.
- **Linux:** tested with Proton (DXVK), including under gamescope. Plain Wine is untested but should work the same way.
- **Windows:** not tested. It should only need the DLL in the EverQuest folder, with no override.

## Install

Download the zip from the [Releases](https://github.com/schetle/p99plus/releases) page, or build it yourself (see [Building](#building)).

Copy `dinput8.dll` into the EverQuest folder, next to `eqgame.exe`. To uninstall, delete it; `p99plus.ini`, `p99plus_imgui.ini` and `p99plus.log` can stay or go.

### Wine / Proton only: enable the override

Wine prefers its builtin dinput8 unless told otherwise. Use either method:

- **winecfg** (run it against your game's prefix): on the *Libraries* tab add `dinput8` and set it to *Native then Builtin*.
- **Environment variable:** launch with `WINEDLLOVERRIDES="dinput8=n,b"`.

### Check it worked

A "loaded" banner appears at the top of the screen for about 10 seconds after the game starts, and `p99plus.log` appears in the EverQuest folder.

## Use

Press **Insert** to open the menu. While it's open, the game gets no mouse or keyboard input. The key can be changed under *Overlay → Menu toggle key*. Ctrl+click any slider to type an exact value.

Each feature has its own tab with a short description. Mouse and Limiter have an **Enable …** checkbox at the top; switching it off greys out the tab and leaves that part of the game as it was. Camera has a switch per section instead, and Overlay has none.

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

### Limiter

- `Enable frame limiter` (off by default) and `FPS cap`: 10–240, a wider range than the in-game setting offers.

If you use it, set the game's own cap (`MaxFPS` in `eqclient.ini`) at least as high as the overlay cap, or the game's limit applies first.

### Overlay

- `Show FPS when menu hidden`: an FPS counter in the top-left corner while the menu is closed.
- `Cursor speed`: how fast the menu cursor moves. Under gamescope with `--force-grab-cursor`, the menu cursor is driven by raw mouse input, so adjust this if it feels too fast or slow.
- `Menu toggle key`: click it, then press the new key. Hold Ctrl, Shift or Alt for a combo.

### Settings

Changes take effect immediately but aren't saved until you click **Save** (top right of the menu). The file, `p99plus.ini`, lives in the EverQuest folder and is created with the defaults on first launch; `eqclient.ini` is never touched.
A `*` in the menu title (and a tinted Save button, plus an "unsaved" note beside it) means unsaved changes; **Reload** discards them and re-reads the file.

## Building

Both builds produce a 32-bit `build/dinput8.dll`, which you then install as above. MinGW is required (MSVC won't work).

### Linux

```sh
sudo apt install g++-mingw-w64-i686 cmake   # once
./build.sh
cp build/dinput8.dll "/path/to/EverQuest/"
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
4. Copy `build/dinput8.dll` into the EverQuest folder.

## More

How it works internally: [docs/how-it-works.md](docs/how-it-works.md).

## License

MIT, see [LICENSE](LICENSE). p99plus uses [Dear ImGui](https://github.com/ocornut/imgui) (MIT) and [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause); their licenses are in [third_party/](third_party/) and in `THIRD_PARTY_LICENSES.txt` in each release zip.

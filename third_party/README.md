# Third-party code

Vendored copies of the libraries p99plus builds against. Each folder keeps its upstream license.

| Folder | Library | Version | License | Upstream |
|---|---|---|---|---|
| `imgui/` | Dear ImGui | v1.91.5 | MIT | https://github.com/ocornut/imgui |
| `minhook/` | MinHook | v1.3.4 | BSD 2-clause (includes Hacker Disassembler Engine, BSD) | https://github.com/TsudaKageyu/minhook |

## Dear ImGui (trimmed)

Only the files used by the build are kept: the core sources and headers, `imgui_demo.cpp` (reference only, not built), `LICENSE.txt`, and the DirectX 9 and Win32 backends. `docs/`, `examples/`, `misc/` and the other backends were removed.

## MinHook

Kept as released, minus the `build/` folder (Visual Studio and MinGW project files; CMake builds it from `src/`).

## Updating

1. Download the new release.
2. Copy over the files that already exist in the folder, and nothing else.
3. Update the version in the table above, then rebuild.

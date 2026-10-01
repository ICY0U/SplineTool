# AutoGrind for Rollout Inline

[![Tests](https://github.com/ICY0U/SplineTool/actions/workflows/tests.yml/badge.svg)](https://github.com/ICY0U/SplineTool/actions/workflows/tests.yml)

**AutoGrind** is an Unreal Engine 5.4 editor plugin for Rollout Inline mod makers. It finds the ledges, box
edges, coping and rails in a level, including lines that run across modular pieces, shows them for
review, and places the game's grind actors along them. Where a line is missing, click points in the
viewport and it places one, snapped to the edge.

- **[Manual](AutoGrind/README.md)**: install, quick start, the panel, drawing lines, presets and settings
- **[Release notes](AutoGrind/RELEASE_NOTES.md)**
- **[Tests](AutoGrind/Tests/README.md)**: benchmark, regressions, editor and in-engine checks, releasing

## Layout

| Path | What |
| --- | --- |
| `AutoGrind/` | The plugin: copy this folder to `<YourProject>/Plugins/`. |
| `AutoGrind/Source/AutoGrind/Private/` | The editor module: panel, Draw mode, scanning, placing, preview. |
| `AutoGrind/Source/AutoGrind/Private/Core/` | The detector, free of engine code, so it builds and is tested on its own. |
| `AutoGrind/Tests/` | Benchmark scenes, regressions, stress tests, the editor check and the in-engine checks. |
| `scripts/package.ps1` | Builds a release zip with Unreal's BuildPlugin. |

## Build and test

    cmake -S AutoGrind/Tests -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

The plugin itself builds with Unreal Engine 5.4 on Windows: open a C++ project that has it in `Plugins/`, or
run `powershell -ExecutionPolicy Bypass -File scripts/package.ps1`.

MIT licence: see [AutoGrind/LICENSE](AutoGrind/LICENSE).

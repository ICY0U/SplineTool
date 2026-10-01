# AutoGrind tests

Three kinds of check. Only the last needs Unreal; none needs the game.

## The detector on its own

`../Source/AutoGrind/Private/Core` is engine-free: the plugin compiles it, and so do these checks.

    cmake -S AutoGrind/Tests -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

On Windows `run_regression.bat` does the same with Visual Studio's compiler. Warnings are errors, at least
as strictly as in Unreal builds (shadowed names are errors there). `-DAUTOGRIND_SANITIZE=ON` builds with
AddressSanitizer and UndefinedBehaviorSanitizer (GCC or Clang).

| Check | What it covers |
| --- | --- |
| `regression` | One scene per fix: long-edge obstruction splitting, simplification, stairs, rail pairing across posts and brackets, closed curves and rail seams, slat gaps, chamfers, buried and walled edges, seams between blocks, faceted hoops and handrails, progress and cancel. Each fails when its fix is undone. Ends with `AUTOGRIND_CORE_PASS`. |
| `benchmark` | 40 skatepark scenes built from boxes, tubes and extrusions, each with the lines it should have: modular pieces, rails, coping, pipes, convexity (domes, mounds, rounded blocks, kickers), stairs, curbs, walls and messy geometry. `-turn` also runs every scene turned and moved off the origin (200 runs). Lines are scored for coverage, pieces, kind, review status, false positives and forbidden places. `-v` prints every miss; a name part runs only matching scenes. |
| `drawing` | The Draw mode's engine: picking by ray, snapping to sharp edges, paths along edges across modular pieces (and none across a real gap), whole edge runs, closed runs and the polyline helpers. |
| `stress` | A 480,000-triangle ledge, 200 modular pieces and a huge floor under 100 crates, against a time limit (seconds, default 60). |

Built with `-DAUTOGRIND_LEGACY` against an older detector's source, `benchmark` scores it the same way,
counting every line as kept; that is how the 0.11 figures in the release notes were measured.

### The editor module without the engine

    cmake -S AutoGrind/Tests -B build -DAUTOGRIND_EDITOR_CHECK=ON -DCMAKE_CXX_COMPILER=clang++

compiles and links every editor source file against `EditorCheck/`, small stand-ins for the parts of the
engine API the module uses, with warnings as errors. It catches typos, wrong types, delegates bound to
methods of the wrong signature, format strings that do not match their arguments and missing definitions.
It needs GCC or Clang. Passing it does not prove the module builds against the engine; the in-engine
checks below do.

The `Tests` workflow in `.github/workflows` runs all of the above on Windows and Linux for every push.

## Scorer: the detector against retail maps

`harness` runs the same detector over a whole exported map and scores it against the map's hand-placed
GrindActors.

    ./run_outdoor.sh                 # builds the harness, scores OutdoorSkatepark's skate meshes
    ./run_map.sh TheBigHall          # scores a whole map (every mesh but scenery)
    PLOTS=build/plots ./run_outdoor.sh   # also draws a top-down plot per mesh

The last published results are from 0.9.0 (2026-09-25, default settings, no per-map tuning) and have not
been remeasured since:

| Map | Recall | Retail GrindType matched |
| --- | --- | --- |
| OutdoorSkatepark (skate meshes) | 100% of 717 m | 40/40 |
| TheBigHall (whole map) | 100% of 312 m | 17/18 |
| Observatory (whole map) | 98% of 1630 m | 66/69 |

Precision is a lower bound: designers did not line every edge that grinds. On OutdoorSkatepark the
unmatched lines are real lips they skipped (deck backs, box ends, a coping-less quarter pipe).

`build/<map>.found.json.rejected.json` lists every edge turned down and why; `tools/why.py` shows the lines
and rejections near a point, `tools/probe.py` the triangles.

### Regenerating the data

The `.agt` geometry and `.truth.json` lines come from your own USD + JSON export of the game's maps. They
are derived from the game's assets, so they are not included in this repository. `tools/geometry.py` needs
Pixar's `usd-core` (`pip install --target <dir> usd-core`, then `PYTHONPATH=<dir>`), and the export's
`park-raises.usda` has NaN UVs that USD refuses: copy the `.usda` files with `NaN` replaced by `0` and read
the copy. USD is right-handed, so the script mirrors Y (verified: the straight rail and the T-box edge land
exactly on their GrindActors).

    python tools/truth.py <Map>.json data/<Map>.truth.json
    python tools/geometry.py <clean copy>/<Map>.usda data/<Map>.agt flip

Git Bash rewrites arguments that start with `/` into Windows paths, so name parts passed to the harness
never start with a slash.

## In-engine checks

    powershell -File run_unreal.ps1

Run from `<Project>/Plugins/AutoGrind/Tests` with the editor closed. Builds the editor and runs
`AutoGrindTestCommandlet`:

- UE mesh winding on the engine cube;
- a ledge box and a mirrored, turned copy giving exactly their four top edges as stone;
- a thin cylinder giving one rail line on its crest;
- two cube modules end to end giving one line along both, naming both as sources;
- a cube bent along a spline mesh scanned as it is bent;
- the viewport preview drawing, redrawing and clearing only its own lines;
- Generate placing a GrindActor per line made like the hand-placed ones (GrindType, edited spline on the
  line, one Movable query-only grind mesh per segment, a grind-channel trace answering at every segment);
- in an editor world: invalid lines, deleted sources and incompatible classes failing without changing
  earlier output, generating again replacing only its own actors, undo and redo through Generate and
  Remove, and a line drawn by hand surviving Generate and Remove Scanned Lines.

Passes with `AUTOGRIND_TEST_PASS` in `build/unreal-test.log`.

`-run=AutoGrindInspect -map=/Game/...` dumps every GrindActor in a map, for comparing generated ones with
hand-placed ones.

Headless traps: commandlets start without the editor's undo buffer (`GEditor->Trans` is null; the test makes
one with `CreateTrans`), and only non-game worlds record destroyed actors for undo (`UWorld::DestroyActor`
calls `Modify` only there). A headless editor world never builds its collision tree, so the trace checks run
in a game world.

## Releasing

1. The `Tests` workflow is green.
2. In a Rollout Inline mod project with the plugin in `Plugins/AutoGrind`: `run_unreal.ps1` passes.
3. In the editor, on a real map: scan selected actors and the whole level, review and generate, draw a few
   lines (snapped, followed round a curve, Ctrl+Click, closed), undo and redo each, and remove them.
4. Package the mod and ride a ledge, a rail, coping, a joined modular line and a drawn line in the game.
5. Set `VersionName` in `AutoGrind.uplugin`, add the release notes, and build the zip with
   `powershell -ExecutionPolicy Bypass -File scripts/package.ps1` (add `-StrictIncludes` once to check every
   file includes what it uses). Attach the zip to a GitHub release.

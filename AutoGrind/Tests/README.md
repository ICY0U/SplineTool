# AutoGrind tests

## Standalone regressions

Run `run_regression.bat` on Windows with Visual Studio C++ Build Tools installed. This builds the
actual detector with warnings as errors and tests long-edge obstruction splitting, straight-line
simplification, stair/missing-ground rejection, rail pairing across interrupted sides, and closed
curve/rail seam preservation. No game assets are needed.

0.11.0 adds scenes built from boxes and 12-sided tubes, scanned with the drop and wall tests over
their own triangles: a slatted bench seat (only its outer edges), a ledge with a 45 degree chamfer
(one line, at the chamfer's foot), a tube rail with a bracket at crest height (one rail across it),
a ledge buried in a wall (no line inside), two wall blocks with a 1 cm seam (joined lines), a hoop of
tube built from long facets (one closed rail) and a sloped handrail of 3 m facets (one rail). Each
of these fails when the fix it covers is undone.

The retail recall scores below are historical 0.9.0 results, not 0.10.0 measurements.

Two kinds of check. Neither needs the game.

## Scorer: the detector against retail maps

`../Source/AutoGrind/Private/Core` is engine-free, so `build/harness.exe` compiles the same detector the
plugin runs and scores it against each retail map's hand-placed GrindActors in seconds.

    ./run_outdoor.sh                 # builds the harness, scores OutdoorSkatepark's skate meshes
    ./run_map.sh TheBigHall          # scores a whole map (every mesh but scenery)
    PLOTS=build/plots ./run_outdoor.sh   # also draws a top-down plot per mesh

Results on 2026-09-25, default settings, no per-map tuning:

| Map | Recall | Retail GrindType matched |
| --- | --- | --- |
| OutdoorSkatepark (skate meshes) | 100% of 717 m | 40/40 |
| TheBigHall (whole map) | 100% of 312 m | 17/18 |
| Observatory (whole map) | 98% of 1630 m | 66/69 |

Precision is a lower bound: designers did not line every edge that grinds. On OutdoorSkatepark the
unmatched lines are real lips they skipped (deck backs, box ends, a coping-less quarter pipe).
The three type differences are thin tops retail calls stone (a free-standing pipe, a 12 cm beam).

`build/<map>.found.json.rejected.json` lists every edge turned down and why; `tools/why.py` shows
the lines and rejections near a point, `tools/probe.py` the triangles.

### Regenerating the data

The `.agt` geometry and `.truth.json` lines come from your own USD + JSON export of the game's maps.
They are derived from the game's assets, so they are not included in this repository. `tools/geometry.py` needs Pixar's `usd-core` (`pip install --target <dir> usd-core`,
then `PYTHONPATH=<dir>`), and the export's `park-raises.usda` has NaN UVs that USD refuses: copy
the `.usda` files with `NaN` replaced by `0` and read the copy. USD is right-handed, so the script
mirrors Y (verified: the straight rail and the T-box edge land exactly on their GrindActors).

    python tools/truth.py <Map>.json data/<Map>.truth.json
    python tools/geometry.py <clean copy>/<Map>.usda data/<Map>.agt flip

Git Bash rewrites arguments that start with `/` into Windows paths, so name parts passed to the
harness never start with a slash.

## In-engine checks

    powershell -File run_unreal.ps1

Builds the editor (close it first) and runs `AutoGrindTestCommandlet`: UE mesh winding on the
engine cube, a ledge box and a mirrored, turned copy giving exactly their four top edges, a thin
cylinder giving one rail line on its crest, and the viewport preview drawing and clearing only its
own lines. Generate then places a GrindActor per line and each is checked against the hand-placed
ones (GrindType, edited spline on the line, one Movable query-only grind mesh per segment, and a
grind-channel trace answering at every segment). In an editor world, generating again replaces
only its own actors, and undo steps back through Generate and Remove. Passes with
`AUTOGRIND_TEST_PASS` in `build/unreal-test.log`.

`-run=AutoGrindInspect -map=/Game/...` dumps every GrindActor in a map, for comparing generated ones
with hand-placed ones.

Headless traps: commandlets start without the editor's undo buffer (`GEditor->Trans` is null; the
test makes one with `CreateTrans`), and only non-game worlds record destroyed actors for undo
(`UWorld::DestroyActor` calls `Modify` only there). A headless editor world never builds its
collision tree, so the trace checks run in a game world.

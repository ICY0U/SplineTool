# AutoGrind

An Unreal Engine 5.4 editor plugin for **Rollout Inline** mod makers. Select the meshes in your map that
should be grindable, and AutoGrind finds the ledges, box edges, coping and rails on them, shows them in
the viewport, and places the game's `GrindActor` along each one. No more drawing grind splines by hand.

> **0.11.0 preview.** See [release notes](RELEASE_NOTES.md): slat gaps and seams are no longer
> lines, edges against or inside walls are left out, and tube rails with posts come out as one rail.
> Standalone and headless Unreal tests are available; hands-on UI review and packaged-game riding
> are still required.

## What it finds

- **Ledges and box edges**: the outer edge of a flat top where the surface falls away just past it
  (at least 25 cm by default), with nothing standing in the way. Placed as a stone grind.
- **Bank and quarter-pipe lips** without coping: a top edge that drops onto a steep slope.
- **Coping and curved edges**: bowl rims and curved decks become one line that follows the curve.
- **Rails**: a narrow top (15 cm or less) that falls away on both sides becomes one line along the
  crest of the tube, joined across posts and brackets. Placed as a rail grind. Sloped handrails and
  rings work too.

Stair nosings are left out: they only drop one step and land on a flat tread. So are the gaps between
the slats of a bench or table, seams and small steps, where a flat surface carries the top on, and
edges pressed against a wall or buried inside another part.

The **0.9.0 baseline** was measured against the grind lines Rollout Inline's maps place by hand.
These historical scores have **not** been remeasured for 0.10.0:

| Map | Hand-placed grind length found | Rail or stone type matched |
| --- | --- | --- |
| Outdoor Skatepark | 100% | 40 of 40 lines |
| The Big Hall | 100% | 17 of 18 lines |
| Observatory | 98% | 66 of 69 lines |

It also finds real edges the designers chose not to line, such as the backs of decks. You untick
those before placing.

## Requirements

- Unreal Engine **5.4** on Windows (the version Rollout Inline modding uses).
- A Rollout Inline mod project with the grind Blueprint at
  `/Game/MainFolder/Blueprints/Grinding/GrindActor`, the same path the game uses. Scanning and the
  preview work without it; placing needs it.

## Install

1. Close the Unreal editor.
2. Unzip so that you have `<YourProject>/Plugins/AutoGrind/AutoGrind.uplugin`. Create the `Plugins`
   folder if it does not exist.
3. Open your project. AutoGrind is enabled automatically; it is under **Edit > Plugins > Rollout Modding**.

The download includes compiled binaries for UE 5.4 on Windows. If the editor offers to rebuild
AutoGrind (for example on another engine version), you need Visual Studio 2022 with the
"Game development with C++" workload; the full source is included.

AutoGrind is an editor-only plugin: it is never cooked or packaged into your mod.

## Use

1. Open **Tools > AutoGrind**.
2. Select the meshes that should carry grind lines: ledges, boxes, rails, ramps. Instanced meshes work.
   Select only skateable pieces; walls and scenery have edges too.
3. Click **Scan Selected**. Stone lines draw in blue, rails in red.
4. Go through the list:
   - untick lines you do not want (they turn grey);
   - click **Rail / Stone** to change a line's grind type;
   - double-click a row to frame that line in the viewport.
5. Click **Generate**. Each ticked line becomes a `GrindActor` in an **AutoGrind** outliner folder,
   named after the mesh it came from. **Ctrl+Z** undoes it.
6. Package your mod as usual and ride.

Generating again validates new output before replacing what AutoGrind placed for the same meshes
in the current output level. Your
hand-placed GrindActors are never touched. **Remove Generated** deletes everything AutoGrind placed in
the level (also undoable).

### When a line is missing

Turn on **Show Near Misses** in the panel's settings and scan again. Edges that were considered and
turned down are drawn in grey, and the scan summary counts them by reason: the fall past the edge
is too small, something stands just past the edge, another part covers the top, a flat surface
carries on past it (a seam, step or slat gap), a wall or part stands over, beside or across it, the
edge is inside another part, the neighbouring face rises (an inside corner), the edge is too steep,
or the line is too short. Each maps to a setting below.

## Settings

| Setting | Default | What it does |
| --- | --- | --- |
| Max Top Slope | 45° | A face this flat or flatter counts as a top. Only a top's outer edge can carry a line. |
| Min Drop | 25 cm | How far the surface must fall just past an edge. Keeps stair steps out. |
| Steep Lip | 40° | An edge dropping onto a slope this steep counts even with less drop (bank and quarter-pipe lips). 0 turns it off. |
| Min Length | 50 cm | Shorter lines are dropped, such as the caps of rail posts. |
| Max Line Slope | 45° | Steeper edges are skipped. |
| Max Corner | 40° | A sharper turn splits a line, as at the corner of a box. |
| Gap Bridge | 6 cm | A flat surface this close past an edge, level with it or a small step below, carries the top on: slat gaps, seams and steps are not lips. |
| Join Gap | 10 cm | Pieces of one ledge line are joined across a gap this short, such as a seam between blocks. |
| Check Walls | on | Test the space over, across and along each edge against nearby meshes, so edges against or inside walls are left out. Slower on big selections. |
| Rail Max Width | 15 cm | A narrower top with a fall on both sides is a rail. Rollout's rails measure 3–8 cm. |
| Rail Min Drop | 10 cm | How far the surface must fall past a rail's sides, so a railing along a wall top is still a rail. |
| Rail Join Gap | 40 cm | A rail continues across a gap this short, where a post or bracket meets it. |
| Show Near Misses | off | Draw rejected edges in grey. |

The Advanced section holds how far past the edge the drop is measured, clearance, vertex weld and
line simplification tolerances. Settings are remembered per project.

## Limits

- Spline-mesh components (meshes bent along a spline) are not scanned yet.
- Meshes with no source geometry, such as cooked stand-ins, are skipped and named in the panel.
- A few thin tops that Rollout calls stone come out as rails (a free-standing pipe, a narrow beam).
  Switch them with the Rail / Stone button.

## How it places GrindActors

Each line becomes a GrindActor like hand-placed ones: the actor stands on the line's first point and
faces along it, the spline points are set in its own space and marked as edited, and the grind type
is set to rail or stone. The GrindActor's own construction script then builds one grind mesh per
segment, so curves follow the line.

## Building and testing from source

The detector lives in `Source/AutoGrind/Private/Core` and has no engine dependencies. `Tests/`
holds a standalone scorer that runs it against a map's hand-placed GrindActors, and an in-engine test
commandlet; see `Tests/README.md`. The scorer's map data comes from your own export of the game and is
not included.

## Licence

MIT: see `LICENSE`. Rollout Inline and its assets belong to their owners; this plugin contains none of them.

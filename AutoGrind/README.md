# AutoGrind

An Unreal Engine 5.4 editor plugin for **Rollout Inline** mod makers. AutoGrind finds the ledges, box
edges, coping and rails in your level, shows them in the viewport for review, and places the game's
`GrindActor` along each one. Where you want a line it did not find, click points in the viewport and it
places a grind line through them, snapped to the edge and following it round corners and curves.

**1.0.0.** See the [release notes](RELEASE_NOTES.md) for everything that changed since 0.11.

- [What it finds](#what-it-finds)
- [Install](#install)
- [Quick start](#quick-start)
- [The panel](#the-panel)
- [Drawing lines by hand](#drawing-lines-by-hand)
- [Presets](#presets)
- [Settings](#settings)
- [When a line is missing, or one is wrong](#when-a-line-is-missing-or-one-is-wrong)
- [How grind actors are placed](#how-grind-actors-are-placed)
- [Limits](#limits)
- [Building and testing](#building-and-testing)

## What it finds

- **Ledges and box edges**: the outer edge of a flat top where the surface falls away just past it (at
  least 25 cm by default), with nothing standing in the way. Placed as stone grinds.
- **Modular pieces as one line.** Ledge, curb and rail modules placed end to end, with small gaps,
  overlaps or slight misalignment, give one line across all of them, so one grind actor runs the whole
  length instead of one per piece. A line stops at a real corner.
- **Rails**: a narrow top with a fall on both sides becomes one line along its crest, joined across posts,
  brackets and rail segments. Round tubes, square bars, flat bars, sloped handrails, kinked rails and
  rings all work. A narrow top on a deep body, such as a planter wall or a thin concrete wall, is stone.
- **Coping**: the coping of a quarter pipe or a bowl is a rail, a closed one round a bowl.
- **Pipes**: a round top too wide to be a rail (up to 40 cm) gives one stone line along its crest
  instead of two along its shoulders.
- **Bank and quarter-pipe lips** without coping: a top edge that drops onto a steep slope.
- **Curbs and manual pads**: edges that fall less than a ledge onto a wide, level landing are listed
  unticked for you to review, or kept or left out, as you choose.
- **Ridges**, optionally: sharp convex ridges with no top, such as an A-frame rail.

Just as important is what it leaves out:

- **Anything that is not a convex edge.** The surface must turn down sharply over an edge, so domes,
  mounds, rounded shapes like cars and the sloping sides of kickers and banks get no lines. Bevelled and
  bullnosed ledges still count.
- **Stairs**: nosings only drop one step onto a tread.
- **Seams, steps and slat gaps**, where a flat surface carries the top on.
- **Edges pressed against or buried in a wall**, and tops covered by another part.
- **Roofs and high wall tops**, with Max Drop set (the Street preset sets it to 4 m).
- **Messy geometry is repaired**: triangles wound the wrong way and double-sided planes do not break
  lines.

Every line gets a confidence. Lines the scan is less sure of (low ledges, very high drops such as the backs
of decks, rounded edges, partial rails, ridges, short lines) are listed unticked, and the tooltip on each
line says why.

## Install

Rollout Inline modding uses **Unreal Engine 5.4 on Windows**. Placing lines needs the game's grind
Blueprint at `/Game/MainFolder/Blueprints/Grinding/GrindActor`, as in the official mod project;
scanning, the preview and drawing work without it.

**From a release** (no compiler needed):

1. Close the Unreal editor.
2. Download `AutoGrind-<version>-UE5.4-Win64.zip` from the repository's releases and unzip it so that
   you have `<YourProject>/Plugins/AutoGrind/AutoGrind.uplugin`. Create the `Plugins` folder if needed.
3. Open your project. AutoGrind is under **Edit > Plugins > Rollout Modding** and enabled.

**From source**: copy the `AutoGrind` folder to `<YourProject>/Plugins/`. A C++ project builds it when it
opens, with Visual Studio 2022 and its "Game development with C++" workload. For a Blueprint-only
project, build a release zip with `scripts/package.ps1` (see [Building and testing](#building-and-testing)).

AutoGrind is editor-only: it is never cooked or packaged into your mod.

## Quick start

1. Open **Tools > AutoGrind**, or click the **AutoGrind** button in the level editor toolbar.
2. Choose **Selected Actors** and select the skateable meshes, or choose **Whole Level**.
3. Click **Scan**. Stone lines draw in blue, rails in red, lines for review in grey.
4. Review the list: untick what you do not want, switch rail and stone, and look at the unticked lines.
5. Click **Generate**. Each ticked line becomes a `GrindActor` in an **AutoGrind** outliner folder.
   **Ctrl+Z** undoes it.
6. For anything missing, switch on **Draw Lines** and click along the edge in the viewport; press
   **Enter** to place the line.
7. Save the map, package your mod as usual and ride.

## The panel

### Scan

- **Selected Actors** scans the static meshes of the actors selected in the level. **Whole Level** scans
  every static mesh in the level, leaving out what the Filtering settings exclude: names such as
  Foliage, Tree or Water (whole words, so "Tree" does not match "Street"), meshes without collision,
  actors hidden in the editor and props smaller than 25 cm.
- Plain, instanced and spline meshes are scanned; spline meshes as they are bent.
- Actors tagged `NoGrind` or `AutoGrindIgnore`, and existing grind actors, are never scanned.
- **Preset** sets the detection settings for a kind of map in one click (see [Presets](#presets)).
- **Clear** forgets the lines and removes the preview.

A scan shows a progress bar and can be cancelled; the lines found until then are listed.

### Draw by Hand

**Draw Lines** switches the Draw mode on and off (also **Tools > AutoGrind Draw**). **Auto**, **Rail** and
**Stone** choose the grind type of the lines you draw, and **Follow edges** and **Snap** match the F and
S keys of the Draw mode. See [Drawing lines by hand](#drawing-lines-by-hand).

### Lines

The summary says what was scanned, how many lines were found and kept, what the filters left out, and,
with **Show Near Misses** on, why edges were turned down and which setting decides.

The list has a column for each line's **Keep** tick, **Kind** (click to switch rail and stone),
**Length**, **Drop** (how far the surface falls past the edge), **Sure** (the confidence), the **Actor**
it runs along (with how many more for a joined line) and **Notes**. Hover over a line for the details.
Click a column header to sort.

- Search by actor name, `rail`, `stone`, `kept`, `review` or a note such as `joined` or `coping`.
- **All**, **Kept**, **Review**, **Rails** and **Stone** filter the list.
- **Keep**, **Exclude**, **Rail**, **Stone** and **Frame** act on the selected lines, or on every line
  shown when none is selected. **As Suggested** ticks every line the scan was sure of and unticks the rest.
- Selected lines are drawn yellow in the viewport with arrows along them. Double-click a line to frame it.
- Right-click for the same actions, **Select Their Actors** and **Remove from List**.
- With the list focused: **Space** toggles Keep, **Delete** excludes, **F** frames, **R** makes rails,
  **S** makes stone.

The notes are: *joined* (runs across several meshes), *low* (a curb or manual pad), *high* (falls far: the
back of a deck?), *rounded* (only just sharp enough), *pipe* (along a wide round top), *coping*,
*partial* (only part of a narrow top paired into a rail), *bank lip*, *ridge*, *short* and *closed*.

### Place

- **Generate** places a grind actor along every ticked line, hidden ones included. Lines generated
  earlier from the same actors in the current level are replaced, but only once every new actor has been
  made and checked: if anything fails, nothing changes. Lines drawn by hand are never replaced. The
  panel refuses to generate when a scanned actor has moved or been deleted, the level changed, or
  detection settings changed since the scan.
- **Remove** removes the grind actors AutoGrind placed: those from scans, those drawn by hand, or all.
  Grind actors placed by hand are never touched.
- **Select Placed** selects every grind actor AutoGrind placed.

Generate, Remove and each drawn line are single undo steps.

## Drawing lines by hand

Switch on **Draw Lines** in the panel, then click on the level in the viewport.

| Input | Does |
| --- | --- |
| Click | Add a point |
| Shift+Click | Add a point exactly where you click, without snapping |
| Ctrl+Click | Take the whole edge run or scanned line under the cursor and place it at once |
| Enter, or double-click | Place the line |
| Backspace | Remove the last point |
| Esc | Drop the points; press again to leave the Draw mode |
| T | Grind type: auto, rail or stone |
| F | Follow edges between points, or go straight |
| S | Snapping on or off |

Letter keys are left to the camera while the right mouse button is held.

- **Snapping**: a click snaps to a scanned line first, then to a sharp convex edge of the mesh under the
  cursor or of the pieces within 30 cm of it, then to the surface. The snap reach grows with the distance
  from the camera, from 2 to 40 cm. The edge or line it will snap to is highlighted as you move.
- **Following**: between two clicks on the same scanned line, or on connected sharp edges, the line follows
  them round curves and corners and across modular pieces, so two clicks can trace a whole curved ledge.
  Off, or with no edge between them, the line runs straight.
- **Closing**: click the first point again and place the line to close a loop.
- **Auto** makes a line a rail when most of its points snapped to scanned rails, and stone otherwise.
- The viewport shows the mode's state, the line's length and where the cursor would snap.

## Presets

| Preset | For | Changes from the defaults |
| --- | --- | --- |
| Skatepark | Skateparks and plazas | None: the defaults. |
| Street | City maps | Curbs kept; edges falling more than 4 m (roofs, high wall tops) left out; High Drop 1.2 m. |
| Strict | Fewer, surer lines | Low ledges left out; sharper edges only (45°); lines of 1 m and more; drops up to 3 m; Keep Confidence 0.7. |
| Loose | Everything that might grind, for review | Low ledges from 8 cm kept; softer edges (20°); lines from 30 cm; ridges on; Keep Confidence 0.3. |

A preset sets the detection settings only: output, filter lists and preview settings are kept.

## Settings

The settings are below the panel, remembered per user and project. Changing a detection setting asks for
a new scan; preview and output settings apply at once.

**Ledges**

| Setting | Default | What it does |
| --- | --- | --- |
| Max Top Slope | 45° | A face this flat or flatter is a top. Only the outer edge of a top can carry a line. |
| Min Drop | 25 cm | How far the surface must fall just past an edge for it to be a ledge. |
| Low Ledges | List for Review | Edges falling less than Min Drop onto a wide, level landing (curbs, manual pads): Leave Out, List for Review or Keep. |
| Low Ledge Min Drop | 12 cm | How far a low ledge or curb must fall. |
| Steep Lip | 40° | An edge falling onto a slope this steep is a lip even with less drop: banks and quarter pipes without coping. 0 turns it off. |
| Min Edge Angle | 30° | Convexity: how sharply the surface must turn down over an edge. Keeps lines off domes, mounds and rounded shapes. 0 turns it off. |
| Gap Bridge | 6 cm | A flat surface this close past an edge, level with it or a small step below, carries the top on: seams, steps and slat gaps are not lips. |
| Check Walls | on | Test the space over and beside each edge against the meshes around it, so edges against or inside walls are left out. Slower on big scans. |
| Reject Stairs (advanced) | on | Leave out stair nosings. |

**Rails**

| Setting | Default | What it does |
| --- | --- | --- |
| Rail Max Width | 15 cm | A narrower top with a fall on both sides gives one line along its crest. Rollout's rails measure 3 to 8 cm. |
| Rail Max Thickness | 20 cm | A flat narrow top on a deeper body is a wall or beam, placed as stone. 0 makes every narrow top a rail. |
| Rail Min Drop | 10 cm | How far the surface must fall past a rail's sides, so a railing along a wall is still a rail. |
| Round Top Max Width | 40 cm | A round top up to this wide (a pipe) gives one line along its crest. |
| Detect Ridges | off | Also find sharp ridges with no top, listed for review. |
| Min Ridge Angle | 70° | How sharply a ridge's faces must meet. |

**Lines**

| Setting | Default | What it does |
| --- | --- | --- |
| Min Length | 50 cm | Shorter lines are dropped, measured after joining across meshes. |
| Max Line Slope | 45° | Steeper edges are skipped. |
| Max Corner | 40° | A sharper turn ends one line and starts the next, as at the corner of a box. Also where Ctrl+Click stops. |
| Join Across Meshes | on | Join the lines of separate meshes placed next to each other into one line. |
| Join Gap | 10 cm | Ledge pieces are joined across a gap this short. |
| Rail Join Gap | 40 cm | A rail continues across a gap this short, where a post or bracket meets it. |

**Filtering**

| Setting | Default | What it does |
| --- | --- | --- |
| Max Drop | 0 (no limit) | Edges falling further are left out: roofs, cliffs, high wall tops. |
| High Drop | 150 cm | Lines falling further are less sure: often the back of a deck. |
| Keep Confidence | 0.5 | Lines at or above this are ticked after a scan; the rest are listed for review. |
| Repair Winding | on | Turn round triangles wound against their neighbours and drop double-sided copies. |
| Exclude Names | Foliage, Tree, Grass... | Whole Level scans skip actors and meshes with one of these words in their names. |
| Exclude Tags | NoGrind, AutoGrindIgnore | Actors with one of these tags are never scanned. |
| Require Collision | on | Whole Level scans skip meshes with collision off. |
| Skip Hidden | on | Whole Level scans skip actors hidden in the editor. |
| Min Mesh Size | 25 cm | Whole Level scans skip meshes smaller than this in every direction. |

**Review**: Show Near Misses (draw rejected edges in grey), Show Directions (arrows along every line) and
Preview Thickness.

**Output**

| Setting | Default | What it does |
| --- | --- | --- |
| Grind Actor Class | Rollout's GrindActor | The actor placed along each line. |
| Output Folder | AutoGrind | Outliner folder for placed actors; drawn lines go in a Drawn folder inside it. |
| Label Prefix | AutoGrind_ | Placed actors are named this, then the actor the line runs along. |
| Height Offset | 0 cm | Raises every placed spline. |
| Point Type | Linear | Linear runs exactly on the edge; Curve is smoother on coping and curved rails. |
| Grind Type Property, Rail Type Name, Stone Type Name (advanced) | GrindType, NewEnumerator0, NewEnumerator1 | How the grind actor stores rail or stone. |

**Advanced**: Probe Distance (how far past an edge the fall is measured, 15 cm), Sample Spacing (long
edges are checked in spans this long, 25 cm), Clearance (height that must be clear past an edge, 150 cm),
Max Drop Search (5000 cm), Weld Tolerance (0.1 cm) and Simplify Tolerance (1 cm).

## When a line is missing, or one is wrong

Switch on **Show Near Misses** and scan again. Edges that were considered and turned down are drawn in
grey, and the summary counts them by reason, with the setting that decides:

| Reason | Setting |
| --- | --- |
| The fall past the edge is too small | Min Drop, Low Ledges |
| The fall is more than Max Drop: a roof, wall top or cliff | Max Drop |
| Something stands just past the edge | Probe Distance |
| A flat surface carries on just past the edge: a seam, step or slat gap | Gap Bridge |
| Another part covers the top; a wall or part stands over, beside or across the edge; the edge is inside another part | Check Walls |
| The surface rounds over smoothly: not a sharp convex edge | Min Edge Angle |
| A stair step | Reject Stairs |
| The neighbouring face rises: an inside corner | Max Top Slope |
| Steeper than the maximum line slope; the side of a ramp, bank or kicker | Max Line Slope |
| One side of a narrow top that falls too little for a ledge and pairs with no other side | Rail Min Drop |
| No surface below the edge | Max Drop Search |
| Shorter than the minimum length | Min Length |

Other fixes:

- A line came out as the wrong kind: click its Kind, or select several and press R or S.
- Lines stop at a seam between modules: raise Join Gap a little, or check the modules really meet.
- A line still missing: draw it by hand. Ctrl+Click on the edge takes the whole run at once.
- A mesh is named as skipped: it has no source geometry in this project (a cooked stand-in).

## How grind actors are placed

Each line becomes a grind actor made like the hand-placed ones: it stands on the line's first point and
faces along it, the spline points are set in its own space and marked as edited, and its grind type is set
to rail or stone. The grind actor's construction script then builds one grind mesh per segment, so curves
follow the line. AutoGrind checks the result (the spline must still run along the line after the
construction script has run) and fails without placing anything when it does not.

Placed actors carry the tag `AutoGrind` (lines drawn by hand also `AutoGrindDrawn`) and a tag naming each
actor the line runs along, which is how Generate finds earlier output to replace.

## Limits

- Only static meshes are scanned (plain, instanced and spline meshes). Landscape and brushes are not
  scanned, though the fall past an edge is measured against everything with collision.
- The panel notices moved and deleted actors and settings changes, but not edits to a mesh's geometry or
  instances: scan again after them.
- Rail or stone is decided from shape alone. A few thin tops a map's designer would call stone (a
  free-standing pipe, a narrow beam) can come out as rails; switch them in the list.
- The Draw mode follows the sharp edges of meshes up to 300,000 triangles together with their neighbours;
  on bigger ones clicks snap to scanned lines and the surface.

## Building and testing

The detector lives in `Source/AutoGrind/Private/Core` and has no engine dependencies, so it is built and
tested on its own:

    cmake -S AutoGrind/Tests -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

or `Tests/run_regression.bat` on Windows. The checks include a benchmark of 40 skatepark scenes with known
lines (modular pieces, rails, coping, convexity, stairs, curbs, walls and messy geometry), each also turned
and moved off the origin, and a stress test on a 480,000-triangle mesh. `Tests/run_unreal.ps1` builds the
editor and runs the in-engine checks; `scripts/package.ps1` builds a release zip. See
[Tests/README.md](Tests/README.md).

## Licence

MIT: see [LICENSE](LICENSE). Rollout Inline and its assets belong to their owners; this plugin contains none
of them.

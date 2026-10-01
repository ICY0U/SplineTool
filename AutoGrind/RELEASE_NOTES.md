# AutoGrind 1.0.0

A rework of detection, the panel and placing, and a new way to place lines: click points in the viewport.

## Detection

- **Modular pieces are one line.** Ledge, curb and rail modules placed next to each other, with gaps,
  overlaps or slight misalignment, now give one line across all of them, so one grind actor runs the
  whole length instead of one per piece (Join Across Meshes). Lines still stop at real corners, and two
  copies of the same mesh in one place give one set of lines.
- **Rail or stone.** A narrow top is a rail when it is round or its body is thin, and stone when it sits on
  a deep body: planter and concrete wall tops are stone (Rail Max Thickness). Square and flat bars, kinked
  and sloped rails and rails on posts or above walls are rails; quarter-pipe and bowl coping is a rail; a
  pipe too wide to be a rail gives one stone line along its crest (Round Top Max Width).
- **Convexity.** The surface must turn down sharply over an edge (Min Edge Angle), so domes, mounds,
  rounded shapes such as cars and the sloping sides of kickers and banks get no lines, while bevelled and
  bullnosed ledges keep theirs.
- **What not to line.** Stairs, ramp sides, roofs and high wall tops (Max Drop), seams and slat gaps,
  edges against walls and covered tops are left out. Curbs and manual pads (Low Ledges), very high drops
  such as the backs of decks (High Drop), rounded edges, partial rails, ridges (Detect Ridges) and short
  lines are listed for review instead of being kept.
- **Confidence.** Every line has a confidence and notes saying why; lines below Keep Confidence are listed
  unticked.
- **Messy geometry.** Triangles wound the wrong way are turned round and double-sided planes are read as
  one surface (Repair Winding).
- **Spline meshes** are scanned as they are bent; instanced meshes per instance.
- **Faster.** In the stress test a 480,000-triangle mesh scans in about 2 seconds, and 200 modular pieces in a
  hundredth of one.

## Drawing lines by hand (new)

- Switch on **Draw Lines** and click points in the viewport; Enter places a grind actor along them.
- Clicks snap to scanned lines, then to the sharp convex edges of the mesh under the cursor and the
  pieces around it, then to the surface. Shift+Click places a point without snapping.
- Between two clicks the line follows the edge or scanned line round curves and corners and across
  modular pieces. Ctrl+Click takes a whole edge run or scanned line at once.
- T switches rail, stone or auto, F following edges, S snapping. Each drawn line is one undo step, goes in
  its own outliner folder and is never replaced by Generate.

## Panel

- **Whole Level** scans, with filters for scenery names, collision, hidden actors and small props, and
  `NoGrind` / `AutoGrindIgnore` tags.
- **Presets**: Skatepark, Street, Strict and Loose.
- A sortable, searchable list with Kept, Review, Rails and Stone filters, confidence and notes columns,
  tooltips explaining each line, bulk actions, a context menu and keyboard shortcuts.
- The selected lines are highlighted in the viewport with direction arrows; Show Directions draws arrows
  on every line.
- The summary explains near misses by reason and the setting that decides.
- Remove scanned lines, drawn lines or both; Select Placed; an icon, a toolbar button and a help link.

## Placing

- New output settings: Grind Actor Class (and how it stores rail or stone), Output Folder, Label Prefix,
  Height Offset and Point Type (Linear or Curve).
- Placing still validates every new actor before replacing earlier output, and fails without changing
  anything.

## Measured

On the benchmark of 40 skatepark scenes in `Tests/` (each also turned and moved off the origin: 200 runs):

| | 0.11 | 1.0 |
| --- | --- | --- |
| Scenes passed | 17 of 40 | 40 of 40 (200 of 200) |
| Joining scenes (modular pieces) | 1 of 12 | 12 of 12 |
| Rail scenes | 7 of 12 | 12 of 12 |
| Filtering scenes (what not to line) | 9 of 14 | 14 of 14 |
| Messy geometry scenes | 0 of 2 | 2 of 2 |
| Required length covered by lines of the right kind | 78.3% of 203 m | 100% |
| Required lines missing | 14 | 0 |
| Lines split into extra pieces | 33 | 0 |
| Lines of the wrong kind | 6 | 0 |
| Line length where no line belongs | 25.4 m | 0 |
| Lines on stairs, slopes and other forbidden places | 10 | 0 |

0.11 had no review list, so every line it found counts as kept.

## Upgrading

Close the editor and replace `Plugins/AutoGrind`. Settings saved by 0.11 carry over; new settings start at
their defaults. Lines placed by 0.11 carry the same tags and are replaced by Generate as before.

# AutoGrind 0.11.0 preview

Detection fixes found scanning a ripped city map (NYC Brooklyn: 161 meshes went from 1215 lines to 515).

## Changes

- **Slats and seams.** A flat surface just past an edge, level with it or a small step below (Gap Bridge, 6 cm), carries the top on, so the gaps between bench and table slats, seams and small steps are no longer lips. Picnic tables went from about 15 lines each to 6.
- **Walls.** With Check Walls on (the default), the space over, across and along each edge is tested against the triangles of every collision-enabled static mesh nearby, hit from either side. An edge pressed against a wall, running into one, or buried inside another part is no longer a line. The drop test's physics traces cull back faces, so they could not see a wall they started inside.
- **Rails.** The two sides of a tube are matched point by point and the rail runs midway between them, so a side broken by a post or bracket no longer turns the tube into two stone lines. Rail pieces are joined across gaps up to Rail Join Gap (40 cm) where nothing stands over or beside the rail, and a ring whose ends meet closes. A rail needs a fall of only Rail Min Drop (10 cm) on its sides, so a railing along a wall top is a rail; that lower fall never makes a stone line. A lower bar under a top rail is covered by it and left out.
- **Joined ledges.** Pieces of one ledge line are joined across gaps up to Join Gap (10 cm), such as the seams between the blocks of a wall.
- **Which side is out.** Outward is now level and square to the edge, and its side comes from the top face's winding. The old third-vertex test put it on the wrong side along long facets of sloped handrails and curved tubes, losing most of those rails.
- **45 degree chamfers.** A face at exactly Max Top Slope is kept on one side of it (half a degree of slack), so a 45 degree chamfer no longer gives two lines, one on each side of it.
- New regression tests for each fix; each fails when its fix is undone. The in-engine test logs why an edge was turned down when a box check fails.

# AutoGrind 0.10.0 preview

Reliability and review-workflow update for Unreal Engine 5.4 / Rollout Inline.
Pending packaged-game riding and hands-on editor UI acceptance.

## Changes

- Long edges are checked in spans of at most 25 cm by default, with three probes per span. Failed spans split the line, preserving usable sections. Local obstructions can no longer be outvoted by distant clear probes.
- Rail pairing requires coverage in both directions, preventing a partially interrupted side from recreating its rejected section. Unequal sides can remain separate stone candidates for review.
- Closed curves retain at least three distinct points under extreme simplification. Closed rail centering uses a consistent seam direction.
- Paths remain linear between detected points to follow geometry. This release does not add Bezier smoothing or cross-actor joining.
- Replacement actors are validated before earlier output is deleted. Invalid points, deleted sources, missing splines, incompatible grind types, or construction scripts resetting points fail without removing previous output.
- Undo captures generated components directly, fixing restored actors with invalid spline components in the supplied project Blueprint.
- Replacement is scoped to the current output level. Remove Generated still removes tagged output across loaded levels.
- Search by actor name, rail, stone, open or closed. Keep Visible, Exclude Visible and bulk type buttons affect filtered results. Generate includes ALL kept results, including hidden ones.
- Settings changes require a rescan. Generate checks deleted or transformed actors. Component/mesh edits are not automatically tracked: rescan after editing geometry or instances.
- Toolbar wrapping, a scan busy dialog, and explicit reporting of unsupported spline-mesh components.

## Install

Close the destination editor, back up its plugin, and replace `Plugins/AutoGrind` with this package's `AutoGrind` folder. Open Tools > AutoGrind, select meshes, scan, review, generate, then save the map. This editor-only plugin places the game's GrindActor Blueprint.

## Detection limits

Sample Spacing controls longitudinal resolution. Lower values increase trace cost and detect smaller interruptions. Obstacles between probes can still be missed. Clearance/landing use world collision, so inaccurate or missing collision affects results. Spline-deformed meshes remain unsupported. Rescan after geometry edits.

The 0.9.0 retail-map recall figures are historical and have not been remeasured for this version. Before a stable release, inspect real maps, benchmark large selections, review the panel interactively, package a mod, and ride straight/sloped rails, bowl rims, split ledges and seams in the retail game.

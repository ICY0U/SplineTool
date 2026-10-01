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

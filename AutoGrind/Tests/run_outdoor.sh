#!/bin/sh
# Builds the harness and scores the detector on OutdoorSkatepark. Scans the skateable meshes;
# scenery is left out of the drop context. PLOTS=<dir> also draws a top-down plot per mesh.
cd "$(dirname "$0")"
rm -f build/harness.exe
cmd //c "$(cygpath -w "$PWD/build_harness.bat")" | grep -v -e vswhere -e '^harness.cpp' -e '^AutoGrind[A-Za-z]*\.cpp' -e '^Generating'
test -f build/harness.exe || exit 1
SCAN="side_island corner_stair_raise bowl/ halfpipe park_banks t_shaped_box tilted_rails instanced_straight_rails corner_quarter_pipe round_ramp_small park_raises isle_0"
EXCLUDE="Foliage imposter mountain background electrical spruce BP_"
./build/harness.exe data/OutdoorSkatepark.agt build/OutdoorSkatepark.found.json $SCAN -- $EXCLUDE || exit 1
python tools/score.py data/OutdoorSkatepark.truth.json build/OutdoorSkatepark.found.json data/OutdoorSkatepark.agt "${PLOTS:-}"

#!/bin/sh
# Scores the detector on a whole retail map: every mesh except scenery is scanned.
#   ./run_map.sh <Map>          e.g. TheBigHall, Observatory, OutdoorSkatepark
# Needs data/<Map>.agt and data/<Map>.truth.json (tools/geometry.py, tools/truth.py; see README).
# PLOTS=<dir> also draws a top-down plot per mesh. The harness must already be built (run_outdoor.sh does it).
cd "$(dirname "$0")"
MAP="$1"
# No leading slash on name parts: Git Bash rewrites /x/ arguments into Windows paths.
EXCLUDE="Foliage imposter mountain background electrical spruce BP_ trees skyscraper sign banner windows"
./build/harness.exe data/$MAP.agt build/$MAP.found.json "$MAP/" -- $EXCLUDE || exit 1
python tools/score.py data/$MAP.truth.json build/$MAP.found.json data/$MAP.agt "${PLOTS:-}"

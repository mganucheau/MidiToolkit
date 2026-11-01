#!/bin/zsh
# Lets Claude trigger builds on this Mac. Start it once in Terminal:
#   ./Tools/build-watch.sh
# It waits for a file named .build-request in the repo, runs ./build.sh
# (and the unit tests if the request says "test"), and writes the output
# to build.log and the exit code to build.status. Stop it with Ctrl-C.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
echo "Watching for build requests in $ROOT (Ctrl-C to stop)"
while true; do
  if [[ -f .build-request ]]; then
    req="$(cat .build-request)"
    rm -f .build-request build.status
    echo "$(date '+%H:%M:%S') building..."
    {
      echo "== build $(date)"
      ./build.sh
      code=$?
      if [[ $code -eq 0 && "$req" == *test* ]]; then
        echo "== tests"
        cmake --build build --config Release --target MidiToolkitTests -j"$(sysctl -n hw.ncpu)" && \
          (cd build && ctest --output-on-failure -C Release)
        code=$?
      fi
      echo "== exit $code"
    } > build.log 2>&1
    tail -1 build.log | sed 's/== exit //' > build.status
    echo "$(date '+%H:%M:%S') done: exit $(cat build.status)"
  fi
  sleep 2
done

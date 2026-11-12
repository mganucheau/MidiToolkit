#!/bin/zsh
# Capture Midi Toolkit Standalone into build/qa/current/iterNNN.png
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/qa/current"
mkdir -p "$OUT" "$ROOT/build/qa/ref" "$ROOT/build/qa/logs"

# Next iteration index
n=0
while [[ -f "$OUT/$(printf 'iter%03d.png' $n)" ]]; do n=$((n + 1)); done
FILE="$OUT/$(printf 'iter%03d.png' $n)"

osascript -e 'tell application "Midi Toolkit" to activate' >/dev/null 2>&1 || true
sleep 0.35
osascript <<'EOF' >/dev/null 2>&1 || true
tell application "System Events"
  tell process "Midi Toolkit"
    set frontmost to true
    try
      set position of front window to {40, 40}
      set size of front window to {1280, 860}
    end try
  end tell
end tell
EOF
sleep 0.25

WID=$(swift -e 'import Cocoa; let o: CGWindowListOption=[.optionOnScreenOnly,.excludeDesktopElements]; guard let info=CGWindowListCopyWindowInfo(o,kCGNullWindowID) as? [[String:Any]] else {fatalError()}; for w in info { if (w[kCGWindowOwnerName as String] as? String)=="Midi Toolkit" { print(w[kCGWindowNumber as String] as! Int); break } }')
if [[ -z "${WID:-}" ]]; then
  echo "Midi Toolkit window not found — launch Standalone first" >&2
  exit 1
fi
screencapture -l "$WID" "$FILE"
echo "$FILE"

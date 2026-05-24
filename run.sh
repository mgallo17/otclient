#!/bin/bash
set -e

OTDIR="$(cd "$(dirname "$0")" && pwd)"

# --- Start XQuartz if needed ---
if ! pgrep -qf "Xquartz"; then
    echo "[*] Starting XQuartz..."
    /opt/X11/libexec/launchd_startx /opt/X11/bin/startx -- /opt/X11/bin/Xquartz -listen tcp &
    
    # Wait until display :0 is ready
    for i in $(seq 1 15); do
        if DISPLAY=:0 /opt/X11/bin/xdpyinfo &>/dev/null; then
            echo "[*] XQuartz ready on :0"
            break
        fi
        sleep 1
    done
fi

# Verify display works
if ! DISPLAY=:0 /opt/X11/bin/xdpyinfo &>/dev/null; then
    echo "[!] ERROR: X11 display :0 not available"
    exit 1
fi

# Check TCP
if DISPLAY=127.0.0.1:0 /opt/X11/bin/xdpyinfo &>/dev/null; then
    USE_DISPLAY="127.0.0.1:0"
    echo "[*] Using TCP display (avoids SHM issues)"
else
    USE_DISPLAY=":0"
    echo "[*] Using local display :0"
fi

# --- Maps: keep minimap cache for fast loading ---
echo "[*] Minimap cache preserved"

echo "[*] Launching OTClient..."
echo "[*] Close with Ctrl+C in this terminal"
echo ""

cd "$OTDIR"
exec env \
    DISPLAY="$USE_DISPLAY" \
    DYLD_LIBRARY_PATH="/opt/homebrew/opt/mesa/lib" \
    LIBGL_ALWAYS_SOFTWARE=1 \
    ./otclient "$@"

#!/bin/sh
# Background music playback script (improved)
# Make executable with: chmod +x play_background.sh

# Define variables
AUDIO_FILE="/mnt/nfs/wav/background.wav"
DEVICE="hw:0,0"

# Clean up any existing aplay instances for this file
# Use killall if available
if command -v killall >/dev/null 2>&1; then
    killall -9 aplay >/dev/null 2>&1
fi

# Alternative cleanup using pgrep/pkill if available
if command -v pgrep >/dev/null 2>&1; then
    pgrep -f "$AUDIO_FILE" | xargs -r kill -9
elif command -v pkill >/dev/null 2>&1; then
    pkill -9 -f "$AUDIO_FILE" >/dev/null 2>&1
fi

# Ensure we have a clean start
sleep 1

# Set process to be more resilient
trap "" HUP  # Ignore hangup signal

# Print some debugging info
echo "[BGM] Starting background music playback..."
echo "[BGM] File: $AUDIO_FILE, Device: $DEVICE"

# Infinite loop to continuously play the audio file
while true; do
    if [ ! -f "$AUDIO_FILE" ]; then
        echo "[BGM] File not found: $AUDIO_FILE. Retrying in 5s..."
        sleep 5
        continue
    fi
    echo "[BGM] Playing..."
    # Play the audio file
    aplay -D$DEVICE "$AUDIO_FILE" 
    if [ $? -ne 0 ]; then
        echo "[BGM] aplay failed, trying default device..."
        aplay "$AUDIO_FILE"
    fi
    echo "[BGM] Playback ended, restarting in 2s..."
    sleep 2
done

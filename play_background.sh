#!/bin/sh
# Background music playback script
# This script loops infinitely to play the background music
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
echo "Starting background music playback script"
echo "Audio file: $AUDIO_FILE"
echo "Audio device: $DEVICE"

# Infinite loop to continuously play the audio file
while true; do
    echo "Playing audio file..."
    # Play the audio file
    aplay -D$DEVICE $AUDIO_FILE
    
    # If aplay exits with error, wait before retrying to avoid CPU spinning
    echo "Audio playback ended, restarting in 2 seconds..."
    sleep 2
done

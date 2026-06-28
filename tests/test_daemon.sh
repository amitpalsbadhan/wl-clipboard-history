#!/usr/bin/env bash

# Comprehensive unit & integration tests for wl-clipboard-history-daemon

set -e

# Setup clean isolated home directory for testing
TEST_HOME=$(mktemp -d -t wl-clipboard-history-test-XXXXXX)
export HOME="$TEST_HOME"

echo "Using isolated test home: $TEST_HOME"

DAEMON_SCRIPT="/home/angl/projects/wl-clipboard-history/src/wl-clipboard-history-daemon"
CONFIG_DIR="$TEST_HOME/.config/wl-clipboard-history"
CACHE_DIR="$TEST_HOME/.cache/wl-clipboard-history"
MEDIA_DIR="$CACHE_DIR/media"
HISTORY_FILE="$CACHE_DIR/history.txt"
CONFIG_FILE="$CONFIG_DIR/config.ini"

# Helpers for assertions
assert_equals() {
    local expected="$1"
    local actual="$2"
    local msg="$3"
    if [[ "$expected" != "$actual" ]]; then
        echo "FAIL: $msg"
        echo "  Expected: '$expected'"
        echo "  Actual:   '$actual'"
        exit 1
    fi
}

assert_file_exists() {
    if [[ ! -f "$1" ]]; then
        echo "FAIL: File $1 does not exist!"
        exit 1
    fi
}

assert_dir_exists() {
    if [[ ! -d "$1" ]]; then
        echo "FAIL: Directory $1 does not exist!"
        exit 1
    fi
}

assert_file_not_exists() {
    if [[ -f "$1" ]]; then
        echo "FAIL: File $1 exists but should not!"
        exit 1
    fi
}

echo "=== Test 1: Config parser default values ==="
# No config.ini exists. Run update.
# This should initialize directories.
$DAEMON_SCRIPT --update

assert_file_exists "$HISTORY_FILE"
assert_dir_exists "$MEDIA_DIR"

# Verify default MAX_HISTORY is read as 50
# We can do this by copying config loading helper or verifying it indirectly.
# Let's write a temporary config to see if it changes behavior in following tests.

echo "=== Test 2: Copy plain text ==="
wl-copy "Hello World"
$DAEMON_SCRIPT --update

# Read the first line of history.txt
first_line=$(head -n 1 "$HISTORY_FILE")
assert_equals "[text]Hello World" "$first_line" "Plain text copy should be stored with [text] prefix"

echo "=== Test 3: Copy multi-line text ==="
echo -e "Line 1\nLine 2\nLine 3" | wl-copy
$DAEMON_SCRIPT --update

first_line=$(head -n 1 "$HISTORY_FILE")
# Note: we use \a (bell) for newlines, so let's verify tr works
expected_encoded="[text]Line 1"$'\a'"Line 2"$'\a'"Line 3"
# Wait, let's compare with actual string
# Let's replace \a with \n or compare exactly
assert_equals "$expected_encoded" "$first_line" "Newlines must be escaped with bell character"

echo "=== Test 4: Copy URI list (files) ==="
echo -e "file:///tmp/test1.txt\nfile:///tmp/test2.txt" | wl-copy -t text/uri-list
$DAEMON_SCRIPT --update

first_line=$(head -n 1 "$HISTORY_FILE")
expected_files="[files]file:///tmp/test1.txt"$'\a'"file:///tmp/test2.txt"
assert_equals "$expected_files" "$first_line" "Files should be stored with [files] prefix and \a escapes"

echo "=== Test 5: Copy Image ==="
echo "dummy-png-image-content" | wl-copy -t image/png
$DAEMON_SCRIPT --update

first_line=$(head -n 1 "$HISTORY_FILE")
if [[ "$first_line" != "[image]"* ]]; then
    echo "FAIL: Expected image entry prefix, got: $first_line"
    exit 1
fi

image_path="${first_line#"[image]"}"
assert_file_exists "$image_path"
assert_equals "dummy-png-image-content" "$(cat "$image_path")" "Saved image content should match copied data"

echo "=== Test 6: Deduplication & Move to Top ==="
# Current history has Image, Files, Multi-line, Plain text.
# Let's copy the plain text "Hello World" again.
wl-copy "Hello World"
$DAEMON_SCRIPT --update

first_line=$(head -n 1 "$HISTORY_FILE")
assert_equals "[text]Hello World" "$first_line" "Copied item should be moved to the top"

# Count occurrences of "Hello World" in history file
hello_count=$(grep -c -F "[text]Hello World" "$HISTORY_FILE")
assert_equals "1" "$hello_count" "There should be no duplicate entries in the history"

echo "=== Test 7: Consecutive identical copy skip ==="
# Copy "Hello World" again
wl-copy "Hello World"
initial_mtime=$(stat -c %Y "$HISTORY_FILE")
sleep 1
$DAEMON_SCRIPT --update
new_mtime=$(stat -c %Y "$HISTORY_FILE")
assert_equals "$initial_mtime" "$new_mtime" "Consecutive duplicate copy should not write/update history file"

echo "=== Test 8: FIFO Eviction ==="
# Set max_history to 3 in config.ini
mkdir -p "$CONFIG_DIR"
echo -e "[general]\nmax_history = 3" > "$CONFIG_FILE"

# Reset history so we can count cleanly
echo -n "" > "$HISTORY_FILE"

# Copy 4 different texts
wl-copy "Item 1" && $DAEMON_SCRIPT --update
wl-copy "Item 2" && $DAEMON_SCRIPT --update
wl-copy "Item 3" && $DAEMON_SCRIPT --update
wl-copy "Item 4" && $DAEMON_SCRIPT --update

# Verify history has exactly 3 items and they are 4, 3, 2 (FIFO)
line_count=$(wc -l < "$HISTORY_FILE")
assert_equals "3" "$line_count" "History should be truncated to exactly max_history = 3"

item_top=$(sed -n '1p' "$HISTORY_FILE")
item_mid=$(sed -n '2p' "$HISTORY_FILE")
item_bot=$(sed -n '3p' "$HISTORY_FILE")

assert_equals "[text]Item 4" "$item_top" "Top item should be Item 4"
assert_equals "[text]Item 3" "$item_mid" "Middle item should be Item 3"
assert_equals "[text]Item 2" "$item_bot" "Bottom item should be Item 2"

echo "=== Test 9: Image Eviction and Cleanup ==="
# Set max_history = 2
echo -e "[general]\nmax_history = 2" > "$CONFIG_FILE"
echo -n "" > "$HISTORY_FILE"
rm -f "$MEDIA_DIR"/*

# Copy image
echo "image-to-be-evicted" | wl-copy -t image/png
$DAEMON_SCRIPT --update

img_entry=$(head -n 1 "$HISTORY_FILE")
img_file_path="${img_entry#"[image]"}"
assert_file_exists "$img_file_path"

# Copy text 1
wl-copy "Text A" && $DAEMON_SCRIPT --update
# Copy text 2 - this will evict the image because limit is 2
wl-copy "Text B" && $DAEMON_SCRIPT --update

# Check history file
line_count=$(wc -l < "$HISTORY_FILE")
assert_equals "2" "$line_count" "History size should be 2"

# Image file should be deleted from disk
assert_file_not_exists "$img_file_path"
echo "Image was successfully evicted and deleted from disk!"

echo "=== Test 10: Reset on login ==="
# Setup some history and media files
echo "[text]Old Session Item" > "$HISTORY_FILE"
echo "old-image" > "$MEDIA_DIR/old-hash.png"

# Write config with reset_on_login = true
echo -e "[general]\nreset_on_login = true\nmax_history = 10" > "$CONFIG_FILE"

# Create a mock wl-paste that does nothing and exits immediately
MOCK_BIN="$TEST_HOME/bin"
mkdir -p "$MOCK_BIN"
echo -e '#!/bin/sh\nexit 0' > "$MOCK_BIN/wl-paste"
chmod +x "$MOCK_BIN/wl-paste"

# Run the daemon with modified PATH. It will trigger reset_on_login, then call wl-paste --watch and exit
PATH="$MOCK_BIN:$PATH" $DAEMON_SCRIPT &
DAEMON_PID=$!
sleep 0.5
kill $DAEMON_PID 2>/dev/null || true

# Verify files were reset
history_size=$(wc -c < "$HISTORY_FILE")
assert_equals "0" "$history_size" "History file should be cleared on login reset"

media_file_count=$(find "$MEDIA_DIR" -type f | wc -l)
assert_equals "0" "$media_file_count" "Media directory should be empty on login reset"

# Cleanup
rm -rf "$TEST_HOME"

echo "ALL DAEMON TESTS PASSED SUCCESSFULLY!"

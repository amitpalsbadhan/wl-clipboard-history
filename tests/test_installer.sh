#!/usr/bin/env bash

# Integration test for install.sh of wl-clipboard-history

set -e

# Setup clean isolated home directory for testing
TEST_HOME=$(mktemp -d -t wl-clipboard-history-install-test-XXXXXX)
export HOME="$TEST_HOME"

echo "Using isolated test home: $TEST_HOME"

INSTALL_SCRIPT="/home/angl/projects/wl-clipboard-history/install.sh"
CONFIG_DIR="$TEST_HOME/.config/wl-clipboard-history"
LOCAL_BIN="$TEST_HOME/.local/bin"

# Helpers for assertions
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

assert_executable() {
    if [[ ! -x "$1" ]]; then
        echo "FAIL: File $1 is not executable!"
        exit 1
    fi
}

assert_content_matches() {
    local file="$1"
    local pattern="$2"
    if ! grep -q "$pattern" "$file"; then
        echo "FAIL: File $file does not contain pattern '$pattern'!"
        exit 1
    fi
}

# Run first installation
echo "Running install.sh for the first time..."
"$INSTALL_SCRIPT"

# Verify directories and files
assert_dir_exists "$CONFIG_DIR"
assert_dir_exists "$LOCAL_BIN"

assert_file_exists "$CONFIG_DIR/config.ini"
assert_file_exists "$CONFIG_DIR/style.css"

assert_file_exists "$LOCAL_BIN/wl-clipboard-history-gui"
assert_file_exists "$LOCAL_BIN/wl-clipboard-history-daemon"

assert_executable "$LOCAL_BIN/wl-clipboard-history-gui"
assert_executable "$LOCAL_BIN/wl-clipboard-history-daemon"

# Test non-overwriting behavior
echo "Testing non-overwriting config files behavior..."
echo "custom_value = true" >> "$CONFIG_DIR/config.ini"
echo "custom_css { color: red; }" >> "$CONFIG_DIR/style.css"

# Re-run installer
echo "Running install.sh for the second time..."
"$INSTALL_SCRIPT"

# Verify custom lines are still intact (not overwritten)
assert_content_matches "$CONFIG_DIR/config.ini" "custom_value = true"
assert_content_matches "$CONFIG_DIR/style.css" "custom_css { color: red; }"

# Clean up
rm -rf "$TEST_HOME"
echo "All installation tests passed successfully!"

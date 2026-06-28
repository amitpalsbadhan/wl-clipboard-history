#!/usr/bin/env bash

set -e

echo "=================================================="
echo " Installing wl-clipboard-history"
echo "=================================================="

# Check for essential dependencies
dependencies=(wl-copy wl-paste wtype make gcc pkg-config)
missing_deps=()

for dep in "${dependencies[@]}"; do
    if ! command -v "$dep" &> /dev/null; then
        missing_deps+=("$dep")
    fi
done

# Check for GTK4 dependency
if command -v pkg-config &> /dev/null; then
    if ! pkg-config --exists gtk4; then
        missing_deps+=("gtk4 (development library)")
    fi
else
    missing_deps+=("pkg-config")
fi

if [ ${#missing_deps[@]} -ne 0 ]; then
    echo "⚠️  Warning: The following dependency/dependencies are missing:"
    for dep in "${missing_deps[@]}"; do
        echo "   - $dep"
    done
    echo "Please ensure they are installed for wl-clipboard-history to function correctly."
    echo ""
fi

# Build GUI binary
echo "Building wl-clipboard-history-gui..."
make clean
make

# Establish default config dirs
CONFIG_DIR="$HOME/.config/wl-clipboard-history"
mkdir -p "$CONFIG_DIR"

# Copy configurations if they do not exist
if [[ ! -f "$CONFIG_DIR/config.ini" ]]; then
    cp config/config.ini "$CONFIG_DIR/config.ini"
    echo "Installed default config.ini to $CONFIG_DIR/config.ini"
else
    echo "Existing config.ini found at $CONFIG_DIR/config.ini, skipping."
fi

if [[ ! -f "$CONFIG_DIR/style.css" ]]; then
    cp config/style.css "$CONFIG_DIR/style.css"
    echo "Installed default style.css to $CONFIG_DIR/style.css"
else
    echo "Existing style.css found at $CONFIG_DIR/style.css, skipping."
fi

# Install executables to local user bin
LOCAL_BIN="$HOME/.local/bin"
mkdir -p "$LOCAL_BIN"

cp wl-clipboard-history-gui "$LOCAL_BIN/wl-clipboard-history-gui"
cp src/wl-clipboard-history-daemon "$LOCAL_BIN/wl-clipboard-history-daemon"
chmod +x "$LOCAL_BIN/wl-clipboard-history-gui"
chmod +x "$LOCAL_BIN/wl-clipboard-history-daemon"

echo "Binaries copied successfully to $LOCAL_BIN"

# Check if LOCAL_BIN is in PATH
if [[ ":$PATH:" != *":$LOCAL_BIN:"* ]]; then
    echo ""
    echo "⚠️  Note: $LOCAL_BIN is not in your PATH."
    echo "   You may want to add it to your shell profile (e.g. .bashrc, .zshrc):"
    echo "   export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

echo ""
echo "=================================================="
echo " Setup Instructions for Hyprland:"
echo "=================================================="
echo "1. Add this to your ~/.config/hypr/hyprland.conf to start the daemon:"
echo "   exec-once = $LOCAL_BIN/wl-clipboard-history-daemon"
echo ""
echo "2. Add a keybinding to open the history popup:"
echo "   bind = SUPER, V, exec, $LOCAL_BIN/wl-clipboard-history-gui"
echo ""
echo "3. Add window rules to style the popup beautifully:"
echo "   windowrulev2 = float, class:^(org.hyprland.wl-clipboard-history)$"
echo "   windowrulev2 = size 450 500, class:^(org.hyprland.wl-clipboard-history)$"
echo "   windowrulev2 = center, class:^(org.hyprland.wl-clipboard-history)$"
echo "   windowrulev2 = pin, class:^(org.hyprland.wl-clipboard-history)$"
echo "   windowrulev2 = stayfocused, class:^(org.hyprland.wl-clipboard-history)$"
echo "=================================================="

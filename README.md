# wl-clipboard-history

A beautiful, high-performance, and fully-riceable clipboard history manager designed specifically for Hyprland under Wayland. Written in pure **C** with **GTK4** for the GUI popup, paired with a lightweight **Bash** daemon to track and manage history on disk.

---

## ✨ Features

- 🏎️ **Extreme Performance**: Low-overhead C executable with GTK4 GPU acceleration, sub-megabyte memory usage, and instant startup times.
- 🖼️ **Rich Content Previews**: Supports plain text (escaped line breaks), scaled image thumbnails (cached on disk under unique hashes), and file copy listings.
- ⚙️ **Deduplication & FIFO Eviction**: Keeps history tidy. If you re-copy an item, it moves back to the top. When the configured limit (`max_history`) is exceeded, the oldest items are automatically deleted, with cached image files cleaned from disk.
- 📋 **Double Clipboard Synchronization**: Automatically writes selected items to both the standard Clipboard and the Primary Selection (`--primary`), ensuring perfect compatibility across standard apps and XWayland clients.
- 💻 **Active Window Terminal Awareness**: Queries `hyprctl activewindow` on item selection to automatically identify if the target is a terminal.
- ⌨️ **Safe Auto-Paste**: Simulates typing via `wtype` using context-specific key sequences:
  - **Terminal Targets**: Dispatches `Ctrl+Shift+V` to prevent process termination signals (`Ctrl+C` emulation errors).
  - **Standard Apps**: Dispatches standard `Ctrl+V`.
- 🎨 **Fully Riceable (GTK4 CSS)**: Easily theme-able and customizable with CSS styles. Pre-processes stylesheet at runtime to dynamically interpolate user configurations.

---

## 🛠️ Installation

### Option 1: Via AUR (Using `yay`)
If you are on Arch Linux, you can clone and install the package natively using your AUR helper:
```bash
git clone https://aur.archlinux.org/wl-clipboard-history.git
cd wl-clipboard-history
makepkg -si
# Or directly via your helper:
yay -S wl-clipboard-history
```

### Option 2: Manual Installation (From Source)
You can clone the main repository and build it directly using the automated installer script:
```bash
git clone https://github.com/amitpalsbadhan/wl-clipboard-history.git
cd wl-clipboard-history
./install.sh
```

---

## ⚙️ Hyprland Setup (Lua 2026 Config)

Add the following snippets to your Hyprland Lua configuration file (`~/.config/hypr/hyprland.lua`):

```lua
-- 1. Bind SUPER+V (Win+V) to launch the popup GUI
hl.bind(mainMod .. " + V", hl.dsp.exec_cmd("wl-clipboard-history-gui"))

-- 2. Start/reload tracker daemon on config load (auto-kills previous processes)
hl.exec_cmd("pkill -f wl-clipboard-history-daemon; wl-clipboard-history-daemon &")

-- 3. Add window rules for beautiful floating, cursor-centered ricing (matched robustly on Title)
hl.exec_cmd("hyprctl keyword windowrulev2 float,title:^(Clipboard History)$")
hl.exec_cmd("hyprctl keyword windowrulev2 size 450 500,title:^(Clipboard History)$")
hl.exec_cmd("hyprctl keyword windowrulev2 move cursor -50% -50%,title:^(Clipboard History)$")
hl.exec_cmd("hyprctl keyword windowrulev2 pin,title:^(Clipboard History)$")
hl.exec_cmd("hyprctl keyword windowrulev2 stayfocused,title:^(Clipboard History)$")
```

---

## 🎨 Configuration & Styling

Config files live in `~/.config/wl-clipboard-history/` and are auto-generated on first launch:

### `config.ini` (General Options)
```ini
[general]
max_history = 50       # Max number of items to keep
reset_on_login = false # Wipe history on startup/login
paste_on_select = true # Automatically paste on select

[ui]
window_width = 450
window_height = 500
font_face = "JetBrains Mono"
font_size = 11
```

### `style.css` (GTK4 CSS Styling)
You can rice the popup popup using standard GTK CSS rules:
```css
#main-window {
    background-color: rgba(30, 30, 46, 0.95);
    border: 2px solid #cba6f7;
    border-radius: 12px;
}
#search-entry {
    background-color: #313244;
    color: #cdd6f4;
    border-radius: 8px;
    padding: 8px;
    margin: 10px;
    font-family: @font_face;
    font-size: @font_sizept;
}
#history-list {
    background-color: transparent;
}
.history-row {
    padding: 12px;
    border-bottom: 1px solid #313244;
    color: #cdd6f4;
    font-family: @font_face;
    font-size: @font_sizept;
}
.history-row:hover {
    background-color: #45475a;
}
.history-row:selected {
    background-color: #cba6f7;
    color: #11111b;
}
```

---

## 🧪 Running Tests
To compile and execute the complete unit and integration test suites:
```bash
make test
```
- `test_config`: Verifies config ini parser behaviors.
- `test_gui_logic`: Simulates active window terminal classes check.
- `tests/test_daemon.sh`: Tests copying formats, caching, and FIFO eviction.

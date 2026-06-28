# Design Specification: wl-clipboard-history

A beautiful, high-performance, fully-riceable clipboard history manager for Hyprland using C, GTK4, CSS, and a lightweight bash tracker daemon.

## 1. Overview & Requirements
- **Goal**: Create a clipboard manager popup for Hyprland that handles text, images, and files, featuring a configurable length, high-performance C + GTK4 frontend, and full CSS-based customizability ("ricing").
- **Binary/Command Name**: `wl-clipboard-history`
- **Frontend**: Custom C application using GTK4, compiled to a single native executable.
- **Backend/Tracker**: Light, robust bash daemon using `wl-paste --watch` that runs once per session via Hyprland's `exec-once`.
- **Configurability**: Key-value/INI config file (`config.ini`) and CSS stylesheet (`style.css`).
- **Support**: 
  - Standard UTF-8 text.
  - Image copying (binary stored as PNG in a cache folder).
  - File copying (supporting `text/uri-list` protocol).
  - Configurable persistent history or reset-on-login (session-only).
  - Eviction strategy: FIFO (First In First Out) where the oldest items are discarded when history length exceeds the config limit.

---

## 2. Directory Structure & Paths
All files reside in user home directories:
- **Config Directory**: `~/.config/wl-clipboard-history/`
  - `config.ini` — Options for UI, limit, persistence.
  - `style.css` — Stylesheet for GTK4 styling.
- **Data/Cache Directory**: `~/.cache/wl-clipboard-history/`
  - `history.txt` — Log of history entries. Format: `[type]content` or raw text.
  - `media/` — Folder for cached images (e.g., `media/d3b07384d113edec.png`).

---

## 3. Configuration Schema (`config.ini`)
```ini
[general]
# Max number of clipboard items to keep in history.
max_history = 50

# Whether to wipe history when the daemon starts (login/reboot).
reset_on_login = false

# Enable automatic typing/pasting of the item on selection.
paste_on_select = true

[ui]
window_width = 450
window_height = 500
font_face = "JetBrains Mono"
font_size = 11
```

---

## 4. CSS Styling Classes (`style.css`)
Users can style the popup precisely using CSS rules targeting:
- `#main-window` (custom margins, borders, transparent colors for Hyprland blur)
- `#search-entry` (search bar style, padding, focus glow)
- `#history-list` (container scroll properties)
- `.history-row` (padding, background, border)
- `.history-row:hover`, `.history-row:selected` (different background, font weight, animations)
- `.item-text`, `.item-image`, `.item-icon` (styling individual components)

---

## 5. Technical Architecture & Components

### 5.1. Backend: The Tracker Daemon (`wl-clipboard-history-daemon`)
A simple Bash script utilizing `wl-paste --watch`:
1. On startup:
   - Read `reset_on_login` from `config.ini`. If true, delete `~/.cache/wl-clipboard-history/history.txt` and empty `media/` folder.
2. Run `wl-paste --watch` in background.
3. For each clipboard change:
   - Identify the mime-type of clipboard data.
   - **Case 1: Image (`image/png`, `image/jpeg`)**
     - Hash the image content to create a unique filename (e.g., `~/.cache/wl-clipboard-history/media/<hash>.png`).
     - Save the binary data using `wl-paste -t image/png > ~/.cache/wl-clipboard-history/media/<hash>.png`.
     - Prepend `[image]/absolute/path/to/image.png` to the history list.
   - **Case 2: Files (`text/uri-list`)**
     - Capture URIs.
     - Prepend `[files]file:///path/to/file1.txt\nfile:///path/to/file2.txt` (newline escaped) to the history list.
   - **Case 3: Text (`text/plain` or similar)**
     - Capture raw text.
     - Escape newlines so each entry is exactly one line.
     - Prepend `[text]raw_text` to the history list.
4. **De-duplication**: If the exact same item already exists in the history list, remove its old instance and place the new one at the top.
5. **FIFO Eviction**: Check line count. If it exceeds `max_history`, discard the oldest line at the bottom. If the discarded line was an image, delete its corresponding cached image file.

### 5.2. Frontend: The GTK4 C GUI (`wl-clipboard-history-gui`)
A single-window GTK4 app optimized for speed and keyboard navigation:
1. On start:
   - Load `config.ini` for dimensions, font, and preferences. If files are missing, use sensible defaults.
   - Load `style.css` into GTK CSS provider.
   - Set Window class to `wl-clipboard-history` (for Hyprland window rule mapping).
   - Read lines from `~/.cache/wl-clipboard-history/history.txt`.
2. Build UI layout:
   - A `GtkBox` container holding:
     - Search Entry (`GtkSearchEntry`): Fuzzy filters list rows based on search text.
     - Scrolled Window (`GtkScrolledWindow`) containing a `GtkListBox`.
3. For each line in history file:
   - Parse `[type]` prefix.
   - Create a `GtkBox` row:
     - Icon representing the item type (text, image, or files).
     - **If Text**: A single-line `GtkLabel` showing text preview.
     - **If Image**: A tiny `GtkPicture` displaying a scaled thumbnail of the cached image.
     - **If Files**: A text preview with file count/paths.
   - Append row to `GtkListBox`.
4. Keyboard Shortcuts:
   - `Escape`: Close window.
   - `Up`/`Down`: Navigate list box.
   - `Enter`: Select item.
5. Interaction & Pasting:
   - When a row is selected/clicked:
     - Write the item back to Wayland clipboard using `wl-copy`.
       - For text: `wl-copy "text"`
       - For image: `wl-copy -t image/png < /path/to/img`
       - For files: `wl-copy -t text/uri-list < URIs`
     - If `paste_on_select` is enabled:
       - Run a short background command to dispatch a keyboard paste action to the active window using `wtype` or `ydotool` (or wait briefly, hide window, and run key press simulation).
     - Exit application immediately.

---

## 6. Testing Strategy
1. **Daemon Tests**: Manual copy of text, file, and images. Verify they write to `history.txt` and `media/` with expected formats.
2. **Eviction Tests**: Copy more items than `max_history` and verify older ones are discarded and cached images are removed.
3. **Reset tests**: Set `reset_on_login = true` and check if history clears on daemon restart.
4. **GUI Rendering Tests**: Run GUI, check for visual alignment of text, file listings, and thumbnail image scaling.
5. **Clipboard Writing Tests**: Select item, check if system clipboard contents now match the selected item.

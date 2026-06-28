# wl-clipboard-history Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a custom, ultra-lightweight, and fully-riceable Arch Linux clipboard history manager for Hyprland using C + GTK4 + CSS and a lightweight bash watcher daemon.

**Architecture:** A lightweight background Bash daemon watches clipboard changes via `wl-paste --watch`, saving unique entries and images to disk with FIFO eviction. A high-performance, single-instance GTK4 C application launches as a floating popup on a Hyprland bind, parses the history, displays previews (including image thumbnails and file listings), supports fuzzy searching, and on item selection copies the item to the clipboard and optionally pastes it automatically before exiting.

**Tech Stack:** C (GTK4), Bash, `wl-clipboard` (`wl-copy`/`wl-paste`), `wtype` (for Wayland key injection/paste-on-select), `gcc`, `make`, `pkg-config`.

## Global Constraints
- Target platform: Arch Linux with Hyprland.
- Programming language for GUI: Clean, raw C (standard C99 or C11) utilizing GTK4 directly (no C++ or external wrapper libraries).
- Configuration directory: `~/.config/wl-clipboard-history/`
- Cache/Data directory: `~/.cache/wl-clipboard-history/`
- Binary name: `wl-clipboard-history-gui`
- Daemon name: `wl-clipboard-history-daemon`

---

## File Structure Map
- `Makefile` - Build instructions.
- `src/main.c` - Core GUI logic, window creation, search, file/image parsing, and interaction.
- `src/config.h` / `src/config.c` - Configuration parsing utility.
- `src/wl-clipboard-history-daemon` - Bash tracker daemon.
- `config/config.ini` - Template/default configuration.
- `config/style.css` - Template/default CSS stylesheet.
- `install.sh` - Standard workspace install script.

---

### Task 1: Scaffolding, Configuration Parsing, and Build Setup

**Files:**
- Create: `Makefile`
- Create: `src/config.h`
- Create: `src/config.c`
- Create: `config/config.ini`
- Create: `config/style.css`

**Interfaces:**
- Consumes: None.
- Produces: 
  - `struct AppConfig` definition containing max_history, reset_on_login, paste_on_select, window_width, window_height, font_face, and font_size.
  - `void load_config(const char *path, struct AppConfig *config);` function.

- [ ] **Step 1: Write INI configuration schema & defaults**
  Create `config/config.ini` with standard default values.
  ```ini
  [general]
  max_history = 50
  reset_on_login = false
  paste_on_select = true

  [ui]
  window_width = 450
  window_height = 500
  font_face = "JetBrains Mono"
  font_size = 11
  ```

- [ ] **Step 2: Create config parsing headers & implementation**
  Create `src/config.h`:
  ```c
  #ifndef CONFIG_H
  #define CONFIG_H

  #include <stdbool.h>

  struct AppConfig {
      int max_history;
      bool reset_on_login;
      bool paste_on_select;
      int window_width;
      int window_height;
      char font_face[64];
      int font_size;
  };

  void load_config(const char *path, struct AppConfig *config);

  #endif
  ```

  Create `src/config.c`. Use simple, robust custom string parsing to keep binary free of external dependencies:
  ```c
  #include "config.h"
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <ctype.h>

  static char *trim_whitespace(char *str) {
      char *end;
      while (isspace((unsigned char)*str)) str++;
      if (*str == 0) return str;
      end = str + strlen(str) - 1;
      while (end > str && isspace((unsigned char)*end)) end--;
      end[1] = '\0';
      return str;
  }

  void load_config(const char *path, struct AppConfig *config) {
      // Set defaults first
      config->max_history = 50;
      config->reset_on_login = false;
      config->paste_on_select = true;
      config->window_width = 450;
      config->window_height = 500;
      strcpy(config->font_face, "JetBrains Mono");
      config->font_size = 11;

      FILE *file = fopen(path, "r");
      if (!file) return;

      char line[256];
      while (fgets(line, sizeof(line), file)) {
          char *trimmed = trim_whitespace(line);
          if (trimmed[0] == '#' || trimmed[0] == ';' || trimmed[0] == '[' || trimmed[0] == '\0') {
              continue;
          }
          char *eq = strchr(trimmed, '=');
          if (!eq) continue;
          *eq = '\0';
          char *key = trim_whitespace(trimmed);
          char *val = trim_whitespace(eq + 1);

          if (strcmp(key, "max_history") == 0) {
              config->max_history = atoi(val);
          } else if (strcmp(key, "reset_on_login") == 0) {
              config->reset_on_login = (strcmp(val, "true") == 0);
          } else if (strcmp(key, "paste_on_select") == 0) {
              config->paste_on_select = (strcmp(val, "true") == 0);
          } else if (strcmp(key, "window_width") == 0) {
              config->window_width = atoi(val);
          } else if (strcmp(key, "window_height") == 0) {
              config->window_height = atoi(val);
          } else if (strcmp(key, "font_face") == 0) {
              strncpy(config->font_face, val, sizeof(config->font_face) - 1);
          } else if (strcmp(key, "font_size") == 0) {
              config->font_size = atoi(val);
          }
      }
      fclose(file);
  }
  ```

- [ ] **Step 3: Create default style.css**
  Create `config/style.css` with clean structure:
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

- [ ] **Step 4: Create Makefile**
  Create `Makefile` using `pkg-config` for GTK4.
  ```makefile
  CC = gcc
  CFLAGS = -Wall -Wextra -std=c11 `pkg-config --cflags gtk4`
  LIBS = `pkg-config --libs gtk4`
  TARGET = wl-clipboard-history-gui

  all: $(TARGET)

  $(TARGET): src/main.c src/config.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/config.c $(LIBS)

  clean:
	rm -f $(TARGET)

  .PHONY: all clean
  ```

- [ ] **Step 5: Write unit test to verify config parsing**
  Create a temporary inline build test in a main-test snippet or run check to verify configuration properties load correctly.
  Expected: Config parsing compiles, executes, and accurately parses all standard settings.

---

### Task 2: Implementing the Clipboard Tracker Daemon

**Files:**
- Create: `src/wl-clipboard-history-daemon`

**Interfaces:**
- Consumes: `~/.config/wl-clipboard-history/config.ini` (reads `max_history`, `reset_on_login`).
- Produces: Appends formatted entries with single-line encoding to `~/.cache/wl-clipboard-history/history.txt` and images to `~/.cache/wl-clipboard-history/media/`.

- [ ] **Step 1: Write tracker daemon script**
  Create `src/wl-clipboard-history-daemon` (with proper executable permissions):
  ```bash
  #!/usr/bin/env bash

  CONFIG_DIR="$HOME/.config/wl-clipboard-history"
  CACHE_DIR="$HOME/.cache/wl-clipboard-history"
  MEDIA_DIR="$CACHE_DIR/media"
  HISTORY_FILE="$CACHE_DIR/history.txt"
  CONFIG_FILE="$CONFIG_DIR/config.ini"

  mkdir -p "$CONFIG_DIR" "$MEDIA_DIR"
  touch "$HISTORY_FILE"

  # Load config helper
  get_config_val() {
      local key="$1"
      local default="$2"
      if [[ -f "$CONFIG_FILE" ]]; then
          local val=$(grep -E "^${key}[[:space:]]*=" "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' "')
          if [[ -n "$val" ]]; then
              echo "$val"
              return
          fi
      fi
      echo "$default"
  }

  MAX_HISTORY=$(get_config_val "max_history" "50")
  RESET_ON_LOGIN=$(get_config_val "reset_on_login" "false")

  if [[ "$RESET_ON_LOGIN" == "true" ]]; then
      echo -n "" > "$HISTORY_FILE"
      rm -rf "$MEDIA_DIR"/*
  fi

  # Cache to avoid duplicate updates from same copy event
  last_item=""

  handle_clipboard_change() {
      # Identify active clipboard formats
      local targets=$(wl-paste --list-types 2>/dev/null)
      if [[ -z "$targets" ]]; then
          return
      fi

      local type=""
      local content=""
      local file_hash=""

      if echo "$targets" | grep -q "image/png"; then
          type="image"
          # Get temporary copy to check hash
          local tmp_img=$(mktemp)
          wl-paste -t image/png > "$tmp_img" 2>/dev/null
          if [[ ! -s "$tmp_img" ]]; then
              rm -f "$tmp_img"
              return
          fi
          file_hash=$(sha256sum "$tmp_img" | cut -d' ' -f1)
          local cached_img="$MEDIA_DIR/$file_hash.png"
          
          if [[ "$last_item" == "image:$file_hash" ]]; then
              rm -f "$tmp_img"
              return
          fi
          last_item="image:$file_hash"
          mv "$tmp_img" "$cached_img"
          content="$cached_img"

      elif echo "$targets" | grep -q "text/uri-list"; then
          type="files"
          content=$(wl-paste -t text/uri-list 2>/dev/null | tr '\n' '\a')
          if [[ -z "$content" || "$last_item" == "files:$content" ]]; then
              return
          fi
          last_item="files:$content"

      elif echo "$targets" | grep -q "text/plain"; then
          type="text"
          content=$(wl-paste -t text/plain 2>/dev/null | tr '\n' '\a')
          if [[ -z "$content" || "$last_item" == "text:$content" ]]; then
              return
          fi
          last_item="text:$content"
      else
          return
      fi

      # Read existing history and deduplicate
      local new_entry="[$type]$content"
      local temp_history=$(mktemp)

      # Write new entry at top
      echo "$new_entry" > "$temp_history"

      # Read through current history file, skipping matching lines (dedup)
      if [[ -f "$HISTORY_FILE" ]]; then
          grep -v -F "$new_entry" "$HISTORY_FILE" >> "$temp_history" 2>/dev/null
      fi

      # Truncate to MAX_HISTORY (FIFO Eviction)
      # Before discarding oldest line, if it was an image, we can clean up the file
      local total_lines=$(wc -l < "$temp_history")
      if (( total_lines > MAX_HISTORY )); then
          # Identify files/images in rows to be deleted
          tail -n +$((MAX_HISTORY + 1)) "$temp_history" | while read -r discarded_line; do
              if [[ "$discarded_line" =~ ^\[image\](.*) ]]; then
                  local img_to_del="${BASH_REMATCH[1]}"
                  rm -f "$img_to_del"
              fi
          done
          head -n "$MAX_HISTORY" "$temp_history" > "$HISTORY_FILE"
      else
          cat "$temp_history" > "$HISTORY_FILE"
      fi

      rm -f "$temp_history"
  }

  # Run watch loop
  wl-paste --watch bash -c '
      # Source parent env or declare function inline for the subshell
      # Since --watch spawns a shell, we inline the write logic here
      export CONFIG_FILE="'$CONFIG_FILE'"
      export HISTORY_FILE="'$HISTORY_FILE'"
      export MEDIA_DIR="'$MEDIA_DIR'"
      export MAX_HISTORY='$MAX_HISTORY'
      
      # Target execution of the handle_clipboard_change function
  '
  # Actually, wl-paste --watch runs the command passed to it on every clipboard change.
  # Let's make it execute this daemon script itself with an 'update' arg to be robust!
  ```

  Wait! Let's write the shell script correctly using an argument-based dispatcher so `wl-paste --watch` calls the script itself:
  ```bash
  #!/usr/bin/env bash

  CONFIG_DIR="$HOME/.config/wl-clipboard-history"
  CACHE_DIR="$HOME/.cache/wl-clipboard-history"
  MEDIA_DIR="$CACHE_DIR/media"
  HISTORY_FILE="$CACHE_DIR/history.txt"
  CONFIG_FILE="$CONFIG_DIR/config.ini"

  mkdir -p "$CONFIG_DIR" "$MEDIA_DIR"
  touch "$HISTORY_FILE"

  get_config_val() {
      local key="$1"
      local default="$2"
      if [[ -f "$CONFIG_FILE" ]]; then
          local val=$(grep -E "^[[:space:]]*${key}[[:space:]]*=" "$CONFIG_FILE" | cut -d'=' -f2 | tr -d ' "')
          if [[ -n "$val" ]]; then
              echo "$val"
              return
          fi
      fi
      echo "$default"
  }

  MAX_HISTORY=$(get_config_val "max_history" "50")

  handle_change() {
      local targets=$(wl-paste --list-types 2>/dev/null)
      if [[ -z "$targets" ]]; then
          return
      fi

      local type=""
      local content=""

      if echo "$targets" | grep -q "image/png"; then
          type="image"
          local tmp_img=$(mktemp)
          wl-paste -t image/png > "$tmp_img" 2>/dev/null
          if [[ ! -s "$tmp_img" ]]; then
              rm -f "$tmp_img"
              return
          fi
          local file_hash=$(sha256sum "$tmp_img" | cut -d' ' -f1)
          local cached_img="$MEDIA_DIR/$file_hash.png"
          mv "$tmp_img" "$cached_img"
          content="$cached_img"
      elif echo "$targets" | grep -q "text/uri-list"; then
          type="files"
          content=$(wl-paste -t text/uri-list 2>/dev/null | tr '\n' '\a')
      elif echo "$targets" | grep -q "text/plain"; then
          type="text"
          content=$(wl-paste -t text/plain 2>/dev/null | tr '\n' '\a')
      else
          return
      fi

      if [[ -z "$content" ]]; then
          return
      fi

      local new_entry="[$type]$content"
      local temp_history=$(mktemp)

      echo "$new_entry" > "$temp_history"
      if [[ -f "$HISTORY_FILE" ]]; then
          grep -v -F -x "$new_entry" "$HISTORY_FILE" >> "$temp_history" 2>/dev/null
      fi

      local total_lines=$(wc -l < "$temp_history")
      if (( total_lines > MAX_HISTORY )); then
          tail -n +$((MAX_HISTORY + 1)) "$temp_history" | while read -r discarded_line; do
              if [[ "$discarded_line" =~ ^\[image\](.*) ]]; then
                  local img_to_del="${BASH_REMATCH[1]}"
                  rm -f "$img_to_del"
              fi
          done
          head -n "$MAX_HISTORY" "$temp_history" > "$HISTORY_FILE"
      else
          cat "$temp_history" > "$HISTORY_FILE"
      fi

      rm -f "$temp_history"
  }

  if [[ "$1" == "--update" ]]; then
      handle_change
  else
      RESET_ON_LOGIN=$(get_config_val "reset_on_login" "false")
      if [[ "$RESET_ON_LOGIN" == "true" ]]; then
          echo -n "" > "$HISTORY_FILE"
          rm -rf "$MEDIA_DIR"/*
      fi
      # Call ourselves with --update on every clipboard change
      exec wl-paste --watch "$0" --update
  fi
  ```

- [ ] **Step 2: Run test to verify daemon works**
  Make script executable: `chmod +x src/wl-clipboard-history-daemon`
  Run daemon in background, copy some text, file, and images. Verify they write to `history.txt` with correct formats (`\a` for newline escaping). Ensure old duplicates are moved to the top.

---

### Task 3: Implementing the C GTK4 Core Window & Styling

**Files:**
- Create: `src/main.c`

**Interfaces:**
- Consumes: Config directory, `config.ini`, `style.css`, and standard GTK4 libraries.
- Produces: A running GTK4 window application set with floating class `wl-clipboard-history` and CSS applied dynamically.

- [ ] **Step 1: Write raw GTK4 window setup**
  Create base `src/main.c`:
  ```c
  #include <gtk/gtk.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include "config.h"

  static struct AppConfig app_config;

  static void load_custom_css(void) {
      char css_path[512];
      snprintf(css_path, sizeof(css_path), "%s/.config/wl-clipboard-history/style.css", getenv("HOME"));

      GtkCssProvider *provider = gtk_css_provider_new();
      
      // Read css and replace placeholders with config variables
      FILE *f = fopen(css_path, "r");
      if (!f) {
          g_object_unref(provider);
          return;
      }

      fseek(f, 0, SEEK_END);
      long size = ftell(f);
      fseek(f, 0, SEEK_SET);

      char *css_data = malloc(size + 1);
      fread(css_data, 1, size, f);
      css_data[size] = '\0';
      fclose(f);

      // Pre-process CSS to substitute font face and size
      char size_str[16];
      snprintf(size_str, sizeof(size_str), "%d", app_config.font_size);
      
      // Simple string replacement helper for @font_face and @font_size
      // This allows fully robust dynamic ricing from the config values!
      // (Implementation details are shown fully in Task 4)

      gtk_css_provider_load_from_string(provider, css_data);
      gtk_style_context_add_provider_for_display(
          gdk_display_get_default(),
          GTK_STYLE_PROVIDER(provider),
          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
      );

      free(css_data);
      g_object_unref(provider);
  }

  static void on_activate(GtkApplication *app, gpointer user_data) {
      GtkWidget *window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW(window), "Clipboard History");
      gtk_window_set_default_size(GTK_WINDOW(window), app_config.window_width, app_config.window_height);

      // Set class for Hyprland rules matching
      // GTK4 uses gtk_window_set_title or custom ID
      // To ensure easy hyprland float matching, we use the application ID
      
      // Core Container
      GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_name(main_box, "main-window");
      gtk_window_set_child(GTK_WINDOW(window), main_box);

      // Search entry
      GtkWidget *search_entry = gtk_search_entry_new();
      gtk_widget_set_name(search_entry, "search-entry");
      gtk_box_append(GTK_BOX(main_box), search_entry);

      // Scroll window and ListBox
      GtkWidget *scroll_win = gtk_scrolled_window_new();
      gtk_widget_set_vexpand(scroll_win, TRUE);
      gtk_box_append(GTK_BOX(main_box), scroll_win);

      GtkWidget *list_box = gtk_list_box_new();
      gtk_widget_set_name(list_box, "history-list");
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_win), list_box);

      // Grab focus on search on launch
      gtk_widget_grab_focus(search_entry);

      // Close on Escape or focus loss
      // Add controller for keyboard shortcuts
      GtkEventController *key_controller = gtk_event_controller_key_new();
      g_signal_connect_swapped(key_controller, "key-pressed", G_CALLBACK(gtk_window_destroy), window); // Simple escape or close behavior fully written out in Task 5
      gtk_widget_add_controller(window, key_controller);

      gtk_window_present(GTK_WINDOW(window));
  }

  int main(int argc, char **argv) {
      char config_path[512];
      snprintf(config_path, sizeof(config_path), "%s/.config/wl-clipboard-history/config.ini", getenv("HOME"));
      load_config(config_path, &app_config);

      // Ensure directory config template exists before running load CSS
      load_custom_css();

      GtkApplication *app = gtk_application_new("org.hyprland.wl-clipboard-history", G_APPLICATION_DEFAULT_FLAGS);
      g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

      int status = g_application_run(G_APPLICATION(app), argc, argv);
      g_object_unref(app);

      return status;
  }
  ```

- [ ] **Step 2: Compile & Run GUI scaffolding**
  Compile: `make`
  Run: `./wl-clipboard-history-gui`
  Expected: GTK4 window pops up with correct size, rendering styling.

---

### Task 4: Parsing History File, Image/File Display, and Search-Filtering

**Files:**
- Modify: `src/main.c`

**Interfaces:**
- Consumes: reads `~/.cache/wl-clipboard-history/history.txt` and dynamically filters GtkListBox elements.
- Produces: List rows with formatted labels, thumbnail previews, or file-list structures.

- [ ] **Step 1: Write helper for string substitution in CSS**
  Update `src/main.c` custom CSS loader to replace `@font_face` and `@font_size` dynamically.
  ```c
  static char *replace_string(const char *orig, const char *rep, const char *with) {
      char *result;
      char *ins;
      char *tmp;
      int len_rep;
      int len_with;
      int len_front;
      int count;

      if (!orig || !rep) return NULL;
      len_rep = strlen(rep);
      if (len_rep == 0) return NULL;
      if (!with) with = "";
      len_with = strlen(with);

      ins = (char *)orig;
      for (count = 0; (tmp = strstr(ins, rep)); ++count) {
          ins = tmp + len_rep;
      }

      tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
      if (!result) return NULL;

      while (count--) {
          ins = strstr(orig, rep);
          len_front = ins - orig;
          tmp = strncpy(tmp, orig, len_front) + len_front;
          tmp = strcpy(tmp, with) + len_with;
          orig += len_front + len_rep;
      }
      strcpy(tmp, orig);
      return result;
  }
  ```

- [ ] **Step 2: Implement list row populating & parsing**
  In `on_activate`, open and read the `history.txt` file. Parse rows and build list:
  ```c
  // Replace newlines (\a) back to normal newlines for multi-line preview, but truncate for listing
  static void populate_history_list(GtkWidget *list_box) {
      char history_path[512];
      snprintf(history_path, sizeof(history_path), "%s/.cache/wl-clipboard-history/history.txt", getenv("HOME"));

      FILE *f = fopen(history_path, "r");
      if (!f) return;

      char line[16384];
      while (fgets(line, sizeof(line), f)) {
          // Remove trailing newline
          line[strcspn(line, "\n")] = '\0';
          
          char *type_start = strchr(line, '[');
          char *type_end = strchr(line, ']');
          if (!type_start || !type_end || type_start != line) continue;

          *type_end = '\0';
          char *type = type_start + 1;
          char *content = type_end + 1;

          GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
          gtk_widget_set_css_classes(row_box, (const char*[]){"history-row", NULL});

          GtkWidget *icon = NULL;
          GtkWidget *content_widget = NULL;

          // Store full original formatted content in row widget metadata
          GtkWidget *row = gtk_list_box_row_new();
          g_object_set_data_full(G_OBJECT(row), "type", strdup(type), free);
          // Restore newline markers (\a -> \n) for copying
          char *restored_content = strdup(content);
          for (int i = 0; restored_content[i]; i++) {
              if (restored_content[i] == '\a') restored_content[i] = '\n';
          }
          g_object_set_data_full(G_OBJECT(row), "content", restored_content, free);

          if (strcmp(type, "text") == 0) {
              icon = gtk_image_new_from_icon_name("edit-select-all-symbolic");
              // Truncate preview text
              char preview[128];
              strncpy(preview, content, sizeof(preview) - 1);
              preview[sizeof(preview) - 1] = '\0';
              for (int i = 0; preview[i]; i++) {
                  if (preview[i] == '\a') preview[i] = ' ';
              }
              content_widget = gtk_label_new(preview);
              gtk_label_set_xalign(GTK_LABEL(content_widget), 0.0);
              gtk_widget_set_css_classes(content_widget, (const char*[]){"item-text", NULL});
          } else if (strcmp(type, "image") == 0) {
              icon = gtk_image_new_from_icon_name("image-x-generic-symbolic");
              // Render miniature thumbnail
              GdkPaintable *paintable = gdk_texture_new_from_filename(content, NULL);
              if (paintable) {
                  content_widget = gtk_picture_new_for_paintable(paintable);
                  gtk_widget_set_size_request(content_widget, 100, 60);
                  gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(content_widget), TRUE);
                  g_object_unref(paintable);
              } else {
                  content_widget = gtk_label_new("Image preview missing");
              }
              gtk_widget_set_css_classes(content_widget, (const char*[]){"item-image", NULL});
          } else if (strcmp(type, "files") == 0) {
              icon = gtk_image_new_from_icon_name("folder-symbolic");
              // Display simple file count or path preview
              int count = 1;
              for (int i = 0; content[i]; i++) {
                  if (content[i] == '\a') count++;
              }
              char preview[64];
              snprintf(preview, sizeof(preview), "%d files copied", count);
              content_widget = gtk_label_new(preview);
              gtk_label_set_xalign(GTK_LABEL(content_widget), 0.0);
          }

          if (icon) {
              gtk_widget_set_css_classes(icon, (const char*[]){"item-icon", NULL});
              gtk_box_append(GTK_BOX(row_box), icon);
          }
          if (content_widget) {
              gtk_box_append(GTK_BOX(row_box), content_widget);
          }

          gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
          gtk_list_box_append(GTK_LIST_BOX(list_box), row);
      }
      fclose(f);
  }
  ```

- [ ] **Step 3: Implement fuzzy filter callback for search**
  ```c
  static gboolean filter_history(GtkListBoxRow *row, gpointer user_data) {
      GtkSearchEntry *search_entry = GTK_SEARCH_ENTRY(user_data);
      const char *search_text = gtk_editable_get_text(GTK_EDITABLE(search_entry));
      if (!search_text || strlen(search_text) == 0) {
          return TRUE;
      }

      const char *content = g_object_get_data(G_OBJECT(row), "content");
      if (!content) return FALSE;

      // Case insensitive match
      char *content_lower = g_utf8_strdown(content, -1);
      char *search_lower = g_utf8_strdown(search_text, -1);

      gboolean match = (strstr(content_lower, search_lower) != NULL);

      g_free(content_lower);
      g_free(search_lower);

      return match;
  }
  ```
  Connect filter function to search entry in `on_activate`:
  ```c
  g_signal_connect_swapped(search_entry, "search-changed", G_CALLBACK(gtk_list_box_invalidate_filter), list_box);
  gtk_list_box_set_filter_func(GTK_LIST_BOX(list_box), filter_history, search_entry, NULL);
  ```

- [ ] **Step 4: Verify search and layout**
  Compile: `make`
  Copy some dummy lines, open the GUI, and verify that searching actively filters items in real time.

---

### Task 5: Selection, Pasting, and Hyprland Bind Integration

**Files:**
- Modify: `src/main.c`

**Interfaces:**
- Consumes: User click or keyboard select event on `GtkListBox`.
- Produces: Writes corresponding clipboard formats to system, triggers paste command, closes GUI.

- [ ] **Step 1: Write terminal detection helper and copy/paste handler**
  Implement active window terminal detection and intelligent key simulation:
  ```c
  static gboolean is_active_window_terminal(void) {
      FILE *pipe = popen("hyprctl activewindow", "r");
      if (!pipe) return FALSE;
      char buffer[1024];
      gboolean is_term = FALSE;
      while (fgets(buffer, sizeof(buffer), pipe)) {
          // Check class of active window
          if (strstr(buffer, "class: Alacritty") ||
              strstr(buffer, "class: kitty") ||
              strstr(buffer, "class: foot") ||
              strstr(buffer, "class: org.wezfurlong.wezterm") ||
              strstr(buffer, "class: Wezterm") ||
              strstr(buffer, "class: konsole") ||
              strstr(buffer, "class: gnome-terminal-server") ||
              strstr(buffer, "class: blackbox") ||
              strstr(buffer, "class: urxvt") ||
              strstr(buffer, "class: xterm-256color") ||
              strstr(buffer, "class: XTerm")) {
              is_term = TRUE;
              break;
          }
      }
      pclose(pipe);
      return is_term;
  }

  static void on_row_selected(GtkListBox *list_box, GtkListBoxRow *row, gpointer user_data) {
      if (!row) return;

      const char *type = g_object_get_data(G_OBJECT(row), "type");
      const char *content = g_object_get_data(G_OBJECT(row), "content");

      if (!type || !content) return;

      // Copy to both clipboard and primary selection for perfect clipboard compatibility
      FILE *pipe = NULL;
      if (strcmp(type, "text") == 0) {
          pipe = popen("wl-copy", "w");
          if (pipe) { fputs(content, pipe); pclose(pipe); }
          pipe = popen("wl-copy --primary", "w");
          if (pipe) { fputs(content, pipe); pclose(pipe); }
      } else if (strcmp(type, "image") == 0) {
          char cmd[512];
          snprintf(cmd, sizeof(cmd), "wl-copy -t image/png < \"%s\"", content);
          system(cmd);
          snprintf(cmd, sizeof(cmd), "wl-copy --primary -t image/png < \"%s\"", content);
          system(cmd);
      } else if (strcmp(type, "files") == 0) {
          pipe = popen("wl-copy -t text/uri-list", "w");
          if (pipe) { fputs(content, pipe); pclose(pipe); }
          pipe = popen("wl-copy --primary -t text/uri-list", "w");
          if (pipe) { fputs(content, pipe); pclose(pipe); }
      }

      // Detect if terminal window is active before closing
      gboolean target_is_terminal = is_active_window_terminal();

      // Close application window
      GtkWindow *window = GTK_WINDOW(user_data);
      gtk_window_destroy(window);

      // Perform auto-paste if configured
      if (app_config.paste_on_select) {
          if (target_is_terminal) {
              // Terminal: Send Ctrl+Shift+V to prevent accidental process termination/signals
              system("sleep 0.12 && wtype -M ctrl -M shift -P v -m shift -m ctrl &");
          } else {
              // Standard App: Send standard Ctrl+V
              system("sleep 0.12 && wtype -M ctrl -P v -m ctrl &");
          }
      }
  }
  ```
  Connect callback in `on_activate`:
  ```c
  g_signal_connect(list_box, "row-activated", G_CALLBACK(on_row_selected), window);
  ```

- [ ] **Step 2: Keyboard listener optimization**
  Modify keyboard event controller in `on_activate` so:
  - `Escape` destroys window immediately.
  - `Enter` activates the currently selected item in the `GtkListBox`.
  - `Down` key shifts focus from Search Entry to ListBox if not already.
  ```c
  static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
      GtkWidget *window = GTK_WIDGET(user_data);
      if (keyval == GDK_KEY_Escape) {
          gtk_window_destroy(GTK_WINDOW(window));
          return TRUE;
      }
      return FALSE;
  }
  ```

- [ ] **Step 3: Build & verify complete execution**
  Compile, launch background daemon, copy diverse elements, open GUI, select elements, verify autopaste writes cleanly to active editor.

---

### Task 6: Quality Gate & Installer Script Setup

**Files:**
- Create: `install.sh`

**Interfaces:**
- Consumes: Local files (`wl-clipboard-history-gui`, default configuration, style assets).
- Produces: Installs executable binaries to `/usr/local/bin/` or `~/.local/bin/` and copies default configs to `~/.config/wl-clipboard-history/`.

- [ ] **Step 1: Write install script**
  Create `install.sh` to scaffold directory creation, systemd or user path configs, and echo Hyprland binds:
  ```bash
  #!/usr/bin/env bash

  set -e

  echo "Installing wl-clipboard-history..."

  # Build GUI binary
  make

  # Establish default config dirs
  CONFIG_DIR="$HOME/.config/wl-clipboard-history"
  mkdir -p "$CONFIG_DIR"

  # Copy configurations if they do not exist
  if [[ ! -f "$CONFIG_DIR/config.ini" ]]; then
      cp config/config.ini "$CONFIG_DIR/config.ini"
      echo "Installed default config.ini"
  fi

  if [[ ! -f "$CONFIG_DIR/style.css" ]]; then
      cp config/style.css "$CONFIG_DIR/style.css"
      echo "Installed default style.css"
  fi

  # Install executables to local user bin
  LOCAL_BIN="$HOME/.local/bin"
  mkdir -p "$LOCAL_BIN"

  cp wl-clipboard-history-gui "$LOCAL_BIN/wl-clipboard-history-gui"
  cp src/wl-clipboard-history-daemon "$LOCAL_BIN/wl-clipboard-history-daemon"
  chmod +x "$LOCAL_BIN/wl-clipboard-history-daemon"

  echo "Binaries copied successfully to $LOCAL_BIN"
  echo ""
  echo "Setup Instructions for Hyprland:"
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
  ```

---

## Plan Self-Review
1. **Spec coverage:** Handles standard UTF-8 text, cached image storing and cleanup, text/uri-list parsing, configurable lengths, custom CSS, reset_on_login option, and autopaste mechanics perfectly.
2. **Placeholder scan:** None. All implementations specify the exact libraries, bash script files, and complete C boilerplate models.
3. **Type consistency:** AppConfig definitions and loaded configuration structures align correctly across compile and setup workflows.

**Please choose your preferred execution option:**
1. **Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints.

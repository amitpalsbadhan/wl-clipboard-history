#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static struct AppConfig app_config;

static char *replace_string(const char *orig, const char *rep, const char *with) {
    if (!orig || !rep) return NULL;
    int len_rep = strlen(rep);
    if (len_rep == 0) return NULL;
    if (!with) with = "";
    int len_with = strlen(with);

    // Count occurrences
    int count = 0;
    const char *pos = orig;
    while ((pos = strstr(pos, rep)) != NULL) {
        count++;
        pos += len_rep;
    }

    char *result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result) return NULL;

    char *dst = result;
    pos = orig;
    while (count--) {
        const char *next = strstr(pos, rep);
        int len_front = next - pos;
        memcpy(dst, pos, len_front);
        dst += len_front;
        memcpy(dst, with, len_with);
        dst += len_with;
        pos = next + len_rep;
    }
    strcpy(dst, pos);
    return result;
}

static void load_custom_css(void) {
    char css_path[512];
    const char *home = getenv("HOME");
    if (!home) home = "";
    snprintf(css_path, sizeof(css_path), "%s/.config/wl-clipboard-history/style.css", home);

    GtkCssProvider *provider = gtk_css_provider_new();
    
    // Read css and replace placeholders with config variables
    FILE *f = fopen(css_path, "r");
    if (!f) {
        // Fallback to system-wide template if installed via pacman/AUR
        f = fopen("/usr/share/wl-clipboard-history/style.css", "r");
        if (!f) {
            // Fallback to local config/style.css if running in repo/development
            f = fopen("config/style.css", "r");
            if (!f) {
                g_object_unref(provider);
                return;
            }
        }
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) size = 0;
    fseek(f, 0, SEEK_SET);

    char *css_data = malloc(size + 1);
    if (!css_data) {
        fclose(f);
        g_object_unref(provider);
        return;
    }
    size_t read_bytes = fread(css_data, 1, size, f);
    css_data[read_bytes] = '\0';
    fclose(f);

    // Pre-process CSS to substitute font face and size
    char size_str[16];
    snprintf(size_str, sizeof(size_str), "%d", app_config.font_size);

    char *css_processed = replace_string(css_data, "@font_face", app_config.font_face);
    if (css_processed) {
        char *final_css = replace_string(css_processed, "@font_size", size_str);
        if (final_css) {
            gtk_css_provider_load_from_string(provider, final_css);
            free(final_css);
        } else {
            gtk_css_provider_load_from_string(provider, css_processed);
        }
        free(css_processed);
    } else {
        gtk_css_provider_load_from_string(provider, css_data);
    }

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    free(css_data);
    g_object_unref(provider);
}

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
    (void)list_box;
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
        int res1 = system(cmd);
        snprintf(cmd, sizeof(cmd), "wl-copy --primary -t image/png < \"%s\"", content);
        int res2 = system(cmd);
        (void)res1;
        (void)res2;
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
            int res = system("sleep 0.12 && wtype -M ctrl -M shift -P v -m shift -m ctrl &");
            (void)res;
        } else {
            // Standard App: Send standard Ctrl+V
            int res = system("sleep 0.12 && wtype -M ctrl -P v -m ctrl &");
            (void)res;
        }
    }
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    (void)state;
    GtkWidget *window = GTK_WIDGET(user_data);
    if (keyval == GDK_KEY_Escape) {
        gtk_window_destroy(GTK_WINDOW(window));
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        GtkWidget *list_box = g_object_get_data(G_OBJECT(window), "list-box");
        if (list_box) {
            GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
            if (!row) {
                // If no row is selected, let's find the first visible row
                GtkWidget *child = gtk_widget_get_first_child(list_box);
                while (child) {
                    if (GTK_IS_LIST_BOX_ROW(child)) {
                        GtkListBoxRow *r = GTK_LIST_BOX_ROW(child);
                        if (gtk_widget_get_child_visible(GTK_WIDGET(r)) && gtk_widget_get_visible(GTK_WIDGET(r))) {
                            row = r;
                            break;
                        }
                    }
                    child = gtk_widget_get_next_sibling(child);
                }
            }
            if (row) {
                g_signal_emit_by_name(list_box, "row-activated", row);
                return TRUE;
            }
        }
    }

    if (keyval == GDK_KEY_Down) {
        GtkWidget *search_entry = g_object_get_data(G_OBJECT(window), "search-entry");
        GtkWidget *list_box = g_object_get_data(G_OBJECT(window), "list-box");
        if (search_entry && list_box && gtk_widget_has_focus(search_entry)) {
            gtk_widget_grab_focus(list_box);
            // Also select the first visible row if nothing is selected
            GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
            if (!row) {
                GtkWidget *child = gtk_widget_get_first_child(list_box);
                while (child) {
                    if (GTK_IS_LIST_BOX_ROW(child)) {
                        GtkListBoxRow *r = GTK_LIST_BOX_ROW(child);
                        if (gtk_widget_get_child_visible(GTK_WIDGET(r)) && gtk_widget_get_visible(GTK_WIDGET(r))) {
                            gtk_list_box_select_row(GTK_LIST_BOX(list_box), r);
                            break;
                        }
                    }
                    child = gtk_widget_get_next_sibling(child);
                }
            }
            return TRUE;
        }
    }

    return FALSE;
}

static void populate_history_list(GtkWidget *list_box) {
    char history_path[512];
    const char *home = getenv("HOME");
    if (!home) home = "";
    snprintf(history_path, sizeof(history_path), "%s/.cache/wl-clipboard-history/history.txt", home);

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
        gtk_widget_add_css_class(row_box, "history-row");

        GtkWidget *icon = NULL;
        GtkWidget *content_widget = NULL;

        // Store full original formatted content in row widget metadata
        GtkWidget *row = gtk_list_box_row_new();
        g_object_set_data_full(G_OBJECT(row), "type", g_strdup(type), (GDestroyNotify)g_free);
        // Restore newline markers (\a -> \n) for copying
        char *restored_content = g_strdup(content);
        for (int i = 0; restored_content[i]; i++) {
            if (restored_content[i] == '\a') restored_content[i] = '\n';
        }
        g_object_set_data_full(G_OBJECT(row), "content", restored_content, (GDestroyNotify)g_free);

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
            gtk_widget_add_css_class(content_widget, "item-text");
        } else if (strcmp(type, "image") == 0) {
            icon = gtk_image_new_from_icon_name("image-x-generic-symbolic");
            // Render miniature thumbnail
            GdkTexture *texture = gdk_texture_new_from_filename(content, NULL);
            if (texture) {
                content_widget = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
                gtk_widget_set_size_request(content_widget, 100, 60);
                g_object_unref(texture);
            } else {
                content_widget = gtk_label_new("Image preview missing");
            }
            gtk_widget_add_css_class(content_widget, "item-image");
        } else if (strcmp(type, "files") == 0) {
            icon = gtk_image_new_from_icon_name("folder-symbolic");
            // Display simple file count or path preview
            int count = 0;
            if (content[0] != '\0') {
                count = 1;
                for (int i = 0; content[i]; i++) {
                    if (content[i] == '\a') count++;
                }
            }
            char preview[64];
            snprintf(preview, sizeof(preview), "%d files copied", count);
            content_widget = gtk_label_new(preview);
            gtk_label_set_xalign(GTK_LABEL(content_widget), 0.0);
        }

        if (icon) {
            gtk_widget_add_css_class(icon, "item-icon");
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

static gboolean filter_history(GtkListBoxRow *row, gpointer user_data) {
    GtkSearchEntry *search_entry = GTK_SEARCH_ENTRY(user_data);
    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(search_entry));
    if (!search_text || strlen(search_text) == 0) {
        return TRUE;
    }

    const char *content = g_object_get_data(G_OBJECT(row), "content");
    if (!content) return FALSE;

    // Case insensitive match using GLib utf8 strdown
    char *content_lower = g_utf8_strdown(content, -1);
    char *search_lower = g_utf8_strdown(search_text, -1);

    gboolean match = (strstr(content_lower, search_lower) != NULL);

    g_free(content_lower);
    g_free(search_lower);

    return match;
}

static void on_window_active_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    (void)user_data;
    GtkWindow *window = GTK_WINDOW(object);
    if (!gtk_window_is_active(window)) {
        gtk_window_destroy(window);
    }
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    // Load CSS after GdkDisplay is initialized during activation
    load_custom_css();

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Clipboard History");
    gtk_window_set_default_size(GTK_WINDOW(window), app_config.window_width, app_config.window_height);

    // Core Container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(main_box, "main-window");
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    // Header Box for Search bar and Close button
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(header_box, "header-bar");
    gtk_box_append(GTK_BOX(main_box), header_box);

    // Search entry (expanded to take all space)
    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_set_name(search_entry, "search-entry");
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_box_append(GTK_BOX(header_box), search_entry);

    // Beautiful circular Close button
    GtkWidget *close_button = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_name(close_button, "close-button");
    gtk_widget_set_valign(close_button, GTK_ALIGN_CENTER);
    g_signal_connect_swapped(close_button, "clicked", G_CALLBACK(gtk_window_destroy), window);
    gtk_box_append(GTK_BOX(header_box), close_button);

    // Scroll window and ListBox
    GtkWidget *scroll_win = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_win, TRUE);
    gtk_box_append(GTK_BOX(main_box), scroll_win);

    GtkWidget *list_box = gtk_list_box_new();
    gtk_widget_set_name(list_box, "history-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_win), list_box);

    // Populate history items
    populate_history_list(list_box);

    // Set up search filter function
    g_signal_connect_swapped(search_entry, "search-changed", G_CALLBACK(gtk_list_box_invalidate_filter), list_box);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(list_box), filter_history, search_entry, NULL);

    // Grab focus on search on launch
    gtk_widget_grab_focus(search_entry);

    // Store references on window for key pressed and row selection handling
    g_object_set_data(G_OBJECT(window), "list-box", list_box);
    g_object_set_data(G_OBJECT(window), "search-entry", search_entry);

    // Connect row-activated to copy & paste handler
    g_signal_connect(list_box, "row-activated", G_CALLBACK(on_row_selected), window);

    // Close on Escape
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), window);
    gtk_widget_add_controller(window, key_controller);

    // Close when window loses focus (clicked off screen)
    g_signal_connect(window, "notify::is-active", G_CALLBACK(on_window_active_changed), NULL);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("wl-clipboard-history-gui: Fast C/GTK4 Clipboard popup for Hyprland\n\n");
            printf("Usage: wl-clipboard-history-gui [options]\n\n");
            printf("Options:\n");
            printf("  -h, --help     Show this help message and exit\n\n");
            printf("Hyprland 2026 Lua Setup:\n");
            printf("  -- Bind SUPER+V (Win+V) to launch popup GUI\n");
            printf("  hl.bind(mainMod .. \" + V\", hl.dsp.exec_cmd(\"wl-clipboard-history-gui\"))\n\n");
            printf("  -- Clipboard history window rules (Floating/cursor-centered & auto-pkill reload)\n");
            printf("  hl.exec_cmd(\"pkill -f wl-clipboard-history-daemon; wl-clipboard-history-daemon &\")\n");
            printf("  hl.exec_cmd(\"hyprctl keyword windowrulev2 float,title:^(Clipboard History)$\")\n");
            printf("  hl.exec_cmd(\"hyprctl keyword windowrulev2 size 450 500,title:^(Clipboard History)$\")\n");
            printf("  hl.exec_cmd(\"hyprctl keyword windowrulev2 move cursor -50%% -50%%,title:^(Clipboard History)$\")\n");
            printf("  hl.exec_cmd(\"hyprctl keyword windowrulev2 pin,title:^(Clipboard History)$\")\n");
            printf("  hl.exec_cmd(\"hyprctl keyword windowrulev2 stayfocused,title:^(Clipboard History)$\")\n");
            return 0;
        }
    }

    char config_path[512];
    const char *home = getenv("HOME");
    if (!home) home = "";
    snprintf(config_path, sizeof(config_path), "%s/.config/wl-clipboard-history/config.ini", home);
    load_config(config_path, &app_config);

    GtkApplication *app = gtk_application_new("org.hyprland.wl-clipboard-history", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}

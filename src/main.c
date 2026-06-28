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
        // Fallback to local config/style.css if running in repo/development
        f = fopen("config/style.css", "r");
        if (!f) {
            g_object_unref(provider);
            return;
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

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    (void)state;
    GtkWidget *window = GTK_WIDGET(user_data);
    if (keyval == GDK_KEY_Escape) {
        gtk_window_destroy(GTK_WINDOW(window));
        return TRUE;
    }
    return FALSE;
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

    // Close on Escape
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), window);
    gtk_widget_add_controller(window, key_controller);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
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

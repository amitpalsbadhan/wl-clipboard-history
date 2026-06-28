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

static char *strip_quotes(char *str) {
    int len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        str[len - 1] = '\0';
        str++;
    } else if (len >= 2 && str[0] == '\'' && str[len - 1] == '\'') {
        str[len - 1] = '\0';
        str++;
    }
    return str;
}

void load_config(const char *path, struct AppConfig *config) {
    // Set defaults first
    config->max_history = 50;
    config->reset_on_login = false;
    config->paste_on_select = true;
    config->window_width = 450;
    config->window_height = 500;
    strncpy(config->font_face, "JetBrains Mono", sizeof(config->font_face) - 1);
    config->font_face[sizeof(config->font_face) - 1] = '\0';
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
        char *val = strip_quotes(trim_whitespace(eq + 1));

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
            config->font_face[sizeof(config->font_face) - 1] = '\0';
        } else if (strcmp(key, "font_size") == 0) {
            config->font_size = atoi(val);
        }
    }
    fclose(file);
}

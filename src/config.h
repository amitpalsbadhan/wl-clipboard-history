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

#define main gui_main
#include "../src/main.c"
#undef main

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test_terminal_detection(const char *mock_class, gboolean expected_result) {
    // Create a mock hyprctl executable
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "echo '#!/bin/sh' > /tmp/hyprctl");
    int res = system(cmd);
    (void)res;
    snprintf(cmd, sizeof(cmd), "echo 'echo \"class: %s\"' >> /tmp/hyprctl", mock_class);
    res = system(cmd);
    (void)res;
    res = system("chmod +x /tmp/hyprctl");
    (void)res;

    // Prepend /tmp to PATH
    char *old_path = getenv("PATH");
    char new_path[4096];
    snprintf(new_path, sizeof(new_path), "/tmp:%s", old_path ? old_path : "");
    setenv("PATH", new_path, 1);

    gboolean result = is_active_window_terminal();
    assert(result == expected_result);

    // Clean up mock
    remove("/tmp/hyprctl");
}

int main(void) {
    printf("Starting GUI logic tests...\n");

    // Test terminal classes
    test_terminal_detection("Alacritty", TRUE);
    test_terminal_detection("kitty", TRUE);
    test_terminal_detection("foot", TRUE);
    test_terminal_detection("org.wezfurlong.wezterm", TRUE);
    test_terminal_detection("konsole", TRUE);

    // Test non-terminal classes
    test_terminal_detection("firefox", FALSE);
    test_terminal_detection("Google-chrome", FALSE);
    test_terminal_detection("code-oss", FALSE);

    printf("All GUI logic tests passed successfully!\n");
    return 0;
}

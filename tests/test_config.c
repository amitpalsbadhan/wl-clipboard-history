#include "../src/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("Starting configuration parser tests...\n");

    // Test 1: Load non-existent file (should use default values)
    struct AppConfig config1;
    load_config("non_existent_file.ini", &config1);
    
    assert(config1.max_history == 50);
    assert(config1.reset_on_login == false);
    assert(config1.paste_on_select == true);
    assert(config1.window_width == 450);
    assert(config1.window_height == 500);
    assert(strcmp(config1.font_face, "JetBrains Mono") == 0);
    assert(config1.font_size == 11);
    printf("Test 1 (Default settings when missing) passed.\n");

    // Test 2: Load a custom config file
    // Create tests directory first
    system("mkdir -p tests");
    const char *test_ini_path = "tests/temp_test_config.ini";
    FILE *f = fopen(test_ini_path, "w");
    assert(f != NULL);

    fprintf(f, "[general]\n");
    fprintf(f, "max_history = 100\n");
    fprintf(f, "reset_on_login = true\n");
    fprintf(f, "paste_on_select = false\n\n");
    fprintf(f, "[ui]\n");
    fprintf(f, "window_width = 800\n");
    fprintf(f, "window_height = 600\n");
    fprintf(f, "font_face = \"Fira Code\"\n");
    fprintf(f, "font_size = 14\n");
    fclose(f);

    struct AppConfig config2;
    load_config(test_ini_path, &config2);

    assert(config2.max_history == 100);
    assert(config2.reset_on_login == true);
    assert(config2.paste_on_select == false);
    assert(config2.window_width == 800);
    assert(config2.window_height == 600);
    assert(strcmp(config2.font_face, "Fira Code") == 0);
    assert(config2.font_size == 14);
    printf("Test 2 (Custom settings load) passed.\n");

    // Test 3: Load with single quotes and extra whitespaces/comments
    f = fopen(test_ini_path, "w");
    assert(f != NULL);
    fprintf(f, "  # This is a comment\n");
    fprintf(f, "; Another comment\n");
    fprintf(f, "max_history   =    120   \n");
    fprintf(f, "reset_on_login=false\n");
    fprintf(f, "font_face = 'Comic Sans MS'\n");
    fclose(f);

    struct AppConfig config3;
    load_config(test_ini_path, &config3);

    assert(config3.max_history == 120);
    assert(config3.reset_on_login == false);
    assert(strcmp(config3.font_face, "Comic Sans MS") == 0);
    printf("Test 3 (Whitespace and quote handling) passed.\n");

    // Clean up
    remove(test_ini_path);

    printf("All configuration parser tests passed successfully!\n");
    return 0;
}

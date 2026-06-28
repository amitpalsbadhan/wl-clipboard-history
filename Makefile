CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4`
TARGET = wl-clipboard-history-gui

all: $(TARGET)

$(TARGET): src/main.c src/config.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/config.c $(LIBS)

test_config: src/config.c tests/test_config.c
	$(CC) -Wall -Wextra -std=gnu11 -o test_config src/config.c tests/test_config.c

test_gui_logic: src/config.c tests/test_gui_logic.c
	$(CC) $(CFLAGS) -o test_gui_logic src/config.c tests/test_gui_logic.c $(LIBS)

test: test_config test_gui_logic
	./test_config
	./test_gui_logic

clean:
	rm -f $(TARGET) test_config test_gui_logic

.PHONY: all clean test

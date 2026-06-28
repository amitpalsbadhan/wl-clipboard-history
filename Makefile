CC = gcc
CFLAGS = -Wall -Wextra -std=c11 `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4`
TARGET = wl-clipboard-history-gui

all: $(TARGET)

$(TARGET): src/main.c src/config.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/config.c $(LIBS)

test_config: src/config.c tests/test_config.c
	$(CC) -Wall -Wextra -std=c11 -o test_config src/config.c tests/test_config.c

test: test_config
	./test_config

clean:
	rm -f $(TARGET) test_config

.PHONY: all clean test

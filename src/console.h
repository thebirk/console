#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdbool.h>
#include <stdint.h>
#include "console_keys.h"

typedef struct Key {
	KeyCode key;
	Modifier mod;
} Key;

typedef struct Color {
	uint8_t r, g, b;
} Color;

extern Color COLOR_BLACK;
extern Color COLOR_WHITE;

bool console_init(const char *title, int w, int h, const char *tileset, int cell_w, int cell_h);
int console_start(int (*game_main)(void*), void *data);

bool console_should_quit(void);
void console_sleep(int ms);

Key console_readkey(void);
Key console_waitkey(void);

void console_printf(Color fg, Color bg, char *format, ...);
void console_set_cursor(int x, int y);

void console_set(int x, int y, int c, Color fg, Color bg);

// 800hz 200ms
void console_beep(void);
void console_beep_freq(int freq, int ms);

#endif /* CONSOLE_H */

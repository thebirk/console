#include <stdio.h>
#include "console.h"

int game_main(void *data) {
	console_set_cursor(0, 0);
	for (int i = 0; i < 15; i++) {
		console_printf(COLOR_WHITE, COLOR_BLACK, ".");
		//console_beep();
	}
	console_printf(COLOR_WHITE, COLOR_BLACK, "\nWelcome!\n");

	console_sleep(500);
	console_printf(COLOR_WHITE, COLOR_BLACK, "Press all the keys!\n");

	uint8_t i = 0;
	while (!console_should_quit()) {
		Key kp = console_waitkey();
		console_set(5, 5, i++, COLOR_BLACK, COLOR_WHITE);
		//console_beep();
		console_printf(COLOR_BLACK, COLOR_WHITE, "aaaaaa!!");
		//console_beep_freq(440, 1000);
	}

	return 0;
}

int main() {
	console_init("Dungeon", 80, 25, "res/VGA8x16.png", 8, 16);
	return console_start(game_main, 0);
}
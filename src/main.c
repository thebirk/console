#include <stdio.h>
#include "console.h"

int game_main(void *data) {
	console_set_cursor(0, 0);
	for (int i = 0; i < 15; i++) {
		console_printf(COLOR_WHITE, COLOR_BLACK, ".");
		console_beep();
		console_sleep(250);
	}
	console_printf(COLOR_WHITE, COLOR_BLACK, "\nWelcome!");

	console_sleep(500);

	int i = 0;
	while (!console_should_quit()) {
		console_sleep(10);

		Key kp = console_waitkey();
		console_set(5, 5, i++, COLOR_BLACK, COLOR_WHITE);
		console_beep();
		//console_beep_freq(440, 1000);
		printf("keys!\n");
	}

	return 0;
}

int main() {
	console_init("Dungeon", 80, 25, "res/VGA8x16.png", 8, 16);
	return console_start(game_main, 0);
}
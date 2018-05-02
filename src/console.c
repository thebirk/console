#include "console.h"
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

typedef struct Glyph {
	int c;
	Color fg;
	Color bg;
	bool dirty;
} Glyph;

typedef struct KeyPress KeyPress;
struct KeyPress {
	KeyCode key;
	Modifier mod;
	KeyPress *next;
};

Color COLOR_BLACK = { 0,   0,   0 };
Color COLOR_WHITE = { 255, 255, 255 };

typedef struct ConsoleState {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Texture *backbuffer;

	SDL_AudioDeviceID beeper;
	int beeper_freq;
	int beeper_time;
	int beeper_sps;
	float beeper_pos;

	int width;
	int height;
	int cell_w;
	int cell_h;
	int cells_x;
	int cells_y;

	int cursor_x;
	int cursor_y;

	Glyph *glyphs;
	SDL_mutex *glyphs_mutex;

	KeyPress *first_key;
	KeyPress *last_key;
	SDL_mutex *keys_mutex;

	bool running;
	bool fullscreen;

	const char *tileset_path;
} ConsoleState;
ConsoleState state = { 0 };

#define TAU_CONSTANT (2*3.14159265359)

void _console_audio_callback(void *userdata, uint8_t *data, int len) {
	ConsoleState *state = (ConsoleState*)userdata;
	
	if (state->beeper_time > 0) {
		int tone = state->beeper_freq;
		int16_t volume = 4000;
		int sampleindex = 0;
		int halfperiod = 44100 / (2*tone);

		uint16_t *out = (uint16_t*)data;
		for (int i = 0; i < len/2; i++) {
			int16_t sample = (int16_t) (volume * sin(state->beeper_pos++ * TAU_CONSTANT * ((float)tone / 44100.0f)));
			//int16_t sample = (((int)state->beeper_pos++ / halfperiod) % 2) ? volume : -volume;
			out[i] = sample;
		}

		int time = ((float)(len / 2) / (float)44100) * 1000;
		state->beeper_time -= time;
	}
	else {
		for (int i = 0; i < len; i++) {
			data[i] = 0;
		}
		SDL_PauseAudioDevice(state->beeper, 1);
	}
}

void _console_init_beeper(void) {
	SDL_AudioSpec want, have;
	SDL_memset(&want, 0, sizeof(SDL_AudioSpec));
	want.freq = 44100;
	want.format = AUDIO_S16;
	want.channels = 1;
	want.samples = 512;
	want.userdata = &state;
	want.callback = _console_audio_callback;

	state.beeper = SDL_OpenAudioDevice(0, 0, &want, &have, 0);
	state.beeper_freq = 0;
	state.beeper_time = 0;
	state.beeper_pos = 0;
	SDL_PauseAudioDevice(state.beeper, 1);
}

bool console_init(const char *title, int w, int h, const char *tileset, int cell_w, int cell_h) {
	state.width = w;
	state.height = h;
	state.cell_w = cell_w;
	state.cell_h = cell_h;
	state.tileset_path = tileset;

	SDL_Init(SDL_INIT_EVERYTHING);

	state.window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w*cell_w*2, h*cell_h*2, SDL_WINDOW_RESIZABLE);
	if (!state.window) {
		return false;
	}

	state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_ACCELERATED);
	if (!state.renderer) {
		return false;
	}

	IMG_Init(IMG_INIT_PNG);
	state.texture = IMG_LoadTexture(state.renderer, tileset);
	if (!state.texture) {
		return false;
	}

	state.backbuffer = SDL_CreateTexture(state.renderer, SDL_GetWindowPixelFormat(state.window), SDL_TEXTUREACCESS_TARGET, w*cell_w, h*cell_h);
	if (!state.backbuffer) {
		return false;
	}

	int texture_width, texture_height;
	SDL_QueryTexture(state.texture, 0, 0, &texture_width, &texture_height);
	state.cells_x = texture_width / state.cell_w;
	state.cells_y = texture_height / state.cell_h;

	state.glyphs = malloc(sizeof(Glyph)*w*h);
	for (int i = 0; i < w*h; i++) {
		state.glyphs[i] = (Glyph) { ' ', COLOR_WHITE, COLOR_BLACK, true };
	}

	state.keys_mutex = SDL_CreateMutex();

	_console_init_beeper();

	return true;
}

void _console_draw(bool redraw) {
	SDL_SetRenderTarget(state.renderer, state.backbuffer);

	SDL_Rect dst = {0, 0, state.cell_w, state.cell_h};
	SDL_Rect src = {0, 0, state.cell_w, state.cell_h};

	for (int y = 0; y < state.height; y++) {
		for (int x = 0; x < state.width; x++) {
			Glyph *g = &state.glyphs[x + y * state.width];

			if (g->dirty || redraw) {
				SDL_SetTextureColorMod(state.texture, g->fg.r, g->fg.g, g->fg.b);

				int xpos = x * state.cell_w;
				int ypos = y * state.cell_h;
				dst.x = xpos;
				dst.y = ypos;

				int tx = g->c % state.cells_x;
				int ty = g->c / state.cells_y;
				src.x = tx * state.cell_w;
				src.y = ty * state.cell_h;

				SDL_SetRenderDrawColor(state.renderer, g->bg.r, g->bg.g, g->bg.b, 255);
				SDL_RenderFillRect(state.renderer, &dst);
				SDL_RenderCopy(state.renderer, state.texture, &src, &dst);
			}
		}
	}
	SDL_SetRenderTarget(state.renderer, 0);
	SDL_RenderCopy(state.renderer, state.backbuffer, 0, 0);

	SDL_RenderPresent(state.renderer);
}

int console_start(int(*game_main)(void*), void *data) {
	state.running = true;

	SDL_Thread *game_thread = SDL_CreateThread(game_main, "Game Thread", data);
	if (!game_thread) return 1;

	loop:
	while (state.running) {
		SDL_Event e;
		while (SDL_WaitEvent(&e)) {
			_console_draw(false);

			switch (e.type) {
			case SDL_QUIT: {
				state.running = false;
				goto loop;
			} break;
			case SDL_WINDOWEVENT: {
				_console_draw(true);
			} break;

			case SDL_KEYUP: {
				if (e.key.keysym.sym == SDLK_F11) {
					state.fullscreen = !state.fullscreen;
					if (state.fullscreen) {
						SDL_SetWindowFullscreen(state.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
					}
					else {
						SDL_SetWindowFullscreen(state.window, 0);
					}
				}
			} break;

			case SDL_KEYDOWN: {
				KeyPress *kp = calloc(1, sizeof(KeyPress));
				kp->key = e.key.keysym.sym;
				kp->mod = e.key.keysym.mod;

				SDL_LockMutex(state.keys_mutex);
				if (state.last_key) {
					state.last_key->next = kp;
				}
				state.last_key = kp;
				if (!state.first_key) {
					state.first_key = state.last_key;
				}
				SDL_UnlockMutex(state.keys_mutex);
			} break;
			}
		}
	}

	int ret = 0;
	SDL_WaitThread(game_thread, &ret);
	return ret;
}

Key console_readkey(void) {
	if (state.first_key) {
		SDL_LockMutex(state.keys_mutex);
		KeyPress *kp = state.first_key;
		state.first_key = kp->next;
		if (!state.first_key) {
			state.last_key = 0;
		}
		Key result = { .key = kp->key, .mod = kp->mod };
		SDL_UnlockMutex(state.keys_mutex);
		return result;
	}
	return (Key){0};
}

Key console_waitkey(void) {
	if (state.first_key) {
		return console_readkey();
	}
	else {
		while (!state.first_key) {
			if (!state.running) return (Key){ 0 };
			SDL_Delay(1);
		}
		return console_readkey();
	}
}

void console_printf(Color fg, Color bg, char *format, ...) {
	char buffer[4096];

	va_list args;
	va_start(args, format);
	SDL_vsnprintf(buffer, 4096, format, args);
	va_end(args);

	SDL_LockMutex(state.glyphs_mutex);
	for (int i = 0; buffer[i]; i++) {
		char c = buffer[i];
		if (c == '\n') {
			state.cursor_x = 0;
			state.cursor_y++;
			if (state.cursor_y >= state.height) {
				state.cursor_y = 0;
			}
		}
		else {
			Glyph *g = &state.glyphs[state.cursor_x + state.cursor_y*state.width];
			g->c = c;
			g->fg = fg;
			g->bg = bg;
			state.cursor_x++;
			if (state.cursor_x >= state.width) {
				state.cursor_y++;
				state.cursor_x = 0;
				
				if (state.cursor_y >= state.height) {
					state.cursor_y = 0;
				}
			}
		}
	}
	SDL_UnlockMutex(state.glyphs_mutex);

	SDL_Event e;
	e.type = SDL_USEREVENT;
	SDL_PushEvent(&e);
}

void console_set(int x, int y, int c, Color fg, Color bg) {
	assert(x >= 0 && x < state.width);
	assert(y >= 0 && y < state.height);

	Glyph *g = &state.glyphs[y + y*state.width];
	g->c = c;
	g->fg = fg;
	g->bg = bg;
	g->dirty = true;

	SDL_Event e;
	e.type = SDL_USEREVENT;
	SDL_PushEvent(&e);
}

void console_set_cursor(int x, int y) {
	assert(x >= 0 && x < state.width);
	assert(y >= 0 && y < state.height);
	state.cursor_x = x;
	state.cursor_y = y;
}

bool console_should_quit(void) {
	return !state.running;
}

void console_sleep(int ms) {
	SDL_Delay(ms);
}

void console_beep(void) {
	console_beep_freq(800, 200);
}

void console_beep_freq(int freq, int ms) {
	state.beeper_freq = freq;
	state.beeper_time = ms;
	state.beeper_pos = 0;
	SDL_PauseAudioDevice(state.beeper, 0);
	console_sleep(ms);
}
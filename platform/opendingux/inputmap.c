//	For TRIMUI Modified

#include <stdlib.h>
#include <SDL_keysym.h>

#include "../libpicofe/input.h"
#include "../libpicofe/in_sdl.h"
#include "../common/input_pico.h"
#include "../common/plat_sdl.h"

const struct in_default_bind in_sdl_defbinds[] = {
	{ SDLK_UP,		IN_BINDTYPE_PLAYER12, GBTN_UP },
	{ SDLK_DOWN,	IN_BINDTYPE_PLAYER12, GBTN_DOWN },
	{ SDLK_LEFT,	IN_BINDTYPE_PLAYER12, GBTN_LEFT },
	{ SDLK_RIGHT,	IN_BINDTYPE_PLAYER12, GBTN_RIGHT },
	{ SDLK_LALT,	IN_BINDTYPE_PLAYER12, GBTN_A },		// Y
	{ SDLK_LSHIFT,	IN_BINDTYPE_PLAYER12, GBTN_B },		// X
	{ SDLK_LCTRL,	IN_BINDTYPE_PLAYER12, GBTN_B },		// B
	{ SDLK_SPACE,	IN_BINDTYPE_PLAYER12, GBTN_C },		// A
	{ SDLK_RETURN,	IN_BINDTYPE_PLAYER12, GBTN_START },	// START
	{ SDLK_ESCAPE,	IN_BINDTYPE_EMU, PEVB_MENU },		// MENU
	{ 0, 0, 0 }
};

const struct menu_keymap in_sdl_key_map[] = {
	{ SDLK_UP,			PBTN_UP },
	{ SDLK_DOWN,		PBTN_DOWN },
	{ SDLK_LEFT,		PBTN_LEFT },
	{ SDLK_RIGHT,		PBTN_RIGHT },
	{ SDLK_SPACE,		PBTN_MOK },		// A
	{ SDLK_LCTRL,		PBTN_MBACK },	// B
	{ SDLK_LSHIFT,		PBTN_MA2 },		// X
	{ SDLK_LALT,		PBTN_MA3 },		// Y
	{ SDLK_TAB,			PBTN_L },
	{ SDLK_BACKSPACE,	PBTN_R },
};
const int in_sdl_key_map_sz = sizeof(in_sdl_key_map) / sizeof(in_sdl_key_map[0]);

const struct menu_keymap in_sdl_joy_map[] = {
	{ SDLK_UP,		PBTN_UP },
	{ SDLK_DOWN,	PBTN_DOWN },
	{ SDLK_LEFT,	PBTN_LEFT },
	{ SDLK_RIGHT,	PBTN_RIGHT },
	/* joystick */
	{ SDLK_WORLD_0,	PBTN_MOK },
	{ SDLK_WORLD_1,	PBTN_MBACK },
	{ SDLK_WORLD_2,	PBTN_MA2 },
	{ SDLK_WORLD_3,	PBTN_MA3 },
};
const int in_sdl_joy_map_sz = sizeof(in_sdl_joy_map) / sizeof(in_sdl_joy_map[0]);

const char * const _in_sdl_key_names[SDLK_LAST] = {
	[SDLK_UP] = "UP",
	[SDLK_DOWN] = "DOWN",
	[SDLK_LEFT] = "LEFT",
	[SDLK_RIGHT] = "RIGHT",
	[SDLK_SPACE] = "A",
	[SDLK_LCTRL] = "B",
	[SDLK_LSHIFT] = "X",
	[SDLK_LALT] = "Y",
	[SDLK_RETURN] = "START",
	[SDLK_RCTRL] = "SELECT",
	[SDLK_TAB] = "L",
	[SDLK_BACKSPACE] = "R",
	[SDLK_ESCAPE] = "MENU",
};
const char * const (*in_sdl_key_names)[SDLK_LAST] = &_in_sdl_key_names;

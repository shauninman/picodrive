/*
 * PicoDrive
 * (C) notaz, 2006-2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#ifdef USE_SDL
#include <SDL.h>
#endif

#include "../libpicofe/input.h"
#include "../libpicofe/plat.h"
#include "../libpicofe/plat_sdl.h"
#include "menu_pico.h"
#include "emu.h"
#include "version.h"
#include <cpu/debug.h>

#ifdef USE_LIBRETRO_VFS
#include "file_stream_transforms.h"
#endif

#include <dlfcn.h>
#include <mmenu.h> // wrap in extern "C" { } if adding to a .cpp file
static void* mmenu = NULL;

static int load_state_slot = -1;
char **g_argv;

void parse_cmd_line(int argc, char *argv[])
{
	int x, unrecognized = 0;

	for (x = 1; x < argc; x++)
	{
		if (argv[x][0] == '-')
		{
			if (strcasecmp(argv[x], "-config") == 0) {
				if (x+1 < argc) { ++x; PicoConfigFile = argv[x]; }
			}
			else if (strcasecmp(argv[x], "-loadstate") == 0
				 || strcasecmp(argv[x], "-load") == 0)
			{
				if (x+1 < argc) { ++x; load_state_slot = atoi(argv[x]); }
			}
			else if (strcasecmp(argv[x], "-pdb") == 0) {
				if (x+1 < argc) { ++x; pdb_command(argv[x]); }
			}
			else if (strcasecmp(argv[x], "-pdb_connect") == 0) {
				if (x+2 < argc) { pdb_net_connect(argv[x+1], argv[x+2]); x += 2; }
			}
			else {
				unrecognized = 1;
				break;
			}
		} else {
			FILE *f = fopen(argv[x], "rb");
			if (f) {
				fclose(f);
				rom_fname_reload = argv[x];
				engineState = PGS_ReloadRom;
			}
			else
				unrecognized = 1;
			break;
		}
	}

	if (unrecognized) {
		printf("\n\n\nPicoDrive v" VERSION " (c) notaz, 2006-2009,2013\n");
		printf("usage: %s [options] [romfile]\n", argv[0]);
		printf("options:\n"
			" -config <file>    use specified config file instead of default 'config.cfg'\n"
			" -loadstate <num>  if ROM is specified, try loading savestate slot <num>\n");
		exit(1);
	}
}

static void change_disc(char* path)
{
	if (cdd_loaded()) cdd_unload();

	static char tmp[256];
	strcpy(tmp, path);
	rom_fname_reload = tmp;

	engineState = PGS_RestartRun;
	emu_swap_cd(path);
}

static void get_file(char* path, char* buffer) {
	FILE *file = fopen(path, "r");
	fseek(file, 0L, SEEK_END);
	size_t size = ftell(file);
	rewind(file);
	fread(buffer, size, sizeof(char), file);
	fclose(file);
	buffer[size] = '\0';
}

int main(int argc, char *argv[])
{
	g_argv = argv;

	plat_early_init();

	in_init();
	//in_probe();

	plat_target_init();
	plat_init();
	menu_init();

	emu_prep_defconfig(); // depends on input
	emu_read_config(NULL, 0);

	emu_init();

	engineState = PGS_Menu;

	if (argc > 1)
		parse_cmd_line(argc, argv);

	if (engineState == PGS_ReloadRom)
	{
		if (emu_reload_rom(rom_fname_reload)) {
			engineState = PGS_Running;
			if (load_state_slot >= 0) {
				state_slot = load_state_slot;
				emu_save_load_game(1, 0);
			}
		}
	}
	
	mmenu = dlopen("libmmenu.so", RTLD_LAZY);
	
	for (;;)
	{
		switch (engineState)
		{
			case PGS_TrayMenu:
			case PGS_Menu:{
				if (mmenu) {
					char* save_path = emu_get_save_fname(0, 0, -1, NULL);
					ShowMenu_t ShowMenu = (ShowMenu_t)dlsym(mmenu, "ShowMenu");
					MenuReturnStatus status = ShowMenu(rom_fname_reload, save_path, plat_sdl_screen, kMenuEventKeyDown);
					
					engineState = PGS_Running;
					
					if (status==kStatusExitGame) {
						engineState = PGS_Quit;
					}
					else if (status==kStatusChangeDisc && access("/tmp/change_disc.txt", F_OK)==0) {
						char disc_path[256];
						get_file("/tmp/change_disc.txt", disc_path);
						change_disc(disc_path);
					}
					else if (status==kStatusOpenMenu) {
						menu_loop();
					}
					else if (status>=kStatusLoadSlot) {
						state_slot = status - kStatusLoadSlot;
						emu_save_load_game(1, 0);
					}
					else if (status>=kStatusSaveSlot) {
						state_slot = status - kStatusSaveSlot;
						emu_save_load_game(0, 0);
					}
					
					// release that menu key
					SDL_Event sdlevent;
					sdlevent.type = SDL_KEYUP;
					sdlevent.key.keysym.sym = SDLK_ESCAPE;
					SDL_PushEvent(&sdlevent);
				}
				else {
					if (engineState==PGS_TrayMenu) {
						menu_loop_tray();
					}
					else {
						menu_loop();
					}
				}
				} break;

			// case PGS_TrayMenu:
			// 	menu_loop_tray();
			// 	break;

			case PGS_ReloadRom:
				if (emu_reload_rom(rom_fname_reload))
					engineState = PGS_Running;
				else {
					printf("PGS_ReloadRom == 0\n");
					engineState = PGS_Menu;
				}
				break;

			case PGS_RestartRun:
				engineState = PGS_Running;
				/* vvv fallthrough */

			case PGS_Running:
#ifdef GPERF
	ProfilerStart("gperf.out");
#endif
				emu_loop();
#ifdef GPERF
	ProfilerStop();
#endif
				break;

			case PGS_Quit:
				goto endloop;

			default:
				printf("engine got into unknown state (%i), exitting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	emu_finish();
	plat_finish();
	plat_target_finish();

	return 0;
}

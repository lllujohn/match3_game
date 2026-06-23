
#ifndef MATCH3_CONTROLLER_H
#define MATCH3_CONTROLLER_H

#include "types.h"
#include <SDL2/SDL.h>   

bool controller_init(void);

void controller_destroy(void);

void controller_handle_event(GameBoard *board, const SDL_Event *event);

bool controller_update_state_machine(GameBoard *board, float delta_time);

bool controller_restart_game(GameBoard *board);

bool controller_toggle_pause(GameBoard *board);

bool controller_wants_quit(void);

bool controller_save_game(const GameBoard *board, const char *filename);

bool controller_load_game(GameBoard *board, const char *filename);

bool controller_quick_save(const GameBoard *board);

bool controller_quick_load(GameBoard *board);

void controller_set_difficulty(int difficulty);

int  controller_get_difficulty(void);

#endif 

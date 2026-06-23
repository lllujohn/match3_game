
#ifndef MATCH3_VIEW_H
#define MATCH3_VIEW_H

#include "types.h"

bool view_init_window(void);

void view_destroy_window(void);

bool view_load_assets(void);

void view_unload_assets(void);

bool view_render_frame(const GameBoard *board);

void view_update_animations(GameBoard *board, float dt);

void view_draw_game_ui_complete(const GameBoard *board);

void view_draw_main_menu(const GameBoard *board);

void view_draw_difficulty_menu(const GameBoard *board);

void view_draw_pause_menu(const GameBoard *board);

void view_draw_game_over_screen(const GameBoard *board);

void view_set_window_title(const char *title);

bool view_should_close_window(void);

bool view_all_gems_settled(const GameBoard *board);

void view_play_sound_effect(const char *event_name);

void view_spawn_particles(float cx, float cy, uint8_t gem_type);

void view_set_bgm(int state);

bool view_has_badge(void);

#endif 

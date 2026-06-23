
#ifndef MATCH3_MODEL_H
#define MATCH3_MODEL_H

#include "types.h"

bool model_init_board(GameBoard *board);

void model_destroy_board(GameBoard *board);

bool model_init_board_with_difficulty(GameBoard *board, int difficulty);

Gem model_generate_gem(GameBoard *board, uint8_t row, uint8_t col, bool offscreen_spawn);

uint32_t model_check_eliminations(GameBoard *board, EliminationSet *out_set);

uint32_t model_check_eliminations_advanced(GameBoard *board, EliminationSet *out_set);

void model_apply_eliminations(GameBoard *board);

bool model_apply_gravity(GameBoard *board);

void model_refill_board(GameBoard *board);

bool model_swap_gems(GameBoard *board,
                     uint8_t r1, uint8_t c1,
                     uint8_t r2, uint8_t c2);

bool model_is_deadlock(GameBoard *board);

bool model_is_adjacent(const GameBoard *board, uint8_t row, uint8_t col);

bool model_screen_to_board_coord(int px, int py,
                                 uint8_t *out_row, uint8_t *out_col);

void model_trigger_bomb_chain(GameBoard *board, uint8_t row, uint8_t col);

bool model_save_game(const GameBoard *board, const char *filename);

bool model_load_game(GameBoard *board, const char *filename);

bool model_find_best_hint(GameBoard *board,
                          uint8_t *hr, uint8_t *hc, uint8_t *hd);

int model_prop_hammer_smash(GameBoard *board, uint8_t row, uint8_t col);

bool model_prop_wand_swap(GameBoard *board, uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2);

bool model_force_shuffle(GameBoard *board);

bool model_prop_shuffle(GameBoard *board);

bool model_prop_add_moves(GameBoard *board);

#endif 

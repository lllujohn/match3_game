#include "model.h"
#include "types.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ================================================================
 *  Internal helpers — pixel coordinate arithmetic
 * ================================================================ */

/** Centre-X of a gem cell at logical column @p col (pixels). */
static inline float gem_target_x(uint8_t col)
{
    return (float)BOARD_OFFSET_X + ((float)col * (float)GEM_SIZE) + ((float)GEM_SIZE / 2.0f);
}

/** Centre-Y of a gem cell at logical row @p row (pixels). */
static inline float gem_target_y(uint8_t row)
{
    return (float)BOARD_OFFSET_Y + ((float)row * (float)GEM_SIZE) + ((float)GEM_SIZE / 2.0f);
}

/* ================================================================
 *  Initialisation helpers
 * ================================================================ */

/**
 * @brief Check whether the board already contains a 3-in-a-row.
 *
 * Used during board generation to guarantee a clean start state.
 */
static bool has_initial_match(const GameBoard *board)
{
    /* Check horizontal */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c <= BOARD_WIDTH - 3; c++) {
            uint8_t t1 = board->board[r][c].gem_type;
            uint8_t t2 = board->board[r][c+1].gem_type;
            uint8_t t3 = board->board[r][c+2].gem_type;
            
            bool match = true;
            uint8_t color = GEM_EMPTY;
            uint8_t types[3] = {t1, t2, t3};
            for (int k = 0; k < 3; k++) {
                if (types[k] >= MAX_GEM_TYPES && types[k] != GEM_WILDCARD) { match = false; break; }
                if (types[k] != GEM_WILDCARD) {
                    if (color == GEM_EMPTY) color = types[k];
                    else if (color != types[k]) { match = false; break; }
                }
            }
            if (match) return true;
        }
    }
    /* Check vertical */
    for (int c = 0; c < BOARD_WIDTH; c++) {
        for (int r = 0; r <= BOARD_HEIGHT - 3; r++) {
            uint8_t t1 = board->board[r][c].gem_type;
            uint8_t t2 = board->board[r+1][c].gem_type;
            uint8_t t3 = board->board[r+2][c].gem_type;
            
            bool match = true;
            uint8_t color = GEM_EMPTY;
            uint8_t types[3] = {t1, t2, t3};
            for (int k = 0; k < 3; k++) {
                if (types[k] >= MAX_GEM_TYPES && types[k] != GEM_WILDCARD) { match = false; break; }
                if (types[k] != GEM_WILDCARD) {
                    if (color == GEM_EMPTY) color = types[k];
                    else if (color != types[k]) { match = false; break; }
                }
            }
            if (match) return true;
        }
    }
    return false;
}

/* ================================================================
 *  Gem generation
 * ================================================================ */

Gem model_generate_gem(uint8_t row, uint8_t col, bool offscreen_spawn)
{
    Gem gem;
    memset(&gem, 0, sizeof(Gem));

    gem.row       = row;
    gem.col       = col;
    
    /* 5% chance for a wildcard, otherwise uniform across 0-4 (5 regular colors) */
    if ((rand() % 100) < 5) {
        gem.gem_type = GEM_WILDCARD;
    } else {
        gem.gem_type = (uint8_t)(rand() % 5); /* 0 to 4 */
    }
    
    gem.bomb_type = BOMB_NONE;
    gem.is_marked_for_elimination = false;
    gem.animation_progress        = 0.0f;
    gem.elim_scale                = 1.0f;

    gem.target_x = gem_target_x(col);
    gem.target_y = gem_target_y(row);

    if (offscreen_spawn) {
        gem.screen_x = gem.target_x;
        gem.screen_y = gem.target_y - (float)(BOARD_HEIGHT * GEM_SIZE);
    } else {
        gem.screen_x = gem.target_x;
        gem.screen_y = gem.target_y;
    }

    return gem;
}

/* ================================================================
 *  Board initialisation
 * ================================================================ */

bool model_init_board(GameBoard *board)
{
    if (!board)
        return false;

    memset(board, 0, sizeof(GameBoard));
    board->current_state          = GAME_STATE_MAIN_MENU;
    board->highlighted_menu_option = 0;
    board->score              = 0;
    board->level              = 1;
    board->moves_remaining    = DEFAULT_MOVES;
    board->first_gem_selected = false;
    board->combo_multiplier   = 1;
    board->animations_settled = true;

    /* Re-roll until the initial board has no 3-matches */
    do {
        for (int r = 0; r < BOARD_HEIGHT; r++)
            for (int c = 0; c < BOARD_WIDTH; c++)
                board->board[r][c] =
                    model_generate_gem((uint8_t)r, (uint8_t)c, false);
    } while (has_initial_match(board) || model_is_deadlock(board));

    return true;
}

void model_destroy_board(GameBoard *board)
{
    if (board)
        memset(board, 0, sizeof(GameBoard));
}

bool model_init_board_with_difficulty(GameBoard *board, int difficulty)
{
    if (!board || difficulty < 0 || difficulty > 2)
        return false;

    memset(board, 0, sizeof(GameBoard));

    board->current_state      = GAME_STATE_WAITING_INPUT;
    board->score              = 0;
    board->prop_hammer_count  = 5;
    board->prop_wand_count    = 5;
    board->prop_shuffle_count = 5;
    board->prop_moves_count   = 5;

    board->level              = 1;
    board->difficulty         = difficulty;
    board->first_gem_selected = false;
    board->highlighted_difficulty = difficulty;
    board->combo_multiplier   = 1;
    board->animations_settled = false; /* triggers the fall-in animation */

    switch (difficulty) {
        case 0:  board->moves_remaining = EASY_MOVES;   break;
        case 2:  board->moves_remaining = HARD_MOVES;   break;
        default: board->moves_remaining = NORMAL_MOVES; break;
    }

    /* Generate a match-free, non-deadlock initial board */
    do {
        for (int r = 0; r < BOARD_HEIGHT; r++)
            for (int c = 0; c < BOARD_WIDTH; c++)
                board->board[r][c] =
                    model_generate_gem((uint8_t)r, (uint8_t)c, false);
    } while (has_initial_match(board) || model_is_deadlock(board));

    /* Push all gems above the viewport for a cascade drop-in animation */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            Gem *g   = &board->board[r][c];
            g->screen_x = g->target_x;
            g->screen_y = g->target_y - (float)((BOARD_HEIGHT - r) * GEM_SIZE + 200);
        }
    }

    return true;
}

/* ================================================================
 *  Elimination detection  (O(n), single-pass row+column scan)
 * ================================================================ */

uint32_t model_check_eliminations(GameBoard *board, EliminationSet *out_set)
{
    if (!board || !out_set)
        return 0;

    memset(out_set, 0, sizeof(EliminationSet));

    /* Clear all elimination marks and bomb intents */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            board->board[r][c].is_marked_for_elimination = false;
            board->board[r][c].next_bomb_type = BOMB_NONE;
        }
    }

    /* Horizontal scan */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH - 2; ) {
            uint8_t color = GEM_EMPTY;
            int end = c;
            while (end < BOARD_WIDTH) {
                uint8_t t = board->board[r][end].gem_type;
                if (t >= MAX_GEM_TYPES && t != GEM_WILDCARD) break;
                if (t != GEM_WILDCARD) {
                    if (color == GEM_EMPTY) color = t;
                    else if (color != t) break;
                }
                end++;
            }

            if (end - c >= 3) {
                int bomb_idx = c + (end - c) / 2;
                for (int k = c; k < end; k++) {
                    board->board[r][k].is_marked_for_elimination = true;
                    if (end - c >= 5 && k == bomb_idx) {
                        board->board[r][k].next_bomb_type = BOMB_RADIUS;
                    } else if (end - c == 4 && k == bomb_idx) {
                        board->board[r][k].next_bomb_type = BOMB_LINE_V;
                    }
                }
                c = end;
            } else {
                c++;
            }
        }
    }

    /* Vertical scan */
    for (int c = 0; c < BOARD_WIDTH; c++) {
        for (int r = 0; r < BOARD_HEIGHT - 2; ) {
            uint8_t color = GEM_EMPTY;
            int end = r;
            while (end < BOARD_HEIGHT) {
                uint8_t t = board->board[end][c].gem_type;
                if (t >= MAX_GEM_TYPES && t != GEM_WILDCARD) break;
                if (t != GEM_WILDCARD) {
                    if (color == GEM_EMPTY) color = t;
                    else if (color != t) break;
                }
                end++;
            }

            if (end - r >= 3) {
                int bomb_idx = r + (end - r) / 2;
                for (int k = r; k < end; k++) {
                    board->board[k][c].is_marked_for_elimination = true;
                    if (end - r >= 5 && k == bomb_idx) {
                        board->board[k][c].next_bomb_type = BOMB_RADIUS;
                    } else if (end - r == 4 && k == bomb_idx) {
                        board->board[k][c].next_bomb_type = BOMB_LINE_H;
                    }
                }
                r = end;
            } else {
                r++;
            }
        }
    }

    /* Collect results */
    uint32_t points = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (board->board[r][c].is_marked_for_elimination &&
                out_set->count < (uint8_t)(BOARD_WIDTH * BOARD_HEIGHT)) {
                out_set->positions[out_set->count].row = (uint8_t)r;
                out_set->positions[out_set->count].col = (uint8_t)c;
                out_set->count++;
                points += 10;
                if (board->board[r][c].next_bomb_type == BOMB_RADIUS) points += 50;
                else if (board->board[r][c].next_bomb_type != BOMB_NONE) points += 30;
                if (board->board[r][c].bomb_type != BOMB_NONE) points += 20;
            }
        }
    }
    return points;
}

uint32_t model_check_eliminations_advanced(GameBoard *board, EliminationSet *out_set)
{
    uint32_t base = model_check_eliminations(board, out_set);
    return base * (board ? board->combo_multiplier : 1u);
}

/* ================================================================
 *  Apply eliminations — writes GEM_EMPTY to marked cells
 * ================================================================ */

void model_apply_eliminations(GameBoard *board)
{
    if (!board)
        return;

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (board->board[r][c].is_marked_for_elimination) {
                int next_bomb = board->board[r][c].next_bomb_type;
                if (next_bomb != BOMB_NONE) {
                    /* Transform into bomb instead of clearing */
                    board->board[r][c].is_marked_for_elimination = false;
                    board->board[r][c].bomb_type = next_bomb;
                    board->board[r][c].next_bomb_type = BOMB_NONE;
                    board->board[r][c].elim_scale = 1.0f;
                } else {
                    memset(&board->board[r][c], 0, sizeof(Gem));
                    board->board[r][c].gem_type  = (uint8_t)GEM_EMPTY;
                    board->board[r][c].row       = (uint8_t)r;
                    board->board[r][c].col       = (uint8_t)c;
                    board->board[r][c].elim_scale = 0.0f;
                }
            }
        }
    }
}

/* ================================================================
 *  Gravity — compact non-empty gems toward the bottom of each column
 * ================================================================ */

bool model_apply_gravity(GameBoard *board)
{
    if (!board)
        return false;

    bool moved = false;

    for (int c = 0; c < BOARD_WIDTH; c++) {
        int write_row = BOARD_HEIGHT - 1;

        for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
            if (board->board[r][c].gem_type != (uint8_t)GEM_EMPTY) {
                if (write_row != r) {
                    /* Move gem down; screen_x/y retain old value for Lerp */
                    board->board[write_row][c]         = board->board[r][c];
                    board->board[write_row][c].row     = (uint8_t)write_row;
                    board->board[write_row][c].col     = (uint8_t)c;
                    board->board[write_row][c].target_x = gem_target_x((uint8_t)c);
                    board->board[write_row][c].target_y = gem_target_y((uint8_t)write_row);

                    /* Clear the source cell */
                    memset(&board->board[r][c], 0, sizeof(Gem));
                    board->board[r][c].gem_type  = (uint8_t)GEM_EMPTY;
                    board->board[r][c].row       = (uint8_t)r;
                    board->board[r][c].col       = (uint8_t)c;
                    board->board[r][c].elim_scale = 0.0f;

                    moved = true;
                }
                write_row--;
            }
        }
    }
    return moved;
}

/* ================================================================
 *  Refill — spawn new gems above the viewport for empty cells
 * ================================================================ */

void model_refill_board(GameBoard *board)
{
    if (!board)
        return;

    for (int c = 0; c < BOARD_WIDTH; c++) {
        int spawn_offset = 1;
        for (int r = 0; r < BOARD_HEIGHT; r++) {
            if (board->board[r][c].gem_type == (uint8_t)GEM_EMPTY) {
                board->board[r][c]          = model_generate_gem((uint8_t)r, (uint8_t)c, false);
                board->board[r][c].screen_x = gem_target_x((uint8_t)c);
                board->board[r][c].screen_y = (float)(BOARD_OFFSET_Y - spawn_offset * GEM_SIZE);
                board->board[r][c].elim_scale = 1.0f;
                spawn_offset++;
            }
        }
    }
}

/* ================================================================
 *  Gem swap  (swap → check → rollback if no match)
 * ================================================================ */

bool model_swap_gems(GameBoard *board,
                     uint8_t r1, uint8_t c1,
                     uint8_t r2, uint8_t c2)
{
    if (!board)
        return false;
    if (r1 >= BOARD_HEIGHT || c1 >= BOARD_WIDTH ||
        r2 >= BOARD_HEIGHT || c2 >= BOARD_WIDTH)
        return false;

    int dr = abs((int)r1 - (int)r2);
    int dc = abs((int)c1 - (int)c2);
    if (!((dr == 1 && dc == 0) || (dr == 0 && dc == 1)))
        return false;

    /* Perform swap */
    Gem tmp              = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = tmp;

    board->board[r1][c1].row = r1; board->board[r1][c1].col = c1;
    board->board[r2][c2].row = r2; board->board[r2][c2].col = c2;

    board->board[r1][c1].target_x = gem_target_x(c1);
    board->board[r1][c1].target_y = gem_target_y(r1);
    board->board[r2][c2].target_x = gem_target_x(c2);
    board->board[r2][c2].target_y = gem_target_y(r2);

    /* Check for match */
    EliminationSet es;
    model_check_eliminations(board, &es);

    if (es.count == 0) {
        /* Roll back */
        tmp              = board->board[r1][c1];
        board->board[r1][c1] = board->board[r2][c2];
        board->board[r2][c2] = tmp;

        board->board[r1][c1].row = r1; board->board[r1][c1].col = c1;
        board->board[r2][c2].row = r2; board->board[r2][c2].col = c2;

        board->board[r1][c1].target_x = gem_target_x(c1);
        board->board[r1][c1].target_y = gem_target_y(r1);
        board->board[r2][c2].target_x = gem_target_x(c2);
        board->board[r2][c2].target_y = gem_target_y(r2);

        /* screen_x/y already hold the opposite cell's position from
         * before the swap — leave them there so the lerp animates a
         * bounce-back to the just-restored targets. */

        for (int r = 0; r < BOARD_HEIGHT; r++)
            for (int c = 0; c < BOARD_WIDTH; c++)
                board->board[r][c].is_marked_for_elimination = false;

        return false;
    }

    return true;
}

/* ================================================================
 *  Deadlock detection  (O(BOARD_WIDTH × BOARD_HEIGHT))
 * ================================================================ */

/**
 * @brief Simulate a swap and return the number of gems it would match.
 *
 * Swaps, calls model_check_eliminations, restores the swap, and clears
 * only the marks touched by that swap (O(BOARD_WIDTH+BOARD_HEIGHT)).
 *
 * @return Number of gems that would be eliminated (0 = no match).
 */
static uint32_t simulate_swap(GameBoard *board,
                               int r1, int c1, int r2, int c2)
{
    /* Swap */
    Gem tmp              = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = tmp;
    board->board[r1][c1].row = (uint8_t)r1; board->board[r1][c1].col = (uint8_t)c1;
    board->board[r2][c2].row = (uint8_t)r2; board->board[r2][c2].col = (uint8_t)c2;

    EliminationSet es;
    model_check_eliminations(board, &es);

    /* Restore */
    tmp              = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = tmp;
    board->board[r1][c1].row = (uint8_t)r1; board->board[r1][c1].col = (uint8_t)c1;
    board->board[r2][c2].row = (uint8_t)r2; board->board[r2][c2].col = (uint8_t)c2;

    /* Clear only the two rows and two columns that were affected */
    for (int k = 0; k < BOARD_WIDTH; k++) {
        board->board[r1][k].is_marked_for_elimination = false;
        board->board[r2][k].is_marked_for_elimination = false;
    }
    for (int k = 0; k < BOARD_HEIGHT; k++) {
        board->board[k][c1].is_marked_for_elimination = false;
        board->board[k][c2].is_marked_for_elimination = false;
    }

    return es.count;
}

bool model_is_deadlock(GameBoard *board)
{
    if (!board)
        return true;

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (c + 1 < BOARD_WIDTH &&
                simulate_swap(board, r, c, r, c + 1) > 0)
                return false;

            if (r + 1 < BOARD_HEIGHT &&
                simulate_swap(board, r, c, r + 1, c) > 0)
                return false;
        }
    }
    return true;
}

/* ================================================================
 *  Bomb chain trigger
 * ================================================================ */

void model_trigger_bomb_chain(GameBoard *board, uint8_t row, uint8_t col)
{
    if (!board)
        return;

    switch (board->board[row][col].bomb_type) {
        case BOMB_LINE_H:
            for (int c = 0; c < BOARD_WIDTH; c++)
                if (board->board[row][c].gem_type < MAX_GEM_TYPES)
                    board->board[row][c].is_marked_for_elimination = true;
            break;

        case BOMB_LINE_V:
            for (int r = 0; r < BOARD_HEIGHT; r++)
                if (board->board[r][col].gem_type < MAX_GEM_TYPES)
                    board->board[r][col].is_marked_for_elimination = true;
            break;

        case BOMB_CROSS:
            for (int r = 0; r < BOARD_HEIGHT; r++)
                if (board->board[r][col].gem_type < MAX_GEM_TYPES)
                    board->board[r][col].is_marked_for_elimination = true;
            for (int c = 0; c < BOARD_WIDTH; c++)
                if (board->board[row][c].gem_type < MAX_GEM_TYPES)
                    board->board[row][c].is_marked_for_elimination = true;
            break;

        case BOMB_RADIUS:
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int rr = row + dr, cc = col + dc;
                    if (rr >= 0 && rr < BOARD_HEIGHT &&
                        cc >= 0 && cc < BOARD_WIDTH &&
                        board->board[rr][cc].gem_type < MAX_GEM_TYPES)
                        board->board[rr][cc].is_marked_for_elimination = true;
                }
            }
            break;

        default:
            break;
    }
}

/* ================================================================
 *  State accessors
 * ================================================================ */

GameState model_get_game_state(const GameBoard *board)
{
    return board ? board->current_state : GAME_STATE_GAME_OVER;
}

void model_set_game_state(GameBoard *board, GameState state)
{
    if (board)
        board->current_state = state;
}

bool model_is_adjacent(const GameBoard *board, uint8_t row, uint8_t col)
{
    if (!board || !board->first_gem_selected)
        return false;

    int dr = abs((int)row - (int)board->selected_row);
    int dc = abs((int)col - (int)board->selected_col);
    return (dr == 1 && dc == 0) || (dr == 0 && dc == 1);
}

bool model_screen_to_board_coord(int px, int py,
                                 uint8_t *out_row, uint8_t *out_col)
{
    if (!out_row || !out_col)
        return false;
    if (px < BOARD_OFFSET_X || px >= BOARD_OFFSET_X + BOARD_WIDTH  * GEM_SIZE ||
        py < BOARD_OFFSET_Y || py >= BOARD_OFFSET_Y + BOARD_HEIGHT * GEM_SIZE)
        return false;

    *out_col = (uint8_t)((px - BOARD_OFFSET_X) / GEM_SIZE);
    *out_row = (uint8_t)((py - BOARD_OFFSET_Y) / GEM_SIZE);
    return *out_row < BOARD_HEIGHT && *out_col < BOARD_WIDTH;
}

bool model_undo_move(GameBoard *board)
{
    if (!board || !board->undo_available)
        return false;

    /* Swap the two gems back to their previous positions */
    uint8_t r1 = board->undo_r1, c1 = board->undo_c1;
    uint8_t r2 = board->undo_r2, c2 = board->undo_c2;

    Gem tmp              = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = tmp;

    board->board[r1][c1].row = r1; board->board[r1][c1].col = c1;
    board->board[r2][c2].row = r2; board->board[r2][c2].col = c2;

    board->board[r1][c1].target_x = (float)(BOARD_OFFSET_X + c1 * GEM_SIZE + GEM_SIZE / 2);
    board->board[r1][c1].target_y = (float)(BOARD_OFFSET_Y + r1 * GEM_SIZE + GEM_SIZE / 2);
    board->board[r2][c2].target_x = (float)(BOARD_OFFSET_X + c2 * GEM_SIZE + GEM_SIZE / 2);
    board->board[r2][c2].target_y = (float)(BOARD_OFFSET_Y + r2 * GEM_SIZE + GEM_SIZE / 2);

    /* Save old screen positions (from before the original swap) for
     * the reverse-Lerp: screen is set to the opposite gem's OLD target,
     * which is still sitting in the other gem's screen_x/y at this point. */
    float s1x = board->board[r1][c1].screen_x;
    float s1y = board->board[r1][c1].screen_y;
    float s2x = board->board[r2][c2].screen_x;
    float s2y = board->board[r2][c2].screen_y;

    board->board[r1][c1].screen_x = s2x;
    board->board[r1][c1].screen_y = s2y;
    board->board[r2][c2].screen_x = s1x;
    board->board[r2][c2].screen_y = s1y;

    board->undo_available = false;
    board->moves_remaining++;           /* Refund the move */
    board->score = board->undo_score;   /* Restore pre-swap score */
    board->combo_multiplier = board->undo_combo; /* Restore pre-swap combo */
    return true;
}

/* ================================================================
 *  Persistence
 * ================================================================ */

/* ---- Save-file format header ---- */
#define SAVE_MAGIC   0x4D335356u  /* 'M3SV' */
#define SAVE_VERSION 2u

typedef struct {
    uint32_t magic;   /* Must equal SAVE_MAGIC   */
    uint16_t version; /* Must equal SAVE_VERSION */
    uint16_t pad;     /* Reserved, write as 0    */
} SaveHeader;

bool model_save_game(const GameBoard *board, const char *filename)
{
    if (!board || !filename)
        return false;

    FILE *f = fopen(filename, "wb");
    if (!f)
        return false;

    SaveHeader hdr = {SAVE_MAGIC, SAVE_VERSION, 0};
    bool ok = (fwrite(&hdr,  sizeof(hdr),       1, f) == 1) &&
              (fwrite(board, sizeof(GameBoard),  1, f) == 1);
    fclose(f);
    return ok;
}

bool model_load_game(GameBoard *board, const char *filename)
{
    if (!board || !filename)
        return false;

    FILE *f = fopen(filename, "rb");
    if (!f)
        return false;

    SaveHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 ||
        hdr.magic   != SAVE_MAGIC       ||
        hdr.version != SAVE_VERSION) {
        fprintf(stderr, "[model] save file corrupt or version mismatch\n");
        fclose(f);
        return false;
    }

    bool ok = fread(board, sizeof(GameBoard), 1, f) == 1;
    fclose(f);
    return ok;
}

/* ================================================================
 *  Hint system  (finds global optimum)
 * ================================================================ */

bool model_find_best_hint(GameBoard *board,
                          uint8_t *hr, uint8_t *hc, uint8_t *hd)
{
    if (!board || !hr || !hc || !hd)
        return false;

    uint32_t best_pts = 0;
    bool     found    = false;

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {

            if (c + 1 < BOARD_WIDTH) {
                uint32_t pts = simulate_swap(board, r, c, r, c + 1);
                if (pts > best_pts) {
                    best_pts = pts;
                    *hr = (uint8_t)r;
                    *hc = (uint8_t)c;
                    *hd = 0; /* right */
                    found = true;
                }
            }

            if (r + 1 < BOARD_HEIGHT) {
                uint32_t pts = simulate_swap(board, r, c, r + 1, c);
                if (pts > best_pts) {
                    best_pts = pts;
                    *hr = (uint8_t)r;
                    *hc = (uint8_t)c;
                    *hd = 1; /* down */
                    found = true;
                }
            }
        }
    }
    return found;
}

/* ================================================================
 *  Props System
 * ================================================================ */

bool model_prop_hammer_smash(GameBoard *board, uint8_t row, uint8_t col) {
    if (!board || row >= BOARD_HEIGHT || col >= BOARD_WIDTH) return false;
    if (board->board[row][col].gem_type == GEM_EMPTY) return false;
    if (board->prop_hammer_count == 0) return false;

    board->board[row][col].gem_type = GEM_EMPTY;
    board->board[row][col].is_marked_for_elimination = true;
    board->prop_hammer_count--;
    board->undo_available = false; /* Clear undo after using board-altering prop */
    return true;
}

bool model_prop_wand_swap(GameBoard *board, uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    if (!board) return false;
    if (r1 >= BOARD_HEIGHT || c1 >= BOARD_WIDTH || r2 >= BOARD_HEIGHT || c2 >= BOARD_WIDTH) return false;
    if (board->board[r1][c1].gem_type == GEM_EMPTY || board->board[r2][c2].gem_type == GEM_EMPTY) return false;
    if (board->prop_wand_count == 0) return false;

    /* Force swap */
    Gem temp = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = temp;

    /* Update logical coordinates and targets */
    board->board[r1][c1].row = r1;
    board->board[r1][c1].col = c1;
    board->board[r1][c1].target_x = gem_target_x(c1);
    board->board[r1][c1].target_y = gem_target_y(r1);

    board->board[r2][c2].row = r2;
    board->board[r2][c2].col = c2;
    board->board[r2][c2].target_x = gem_target_x(c2);
    board->board[r2][c2].target_y = gem_target_y(r2);

    board->prop_wand_count--;
    board->undo_available = false; /* Clear undo after using board-altering prop */
    return true;
}

bool model_prop_shuffle(GameBoard *board) {
    if (!board || board->prop_shuffle_count == 0) return false;

    /* Collect all non-empty gems */
    Gem* gems[BOARD_WIDTH * BOARD_HEIGHT];
    int count = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (board->board[r][c].gem_type != GEM_EMPTY) {
                gems[count++] = &board->board[r][c];
            }
        }
    }

    if (count == 0) return false;

    do {
        /* Fisher-Yates shuffle of the gem types and bomb types */
        for (int i = count - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            
            uint8_t temp_type = gems[i]->gem_type;
            int temp_bomb = gems[i]->bomb_type;
            
            gems[i]->gem_type = gems[j]->gem_type;
            gems[i]->bomb_type = gems[j]->bomb_type;
            
            gems[j]->gem_type = temp_type;
            gems[j]->bomb_type = temp_bomb;
        }
    } while (has_initial_match(board));

    board->prop_shuffle_count--;
    board->undo_available = false; /* Clear undo after using board-altering prop */
    return true;
}

bool model_prop_add_moves(GameBoard *board) {
    if (!board || board->prop_moves_count == 0) return false;
    board->moves_remaining += 5;
    board->prop_moves_count--;
    return true;
}



#include "controller.h"
#include "model.h"
#include "view.h"
#include "types.h"

#include <SDL2/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ================================================================
 *  Module-private state
 * ================================================================ */

#define SAVE_FILE_DEFAULT "match3_save.dat"

static struct {
    bool wants_quit;
    int  difficulty;
} g_ctrl;

/* ================================================================
 *  Lifecycle
 * ================================================================ */

bool controller_init(void)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
    g_ctrl.difficulty  = 1;
    g_ctrl.wants_quit  = false;
    return true;
}

void controller_destroy(void)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
}

bool controller_wants_quit(void)
{
    return g_ctrl.wants_quit;
}

/* ================================================================
 *  Button hit-test helper
 * ================================================================ */

/** @return true if pixel (px,py) falls within a centred button rectangle. */
static bool point_in_button(int px, int py, int cx, int y, int w, int h)
{
    return px >= cx - w / 2 && px <= cx + w / 2 &&
           py >= y          && py <= y + h;
}

/* ================================================================
 *  Keyboard sub-handlers (one per game state)
 * ================================================================ */

static void handle_key_main_menu(GameBoard *board, SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:
            board->highlighted_menu_option =
                (board->highlighted_menu_option - 1 + 2) % 2;
            break;
        case SDLK_DOWN:
            board->highlighted_menu_option =
                (board->highlighted_menu_option + 1) % 2;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (board->highlighted_menu_option == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = 1;
            } else {
                g_ctrl.wants_quit = true;
            }
            break;
        case SDLK_ESCAPE:
            g_ctrl.wants_quit = true;
            break;
        default:
            break;
    }
}

static void handle_key_difficulty(GameBoard *board, SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:
            board->highlighted_difficulty =
                (board->highlighted_difficulty - 1 + 3) % 3;
            break;
        case SDLK_DOWN:
            board->highlighted_difficulty =
                (board->highlighted_difficulty + 1) % 3;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            model_init_board_with_difficulty(board, board->highlighted_difficulty);
            g_ctrl.difficulty = board->highlighted_difficulty;
            view_set_bgm(1);
            view_play_sound_effect("start");
            break;
        case SDLK_ESCAPE:
            board->current_state           = GAME_STATE_MAIN_MENU;
            board->highlighted_menu_option = 0;
            view_set_bgm(0);
            break;
        default:
            break;
    }
}

static void handle_key_paused(GameBoard *board, SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:
            board->highlighted_menu_option =
                (board->highlighted_menu_option - 1 + 3) % 3;
            break;
        case SDLK_DOWN:
            board->highlighted_menu_option =
                (board->highlighted_menu_option + 1) % 3;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            switch (board->highlighted_menu_option) {
                case 0: /* Resume */
                    board->current_state = board->previous_state;
                    view_set_bgm(1);
                    break;
                case 1: /* Restart */
                    board->current_state           = GAME_STATE_DIFFICULTY_SELECTION;
                    board->highlighted_difficulty  = board->difficulty;
                    break;
                case 2: /* Main menu */
                    board->current_state           = GAME_STATE_MAIN_MENU;
                    board->highlighted_menu_option = 0;
                    view_set_bgm(0);
                    break;
                default:
                    break;
            }
            break;
        case SDLK_ESCAPE:
        case SDLK_p:
            board->current_state = board->previous_state;
            break;
        default:
            break;
    }
}

static void handle_key_game_over(GameBoard *board, SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:
            board->highlighted_menu_option =
                (board->highlighted_menu_option - 1 + 2) % 2;
            break;
        case SDLK_DOWN:
            board->highlighted_menu_option =
                (board->highlighted_menu_option + 1) % 2;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (board->highlighted_menu_option == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = board->difficulty;
            } else {
                board->current_state           = GAME_STATE_MAIN_MENU;
                board->highlighted_menu_option = 0;
                view_set_bgm(0);
            }
            break;
        case SDLK_ESCAPE:
            board->current_state           = GAME_STATE_MAIN_MENU;
            board->highlighted_menu_option = 0;
            view_set_bgm(0);
            break;
        default:
            break;
    }
}

static void handle_key_in_game(GameBoard *board, SDL_Keycode key)
{
    switch (key) {
        case SDLK_ESCAPE:
            /* ESC pauses the game and cancels any selection/prop */
            if (board->current_state == GAME_STATE_FIRST_GEM_SELECT ||
                board->current_state == GAME_STATE_PROP_HAMMER_WAITING ||
                board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL ||
                board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL) {
                board->first_gem_selected = false;
                board->current_state = GAME_STATE_WAITING_INPUT;
            }
            controller_toggle_pause(board);
            break;
        case SDLK_p:
            controller_toggle_pause(board);
            break;
        case SDLK_s:
            controller_quick_save(board);
            break;
        case SDLK_l:
            controller_quick_load(board);
            break;
        case SDLK_r:
            board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
            board->highlighted_difficulty = board->difficulty;
            break;
        case SDLK_u:
            if (model_undo_move(board)) {
                board->current_state      = GAME_STATE_SWAP_ANIMATING;
                board->state_timer        = 0.0f;
                board->animation_duration = 0.35f;
                board->animations_settled = false;
            }
            break;
        default:
            break;
    }
}

/* ================================================================
 *  Mouse sub-handlers
 * ================================================================ */

static void handle_mouse_main_menu(GameBoard *board, int mx, int my)
{
    int cx    = WINDOW_WIDTH / 2;
    int btn_y = view_has_badge() ? 330 : 250;
    int btn_space = view_has_badge() ? 80 : 90;
    int btn_h = view_has_badge() ? 60 : 64;

    for (int i = 0; i < 2; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * btn_space, 280, btn_h)) {
            if (i == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = 1;
            } else {
                g_ctrl.wants_quit = true;
            }
        }
    }
}

static void handle_mouse_difficulty(GameBoard *board, int mx, int my)
{
    int cx    = WINDOW_WIDTH / 2;
    int btn_y = 220;

    for (int i = 0; i < 3; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * 100, 300, 74)) {
            model_init_board_with_difficulty(board, i);
            g_ctrl.difficulty = i;
            view_set_bgm(1);
            view_play_sound_effect("start");
        }
    }
}

static void handle_mouse_paused(GameBoard *board, int mx, int my)
{
    int cx    = WINDOW_WIDTH / 2;
    int btn_y = 220;

    for (int i = 0; i < 3; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * 90, 280, 64)) {
            switch (i) {
                case 0:
                    board->current_state = board->previous_state;
                    view_set_bgm(1);
                    break;
                case 1:
                    board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                    board->highlighted_difficulty = board->difficulty;
                    break;
                case 2:
                    board->current_state           = GAME_STATE_MAIN_MENU;
                    board->highlighted_menu_option = 0;
                    view_set_bgm(0);
                    break;
                default: break;
            }
        }
    }
}

static void handle_mouse_game_over(GameBoard *board, int mx, int my)
{
    int cx    = WINDOW_WIDTH / 2;
    int btn_y = 300;

    for (int i = 0; i < 2; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * 90, 280, 64)) {
            if (i == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = board->difficulty;
            } else {
                board->current_state           = GAME_STATE_MAIN_MENU;
                board->highlighted_menu_option = 0;
                view_set_bgm(0);
            }
        }
    }
}

static void handle_mouse_in_game(GameBoard *board, int mx, int my)
{
    if (board->current_state != GAME_STATE_WAITING_INPUT &&
        board->current_state != GAME_STATE_FIRST_GEM_SELECT &&
        board->current_state != GAME_STATE_PROP_HAMMER_WAITING &&
        board->current_state != GAME_STATE_PROP_WAND_FIRST_SEL &&
        board->current_state != GAME_STATE_PROP_WAND_SECOND_SEL &&
        board->current_state != GAME_STATE_PROP_SHUFFLE_CONFIRM &&
        board->current_state != GAME_STATE_PROP_MOVES_CONFIRM)
        return;

    /* Check props clicks */
    int props_y = BOARD_OFFSET_Y + BOARD_HEIGHT * GEM_SIZE + 24;
    int p_size = 50;
    int p_gap = 24;
    int start_x = BOARD_OFFSET_X + (BOARD_WIDTH * GEM_SIZE - (4 * p_size + 3 * p_gap)) / 2;
    
    for (int i = 0; i < 4; i++) {
        int dx = start_x + i * (p_size + p_gap);
        int dy = props_y;
        
        if (mx >= dx && mx <= dx + p_size && my >= dy && my <= dy + p_size) {
            /* Prop clicked */
            if (board->current_state == GAME_STATE_WAITING_INPUT || 
                board->current_state == GAME_STATE_PROP_HAMMER_WAITING || 
                board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL || 
                board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL ||
                board->current_state == GAME_STATE_FIRST_GEM_SELECT ||
                board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM ||
                board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
                
                /* Cancel if clicking the already selected prop */
                if (i == 0 && board->current_state == GAME_STATE_PROP_HAMMER_WAITING) {
                    board->current_state = GAME_STATE_WAITING_INPUT;
                    view_play_sound_effect("swap");
                    return;
                }
                if (i == 1 && (board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL || board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL)) {
                    board->current_state = GAME_STATE_WAITING_INPUT;
                    board->first_gem_selected = false;
                    view_play_sound_effect("swap");
                    return;
                }
                /* Execute instant props if they were already in confirm state */
                if (i == 2 && board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM) {
                    board->current_state = GAME_STATE_WAITING_INPUT;
                    if (model_prop_shuffle(board)) view_play_sound_effect("clear");
                    return;
                }
                if (i == 3 && board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
                    board->current_state = GAME_STATE_WAITING_INPUT;
                    if (model_prop_add_moves(board)) view_play_sound_effect("start");
                    return;
                }
                
                /* Otherwise select or use the prop */
                if (i == 0 && board->prop_hammer_count > 0) {
                    board->current_state = GAME_STATE_PROP_HAMMER_WAITING;
                    board->first_gem_selected = false;
                    view_play_sound_effect("swap");
                } else if (i == 1 && board->prop_wand_count > 0) {
                    board->current_state = GAME_STATE_PROP_WAND_FIRST_SEL;
                    board->first_gem_selected = false;
                    view_play_sound_effect("swap");
                } else if (i == 2 && board->prop_shuffle_count > 0) {
                    board->current_state = GAME_STATE_PROP_SHUFFLE_CONFIRM;
                    board->first_gem_selected = false;
                    view_play_sound_effect("swap");
                } else if (i == 3 && board->prop_moves_count > 0) {
                    board->current_state = GAME_STATE_PROP_MOVES_CONFIRM;
                    board->first_gem_selected = false;
                    view_play_sound_effect("swap");
                }
            }
            return;
        }
    }

    uint8_t row, col;
    if (!model_screen_to_board_coord(mx, my, &row, &col)) {
        if (board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM || board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
            board->current_state = GAME_STATE_WAITING_INPUT;
        }
        return;
    }

    if (board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM || board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
        board->current_state = GAME_STATE_WAITING_INPUT;
        return;
    }

    if (board->current_state == GAME_STATE_PROP_HAMMER_WAITING) {
        if (model_prop_hammer_smash(board, row, col)) {
            board->current_state = GAME_STATE_ELIMINATING;
            board->state_timer = 0.0f;
            board->animation_duration = 0.35f;
            board->animations_settled = false;
            view_play_sound_effect("pao");
        }
        return;
    }

    if (board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL) {
        board->selected_row = row;
        board->selected_col = col;
        board->first_gem_selected = true;
        board->current_state = GAME_STATE_PROP_WAND_SECOND_SEL;
        return;
    }

    if (board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL) {
        if (row == board->selected_row && col == board->selected_col) {
            board->first_gem_selected = false;
            board->current_state = GAME_STATE_WAITING_INPUT;
            return;
        }
        if (model_is_adjacent(board, row, col)) {
            if (model_prop_wand_swap(board, board->selected_row, board->selected_col, row, col)) {
                board->undo_available = false;
                board->first_gem_selected = false;
                board->current_state = GAME_STATE_SWAP_ANIMATING;
                board->state_timer = 0.0f;
                board->animation_duration = 0.35f;
                board->animations_settled = false;
                view_play_sound_effect("clear");
            }
        }
        return;
    }

    if (board->current_state == GAME_STATE_WAITING_INPUT) {
        board->selected_row       = row;
        board->selected_col       = col;
        board->first_gem_selected = true;
        board->current_state      = GAME_STATE_FIRST_GEM_SELECT;
        return;
    }

    /* GAME_STATE_FIRST_GEM_SELECT */
    if (row == board->selected_row && col == board->selected_col) {
        /* Deselect */
        board->first_gem_selected = false;
        board->current_state      = GAME_STATE_WAITING_INPUT;
        return;
    }

    if (model_is_adjacent(board, row, col)) {
        /* Record undo state before the swap */
        board->undo_r1    = board->selected_row;
        board->undo_c1    = board->selected_col;
        board->undo_r2    = row;
        board->undo_c2    = col;
        board->undo_score = board->score;
        board->undo_combo = board->combo_multiplier;

        bool swapped = model_swap_gems(board,
                                       board->selected_row, board->selected_col,
                                       row, col);
        board->first_gem_selected = false;

        if (swapped) {
            board->undo_available     = true;
            board->current_state      = GAME_STATE_SWAP_ANIMATING;
            board->state_timer        = 0.0f;
            board->animation_duration = 0.35f;
            board->animations_settled = false;
            view_play_sound_effect("swap");
        } else {
            board->undo_available     = false; /* failed swap — nothing to undo */
            board->current_state      = GAME_STATE_SWAP_FAIL_ANIMATING;
            board->state_timer        = 0.0f;
            board->animation_duration = 0.25f;
            view_play_sound_effect("error");

            /* Nudge the gems towards each other to animate a bounce back */
            Gem *g1 = &board->board[board->selected_row][board->selected_col];
            Gem *g2 = &board->board[row][col];
            
            float target_x1 = g1->target_x;
            float target_y1 = g1->target_y;
            float target_x2 = g2->target_x;
            float target_y2 = g2->target_y;
            
            g1->screen_x = target_x1 + (target_x2 - target_x1) * 0.4f;
            g1->screen_y = target_y1 + (target_y2 - target_y1) * 0.4f;
            g2->screen_x = target_x2 + (target_x1 - target_x2) * 0.4f;
            g2->screen_y = target_y2 + (target_y1 - target_y2) * 0.4f;
        }
    } else {
        /* Re-select new gem */
        board->selected_row = row;
        board->selected_col = col;
    }
}

/* ================================================================
 *  Public event dispatcher
 * ================================================================ */

void controller_handle_event(GameBoard *board, const SDL_Event *event)
{
    if (!board || !event)
        return;

    /* Any user interaction resets the idle hint */
    if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEMOTION) {
        board->idle_timer = 0.0f;
        board->has_hint = false;
    }

    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;

        switch (board->current_state) {
            case GAME_STATE_MAIN_MENU:
                handle_key_main_menu(board, key);
                break;
            case GAME_STATE_DIFFICULTY_SELECTION:
                handle_key_difficulty(board, key);
                break;
            case GAME_STATE_PAUSED:
                handle_key_paused(board, key);
                break;
            case GAME_STATE_GAME_OVER:
                handle_key_game_over(board, key);
                break;
            default:
                handle_key_in_game(board, key);
                break;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_RIGHT) {
            if (board->current_state == GAME_STATE_FIRST_GEM_SELECT ||
                board->current_state == GAME_STATE_PROP_HAMMER_WAITING ||
                board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL ||
                board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL) {
                board->first_gem_selected = false;
                board->current_state = GAME_STATE_WAITING_INPUT;
            }
        } else if (event->button.button == SDL_BUTTON_LEFT) {
            int mx = event->button.x;
            int my = event->button.y;

        switch (board->current_state) {
            case GAME_STATE_MAIN_MENU:
                handle_mouse_main_menu(board, mx, my);
                break;
            case GAME_STATE_DIFFICULTY_SELECTION:
                handle_mouse_difficulty(board, mx, my);
                break;
            case GAME_STATE_PAUSED:
                handle_mouse_paused(board, mx, my);
                break;
            case GAME_STATE_GAME_OVER:
                handle_mouse_game_over(board, mx, my);
                break;
            default:
                handle_mouse_in_game(board, mx, my);
                break;
        }
        }
    }
    if (event->type == SDL_MOUSEMOTION) {
        int mx = event->motion.x;
        int my = event->motion.y;
        uint8_t row, col;
        if (model_screen_to_board_coord(mx, my, &row, &col)) {
            board->hover_row = row;
            board->hover_col = col;
        } else {
            board->hover_row = 255;
            board->hover_col = 255;
        }
    }
}

/* ================================================================
 *  State-machine update  (called once per frame)
 * ================================================================ */

bool controller_update_state_machine(GameBoard *board, float dt)
{
    if (!board)
        return false;

    board->state_timer += dt;
    bool changed = false;

    switch (board->current_state) {

        case GAME_STATE_WAITING_INPUT:
        case GAME_STATE_FIRST_GEM_SELECT:
            /* No timer-based transitions in these states. */
            board->idle_timer += dt;
            if (board->idle_timer > 5.0f && !board->has_hint) {
                if (model_find_best_hint(board, &board->hint_r, &board->hint_c, &board->hint_dir)) {
                    board->has_hint = true;
                } else {
                    board->idle_timer = 0.0f; /* Retry later if no hint? (Shouldn't happen unless deadlock) */
                }
            }
            break;

        case GAME_STATE_SWAP_ANIMATING:
            if (view_all_gems_settled(board) ||
                board->state_timer >= board->animation_duration) {
                board->current_state    = GAME_STATE_ELIMINATION_CHECK;
                board->combo_multiplier = 1;
                board->state_timer      = 0.0f;
                changed = true;
            }
            break;

        case GAME_STATE_SWAP_FAIL_ANIMATING:
            if (board->state_timer >= board->animation_duration) {
                board->current_state = GAME_STATE_WAITING_INPUT;
                board->state_timer   = 0.0f;
                changed = true;
            }
            break;

        case GAME_STATE_ELIMINATION_CHECK: {
            EliminationSet es;
            uint32_t pts = model_check_eliminations_advanced(board, &es);

            if (es.count > 0) {
                board->score += pts;
                if (board->score > board->high_score)
                    board->high_score = board->score;

                /* Trigger any bomb chains and spawn particles */
                for (int i = 0; i < es.count; i++) {
                    uint8_t br = es.positions[i].row;
                    uint8_t bc = es.positions[i].col;
                    Gem *g = &board->board[br][bc];
                    if (g->bomb_type != BOMB_NONE)
                        model_trigger_bomb_chain(board, br, bc);
                    
                    view_spawn_particles(g->screen_x, g->screen_y, g->gem_type);
                }

                board->current_state      = GAME_STATE_ELIMINATING;
                board->animation_duration = 0.4f;
                board->state_timer        = 0.0f;
                board->animations_settled = false;

                /* Play sound based on combo count; trigger popup */
                if (board->combo_multiplier > 2) {
                    view_play_sound_effect("combo");
                    board->combo_popup_value = board->combo_multiplier;
                    board->combo_popup_timer = 1.5f; /* seconds to display */
                } else {
                    view_play_sound_effect("match");
                }
            } else {
                /* No match — deduct move, check end-of-game */
                if (board->moves_remaining > 0)
                    board->moves_remaining--;

                if (board->moves_remaining == 0 || model_is_deadlock(board)) {
                    board->current_state = GAME_STATE_GAME_OVER;

                    /* Economy System: Reward coins */
                    uint32_t earned = board->score / 100;
                    if (board->combo_multiplier >= 5) earned += 20;
                    board->total_coins += earned;

                    view_play_sound_effect("game_over");
                    controller_quick_save(board);
                } else {
                    board->current_state = GAME_STATE_WAITING_INPUT;
                }
                board->state_timer = 0.0f;
            }
            changed = true;
            break;
        }

        case GAME_STATE_ELIMINATING:
            if (board->state_timer >= board->animation_duration) {
                model_apply_eliminations(board);
                model_apply_gravity(board);

                board->current_state      = GAME_STATE_GRAVITY_APPLY;
                board->animation_duration = 0.5f;
                board->state_timer        = 0.0f;
                board->animations_settled = false;
                changed = true;
            }
            break;

        case GAME_STATE_GRAVITY_APPLY:
            if (view_all_gems_settled(board) ||
                board->state_timer >= board->animation_duration) {
                model_refill_board(board);

                board->current_state      = GAME_STATE_REFILL;
                board->animation_duration = 0.4f;
                board->state_timer        = 0.0f;
                board->animations_settled = false;
                changed = true;
            }
            break;

        case GAME_STATE_REFILL:
            if (view_all_gems_settled(board) ||
                board->state_timer >= board->animation_duration) {
                board->combo_multiplier++;
                board->current_state = GAME_STATE_ELIMINATION_CHECK;
                board->state_timer   = 0.0f;
                changed = true;
            }
            break;

        case GAME_STATE_GAME_OVER:
        case GAME_STATE_PAUSED:
        default:
            break;
    }

    return changed;
}

/* ================================================================
 *  Game control helpers
 * ================================================================ */

bool controller_restart_game(GameBoard *board)
{
    if (!board)
        return false;
    model_destroy_board(board);
    return model_init_board(board);
}

bool controller_toggle_pause(GameBoard *board)
{
    if (!board)
        return false;

    if (board->current_state == GAME_STATE_PAUSED) {
        board->current_state = board->previous_state;
        view_set_bgm(1);
    } else {
        board->previous_state          = board->current_state;
        board->current_state           = GAME_STATE_PAUSED;
        board->highlighted_menu_option = 0;
        view_set_bgm(0);
    }
    return board->current_state == GAME_STATE_PAUSED;
}

bool controller_save_game(const GameBoard *board, const char *filename)
{
    return model_save_game(board, filename ? filename : SAVE_FILE_DEFAULT);
}

bool controller_load_game(GameBoard *board, const char *filename)
{
    return model_load_game(board, filename ? filename : SAVE_FILE_DEFAULT);
}

bool controller_quick_save(const GameBoard *board)
{
    return controller_save_game(board, SAVE_FILE_DEFAULT);
}

bool controller_quick_load(GameBoard *board)
{
    return controller_load_game(board, SAVE_FILE_DEFAULT);
}

void controller_set_difficulty(int d)
{
    if (d >= 0 && d <= 2)
        g_ctrl.difficulty = d;
}

int controller_get_difficulty(void)
{
    return g_ctrl.difficulty;
}

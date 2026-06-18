
#include "controller.h"
#include "model.h"
#include "view.h"
#include "types.h"

#include <SDL2/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SAVE_FILE_DEFAULT "match3_save.dat"

/* 控制器私有状态 */
static struct {
    bool wants_quit;
    int  difficulty;
} g_ctrl;

bool controller_init(void)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
    g_ctrl.difficulty = 1;
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

/* 判断鼠标点是否在一个以 (cx, y) 为左上基准的矩形按钮内 */
static bool point_in_button(int px, int py, int cx, int y, int w, int h)
{
    return px >= cx - w / 2 && px <= cx + w / 2 &&
           py >= y          && py <= y + h;
}

/* 把"是否死局"的判断结果缓存到 board 里，避免同帧多次全盘扫描 */
static bool deadlock_cached(GameBoard *board)
{
    return model_is_deadlock(board);
}

/* 死局复活逻辑：消耗一张洗牌道具（或用金币购买）来复活，成功返回 true */
static bool try_revive_from_deadlock(GameBoard *board)
{
    if (!board) return false;

    bool has_shuffle = (board->prop_shuffle_count > 0);
    bool can_buy     = (board->total_coins >= board->buy_prop_price);

    if (has_shuffle) {
        model_prop_shuffle(board);
    } else if (can_buy) {
        board->total_coins -= board->buy_prop_price;
        board->prop_shuffle_count++;
        model_prop_shuffle(board);
    } else {
        view_play_sound_effect("error");
        return false;
    }

    if (board->moves_remaining == 0)
        board->moves_remaining = 3;
    board->current_state = GAME_STATE_WAITING_INPUT;
    view_play_sound_effect("start");
    return true;
}

/* ---- 键盘处理（每个界面一个函数） ---- */

static void handle_key_main_menu(GameBoard *board, SDL_Keycode key)
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
            if (board->highlighted_menu_option == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = 1;
            } else if (board->highlighted_menu_option == 1) {
                board->current_state = GAME_STATE_RULES;
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

static void handle_key_rules(GameBoard *board, SDL_Keycode key)
{
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE) {
        board->current_state = GAME_STATE_MAIN_MENU;
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
            if (board->highlighted_difficulty <= board->unlocked_difficulty) {
                model_init_board_with_difficulty(board, board->highlighted_difficulty);
                g_ctrl.difficulty = board->highlighted_difficulty;
                view_set_bgm(1);
                view_play_sound_effect("start");
            } else {
                view_play_sound_effect("error");
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
                default:
                    break;
            }
            break;
        case SDLK_ESCAPE:
        case SDLK_p:
            board->current_state = board->previous_state;
            view_set_bgm(1);
            break;
        default:
            break;
    }
}

static void handle_key_game_over(GameBoard *board, SDL_Keycode key)
{
    bool is_dead = deadlock_cached(board);
    int  num_opts = is_dead ? 3 : 2;

    switch (key) {
        case SDLK_UP:
            board->highlighted_menu_option =
                (board->highlighted_menu_option - 1 + num_opts) % num_opts;
            break;
        case SDLK_DOWN:
            board->highlighted_menu_option =
                (board->highlighted_menu_option + 1) % num_opts;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: {
            int opt = board->highlighted_menu_option;
            if (is_dead) {
                if (opt == 0) {
                    try_revive_from_deadlock(board);
                    break;
                }
                opt--; /* 偏移：选项0是复活，1才是"再来一局" */
            }
            if (opt == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = board->difficulty;
            } else {
                board->current_state           = GAME_STATE_MAIN_MENU;
                board->highlighted_menu_option = 0;
                view_set_bgm(0);
            }
            break;
        }
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
            /* 按 ESC 先取消选中/道具状态，再暂停 */
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

/* ---- 鼠标处理 ---- */

static void handle_hover_main_menu(GameBoard *board, int mx, int my)
{
    int cx        = WINDOW_WIDTH / 2;
    int btn_y     = view_has_badge() ? 330 : 250;
    int btn_space = view_has_badge() ? 80  : 90;
    int btn_h     = view_has_badge() ? 60  : 64;

    board->highlighted_menu_option = -1;
    for (int i = 0; i < 3; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * btn_space, 280, btn_h))
            board->highlighted_menu_option = i;
    }
}

static void handle_hover_rules(GameBoard *board, int mx, int my)
{
    int cx = WINDOW_WIDTH / 2;
    board->highlighted_menu_option = -1;
    if (point_in_button(mx, my, cx, 750, 280, 64))
        board->highlighted_menu_option = 0;
}

static void handle_mouse_main_menu(GameBoard *board, int mx, int my)
{
    int cx        = WINDOW_WIDTH / 2;
    int btn_y     = view_has_badge() ? 330 : 250;
    int btn_space = view_has_badge() ? 80  : 90;
    int btn_h     = view_has_badge() ? 60  : 64;

    for (int i = 0; i < 3; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * btn_space, 280, btn_h)) {
            if (i == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = 1;
            } else if (i == 1) {
                board->current_state = GAME_STATE_RULES;
            } else {
                g_ctrl.wants_quit = true;
            }
        }
    }
}

static void handle_mouse_rules(GameBoard *board, int mx, int my)
{
    int cx = WINDOW_WIDTH / 2;
    if (point_in_button(mx, my, cx, 750, 280, 64))
        board->current_state = GAME_STATE_MAIN_MENU;
}

static void handle_mouse_difficulty(GameBoard *board, int mx, int my)
{
    int cx    = WINDOW_WIDTH / 2;
    int btn_y = 220;

    for (int i = 0; i < 3; i++) {
        if (point_in_button(mx, my, cx, btn_y + i * 100, 300, 74)) {
            if (i <= board->unlocked_difficulty) {
                model_init_board_with_difficulty(board, i);
                g_ctrl.difficulty = i;
                view_set_bgm(1);
                view_play_sound_effect("start");
            } else {
                view_play_sound_effect("error");
            }
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
    int  cx       = WINDOW_WIDTH / 2;
    int  btn_y    = 330;
    bool is_dead  = deadlock_cached(board);
    int  num_opts = is_dead ? 3 : 2;

    for (int i = 0; i < num_opts; i++) {
        if (!point_in_button(mx, my, cx, btn_y + i * 80, 280, 64))
            continue;

        if (is_dead && i == 0) {
            try_revive_from_deadlock(board);
        } else {
            int opt = is_dead ? (i - 1) : i;
            if (opt == 0) {
                board->current_state          = GAME_STATE_DIFFICULTY_SELECTION;
                board->highlighted_difficulty = board->difficulty;
            } else {
                board->current_state           = GAME_STATE_MAIN_MENU;
                board->highlighted_menu_option = 0;
                view_set_bgm(0);
            }
        }
        break;
    }
}

static void handle_mouse_in_game(GameBoard *board, int mx, int my)
{
    if (board->current_state != GAME_STATE_WAITING_INPUT       &&
        board->current_state != GAME_STATE_FIRST_GEM_SELECT    &&
        board->current_state != GAME_STATE_PROP_HAMMER_WAITING &&
        board->current_state != GAME_STATE_PROP_WAND_FIRST_SEL &&
        board->current_state != GAME_STATE_PROP_WAND_SECOND_SEL &&
        board->current_state != GAME_STATE_PROP_SHUFFLE_CONFIRM &&
        board->current_state != GAME_STATE_PROP_MOVES_CONFIRM)
        return;

    /* 道具栏命中检测 */
    int props_y = BOARD_OFFSET_Y + BOARD_HEIGHT * GEM_SIZE + 24;
    int p_size  = 50;
    int p_gap   = 24;
    int start_x = BOARD_OFFSET_X + (BOARD_WIDTH * GEM_SIZE - (4 * p_size + 3 * p_gap)) / 2;

    for (int i = 0; i < 4; i++) {
        int dx = start_x + i * (p_size + p_gap);
        int dy = props_y;

        if (!(mx >= dx && mx <= dx + p_size && my >= dy && my <= dy + p_size))
            continue;

        /* 再次点击同一道具 → 取消 */
        if (i == 0 && board->current_state == GAME_STATE_PROP_HAMMER_WAITING) {
            board->current_state = GAME_STATE_WAITING_INPUT;
            view_play_sound_effect("swap");
            return;
        }
        if (i == 1 && (board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL ||
                       board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL)) {
            board->current_state      = GAME_STATE_WAITING_INPUT;
            board->first_gem_selected = false;
            view_play_sound_effect("swap");
            return;
        }
        /* 道具在确认状态下再次点击 → 立即执行 */
        if (i == 2 && board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM) {
            board->current_state = GAME_STATE_WAITING_INPUT;
            if (model_prop_shuffle(board)) {
                board->used_props_total++;
                board->level |= 2; /* 标记本局用过洗牌 */
                view_play_sound_effect("clear");
            }
            return;
        }
        if (i == 3 && board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
            board->current_state = GAME_STATE_WAITING_INPUT;
            if (model_prop_add_moves(board)) {
                board->used_props_total++;
                board->used_sandglass_count++;
                view_play_sound_effect("start");
            }
            return;
        }

        /* 道具数为零时，尝试用金币购买 */
        uint8_t *counts[] = {
            &board->prop_hammer_count, &board->prop_wand_count,
            &board->prop_shuffle_count, &board->prop_moves_count
        };
        static const uint32_t PRICES[] = {50, 80, 100, 150};

        if (*counts[i] == 0) {
            if (board->total_coins >= PRICES[i]) {
                board->total_coins -= PRICES[i];
                (*counts[i])++;
                view_play_sound_effect("match");
            } else {
                view_play_sound_effect("error");
                return;
            }
        }

        /* 使用次数上限检查 */
        if (board->used_props_total >= board->max_props_per_game) {
            view_play_sound_effect("error");
            return;
        }
        if (i == 3 && board->used_sandglass_count >= board->max_sandglass_per_game) {
            view_play_sound_effect("error");
            return;
        }
        /* 困难模式：洗牌和沙漏互斥 */
        if (board->difficulty == 2) {
            if (i == 2 && (board->level & 2))              { view_play_sound_effect("error"); return; }
            if (i == 3 && board->used_sandglass_count > 0) { view_play_sound_effect("error"); return; }
        }

        /* 激活道具 */
        if (i == 0) {
            board->current_state      = GAME_STATE_PROP_HAMMER_WAITING;
            board->first_gem_selected = false;
            view_play_sound_effect("swap");
        } else if (i == 1) {
            board->current_state      = GAME_STATE_PROP_WAND_FIRST_SEL;
            board->first_gem_selected = false;
            view_play_sound_effect("swap");
        } else if (i == 2) {
            board->current_state      = GAME_STATE_PROP_SHUFFLE_CONFIRM;
            board->first_gem_selected = false;
            view_play_sound_effect("swap");
        } else if (i == 3) {
            board->current_state      = GAME_STATE_PROP_MOVES_CONFIRM;
            board->first_gem_selected = false;
            view_play_sound_effect("swap");
        }
        return;
    }

    uint8_t row, col;
    if (!model_screen_to_board_coord(mx, my, &row, &col)) {
        /* 点到棋盘外：取消待确认的道具 */
        if (board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM ||
            board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
            board->current_state = GAME_STATE_WAITING_INPUT;
        }
        return;
    }

    if (board->current_state == GAME_STATE_PROP_SHUFFLE_CONFIRM ||
        board->current_state == GAME_STATE_PROP_MOVES_CONFIRM) {
        board->current_state = GAME_STATE_WAITING_INPUT;
        return;
    }

    if (board->current_state == GAME_STATE_PROP_HAMMER_WAITING) {
        int result = model_prop_hammer_smash(board, row, col);
        if (result > 0) {
            board->used_props_total++;
            board->current_state      = GAME_STATE_ELIMINATING;
            board->state_timer        = 0.0f;
            board->animation_duration = 0.35f;
            board->animations_settled = false;
            view_play_sound_effect(result == 1 ? "clear" : "pao");
        }
        return;
    }

    if (board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL) {
        board->selected_row       = row;
        board->selected_col       = col;
        board->first_gem_selected = true;
        board->current_state      = GAME_STATE_PROP_WAND_SECOND_SEL;
        return;
    }

    if (board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL) {
        if (row == board->selected_row && col == board->selected_col) {
            board->first_gem_selected = false;
            board->current_state      = GAME_STATE_WAITING_INPUT;
            return;
        }
        if (model_is_adjacent(board, row, col)) {
            if (model_prop_wand_swap(board, board->selected_row, board->selected_col, row, col)) {
                board->used_props_total++;
                board->undo_available     = false;
                board->first_gem_selected = false;
                board->current_state      = GAME_STATE_SWAP_ANIMATING;
                board->state_timer        = 0.0f;
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

    /* 已选中第一颗宝石后的处理 */
    if (row == board->selected_row && col == board->selected_col) {
        board->first_gem_selected = false;
        board->current_state      = GAME_STATE_WAITING_INPUT;
        return;
    }

    if (model_is_adjacent(board, row, col)) {
        board->undo_r1    = board->selected_row;
        board->undo_c1    = board->selected_col;
        board->undo_r2    = row;
        board->undo_c2    = col;
        board->undo_score = board->score;
        board->undo_combo = board->combo_multiplier;

        bool swapped = model_swap_gems(board, board->selected_row, board->selected_col, row, col);
        board->first_gem_selected = false;

        if (swapped) {
            board->undo_available     = true;
            board->current_state      = GAME_STATE_SWAP_ANIMATING;
            board->state_timer        = 0.0f;
            board->animation_duration = 0.35f;
            board->animations_settled = false;
            view_play_sound_effect("swap");
        } else {
            board->undo_available     = false;
            board->current_state      = GAME_STATE_SWAP_FAIL_ANIMATING;
            board->state_timer        = 0.0f;
            board->animation_duration = 0.25f;
            view_play_sound_effect("error");

            /* 让两颗宝石互相弹一下，表示交换失败 */
            Gem *g1 = &board->board[board->selected_row][board->selected_col];
            Gem *g2 = &board->board[row][col];
            g1->screen_x = g1->target_x + (g2->target_x - g1->target_x) * 0.4f;
            g1->screen_y = g1->target_y + (g2->target_y - g1->target_y) * 0.4f;
            g2->screen_x = g2->target_x + (g1->target_x - g2->target_x) * 0.4f;
            g2->screen_y = g2->target_y + (g1->target_y - g2->target_y) * 0.4f;
        }
    } else {
        /* 点了不相邻的格子 → 重新选 */
        board->selected_row = row;
        board->selected_col = col;
    }
}

/* ---- 事件分发 ---- */

void controller_handle_event(GameBoard *board, const SDL_Event *event)
{
    if (!board || !event)
        return;

    /* 有任何操作就重置空闲提示计时 */
    if (event->type == SDL_KEYDOWN ||
        event->type == SDL_MOUSEBUTTONDOWN ||
        event->type == SDL_MOUSEMOTION) {
        board->idle_timer = 0.0f;
        board->has_hint   = false;
    }

    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        switch (board->current_state) {
            case GAME_STATE_MAIN_MENU:           handle_key_main_menu(board, key);  break;
            case GAME_STATE_DIFFICULTY_SELECTION: handle_key_difficulty(board, key); break;
            case GAME_STATE_RULES:               handle_key_rules(board, key);       break;
            case GAME_STATE_PAUSED:              handle_key_paused(board, key);      break;
            case GAME_STATE_GAME_OVER:           handle_key_game_over(board, key);   break;
            default:                             handle_key_in_game(board, key);     break;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_RIGHT) {
            /* 右键随时取消选中/道具 */
            if (board->current_state == GAME_STATE_FIRST_GEM_SELECT    ||
                board->current_state == GAME_STATE_PROP_HAMMER_WAITING ||
                board->current_state == GAME_STATE_PROP_WAND_FIRST_SEL ||
                board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL) {
                board->first_gem_selected = false;
                board->current_state      = GAME_STATE_WAITING_INPUT;
            }
        } else if (event->button.button == SDL_BUTTON_LEFT) {
            int mx = event->button.x;
            int my = event->button.y;
            switch (board->current_state) {
                case GAME_STATE_MAIN_MENU:            handle_mouse_main_menu(board, mx, my);  break;
                case GAME_STATE_DIFFICULTY_SELECTION: handle_mouse_difficulty(board, mx, my); break;
                case GAME_STATE_RULES:                handle_mouse_rules(board, mx, my);      break;
                case GAME_STATE_PAUSED:               handle_mouse_paused(board, mx, my);     break;
                case GAME_STATE_GAME_OVER:            handle_mouse_game_over(board, mx, my);  break;
                default:                              handle_mouse_in_game(board, mx, my);    break;
            }
        }
    }

    if (event->type == SDL_MOUSEMOTION) {
        int mx = event->motion.x;
        int my = event->motion.y;
        switch (board->current_state) {
            case GAME_STATE_MAIN_MENU: handle_hover_main_menu(board, mx, my); break;
            case GAME_STATE_RULES:     handle_hover_rules(board, mx, my);     break;
            default: {
                uint8_t row, col;
                if (model_screen_to_board_coord(mx, my, &row, &col)) {
                    board->hover_row = row;
                    board->hover_col = col;
                } else {
                    board->hover_row = 255;
                    board->hover_col = 255;
                }
                break;
            }
        }
    }
}

/* 结算得星，奖励金币，更新连败计数 */
static void do_game_settlement(GameBoard *board)
{
    /* 算星级 */
    if (board->score >= board->target_score * 2 && board->used_props_total == 0)
        board->stars_earned = 3;
    else if (board->score >= board->target_score + board->target_score / 2)
        board->stars_earned = 2;
    else if (board->score >= board->target_score)
        board->stars_earned = 1;
    else
        board->stars_earned = 0;

    /* 根据星级更新连败 / 解锁进度 */
    if (board->stars_earned > 0) {
        if (board->difficulty == 1) board->consecutive_fails_normal = 0;
        if (board->difficulty == 2) board->consecutive_fails_hard   = 0;
        if (board->difficulty == board->unlocked_difficulty && board->unlocked_difficulty < 2)
            board->unlocked_difficulty++;
    } else {
        if (board->difficulty == 1) board->consecutive_fails_normal++;
        if (board->difficulty == 2) board->consecutive_fails_hard++;
    }

    /* 奖励金币：得分/100，连击达到5以上再加20 */
    uint32_t earned = board->score / 100;
    if (board->max_combo_this_game >= 5) earned += 20;
    board->total_coins += earned;

    view_play_sound_effect("game_over");
    controller_quick_save(board);
}

/* ---- 状态机主循环（每帧调用一次） ---- */

bool controller_update_state_machine(GameBoard *board, float dt)
{
    if (!board)
        return false;

    board->state_timer += dt;
    bool changed = false;

    switch (board->current_state) {

        case GAME_STATE_WAITING_INPUT:
        case GAME_STATE_FIRST_GEM_SELECT:
            board->idle_timer += dt;
            if (board->idle_timer > board->hint_trigger_time && !board->has_hint) {
                if (model_find_best_hint(board, &board->hint_r, &board->hint_c, &board->hint_dir))
                    board->has_hint = true;
                else
                    board->idle_timer = 0.0f;
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

                if (board->combo_multiplier > 2) {
                    view_play_sound_effect("combo");
                    board->combo_popup_value = board->combo_multiplier;
                    board->combo_popup_timer = 1.5f;
                } else {
                    view_play_sound_effect("match");
                }
            } else {
                /* 本轮没有消除：更新最高连击，扣步数 */
                if (board->combo_multiplier > 1 &&
                    board->combo_multiplier - 1 > board->max_combo_this_game) {
                    board->max_combo_this_game = board->combo_multiplier - 1;
                }

                if (board->moves_remaining > 0)
                    board->moves_remaining--;

                if (board->moves_remaining == 0) {
                    /* 步数用完，正常结束 */
                    board->current_state = GAME_STATE_GAME_OVER;
                    do_game_settlement(board);
                } else if (model_is_deadlock(board)) {
                    /* 还有步数但死局了：进 DEAD END 动画，然后结算 */
                    view_play_sound_effect("error");
                    board->current_state      = GAME_STATE_DEAD_END_ANIM;
                    board->animation_duration = 2.0f;
                    board->state_timer        = 0.0f;
                    board->highlighted_menu_option = 0;
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

        case GAME_STATE_DEAD_END_ANIM:
            if (board->state_timer >= board->animation_duration) {
                /* 动画结束后做结算，然后进结束界面 */
                do_game_settlement(board);
                board->current_state = GAME_STATE_GAME_OVER;
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

/* ---- 游戏控制工具函数 ---- */

bool controller_restart_game(GameBoard *board)
{
    if (!board) return false;
    model_destroy_board(board);
    return model_init_board(board);
}

bool controller_toggle_pause(GameBoard *board)
{
    if (!board) return false;

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

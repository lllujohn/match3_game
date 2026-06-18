#include "model.h"
#include "types.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* 把逻辑格坐标转成屏幕像素坐标（中心点） */

static inline float gem_target_x(uint8_t col)
{
    return (float)BOARD_OFFSET_X + ((float)col * (float)GEM_SIZE) + ((float)GEM_SIZE / 2.0f);
}

static inline float gem_target_y(uint8_t row)
{
    return (float)BOARD_OFFSET_Y + ((float)row * (float)GEM_SIZE) + ((float)GEM_SIZE / 2.0f);
}

/* 检查棋盘上有没有已经成立的消除（生成棋盘时用来重新洗牌） */
static bool has_initial_match(const GameBoard *board)
{
    /* 横向扫描 */
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
    /* 纵向扫描 */
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

/* 随机生成一颗宝石。万能牌不会在这里随机出现，只有凑5连消才会触发 */
Gem model_generate_gem(GameBoard *board, uint8_t row, uint8_t col, bool offscreen_spawn)
{
    (void)board;
    Gem gem;
    memset(&gem, 0, sizeof(Gem));

    gem.row      = row;
    gem.col      = col;
    gem.gem_type = (uint8_t)(rand() % 5); /* 随机五种普通颜色之一 */
    
    gem.bomb_type = BOMB_NONE;
    gem.is_stone = false;
    gem.has_ice = false;
    gem.is_marked_for_elimination = false;
    gem.animation_progress        = 0.0f;
    gem.elim_scale                = 1.0f;

    gem.target_x = gem_target_x(col);
    gem.target_y = gem_target_y(row);

    if (offscreen_spawn) {
        /* 从屏幕上方落入，制造掉落动画 */
        gem.screen_x = gem.target_x;
        gem.screen_y = gem.target_y - (float)(BOARD_HEIGHT * GEM_SIZE);
    } else {
        gem.screen_x = gem.target_x;
        gem.screen_y = gem.target_y;
    }

    return gem;
}

/* 初始化棋盘（游戏第一次启动时用，停在主菜单界面） */
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

    /* 一直重新生成，直到初始棋盘上没有现成的消除 */
    do {
        for (int r = 0; r < BOARD_HEIGHT; r++)
            for (int c = 0; c < BOARD_WIDTH; c++)
                board->board[r][c] =
                    model_generate_gem(board, (uint8_t)r, (uint8_t)c, false);
    } while (has_initial_match(board) || model_is_deadlock(board));

    return true;
}

void model_destroy_board(GameBoard *board)
{
    if (board)
        memset(board, 0, sizeof(GameBoard));
}

static uint32_t simulate_swap(GameBoard *board, int r1, int c1, int r2, int c2);

static int count_valid_moves(GameBoard *board) {
    int count = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (c + 1 < BOARD_WIDTH && simulate_swap(board, r, c, r, c + 1) > 0) count++;
            if (r + 1 < BOARD_HEIGHT && simulate_swap(board, r, c, r + 1, c) > 0) count++;
        }
    }
    return count;
}

static void apply_difficulty_configs(GameBoard *board) {
    if (board->difficulty == 0) {
        board->target_score = 3000;
        board->max_props_per_game = 99;
        board->max_sandglass_per_game = 99;
        board->hint_trigger_time = 3.0f;
        board->wildcard_prob = 0.08f;
        board->buy_prop_price = 100;
        board->moves_remaining = EASY_MOVES;
    } else if (board->difficulty == 1) {
        board->target_score = 5000;
        board->max_props_per_game = 5;
        board->max_sandglass_per_game = 2;
        board->hint_trigger_time = 5.0f;
        board->wildcard_prob = 0.05f;
        board->buy_prop_price = 200;
        board->moves_remaining = NORMAL_MOVES + (board->consecutive_fails_normal >= 2 ? 2 : 0);
    } else {
        board->target_score = 8000;
        board->max_props_per_game = 3;
        board->max_sandglass_per_game = 1;
        board->hint_trigger_time = 999999.0f; // effectively no hints
        board->wildcard_prob = 0.02f;
        board->buy_prop_price = 400;
        board->moves_remaining = HARD_MOVES + (board->consecutive_fails_hard >= 2 ? 1 : 0);
    }
}

static void place_stones_ice(GameBoard *board) {
    if (board->difficulty == 1) {
        int coords[10][2] = {{1,1},{1,6},{6,1},{6,6}, {3,3},{3,4},{4,3},{4,4}, {2,2},{5,5}};
        for (int i=0; i<10; i++) board->board[coords[i][0]][coords[i][1]].is_stone = true;
    } else if (board->difficulty == 2) {
        int s_coords[16][2] = {{0,0},{0,7},{7,0},{7,7}, {1,1},{1,6},{6,1},{6,6}, 
                               {3,1},{4,1},{3,6},{4,6}, {1,3},{1,4},{6,3},{6,4}};
        for (int i=0; i<16; i++) board->board[s_coords[i][0]][s_coords[i][1]].is_stone = true;
        
        int i_coords[10][2] = {{3,3},{3,4},{4,3},{4,4}, {2,3},{2,4},{5,3},{5,4}, {3,2},{4,2}};
        for (int i=0; i<10; i++) board->board[i_coords[i][0]][i_coords[i][1]].has_ice = true;
    }
}

bool model_init_board_with_difficulty(GameBoard *board, int difficulty)
{
    if (!board || difficulty < 0 || difficulty > 2)
        return false;

    /* memset 前先把需要保留的字段存起来 */
    uint32_t b_high_score  = board->high_score;
    uint32_t b_total_coins = board->total_coins;
    int b_unlocked_diff    = board->unlocked_difficulty;
    int b_fails_norm       = board->consecutive_fails_normal;
    int b_fails_hard       = board->consecutive_fails_hard;
    uint8_t b_hammer  = board->prop_hammer_count;
    uint8_t b_wand    = board->prop_wand_count;
    uint8_t b_shuffle = board->prop_shuffle_count;
    uint8_t b_moves   = board->prop_moves_count;

    memset(board, 0, sizeof(GameBoard));

    /* 把刚才存的字段写回去 */
    board->high_score              = b_high_score;
    board->total_coins             = b_total_coins;
    board->unlocked_difficulty     = b_unlocked_diff;
    board->consecutive_fails_normal = b_fails_norm;
    board->consecutive_fails_hard  = b_fails_hard;
    board->prop_hammer_count  = b_hammer;
    board->prop_wand_count    = b_wand;
    board->prop_shuffle_count = b_shuffle;
    board->prop_moves_count   = b_moves;

    board->current_state      = GAME_STATE_WAITING_INPUT;
    board->score              = 0;
    board->level              = 1;
    board->difficulty         = difficulty;
    apply_difficulty_configs(board);
    board->first_gem_selected    = false;
    board->highlighted_difficulty = difficulty;
    board->combo_multiplier      = 1;
    board->animations_settled    = false; /* 触发开局的宝石掉落动画 */
    board->used_props_total      = 0;
    board->used_sandglass_count  = 0;
    board->max_combo_this_game   = 0;

    /* 反复重新生成，直到棋盘满足：无初始消除、不死局、有足够可走的步数 */
    do {
        for (int r = 0; r < BOARD_HEIGHT; r++)
            for (int c = 0; c < BOARD_WIDTH; c++)
                board->board[r][c] =
                    model_generate_gem(board, (uint8_t)r, (uint8_t)c, false);
                    
        if (has_initial_match(board) || model_is_deadlock(board)) continue;
        
        int valid_moves = count_valid_moves(board);
        if (difficulty == 0 && valid_moves >= 6) break;
        else if (difficulty == 1 && valid_moves >= 3) break;
        else if (difficulty == 2) break;
    } while (true);

    place_stones_ice(board);

    /* 把所有宝石的初始屏幕位置设到视口上方，触发开场掉落动画 */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            Gem *g   = &board->board[r][c];
            g->screen_x = g->target_x;
            g->screen_y = g->target_y - (float)((BOARD_HEIGHT - r) * GEM_SIZE + 200);
        }
    }

    return true;
}

/* 扫描棋盘上所有可消除的宝石（单次遍历，先横后纵） */
uint32_t model_check_eliminations(GameBoard *board, EliminationSet *out_set)
{
    if (!board || !out_set)
        return 0;

    memset(out_set, 0, sizeof(EliminationSet));

    /* 先清掉上一轮留下的标记 */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            board->board[r][c].is_marked_for_elimination = false;
            board->board[r][c].next_bomb_type = BOMB_NONE;
            board->board[r][c].next_gem_type_override = 0;
        }
    }

    /* 横向扫描：找出同色连续段 */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH - 2; ) {
            if (board->board[r][c].is_stone) { c++; continue; }
            uint8_t color = GEM_EMPTY;
            int end = c;
            while (end < BOARD_WIDTH) {
                if (board->board[r][end].is_stone) break;
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
                        /* 5连消→万能牌 */
                        board->board[r][k].next_bomb_type = BOMB_NONE;
                        board->board[r][k].next_gem_type_override = GEM_WILDCARD;
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

    /* 纵向扫描：同上，方向改成列 */
    for (int c = 0; c < BOARD_WIDTH; c++) {
        for (int r = 0; r < BOARD_HEIGHT - 2; ) {
            if (board->board[r][c].is_stone) { r++; continue; }
            uint8_t color = GEM_EMPTY;
            int end = r;
            while (end < BOARD_HEIGHT) {
                if (board->board[end][c].is_stone) break;
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
                        /* 5连消→万能牌 */
                        board->board[k][c].next_bomb_type = BOMB_NONE;
                        board->board[k][c].next_gem_type_override = GEM_WILDCARD;
                    } else if (end - r == 4 && k == bomb_idx) {
                        /* 若横纵同时是4连消，升级成十字炸弹 */
                        if (board->board[k][c].next_bomb_type == BOMB_LINE_V) {
                            board->board[k][c].next_bomb_type = BOMB_CROSS;
                        } else {
                            board->board[k][c].next_bomb_type = BOMB_LINE_H;
                        }
                    }
                }
                r = end;
            } else {
                r++;
            }
        }
    }

    /* 统计分数并记录所有被标记的格子 */
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
                if (board->board[r][c].bomb_type != BOMB_NONE) points += 20; /* 爆了一颗炸弹额外加分 */
            }
        }
    }
    return points;
}

uint32_t model_check_eliminations_advanced(GameBoard *board, EliminationSet *out_set)
{
    uint32_t pts = model_check_eliminations(board, out_set);
    if (board && board->difficulty == 0 && board->combo_multiplier >= 2) {
        pts *= (board->combo_multiplier * 2);
    } else if (board) {
        pts *= board->combo_multiplier;
    }
    return pts;
}

/* 把所有标记过的格子清掉（冰块先碎，宝石留着；4/5连消留下炸弹/万能牌） */
void model_apply_eliminations(GameBoard *board)
{
    if (!board)
        return;

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (!board->board[r][c].is_marked_for_elimination)
                continue;

            /* 有冰块时先打冰，宝石完好保留，本轮消除到此为止 */
            if (board->board[r][c].has_ice) {
                board->board[r][c].has_ice = false;
                board->board[r][c].is_marked_for_elimination = false;
                continue;
            }

            board->board[r][c].is_stone = false;
            board->board[r][c].is_marked_for_elimination = false;

            if (board->board[r][c].next_gem_type_override == GEM_WILDCARD) {
                /* 5连消：原地留一颗万能牌 */
                board->board[r][c].gem_type               = GEM_WILDCARD;
                board->board[r][c].bomb_type              = BOMB_NONE;
                board->board[r][c].next_bomb_type         = BOMB_NONE;
                board->board[r][c].next_gem_type_override = 0;
            } else if (board->board[r][c].next_bomb_type != BOMB_NONE) {
                /* 4连消：原地留一颗炸弹 */
                board->board[r][c].gem_type       = (uint8_t)(rand() % 5);
                board->board[r][c].bomb_type      = board->board[r][c].next_bomb_type;
                board->board[r][c].next_bomb_type = BOMB_NONE;
            } else {
                board->board[r][c].gem_type  = GEM_EMPTY;
                board->board[r][c].bomb_type = BOMB_NONE;
            }
        }
    }
}

/* 重力：让每列的宝石往下掉，填满空格（石块和冰格是地板，挡住上面的宝石） */
bool model_apply_gravity(GameBoard *board)
{
    if (!board)
        return false;

    bool moved = false;

    for (int c = 0; c < BOARD_WIDTH; c++) {
        int write_row = BOARD_HEIGHT - 1;

        for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
            if (board->board[r][c].is_stone) {
                write_row = r - 1; /* 石块是地板，重置写入位置到石块上方 */
                continue;
            }
            /* 带冰且有宝石的格子也当地板处理 */
            if (board->board[r][c].has_ice && board->board[r][c].gem_type != (uint8_t)GEM_EMPTY) {
                write_row = r - 1;
                continue;
            }

            if (board->board[r][c].gem_type != (uint8_t)GEM_EMPTY) {
                if (write_row != r) {
                    /* 目标格可能本身带冰，移动时要把冰的状态传过去 */
                    bool dest_has_ice = board->board[write_row][c].has_ice;

                    board->board[write_row][c]          = board->board[r][c];
                    board->board[write_row][c].row      = (uint8_t)write_row;
                    board->board[write_row][c].col      = (uint8_t)c;
                    board->board[write_row][c].target_x = gem_target_x((uint8_t)c);
                    board->board[write_row][c].target_y = gem_target_y((uint8_t)write_row);
                    board->board[write_row][c].has_ice  = dest_has_ice;

                    /* 清空源格子，注意不能留下 has_ice=true 的孤儿格 */
                    memset(&board->board[r][c], 0, sizeof(Gem));
                    board->board[r][c].gem_type   = (uint8_t)GEM_EMPTY;
                    board->board[r][c].row        = (uint8_t)r;
                    board->board[r][c].col        = (uint8_t)c;
                    board->board[r][c].elim_scale = 0.0f;

                    moved = true;
                }
                write_row--;
            }
        }
    }
    return moved;
}

/* 填充：在空格上方生成新宝石并让它掉下来 */
void model_refill_board(GameBoard *board)
{
    if (!board)
        return;

    bool refilled = false;
    bool changed;
    int spawn_counts[BOARD_WIDTH] = {0};

    do {
        changed = false;
        for (int r = 0; r < BOARD_HEIGHT; r++) {
            for (int c = 0; c < BOARD_WIDTH; c++) {
                if (board->board[r][c].is_stone) continue;

                /* 第0行是入口；紧贴石块/冰块下方的格子也是入口（因为上方被堵死了） */
                bool is_spawner = (r == 0) || 
                                  (r > 0 && board->board[r-1][c].is_stone) ||
                                  (r > 0 && board->board[r-1][c].has_ice && board->board[r-1][c].gem_type != (uint8_t)GEM_EMPTY);

                if (is_spawner && board->board[r][c].gem_type == GEM_EMPTY) {
                    bool had_ice = board->board[r][c].has_ice;
                    board->board[r][c] = model_generate_gem(board, (uint8_t)r, (uint8_t)c, true);
                    board->board[r][c].has_ice = had_ice;
                    /* 多颗宝石错开起始高度，产生瀑布式掉落效果 */
                    board->board[r][c].screen_y = (float)(BOARD_OFFSET_Y - GEM_SIZE * (1 + spawn_counts[c]));
                    spawn_counts[c]++;
                    refilled = true;
                    changed  = true;
                }
            }
        }
        /* 让刚生成的宝石先落下去，腾出入口格，以便下一轮继续生成 */
        if (model_apply_gravity(board)) {
            changed = true;
        }
    } while (changed);

/* 兜底填充：处理石块/冰块同时消除等边界情况导致的孤立空洞 */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (!board->board[r][c].is_stone &&
                board->board[r][c].gem_type == (uint8_t)GEM_EMPTY) {
                bool had_ice = board->board[r][c].has_ice;
                board->board[r][c] = model_generate_gem(board, (uint8_t)r, (uint8_t)c, true);
                board->board[r][c].has_ice = had_ice;
                board->board[r][c].screen_y = (float)(BOARD_OFFSET_Y - GEM_SIZE * (1 + r));
                refilled = true;
            }
        }
    }

    /* 兜底填充后再跑一次重力，防止新宝石悬空 */
    if (refilled) {
        bool gravity_moved;
        do {
            gravity_moved = model_apply_gravity(board);
        } while (gravity_moved);
    }

    if (refilled) {
        board->animations_settled = false;
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

    /* Forbid swapping unmovable obstacles (stones and ice) */
    if (board->board[r1][c1].is_stone || board->board[r2][c2].is_stone)
        return false;
    if (board->board[r1][c1].has_ice || board->board[r2][c2].has_ice)
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

    /* 试试交换，没消除就恢复 */
    EliminationSet es;
    model_check_eliminations(board, &es);

    if (es.count == 0) {
        tmp               = board->board[r1][c1];
        board->board[r1][c1] = board->board[r2][c2];
        board->board[r2][c2] = tmp;

        board->board[r1][c1].row = r1; board->board[r1][c1].col = c1;
        board->board[r2][c2].row = r2; board->board[r2][c2].col = c2;

        board->board[r1][c1].target_x = gem_target_x(c1);
        board->board[r1][c1].target_y = gem_target_y(r1);
        board->board[r2][c2].target_x = gem_target_x(c2);
        board->board[r2][c2].target_y = gem_target_y(r2);

        /* screen 坐标现在正好是对方的位置，保留不动可以让 Lerp 产生弹回动画 */

        for (int r = 0; r < BOARD_HEIGHT; r++)
            for (int c = 0; c < BOARD_WIDTH; c++)
                board->board[r][c].is_marked_for_elimination = false;

        return false;
    }

    return true;
}

/* 模拟交换，返回会消除多少个宝石（不修改棋盘） */
static uint32_t simulate_swap(GameBoard *board,
                               int r1, int c1, int r2, int c2)
{
    if (board->board[r1][c1].is_stone || board->board[r2][c2].is_stone)
        return 0;
    if (board->board[r1][c1].has_ice || board->board[r2][c2].has_ice)
        return 0;

    Gem tmp              = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = tmp;
    board->board[r1][c1].row = (uint8_t)r1; board->board[r1][c1].col = (uint8_t)c1;
    board->board[r2][c2].row = (uint8_t)r2; board->board[r2][c2].col = (uint8_t)c2;

    EliminationSet es;
    model_check_eliminations(board, &es);

    /* 恢复 */
    tmp              = board->board[r1][c1];
    board->board[r1][c1] = board->board[r2][c2];
    board->board[r2][c2] = tmp;
    board->board[r1][c1].row = (uint8_t)r1; board->board[r1][c1].col = (uint8_t)c1;
    board->board[r2][c2].row = (uint8_t)r2; board->board[r2][c2].col = (uint8_t)c2;

    /* 只清掉此次交换涉及的行列标记，不要全盘清除 */
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

/* 判断棋盘是否死局（遇到死局就自动尝试洗牌） */
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

/* 炸弹连锁：根据炸弹类型标记一整行/列/十字/3×3范围内的宝石 */
void model_trigger_bomb_chain(GameBoard *board, uint8_t row, uint8_t col)
{
    if (!board)
        return;

    switch (board->board[row][col].bomb_type) {
        case BOMB_LINE_H:
            for (int c = 0; c < BOARD_WIDTH; c++) {
                /* 石块是永久障碍，炸弹射不穿 */
                if (!board->board[row][c].is_stone &&
                    board->board[row][c].gem_type < MAX_GEM_TYPES)
                    board->board[row][c].is_marked_for_elimination = true;
            }
            break;

        case BOMB_LINE_V:
            for (int r = 0; r < BOARD_HEIGHT; r++) {
                if (!board->board[r][col].is_stone &&
                    board->board[r][col].gem_type < MAX_GEM_TYPES)
                    board->board[r][col].is_marked_for_elimination = true;
            }
            break;

        case BOMB_CROSS:
            for (int r = 0; r < BOARD_HEIGHT; r++) {
                if (!board->board[r][col].is_stone &&
                    board->board[r][col].gem_type < MAX_GEM_TYPES)
                    board->board[r][col].is_marked_for_elimination = true;
            }
            for (int c = 0; c < BOARD_WIDTH; c++) {
                if (!board->board[row][c].is_stone &&
                    board->board[row][c].gem_type < MAX_GEM_TYPES)
                    board->board[row][c].is_marked_for_elimination = true;
            }
            break;

        case BOMB_RADIUS:
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int rr = row + dr, cc = col + dc;
                    if (rr >= 0 && rr < BOARD_HEIGHT &&
                        cc >= 0 && cc < BOARD_WIDTH &&
                        !board->board[rr][cc].is_stone &&
                        board->board[rr][cc].gem_type < MAX_GEM_TYPES)
                        board->board[rr][cc].is_marked_for_elimination = true;
                }
            }
            break;

        default:
            break;
    }
}

/* 封存档 */
bool model_undo_move(GameBoard *board)
{
    if (!board || !board->undo_available)
        return false;

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

    /* 交换 screen 坐标让 Lerp 产生反向动画 */
    float s1x = board->board[r1][c1].screen_x;
    float s1y = board->board[r1][c1].screen_y;
    float s2x = board->board[r2][c2].screen_x;
    float s2y = board->board[r2][c2].screen_y;

    board->board[r1][c1].screen_x = s2x;
    board->board[r1][c1].screen_y = s2y;
    board->board[r2][c2].screen_x = s1x;
    board->board[r2][c2].screen_y = s1y;

    board->undo_available  = false;
    board->moves_remaining++;
    board->score            = board->undo_score;
    board->combo_multiplier = board->undo_combo;
    return true;
}

/* 状态读写 */

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



/* 存读档 */

#define SAVE_MAGIC   0x4D335356u  /* 'M3SV' */
#define SAVE_VERSION 3u

typedef struct {
    uint32_t magic;   /* 文件标识符 */
    uint16_t version; /* 格式版本，不对就拒展 */
    uint16_t pad;     /* 对齐用，写 0 */
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
        fprintf(stderr, "[model] 档汰损或版本不匹配\n");
        fclose(f);
        return false;
    }

    bool ok = fread(board, sizeof(GameBoard), 1, f) == 1;
    fclose(f);
    return ok;
}

/* 提示：遍历所有可走的步，找出能消除最多宝石的一步 */

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

/* 道具 */

/* 锤子：打一下选中的格子；有冰先碎冰，宝石/石块直接消除 */
/* 返回 0=失败，1=只打了冰，2=消除了宝石/石块 */
int model_prop_hammer_smash(GameBoard *board, uint8_t row, uint8_t col) {
    if (!board || row >= BOARD_HEIGHT || col >= BOARD_WIDTH) return 0;
    if (board->board[row][col].gem_type == GEM_EMPTY && !board->board[row][col].is_stone) return 0;
    if (board->prop_hammer_count == 0) return 0;

    int result = 2;
    if (board->board[row][col].has_ice) {
        board->board[row][col].has_ice = false;
        result = 1; /* 只打了冰，宝石没事 */
    } else {
        board->board[row][col].is_stone = false;
        board->board[row][col].gem_type = GEM_EMPTY;
        board->board[row][col].is_marked_for_elimination = true;
    }
    
    board->prop_hammer_count--;
    board->undo_available = false; /* 用了道具就不能悔棋了 */
    return result;
}

bool model_prop_wand_swap(GameBoard *board, uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    if (!board) return false;
    /* Bounds check before any array access */
    if (r1 >= BOARD_HEIGHT || c1 >= BOARD_WIDTH || r2 >= BOARD_HEIGHT || c2 >= BOARD_WIDTH) return false;
    if (board->board[r1][c1].is_stone || board->board[r2][c2].is_stone) return false;
    if (board->board[r1][c1].has_ice  || board->board[r2][c2].has_ice)  return false;
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
    board->undo_available = false; /* 用了魔法棒就不能悔棋了 */
    return true;
}

bool model_prop_shuffle(GameBoard *board) {
    if (!board || board->prop_shuffle_count == 0) return false;

    /* 只打乱宝石/炸弹类型，不碰 is_stone/has_ice 等位置相关字段 */
    Gem* gems[BOARD_WIDTH * BOARD_HEIGHT];
    int count = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (board->board[r][c].gem_type != GEM_EMPTY && !board->board[r][c].is_stone) {
                gems[count++] = &board->board[r][c];
            }
        }
    }

    if (count == 0) return false;

    do {
        /* Fisher-Yates 洗牌，只交换颜色和炸弹类型 */
        for (int i = count - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            uint8_t temp_type = gems[i]->gem_type;
            int     temp_bomb = gems[i]->bomb_type;
            gems[i]->gem_type  = gems[j]->gem_type;
            gems[i]->bomb_type = gems[j]->bomb_type;
            gems[j]->gem_type  = temp_type;
            gems[j]->bomb_type = temp_bomb;
        }
    } while (has_initial_match(board));

    board->prop_shuffle_count--;
    board->undo_available = false; /* 用了洗牌就不能悔棋了 */
    return true;
}

bool model_prop_add_moves(GameBoard *board) {
    if (!board || board->prop_moves_count == 0) return false;
    board->moves_remaining += 5;
    board->prop_moves_count--;
    return true;
}

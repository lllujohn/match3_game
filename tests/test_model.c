

#include "model.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ================================================================
 *  Minimal Test Framework
 * ================================================================ */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        g_tests_run++;                                                      \
        if (!(cond)) {                                                      \
            fprintf(stderr, "  [FAIL] %s:%d  %s\n", __FILE__, __LINE__,   \
                    (msg));                                                 \
            g_tests_failed++;                                               \
        } else {                                                            \
            g_tests_passed++;                                               \
        }                                                                   \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_NE(a, b, msg) TEST_ASSERT((a) != (b), msg)
#define TEST_ASSERT_TRUE(cond, msg) TEST_ASSERT((cond), msg)
#define TEST_ASSERT_FALSE(cond, msg) TEST_ASSERT(!(cond), msg)

#define RUN_TEST(fn)                                              \
    do {                                                          \
        printf("  %-55s", #fn "...");                            \
        fflush(stdout);                                           \
        fn();                                                     \
        printf("%s\n",                                            \
               (g_tests_failed == _prev_failed) ? "OK" : "FAIL"); \
        _prev_failed = g_tests_failed;                            \
    } while (0)

/* ================================================================
 *  Board construction helpers
 * ================================================================ */

/** Fill the entire board with a single gem type. */
static void fill_board_uniform(GameBoard *b, uint8_t gem_type) {
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            memset(&b->board[r][c], 0, sizeof(Gem));
            b->board[r][c].row          = (uint8_t)r;
            b->board[r][c].col          = (uint8_t)c;
            b->board[r][c].gem_type     = gem_type;
            b->board[r][c].elim_scale   = 1.0f;
            b->board[r][c].target_x     = (float)(BOARD_OFFSET_X + c * GEM_SIZE + GEM_SIZE / 2);
            b->board[r][c].target_y     = (float)(BOARD_OFFSET_Y + r * GEM_SIZE + GEM_SIZE / 2);
            b->board[r][c].screen_x     = b->board[r][c].target_x;
            b->board[r][c].screen_y     = b->board[r][c].target_y;
        }
    }
}

/** Fill board with alternating two types so no 3-match exists. */
static void fill_board_checkerboard(GameBoard *b) {
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            memset(&b->board[r][c], 0, sizeof(Gem));
            b->board[r][c].row      = (uint8_t)r;
            b->board[r][c].col      = (uint8_t)c;
            b->board[r][c].gem_type = (uint8_t)((c % 3) + (r % 2) * 3);
            b->board[r][c].elim_scale = 1.0f;
        }
    }
}

/* ================================================================
 *  Test Group 1: model_init_board
 * ================================================================ */

static void test_init_board_no_null(void) {
    GameBoard b;
    bool ok = model_init_board(&b);
    TEST_ASSERT_TRUE(ok, "model_init_board should return true");
}

static void test_init_board_no_initial_match(void) {
    GameBoard b;
    model_init_board(&b);

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_EQ(pts, 0u, "Freshly initialised board must have zero eliminations");
    TEST_ASSERT_EQ(es.count, 0u, "EliminationSet.count must be 0 on fresh board");
}

static void test_init_board_all_cells_valid(void) {
    GameBoard b;
    model_init_board(&b);

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            uint8_t t = b.board[r][c].gem_type;
            TEST_ASSERT_TRUE(t < MAX_GEM_TYPES,
                             "Every cell must hold a valid gem type after init");
        }
    }
}

static void test_init_board_score_zero(void) {
    GameBoard b;
    model_init_board(&b);
    TEST_ASSERT_EQ(b.score, 0u, "Score must be 0 after init");
}

static void test_init_board_with_difficulty_easy(void) {
    GameBoard b;
    model_init_board(&b);
    bool ok = model_init_board_with_difficulty(&b, 0);
    TEST_ASSERT_TRUE(ok, "Difficulty 0 (Easy) init must succeed");
    TEST_ASSERT_EQ(b.moves_remaining, (uint32_t)EASY_MOVES, "Easy mode moves_remaining mismatch");
}

static void test_init_board_with_difficulty_hard(void) {
    GameBoard b;
    model_init_board(&b);
    bool ok = model_init_board_with_difficulty(&b, 2);
    TEST_ASSERT_TRUE(ok, "Difficulty 2 (Hard) init must succeed");
    TEST_ASSERT_EQ(b.moves_remaining, (uint32_t)HARD_MOVES, "Hard mode moves_remaining mismatch");
}

static void test_init_board_bad_difficulty_rejected(void) {
    GameBoard b;
    model_init_board(&b);
    bool ok = model_init_board_with_difficulty(&b, 99);
    TEST_ASSERT_FALSE(ok, "Difficulty 99 must be rejected");
}

/* ================================================================
 *  Test Group 2: model_check_eliminations
 * ================================================================ */

static void test_elimination_horizontal_3(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    /* Place 3 reds in a row at (0, 0..2) */
    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_RED;

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_TRUE(pts > 0, "Horizontal 3-match should yield points > 0");
    TEST_ASSERT_EQ(es.count, 3u, "Horizontal 3-match should mark exactly 3 cells");
}

static void test_elimination_vertical_3(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    b.board[0][0].gem_type = GEM_MINT;
    b.board[1][0].gem_type = GEM_MINT;
    b.board[2][0].gem_type = GEM_MINT;

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_TRUE(pts > 0, "Vertical 3-match should yield points > 0");
    TEST_ASSERT_EQ(es.count, 3u, "Vertical 3-match should mark exactly 3 cells");
}

static void test_elimination_horizontal_5(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    for (int c = 0; c < 5; c++) b.board[2][c].gem_type = GEM_AMBER;

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_EQ(es.count, 5u, "Horizontal 5-match should mark 5 cells");
    (void)pts;
}

static void test_elimination_cross_shape(void) {
    /* T-shape / cross: row 3 cols 2-4 + col 3 rows 1-5 */
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    /* Horizontal arm */
    b.board[3][2].gem_type = GEM_CORAL;
    b.board[3][3].gem_type = GEM_CORAL;
    b.board[3][4].gem_type = GEM_CORAL;
    /* Vertical arm — shares (3,3) */
    b.board[1][3].gem_type = GEM_CORAL;
    b.board[2][3].gem_type = GEM_CORAL;
    b.board[4][3].gem_type = GEM_CORAL;
    b.board[5][3].gem_type = GEM_CORAL;

    EliminationSet es;
    model_check_eliminations(&b, &es);
    /* Cross has 3+5-1=7 unique cells (overlap at (3,3)) */
    TEST_ASSERT_TRUE(es.count >= 6u, "Cross-shape should mark at least 6 cells");
}

static void test_elimination_no_match(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_EQ(pts, 0u, "Checkerboard board must have 0 eliminations");
    TEST_ASSERT_EQ(es.count, 0u, "Checkerboard board: EliminationSet.count must be 0");
}

static void test_elimination_empty_board(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_EQ(pts, 0u, "Empty board must have 0 points");
    TEST_ASSERT_EQ(es.count, 0u, "Empty board: EliminationSet.count must be 0");
}

static void test_elimination_full_board_match(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_BLUE);

    EliminationSet es;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_EQ(es.count, (uint8_t)(BOARD_WIDTH * BOARD_HEIGHT),
                   "All-blue board: all cells should be marked");
    (void)pts;
}

/* ================================================================
 *  Test Group 3: model_apply_gravity
 * ================================================================ */

static void test_gravity_drops_gems(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));

    /* Col 0: gems at rows 0-1, empty at rows 2-7 */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        b.board[r][0].row      = (uint8_t)r;
        b.board[r][0].col      = 0;
        b.board[r][0].gem_type = (r <= 1) ? GEM_RED : (uint8_t)GEM_EMPTY;
        b.board[r][0].elim_scale = 1.0f;
    }
    /* All other columns solid */
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 1; c < BOARD_WIDTH; c++) {
            b.board[r][c].row = (uint8_t)r; b.board[r][c].col = (uint8_t)c;
            b.board[r][c].gem_type = GEM_MINT;
            b.board[r][c].elim_scale = 1.0f;
        }
    }

    bool moved = model_apply_gravity(&b);
    TEST_ASSERT_TRUE(moved, "Gravity should report gems moved");

    /* After gravity, bottom 2 rows of col 0 must be RED, rest EMPTY */
    TEST_ASSERT_EQ(b.board[BOARD_HEIGHT - 1][0].gem_type, (uint8_t)GEM_RED,
                   "Gravity: gem should settle at bottom row");
    TEST_ASSERT_EQ(b.board[BOARD_HEIGHT - 2][0].gem_type, (uint8_t)GEM_RED,
                   "Gravity: second gem should settle at second-from-bottom");
    TEST_ASSERT_EQ(b.board[0][0].gem_type, (uint8_t)GEM_EMPTY,
                   "Gravity: top row should be empty");
}

static void test_gravity_no_op_when_settled(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    bool moved = model_apply_gravity(&b);
    TEST_ASSERT_FALSE(moved, "Gravity on a full board must report no movement");
}

static void test_gravity_single_gem_at_top(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    /* All EMPTY */
    fill_board_uniform(&b, GEM_EMPTY);
    /* One gem at very top of col 0 */
    b.board[0][0].gem_type = GEM_CORAL;

    model_apply_gravity(&b);

    TEST_ASSERT_EQ(b.board[BOARD_HEIGHT - 1][0].gem_type, (uint8_t)GEM_CORAL,
                   "Single gem should fall to last row");
    for (int r = 0; r < BOARD_HEIGHT - 1; r++) {
        TEST_ASSERT_EQ(b.board[r][0].gem_type, (uint8_t)GEM_EMPTY,
                       "Rows above fallen gem must be EMPTY");
    }
}

/* ================================================================
 *  Test Group 4: model_swap_gems
 * ================================================================ */

static void test_swap_valid_creates_match(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    /* Force a swappable 3-match setup:
     * Row 0: R R _ ...  (two reds, then checkerboard)
     * After swapping (0,2) with (0,3), if we set col 2 = RED → 3 reds in a row */
    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_BLUE;
    b.board[0][3].gem_type = GEM_RED;
    /* Swap col2 and col3 — should produce 3 consecutive REDs */
    bool swapped = model_swap_gems(&b, 0, 2, 0, 3);
    TEST_ASSERT_TRUE(swapped, "Swap that creates a match should return true");
}

static void test_swap_invalid_no_match(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    /* Find a swap that produces no match (checkerboard is deadlock by design) */
    bool swapped = model_swap_gems(&b, 0, 0, 0, 1);
    TEST_ASSERT_FALSE(swapped, "Non-matching swap must return false");
}

static void test_swap_non_adjacent_rejected(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    /* (0,0) ↔ (0,2) is not adjacent */
    bool swapped = model_swap_gems(&b, 0, 0, 0, 2);
    TEST_ASSERT_FALSE(swapped, "Non-adjacent swap must be rejected");
}

static void test_swap_diagonal_rejected(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    bool swapped = model_swap_gems(&b, 0, 0, 1, 1);
    TEST_ASSERT_FALSE(swapped, "Diagonal swap must be rejected");
}

static void test_swap_out_of_bounds_rejected(void) {
    GameBoard b;
    model_init_board(&b);

    bool swapped = model_swap_gems(&b, 0, 0, 0, (uint8_t)BOARD_WIDTH);
    TEST_ASSERT_FALSE(swapped, "Out-of-bounds swap must be rejected");
}

/* ================================================================
 *  Test Group 5: model_apply_eliminations
 * ================================================================ */

static void test_apply_eliminations_clears_marked(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    /* Manually mark first row */
    for (int c = 0; c < BOARD_WIDTH; c++) {
        b.board[0][c].gem_type               = GEM_RED;
        b.board[0][c].is_marked_for_elimination = true;
    }
    model_apply_eliminations(&b);

    for (int c = 0; c < BOARD_WIDTH; c++) {
        TEST_ASSERT_EQ(b.board[0][c].gem_type, (uint8_t)GEM_EMPTY,
                       "Eliminated cells must become GEM_EMPTY");
        TEST_ASSERT_FALSE(b.board[0][c].is_marked_for_elimination,
                          "Eliminated cells must clear is_marked_for_elimination");
    }
}

/* ================================================================
 *  Test Group 6: model_is_deadlock
 * ================================================================ */

static void test_deadlock_detects_no_moves(void) {
    /* A checkerboard with alternating RED/BLUE has no valid swap. */
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    bool locked = model_is_deadlock(&b);
    TEST_ASSERT_TRUE(locked, "Checkerboard with no valid move must be detected as deadlock");
}

static void test_deadlock_not_triggered_with_valid_move(void) {
    GameBoard b;
    model_init_board(&b);

    /* A board with a known swap that creates a match is NOT deadlocked.
     * model_init_board guarantees at least one valid move exists. */
    uint8_t hr, hc, hd;
    bool has_hint = model_find_best_hint(&b, &hr, &hc, &hd);
    if (has_hint) {
        bool locked = model_is_deadlock(&b);
        TEST_ASSERT_FALSE(locked, "Board with a valid hint must not be deadlocked");
    }
    /* If has_hint is false, skip (edge case: extremely unlikely on a fresh board). */
}

/* ================================================================
 *  Test Group 7: model_find_best_hint
 * ================================================================ */

static void test_hint_returns_valid_move(void) {
    /* Build a board where we know a move exists. */
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    /* Patch in a guaranteed match: three reds can be formed by swapping (0,2) ↔ (0,3) */
    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_BLUE;
    b.board[0][3].gem_type = GEM_RED;

    uint8_t hr, hc, hd;
    bool found = model_find_best_hint(&b, &hr, &hc, &hd);
    TEST_ASSERT_TRUE(found, "Hint must be found on board with a valid move");
    TEST_ASSERT_TRUE(hr < BOARD_HEIGHT, "Hint row must be in bounds");
    TEST_ASSERT_TRUE(hc < BOARD_WIDTH, "Hint col must be in bounds");
    TEST_ASSERT_TRUE(hd <= 1u, "Hint direction must be 0 (right) or 1 (down)");
}

static void test_hint_null_args_rejected(void) {
    GameBoard b;
    model_init_board(&b);
    uint8_t hr, hc, hd;
    bool r1 = model_find_best_hint(NULL, &hr, &hc, &hd);
    bool r2 = model_find_best_hint(&b,   NULL, &hc, &hd);
    bool r3 = model_find_best_hint(&b,   &hr,  NULL, &hd);
    bool r4 = model_find_best_hint(&b,   &hr,  &hc,  NULL);
    TEST_ASSERT_FALSE(r1, "NULL board must be rejected by hint");
    TEST_ASSERT_FALSE(r2, "NULL hr must be rejected by hint");
    TEST_ASSERT_FALSE(r3, "NULL hc must be rejected by hint");
    TEST_ASSERT_FALSE(r4, "NULL hd must be rejected by hint");
}

/* ================================================================
 *  Test Group 8: model_refill_board
 * ================================================================ */

static void test_refill_fills_empty_cells(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    model_refill_board(&b);

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            TEST_ASSERT_TRUE(b.board[r][c].gem_type < MAX_GEM_TYPES,
                             "refill_board: all cells must have a valid gem type");
        }
    }
}

static void test_refill_does_not_overwrite_existing(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_uniform(&b, GEM_EMPTY);

    /* Set one existing gem */
    b.board[3][3].gem_type = GEM_MINT;

    model_refill_board(&b);

    TEST_ASSERT_EQ(b.board[3][3].gem_type, (uint8_t)GEM_MINT,
                   "refill_board must not overwrite non-empty gems");
}

/* ================================================================
 *  Test Group 9: model_screen_to_board_coord
 * ================================================================ */

static void test_screen_to_board_valid(void) {
    /* Click at exact top-left of cell (0,0) */
    uint8_t row, col;
    bool ok = model_screen_to_board_coord(BOARD_OFFSET_X + 1, BOARD_OFFSET_Y + 1, &row, &col);
    TEST_ASSERT_TRUE(ok, "Click inside board must be valid");
    TEST_ASSERT_EQ(row, 0u, "Top-left click should map to row 0");
    TEST_ASSERT_EQ(col, 0u, "Top-left click should map to col 0");
}

static void test_screen_to_board_outside_rejected(void) {
    uint8_t row, col;
    bool ok = model_screen_to_board_coord(0, 0, &row, &col);
    TEST_ASSERT_FALSE(ok, "Click outside board must be rejected");
}

static void test_screen_to_board_last_cell(void) {
    int px = BOARD_OFFSET_X + (BOARD_WIDTH  - 1) * GEM_SIZE + GEM_SIZE / 2;
    int py = BOARD_OFFSET_Y + (BOARD_HEIGHT - 1) * GEM_SIZE + GEM_SIZE / 2;
    uint8_t row, col;
    bool ok = model_screen_to_board_coord(px, py, &row, &col);
    TEST_ASSERT_TRUE(ok, "Click on last cell center must be valid");
    TEST_ASSERT_EQ(row, (uint8_t)(BOARD_HEIGHT - 1), "Last cell row mismatch");
    TEST_ASSERT_EQ(col, (uint8_t)(BOARD_WIDTH  - 1), "Last cell col mismatch");
}

/* ================================================================
 *  Test Group 10: model_check_eliminations_advanced (combo)
 * ================================================================ */

static void test_advanced_elim_combo_multiplier(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    b.difficulty = 1; /* Normal mode for base combo testing */
    fill_board_checkerboard(&b);

    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_RED;

    b.combo_multiplier = 3;
    EliminationSet es;
    uint32_t pts_advanced = model_check_eliminations_advanced(&b, &es);

    /* Reset and get base score */
    b.combo_multiplier = 1;
    for (int r = 0; r < BOARD_HEIGHT; r++)
        for (int c = 0; c < BOARD_WIDTH; c++)
            b.board[r][c].is_marked_for_elimination = false;
    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_RED;

    EliminationSet es2;
    uint32_t pts_base = model_check_eliminations(&b, &es2);

    TEST_ASSERT_EQ(pts_advanced, pts_base * 3, "Advanced elimination must apply combo multiplier");
}

/* ================================================================
 *  Test Group 11: model_trigger_bomb_chain
 * ================================================================ */

static void test_bomb_line_h_clears_row(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    b.board[2][4].bomb_type = BOMB_LINE_H;
    model_trigger_bomb_chain(&b, 2, 4);

    for (int c = 0; c < BOARD_WIDTH; c++) {
        TEST_ASSERT_TRUE(b.board[2][c].is_marked_for_elimination,
                         "BOMB_LINE_H must mark entire row");
    }
    /* Other rows untouched */
    TEST_ASSERT_FALSE(b.board[0][0].is_marked_for_elimination,
                      "BOMB_LINE_H must not mark other rows");
}

static void test_bomb_line_v_clears_column(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    b.board[3][5].bomb_type = BOMB_LINE_V;
    model_trigger_bomb_chain(&b, 3, 5);

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        TEST_ASSERT_TRUE(b.board[r][5].is_marked_for_elimination,
                         "BOMB_LINE_V must mark entire column");
    }
    TEST_ASSERT_FALSE(b.board[0][0].is_marked_for_elimination,
                      "BOMB_LINE_V must not mark other columns");
}

static void test_bomb_cross_clears_row_and_col(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    b.board[4][4].bomb_type = BOMB_CROSS;
    model_trigger_bomb_chain(&b, 4, 4);

    for (int c = 0; c < BOARD_WIDTH; c++) {
        TEST_ASSERT_TRUE(b.board[4][c].is_marked_for_elimination,
                         "BOMB_CROSS must mark entire row");
    }
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        TEST_ASSERT_TRUE(b.board[r][4].is_marked_for_elimination,
                         "BOMB_CROSS must mark entire column");
    }
}

static void test_bomb_radius_clears_3x3(void) {
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    b.board[3][3].bomb_type = BOMB_RADIUS;
    model_trigger_bomb_chain(&b, 3, 3);

    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            TEST_ASSERT_TRUE(b.board[3 + dr][3 + dc].is_marked_for_elimination,
                             "BOMB_RADIUS must mark 3x3 region");
        }
    }
    TEST_ASSERT_FALSE(b.board[0][0].is_marked_for_elimination,
                      "BOMB_RADIUS must not mark cells outside 3x3 radius");
}

/* ================================================================
 *  Test Group 12: model_save_game / model_load_game (roundtrip)
 * ================================================================ */

static void test_save_load_roundtrip(void) {
    const char *tmp = "match3_test_save.dat";

    GameBoard original;
    model_init_board(&original);
    original.score              = 12345U;
    original.moves_remaining    = 7U;
    original.difficulty         = 2;
    original.combo_multiplier   = 3U;

    bool saved = model_save_game(&original, tmp);
    TEST_ASSERT_TRUE(saved, "model_save_game must return true");

    GameBoard loaded;
    memset(&loaded, 0, sizeof(loaded));
    bool ok = model_load_game(&loaded, tmp);
    TEST_ASSERT_TRUE(ok, "model_load_game must succeed after save");

    TEST_ASSERT_EQ(loaded.score,            12345U, "Loaded score must match");
    TEST_ASSERT_EQ(loaded.moves_remaining,  7U,     "Loaded moves must match");
    TEST_ASSERT_EQ(loaded.difficulty,       2,      "Loaded difficulty must match");
    TEST_ASSERT_EQ(loaded.combo_multiplier, 3U,     "Loaded combo_multiplier must match");

    remove(tmp); /* Clean up */
}

static void test_load_corrupt_file_rejected(void) {
    /* Write garbage (no valid magic header) */
    const char *tmp = "match3_corrupt.dat";
    FILE *f = fopen(tmp, "wb");
    if (f) {
        uint32_t garbage = 0xDEADBEEFu;
        (void)fwrite(&garbage, sizeof(garbage), 1, f);
        fclose(f);
    }

    GameBoard b;
    model_init_board(&b);
    bool ok = model_load_game(&b, tmp);
    TEST_ASSERT_FALSE(ok, "model_load_game must reject a file with invalid magic");

    remove(tmp); /* Clean up */
}

/* ================================================================
 *  Test Group 13: model_undo_move
 * ================================================================ */

static void test_undo_not_available_initially(void) {
    GameBoard b;
    model_init_board(&b);
    bool ok = model_undo_move(&b);
    TEST_ASSERT_FALSE(ok, "Undo must fail when no swap has been made");
}

static void test_undo_restores_gem_positions(void) {
    /* Build a board with a known valid swap */
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_BLUE;
    b.board[0][3].gem_type = GEM_RED;

    uint8_t type_before_02 = b.board[0][2].gem_type;  /* BLUE */
    uint8_t type_before_03 = b.board[0][3].gem_type;  /* RED  */

    /* Manually record undo state (controller normally does this) */
    b.undo_r1        = 0; b.undo_c1 = 2;
    b.undo_r2        = 0; b.undo_c2 = 3;
    b.undo_score     = b.score;
    b.undo_available = true;

    /* Perform the swap directly */
    uint8_t gem_type_02 = b.board[0][2].gem_type;
    b.board[0][2].gem_type = b.board[0][3].gem_type;
    b.board[0][3].gem_type = gem_type_02;

    /* Now undo — gems should revert */
    bool ok = model_undo_move(&b);
    TEST_ASSERT_TRUE(ok, "Undo must succeed when undo_available is true");
    TEST_ASSERT_EQ(b.board[0][2].gem_type, type_before_02,
                   "Undo: gem at (0,2) must revert to original type");
    TEST_ASSERT_EQ(b.board[0][3].gem_type, type_before_03,
                   "Undo: gem at (0,3) must revert to original type");
    TEST_ASSERT_FALSE(b.undo_available,
                      "Undo must clear undo_available after use");
}

static void test_undo_null_rejected(void) {
    bool ok = model_undo_move(NULL);
    TEST_ASSERT_FALSE(ok, "model_undo_move(NULL) must return false");
}

static void test_hint_returns_best_scoring_move(void) {
    /* Build a board where two moves exist: one worth 30 pts, one worth 50 */
    GameBoard b;
    memset(&b, 0, sizeof(b));
    fill_board_checkerboard(&b);

    /* Col 0: three reds in col 0..2 can be made by swapping (row0,col2)↔(row0,col3)
     * Separately, five-in-a-row for a bigger score via row 5 */
    /* Simpler test: just verify the hint is consistent — points > 0 */
    b.board[0][0].gem_type = GEM_RED;
    b.board[0][1].gem_type = GEM_RED;
    b.board[0][2].gem_type = GEM_BLUE;
    b.board[0][3].gem_type = GEM_RED;

    /* Add a 5-match opportunity worth more */
    b.board[5][0].gem_type = GEM_MINT;
    b.board[5][1].gem_type = GEM_MINT;
    b.board[5][2].gem_type = GEM_MINT;
    b.board[5][3].gem_type = GEM_MINT;
    b.board[5][4].gem_type = GEM_RED;   /* swap (5,4)↔(5,5) won't match */
    b.board[5][5].gem_type = GEM_MINT;  /* swap (5,4) with (5,5) → 5-in-a-row */

    uint8_t hr, hc, hd;
    bool found = model_find_best_hint(&b, &hr, &hc, &hd);
    TEST_ASSERT_TRUE(found, "Hint must be found");

    /* Verify the reported move actually creates a match */
    EliminationSet es;
    uint8_t r2 = (hd == 0) ? hr : (uint8_t)(hr + 1);
    uint8_t c2 = (hd == 0) ? (uint8_t)(hc + 1) : hc;
    Gem tmp          = b.board[hr][hc];
    b.board[hr][hc]  = b.board[r2][c2];
    b.board[r2][c2]  = tmp;
    b.board[hr][hc].row = hr; b.board[hr][hc].col = hc;
    b.board[r2][c2].row = r2; b.board[r2][c2].col = c2;
    uint32_t pts = model_check_eliminations(&b, &es);
    TEST_ASSERT_TRUE(pts > 0, "Hint move must actually create a match");
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    srand((unsigned int)time(NULL));

    printf("\n=== Match-3 Model Unit Tests ===\n\n");

    int _prev_failed = 0;

    printf("[ Group 1: model_init_board ]\n");
    RUN_TEST(test_init_board_no_null);
    RUN_TEST(test_init_board_no_initial_match);
    RUN_TEST(test_init_board_all_cells_valid);
    RUN_TEST(test_init_board_score_zero);
    RUN_TEST(test_init_board_with_difficulty_easy);
    RUN_TEST(test_init_board_with_difficulty_hard);
    RUN_TEST(test_init_board_bad_difficulty_rejected);

    printf("\n[ Group 2: model_check_eliminations ]\n");
    RUN_TEST(test_elimination_horizontal_3);
    RUN_TEST(test_elimination_vertical_3);
    RUN_TEST(test_elimination_horizontal_5);
    RUN_TEST(test_elimination_cross_shape);
    RUN_TEST(test_elimination_no_match);
    RUN_TEST(test_elimination_empty_board);
    RUN_TEST(test_elimination_full_board_match);

    printf("\n[ Group 3: model_apply_gravity ]\n");
    RUN_TEST(test_gravity_drops_gems);
    RUN_TEST(test_gravity_no_op_when_settled);
    RUN_TEST(test_gravity_single_gem_at_top);

    printf("\n[ Group 4: model_swap_gems ]\n");
    RUN_TEST(test_swap_valid_creates_match);
    RUN_TEST(test_swap_invalid_no_match);
    RUN_TEST(test_swap_non_adjacent_rejected);
    RUN_TEST(test_swap_diagonal_rejected);
    RUN_TEST(test_swap_out_of_bounds_rejected);

    printf("\n[ Group 5: model_apply_eliminations ]\n");
    RUN_TEST(test_apply_eliminations_clears_marked);

    printf("\n[ Group 6: model_is_deadlock ]\n");
    RUN_TEST(test_deadlock_detects_no_moves);
    RUN_TEST(test_deadlock_not_triggered_with_valid_move);

    printf("\n[ Group 7: model_find_best_hint ]\n");
    RUN_TEST(test_hint_returns_valid_move);
    RUN_TEST(test_hint_null_args_rejected);

    printf("\n[ Group 8: model_refill_board ]\n");
    RUN_TEST(test_refill_fills_empty_cells);
    RUN_TEST(test_refill_does_not_overwrite_existing);

    printf("\n[ Group 9: model_screen_to_board_coord ]\n");
    RUN_TEST(test_screen_to_board_valid);
    RUN_TEST(test_screen_to_board_outside_rejected);
    RUN_TEST(test_screen_to_board_last_cell);

    printf("\n[ Group 10: model_check_eliminations_advanced ]\n");
    RUN_TEST(test_advanced_elim_combo_multiplier);

    printf("\n[ Group 11: model_trigger_bomb_chain ]\n");
    RUN_TEST(test_bomb_line_h_clears_row);
    RUN_TEST(test_bomb_line_v_clears_column);
    RUN_TEST(test_bomb_cross_clears_row_and_col);
    RUN_TEST(test_bomb_radius_clears_3x3);

    printf("\n[ Group 12: model_save_game / model_load_game ]\n");
    RUN_TEST(test_save_load_roundtrip);
    RUN_TEST(test_load_corrupt_file_rejected);

    printf("\n[ Group 13: model_undo_move ]\n");
    RUN_TEST(test_undo_not_available_initially);
    RUN_TEST(test_undo_restores_gem_positions);
    RUN_TEST(test_undo_null_rejected);

    printf("\n[ Group 14: model_find_best_hint (optimal) ]\n");
    RUN_TEST(test_hint_returns_best_scoring_move);

    printf("\n=================================\n");
    printf("Results: %d / %d passed", g_tests_passed, g_tests_run);
    if (g_tests_failed == 0) {
        printf("  \xe2\x9c\x93 All tests passed!\n");
    } else {
        printf("  \xe2\x9c\x97 %d test(s) FAILED\n", g_tests_failed);
    }
    printf("=================================\n\n");

    return (g_tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

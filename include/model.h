
#ifndef MATCH3_MODEL_H
#define MATCH3_MODEL_H

#include "types.h"

/* ================================================================
 *  Lifecycle
 * ================================================================ */

/**
 * @brief Initialise a GameBoard to the main-menu state.
 *
 * Generates a random, match-free board and zeros all score fields.
 * The board state is set to @c GAME_STATE_MAIN_MENU.
 *
 * @param[out] board  Pointer to uninitialised or stale GameBoard.
 * @return @c true on success; @c false if @p board is NULL.
 */
bool model_init_board(GameBoard *board);

/**
 * @brief Zero-fill a board (reset all fields to defaults).
 * @param[in,out] board  Board to destroy; pointer itself is not freed.
 */
void model_destroy_board(GameBoard *board);

/**
 * @brief Initialise a board for a specific difficulty level.
 *
 * Preserves the current @c high_score across the reset.
 * Sets the board state to @c GAME_STATE_WAITING_INPUT and
 * positions all gems above the viewport for a drop-in animation.
 *
 * @param[in,out] board       Target board.
 * @param[in]     difficulty  0 = Easy, 1 = Normal, 2 = Hard.
 * @return @c true on success; @c false if board is NULL or
 *         @p difficulty is out of range [0, 2].
 */
bool model_init_board_with_difficulty(GameBoard *board, int difficulty);

/* ================================================================
 *  Gem generation
 * ================================================================ */

/**
 * @brief Generate a single new gem with random colour.
 *
 * @param[in] row             Logical row for the gem.
 * @param[in] col             Logical column for the gem.
 * @param[in] offscreen_spawn If @c true, the gem's @c screen_y is set
 *                            above the viewport to trigger a fall-in
 *                            Lerp animation.
 * @return Fully initialised Gem value.
 */
Gem model_generate_gem(GameBoard *board, uint8_t row, uint8_t col, bool offscreen_spawn);

/* ================================================================
 *  Core elimination logic
 * ================================================================ */

/**
 * @brief Scan the board for horizontal and vertical 3+ matches.
 *
 * Sets @c is_marked_for_elimination on every matched gem and
 * populates @p out_set.  Does **not** remove gems from the board.
 *
 * Complexity: O(BOARD_WIDTH × BOARD_HEIGHT).
 *
 * @param[in,out] board    Board to scan; marks are written in-place.
 * @param[out]   out_set  Receives the set of matched positions.
 * @return Base score (10 points × matched count), ignoring combos.
 *         Returns 0 if @p board or @p out_set is NULL.
 */
uint32_t model_check_eliminations(GameBoard *board, EliminationSet *out_set);

/**
 * @brief Like model_check_eliminations() but multiplied by
 *        @c board->combo_multiplier.
 *
 * @param[in,out] board    Board to scan.
 * @param[out]   out_set  Receives matched positions.
 * @return Score × combo_multiplier.
 */
uint32_t model_check_eliminations_advanced(GameBoard *board, EliminationSet *out_set);

/**
 * @brief Commit eliminations: replace every marked gem with GEM_EMPTY.
 *
 * Must be called only after the elimination animation has finished.
 * Clears @c is_marked_for_elimination on all written cells.
 *
 * @param[in,out] board  Board to modify.
 */
void model_apply_eliminations(GameBoard *board);

/**
 * @brief Apply gravity: compact non-empty gems toward the bottom of
 *        each column, leaving empty slots at the top.
 *
 * Updates @c target_x / @c target_y on moved gems so the View can
 * Lerp from their old @c screen_x / @c screen_y.
 *
 * @param[in,out] board  Board to modify.
 * @return @c true if at least one gem moved; @c false otherwise.
 */
bool model_apply_gravity(GameBoard *board);

/**
 * @brief Fill every GEM_EMPTY cell with a freshly generated gem.
 *
 * The new gem's @c screen_y is positioned above the viewport so the
 * View will animate it falling in.
 *
 * @param[in,out] board  Board to fill.
 */
void model_refill_board(GameBoard *board);

/* ================================================================
 *  Gem swap
 * ================================================================ */

/**
 * @brief Attempt to swap two adjacent gems.
 *
 * Performs the swap, checks for eliminations, and rolls back if none
 * are found.  Updates @c target_x / @c target_y so the View can
 * start Lerp animations immediately.
 *
 * @param[in,out] board  Board containing the gems.
 * @param[in] r1, c1     Position of the first gem.
 * @param[in] r2, c2     Position of the second gem.
 * @return @c true if the swap was valid (produced at least one match);
 *         @c false if the positions are non-adjacent, out-of-bounds,
 *         or produce no match (swap is rolled back).
 */
bool model_swap_gems(GameBoard *board,
                     uint8_t r1, uint8_t c1,
                     uint8_t r2, uint8_t c2);

/* ================================================================
 *  Game-state helpers
 * ================================================================ */

/**
 * @brief Check whether the board has no valid swap that produces a
 *        match (deadlock / "no more moves" condition).
 *
 * Uses an exhaustive O(n³) brute-force scan with early exit.
 *
 * @param[in,out] board  Board to inspect (temporarily modified then restored).
 * @return @c true if the board is deadlocked; @c false if a move exists.
 */
bool model_is_deadlock(GameBoard *board);

/**
 * @brief Test whether (@p row, @p col) is orthogonally adjacent to
 *        the currently selected gem in @p board.
 *
 * @param[in] board  Board with selection state.
 * @param[in] row    Target row.
 * @param[in] col    Target column.
 * @return @c true if adjacent and @c first_gem_selected is set.
 */
bool model_is_adjacent(const GameBoard *board, uint8_t row, uint8_t col);

/**
 * @brief Convert a pixel coordinate to a board (row, col) index.
 *
 * @param[in]  px       Pixel X.
 * @param[in]  py       Pixel Y.
 * @param[out] out_row  Board row (written only on success).
 * @param[out] out_col  Board column (written only on success).
 * @return @c true if the pixel is inside the board area.
 */
bool model_screen_to_board_coord(int px, int py,
                                 uint8_t *out_row, uint8_t *out_col);

/**
 * @brief Undo the last move by swapping the two gems back and
 *        restoring the pre-swap score.
 *
 * A reverse-Lerp animation is triggered by setting screen_x/screen_y
 * to the opposite gem's target position.
 *
 * @return @c true if undo was performed; @c false if no undo is
 *         available, board is NULL, or @c undo_available is false.
 */
bool model_undo_move(GameBoard *board);

/* ================================================================
 *  Bomb / special gem effects
 * ================================================================ */

/**
 * @brief Trigger the bomb effect at (@p row, @p col), marking all
 *        affected cells for elimination.
 *
 * Behaviour depends on @c board->board[row][col].bomb_type.
 *
 * @param[in,out] board  Board to modify.
 * @param[in]     row    Row of the bomb gem.
 * @param[in]     col    Column of the bomb gem.
 */
void model_trigger_bomb_chain(GameBoard *board, uint8_t row, uint8_t col);

/* ================================================================
 *  Persistence
 * ================================================================ */

/**
 * @brief Serialise the full board to a binary file.
 *
 * @param[in] board     Board to save.
 * @param[in] filename  Destination path.
 * @return @c true on success.
 */
bool model_save_game(const GameBoard *board, const char *filename);

/**
 * @brief Deserialise a board from a binary file.
 *
 * @param[out] board     Board to overwrite.
 * @param[in]  filename  Source path.
 * @return @c true on success.
 */
bool model_load_game(GameBoard *board, const char *filename);

/**
 * @brief Find the best available move (highest-scoring swap).
 *
 * Searches the entire board for the swap that eliminates the most
 * gems, returning the global optimum.
 *
 * @param[in,out] board  Board to search.
 * @param[out]    hr     Row of the gem to move.
 * @param[out]    hc     Column of the gem to move.
 * @param[out]    hd     Direction: 0 = right, 1 = down.
 * @return @c true if a hint was found; @c false if all args are NULL
 *         or the board is deadlocked.
 */
bool model_find_best_hint(GameBoard *board,
                          uint8_t *hr, uint8_t *hc, uint8_t *hd);

/* ================================================================
 *  Props System
 * ================================================================ */

/**
 * @brief Hammer prop: smashes a single gem, marking it for elimination.
 * @param[in,out] board  Board to modify.
 * @param[in] row        Row of the target gem.
 * @param[in] col        Column of the target gem.
 * @return @c true if the hammer was used; @c false if invalid position or empty.
 */
int model_prop_hammer_smash(GameBoard *board, uint8_t row, uint8_t col);

/**
 * @brief Wand prop: forcefully swaps two adjacent gems.
 * @param[in,out] board  Board to modify.
 * @param[in] r1, c1     Position of the first gem.
 * @param[in] r2, c2     Position of the second gem.
 * @return @c true if swap executed.
 */
bool model_prop_wand_swap(GameBoard *board, uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2);

bool model_force_shuffle(GameBoard *board);

/**
 * @brief Shuffle prop: randomly shuffles all non-empty gems.
 * @param[in,out] board  Board to modify.
 * @return @c true on success.
 */
bool model_prop_shuffle(GameBoard *board);

/**
 * @brief Moves prop: adds 5 extra moves.
 * @param[in,out] board  Board to modify.
 * @return @c true on success.
 */
bool model_prop_add_moves(GameBoard *board);

#endif /* MATCH3_MODEL_H */

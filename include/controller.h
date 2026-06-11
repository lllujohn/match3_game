/**
 * @file controller.h
 * @brief Public API of the Match-3 Controller layer.
 *
 * The Controller is the bridge between SDL2 input events and the
 * game's Model.  It owns the finite-state-machine update loop and
 * all save/load plumbing.
 *
 * Input design:
 * - controller_handle_event() is called once per SDL event from the
 *   main loop's SDL_PollEvent() drain.
 * - controller_update_state_machine() is called once per frame to
 *   advance timer-based state transitions.
 *
 * @author  Match-3 Contributors
 * @license MIT
 */
#ifndef MATCH3_CONTROLLER_H
#define MATCH3_CONTROLLER_H

#include "types.h"
#include <SDL2/SDL.h>   /* SDL_Event — the controller IS the SDL glue layer */

/* ================================================================
 *  Lifecycle
 * ================================================================ */

/**
 * @brief Initialise the controller's internal state.
 * @return @c true on success.
 */
bool controller_init(void);

/** @brief Release any resources owned by the controller. */
void controller_destroy(void);

/* ================================================================
 *  Input handling (called from the main SDL_PollEvent loop)
 * ================================================================ */

/**
 * @brief Process a single SDL event, mutating the board as needed.
 *
 * Handles SDL_KEYDOWN and SDL_MOUSEBUTTONDOWN events, dispatching
 * to the appropriate sub-handler based on @c board->current_state.
 *
 * @param[in,out] board   Board to mutate.
 * @param[in]     event   The SDL event to process (read-only).
 */
void controller_handle_event(GameBoard *board, const SDL_Event *event);

/* ================================================================
 *  State-machine update (called once per frame)
 * ================================================================ */

/**
 * @brief Advance timer-based state transitions.
 *
 * Must be called once per frame with the frame delta time.
 * Drives transitions such as:
 *   SWAP_ANIMATING → ELIMINATION_CHECK (when gems settle)
 *   ELIMINATING    → GRAVITY_APPLY     (after animation timer)
 *
 * @param[in,out] board      Board to update.
 * @param[in]     delta_time Frame time in seconds.
 * @return @c true if a state transition occurred this frame.
 */
bool controller_update_state_machine(GameBoard *board, float delta_time);

/* ================================================================
 *  Game control
 * ================================================================ */

/**
 * @brief Restart the game with the currently selected difficulty.
 * @param[in,out] board  Board to reinitialise.
 * @return @c true on success.
 */
bool controller_restart_game(GameBoard *board);

/**
 * @brief Toggle the paused state (PAUSED ↔ previous state).
 * @param[in,out] board  Board to modify.
 * @return @c true if the game is now paused; @c false if resumed.
 */
bool controller_toggle_pause(GameBoard *board);

/**
 * @brief Query whether the user signalled the application to quit.
 *
 * Set when ESC is pressed on the main menu or SDL_QUIT is received.
 *
 * @return @c true if the main loop should exit.
 */
bool controller_wants_quit(void);

/* ================================================================
 *  Save / load
 * ================================================================ */

/**
 * @brief Save the board to a named file.
 * @param[in] board     Board to serialise.
 * @param[in] filename  Destination path (NULL → default path).
 * @return @c true on success.
 */
bool controller_save_game(const GameBoard *board, const char *filename);

/**
 * @brief Load a board from a named file.
 * @param[out] board     Board to overwrite.
 * @param[in]  filename  Source path (NULL → default path).
 * @return @c true on success.
 */
bool controller_load_game(GameBoard *board, const char *filename);

/** @brief Save to the default quick-save slot. */
bool controller_quick_save(const GameBoard *board);

/** @brief Load from the default quick-save slot. */
bool controller_quick_load(GameBoard *board);

/* ================================================================
 *  Difficulty
 * ================================================================ */

/** @brief Set the active difficulty (0=Easy, 1=Normal, 2=Hard). */
void controller_set_difficulty(int difficulty);

/** @brief Get the active difficulty. */
int  controller_get_difficulty(void);

#endif /* MATCH3_CONTROLLER_H */

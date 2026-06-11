
#ifndef MATCH3_VIEW_H
#define MATCH3_VIEW_H

#include "types.h"

/* ================================================================
 *  Window lifecycle
 * ================================================================ */

/**
 * @brief Initialise SDL2, create the game window and hardware-
 *        accelerated renderer, and init SDL2_ttf + SDL2_mixer.
 *
 * @return @c true on success; @c false if any SDL subsystem fails.
 */
bool view_init_window(void);

/**
 * @brief Destroy the renderer and window, quit SDL2 subsystems.
 *
 * Safe to call even if view_init_window() was never called or failed.
 */
void view_destroy_window(void);

/**
 * @brief Load fonts and audio assets from the assets/ directory.
 *
 * Missing sound files are silently ignored (gameplay continues without
 * audio).  Missing fonts cause text rendering to be disabled.
 *
 * @return @c true on success (or partial success); @c false only if a
 *         critical asset fails and the game cannot proceed.
 */
bool view_load_assets(void);

/**
 * @brief Release all loaded font and audio assets.
 */
void view_unload_assets(void);

/* ================================================================
 *  Frame rendering
 * ================================================================ */

/**
 * @brief Render one complete frame using the current board state.
 *
 * Clears the screen, dispatches to the appropriate sub-renderer based
 * on @c board->current_state, then calls SDL_RenderPresent().
 *
 * @param[in] board  Board to render (read-only from the logic perspective).
 * @return @c true on success; @c false on SDL rendering error.
 */
bool view_render_frame(const GameBoard *board);

/* ================================================================
 *  Animation update (called every frame by the main loop)
 * ================================================================ */

/**
 * @brief Advance all Lerp animations by @p dt seconds.
 *
 * Moves each gem's screen_x/screen_y toward its target_x/target_y
 * and shrinks elim_scale toward 0 for marked gems.
 * Updates @c board->animations_settled when all gems have converged.
 *
 * @param[in,out] board  Board whose gem animation fields are updated.
 * @param[in]     dt     Frame delta time in seconds (clamped to ≤ 0.05).
 */
void view_update_animations(GameBoard *board, float dt);

/* ================================================================
 *  Sub-renderers (called internally; exposed for testing)
 * ================================================================ */

/** @brief Render the full in-game board, gems, and info panel. */
void view_draw_game_ui_complete(const GameBoard *board);

/** @brief Render the main menu screen. */
void view_draw_main_menu(const GameBoard *board);

/** @brief Render the difficulty selection screen. */
void view_draw_difficulty_menu(const GameBoard *board);

/** @brief Render the pause overlay. */
void view_draw_pause_menu(const GameBoard *board);

/** @brief Render the game-over screen. */
void view_draw_game_over_screen(const GameBoard *board);

/* ================================================================
 *  Utilities
 * ================================================================ */

/**
 * @brief Set the OS window title bar text.
 * @param[in] title  UTF-8 title string.
 */
void view_set_window_title(const char *title);

/**
 * @brief Query whether the SDL window received a close event.
 * @return @c true if the window should be closed.
 */
bool view_should_close_window(void);

/**
 * @brief Check whether all gems have reached their target positions.
 *
 * Reads @c board->animations_settled (set by view_update_animations).
 *
 * @param[in] board  Board to query.
 * @return @c true if every gem is settled (or board is NULL).
 */
bool view_all_gems_settled(const GameBoard *board);

/* ================================================================
 *  Audio
 * ================================================================ */

/**
 * @brief Play a named sound effect asynchronously.
 *
 * Looks up @p sound_name in the loaded SFX table.  If the sound is
 * not loaded, this function is a silent no-op.
 *
 * @brief Play a short sound effect asynchronously.
 *
 * @param event_name Key identifier for the sound (e.g., "swap", "match").
 */
void view_play_sound_effect(const char *event_name);

/**
 * @brief Spawn visual particles at the specified location.
 *
 * @param cx Center X pixel coordinate.
 * @param cy Center Y pixel coordinate.
 * @param gem_type The color type of the gem to inherit colors from.
 */
void view_spawn_particles(float cx, float cy, uint8_t gem_type);

/**
 * @brief Start or switch the background music.
 * @param[in] state  0 for Main Menu BGM, 1 for In-game BGM.
 */
void view_set_bgm(int state);

bool view_has_badge(void);

#endif /* MATCH3_VIEW_H */

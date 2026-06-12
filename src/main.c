

#include "model.h"
#include "view.h"
#include "controller.h"
#include "types.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ================================================================
 *  Entry point
 * ================================================================ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

    /* ---- Initialise subsystems ---- */
    if (!view_init_window()) {
        fprintf(stderr, "[main] view_init_window failed\n");
        return EXIT_FAILURE;
    }

    if (!view_load_assets()) {
        fprintf(stderr, "[main] view_load_assets failed\n");
        view_destroy_window();
        return EXIT_FAILURE;
    }

    if (!controller_init()) {
        fprintf(stderr, "[main] controller_init failed\n");
        view_unload_assets();
        view_destroy_window();
        return EXIT_FAILURE;
    }

    /* ---- Initialise game board ---- */
    GameBoard board;

    if (!model_init_board(&board)) {
        fprintf(stderr, "[main] model_init_board failed\n");
        controller_destroy();
        view_unload_assets();
        view_destroy_window();
        return EXIT_FAILURE;
    }

    /* Try to restore high_score, coins and difficulty from a previous save */
    {
        GameBoard temp;
        if (model_load_game(&temp, "match3_save.dat")) {
            board.high_score = temp.high_score;
            board.total_coins = temp.total_coins;
            board.unlocked_difficulty = temp.unlocked_difficulty;
            board.consecutive_fails_normal = temp.consecutive_fails_normal;
            board.consecutive_fails_hard = temp.consecutive_fails_hard;
            board.prop_hammer_count = temp.prop_hammer_count;
            board.prop_wand_count = temp.prop_wand_count;
            board.prop_shuffle_count = temp.prop_shuffle_count;
            board.prop_moves_count = temp.prop_moves_count;
        } else {
            /* First time playing (no save file), give initial gifts */
            board.total_coins = 500;
            board.prop_hammer_count = 1;
            board.prop_wand_count = 1;
            board.prop_shuffle_count = 1;
            board.prop_moves_count = 1;
        }
    }

    view_set_window_title("Match-3 | A cross-platform puzzle game");
    view_set_bgm(0);

    /* ---- Main loop ---- */
    bool   running    = true;
    Uint64 last_ticks = SDL_GetTicks64();

    while (running) {
        /* --- Frame start timestamp --- */
        Uint64 frame_start = SDL_GetTicks64();

        /* --- Delta time (seconds), clamped to 50 ms max --- */
        float dt = (float)(frame_start - last_ticks) / 1000.0f;
        last_ticks = frame_start;
        if (dt > 0.05f)
            dt = 0.05f;

        /* --- Event drain (non-blocking) --- */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            controller_handle_event(&board, &event);
        }

        if (controller_wants_quit())
            running = false;

        if (!running)
            break;

        /* --- Logic update --- */
        bool in_game = (board.current_state != GAME_STATE_MAIN_MENU    &&
                        board.current_state != GAME_STATE_DIFFICULTY_SELECTION &&
                        board.current_state != GAME_STATE_PAUSED        &&
                        board.current_state != GAME_STATE_GAME_OVER);

        if (in_game) {
            view_update_animations(&board, dt);
            controller_update_state_machine(&board, dt);
        }

        /* --- Render --- */
        view_render_frame(&board);

        /* --- Frame-rate cap: sleep for remainder of 16 ms budget --- */
        Uint64 frame_elapsed = SDL_GetTicks64() - frame_start;
        if (frame_elapsed < (Uint64)FRAME_MS)
            SDL_Delay((Uint32)(FRAME_MS - frame_elapsed));
    }

    /* ---- Cleanup (reverse init order) ---- */
    model_destroy_board(&board);
    controller_destroy();
    view_unload_assets();
    view_destroy_window();

    return EXIT_SUCCESS;
}

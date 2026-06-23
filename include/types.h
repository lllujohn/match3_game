
#ifndef MATCH3_TYPES_H
#define MATCH3_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define BOARD_WIDTH    8

#define BOARD_HEIGHT   8

#define GEM_SIZE       50

#define BOARD_OFFSET_X 238

#define BOARD_OFFSET_Y 224


#define WINDOW_WIDTH  881
#define WINDOW_HEIGHT 868


#define MAX_GEM_TYPES  7


#define TARGET_FPS    60

#define FRAME_MS      (1000u / TARGET_FPS)

#define DEFAULT_MOVES  30
#define EASY_MOVES     50
#define NORMAL_MOVES   30
#define HARD_MOVES     15

#define LERP_SPEED_SWAP      14.0f

#define LERP_SPEED_GRAVITY   18.0f

#define LERP_SPEED_REFILL    12.0f

#define ANIM_SETTLE_THRESH   0.5f

typedef enum {
    GAME_STATE_MAIN_MENU           = 0,
    GAME_STATE_DIFFICULTY_SELECTION,
    GAME_STATE_WAITING_INPUT       = 2,
    GAME_STATE_FIRST_GEM_SELECT,
    GAME_STATE_SWAP_ANIMATING,
    GAME_STATE_SWAP_FAIL_ANIMATING,
    GAME_STATE_ELIMINATION_CHECK,
    GAME_STATE_ELIMINATING,
    GAME_STATE_GRAVITY_APPLY,
    GAME_STATE_REFILL,
    GAME_STATE_DEAD_END_ANIM,
    GAME_STATE_GAME_OVER,
    GAME_STATE_PAUSED,
    GAME_STATE_PROP_HAMMER_WAITING,
    GAME_STATE_PROP_WAND_FIRST_SEL,
    GAME_STATE_PROP_WAND_SECOND_SEL,
    GAME_STATE_PROP_SHUFFLE_CONFIRM,
    GAME_STATE_PROP_MOVES_CONFIRM,
    GAME_STATE_PROP_BUY_CONFIRM,
    GAME_STATE_RULES
} GameState;


typedef enum {
    GEM_RED      = 0,
    GEM_MINT     = 1,
    GEM_BLUE     = 2,
    GEM_AMBER    = 3,
    GEM_LAVENDER = 4,
    GEM_CORAL    = 5,
    GEM_WILDCARD = 6,
    GEM_EMPTY    = 255
} GemColorType;


typedef enum {
    BOMB_NONE   = 0,
    BOMB_LINE_H,
    BOMB_LINE_V,
    BOMB_CROSS,
    BOMB_RADIUS
} BombType;

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t gem_type;
    int     bomb_type;
    int     next_bomb_type;
    uint8_t next_gem_type_override;

    bool  is_stone;
    bool  has_ice;
    bool  is_marked_for_elimination;
    float animation_progress;


    float screen_x;
    float screen_y;
    float target_x;
    float target_y;
    float elim_scale;
} Gem;


typedef struct {
    Gem       board[BOARD_HEIGHT][BOARD_WIDTH];
    uint32_t  score;
    uint32_t  high_score;
    uint32_t  moves_remaining;
    int       level;
    int       difficulty;


    uint32_t  target_score;
    uint8_t   max_props_per_game;
    uint8_t   max_sandglass_per_game;
    float     hint_trigger_time;
    float     wildcard_prob;
    uint32_t  buy_prop_price;
    uint8_t   used_props_total;
    uint8_t   used_sandglass_count;
    uint32_t  max_combo_this_game;
    uint8_t   stars_earned;

    GameState current_state;
    GameState previous_state;

    uint8_t   selected_row;
    uint8_t   selected_col;
    bool      first_gem_selected;

    uint8_t   hover_row;
    uint8_t   hover_col;

    int       highlighted_difficulty;
    int       highlighted_menu_option;

    float     state_timer;
    float     animation_duration;
    uint32_t  combo_multiplier;
    bool      animations_settled;


    uint8_t   undo_r1;
    uint8_t   undo_c1;
    uint8_t   undo_r2;
    uint8_t   undo_c2;
    bool      undo_available;
    uint32_t  undo_score;
    uint32_t  undo_combo;


    float     combo_popup_timer;
    uint32_t  combo_popup_value;
    

    uint32_t  total_coins;
    int       unlocked_difficulty;
    int       consecutive_fails_normal;
    int       consecutive_fails_hard;
    uint8_t   prop_hammer_count;
    uint8_t   prop_wand_count;
    uint8_t   prop_shuffle_count;
    uint8_t   prop_moves_count;
    

    float     idle_timer;
    bool      has_hint;
    uint8_t   hint_r;
    uint8_t   hint_c;
    uint8_t   hint_dir;
} GameBoard;


typedef struct {
    uint8_t row;
    uint8_t col;
} Position;


typedef struct {
    Position positions[BOARD_WIDTH * BOARD_HEIGHT];
    uint8_t  count;
} EliminationSet;

#endif

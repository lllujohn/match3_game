
#ifndef MATCH3_TYPES_H
#define MATCH3_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 *  Board layout constants
 * ================================================================ */

/** Number of columns on the game board. */
#define BOARD_WIDTH    8
/** Number of rows on the game board. */
#define BOARD_HEIGHT   8
/** Width/height in pixels of a single gem cell (used for Lerp targets). */
#define GEM_SIZE       50
/** X pixel offset of the board's top-left corner. */
#define BOARD_OFFSET_X 238
/** Y pixel offset of the board's top-left corner. */
#define BOARD_OFFSET_Y 224

/** Window dimensions. */
#define WINDOW_WIDTH  881
#define WINDOW_HEIGHT 868

/** Maximum number of distinct gem colour types (0 to 6). */
#define MAX_GEM_TYPES  7

/** Target frames per second. */
#define TARGET_FPS    60
/** Target milliseconds per frame (integer). */
#define FRAME_MS      (1000u / TARGET_FPS)

/* ================================================================
 *  Move budgets per difficulty level
 * ================================================================ */
#define DEFAULT_MOVES  30  /**< Default if no difficulty is set. */
#define EASY_MOVES     50  /**< Easy difficulty. */
#define NORMAL_MOVES   30  /**< Normal difficulty. */
#define HARD_MOVES     15  /**< Hard difficulty. */

/* ================================================================
 *  Animation constants (shared between View and Controller)
 * ================================================================ */

/** Lerp speed coefficient for gem-swap animation (higher = faster). */
#define LERP_SPEED_SWAP      14.0f
/** Lerp speed coefficient for gravity-drop animation. */
#define LERP_SPEED_GRAVITY   18.0f
/** Lerp speed coefficient for new-gem fall-in animation. */
#define LERP_SPEED_REFILL    12.0f
/** Pixel distance below which a gem is considered "settled". */
#define ANIM_SETTLE_THRESH   0.5f

/* ================================================================
 *  Enumerations
 * ================================================================ */

/**
 * @brief All possible states of the game's finite state machine.
 *
 * Transitions are driven by Controller; View renders accordingly.
 */
typedef enum {
    GAME_STATE_MAIN_MENU           = 0, /**< Title/main menu screen.        */
    GAME_STATE_DIFFICULTY_SELECTION,    /**< Difficulty picker.             */
    GAME_STATE_WAITING_INPUT       = 2, /**< Idle — awaiting first click.   */
    GAME_STATE_FIRST_GEM_SELECT,        /**< First gem selected, waiting 2nd.*/
    GAME_STATE_SWAP_ANIMATING,          /**< Swap Lerp in progress.         */
    GAME_STATE_SWAP_FAIL_ANIMATING,     /**< Invalid swap bounce-back anim. */
    GAME_STATE_ELIMINATION_CHECK,       /**< Check & score eliminations.    */
    GAME_STATE_ELIMINATING,             /**< Shrink-to-zero anim.           */
    GAME_STATE_GRAVITY_APPLY,           /**< Gems falling down.             */
    GAME_STATE_REFILL,                  /**< New gems dropping in.          */
    GAME_STATE_PAUSED,                  /**< Game is paused.                */
    GAME_STATE_GAME_OVER,               /**< No moves left / deadlock.      */
    GAME_STATE_PROP_HAMMER_WAITING,     /**< Waiting for hammer target.     */
    GAME_STATE_PROP_WAND_FIRST_SEL,     /**< Waiting for 1st wand target.   */
    GAME_STATE_PROP_WAND_SECOND_SEL,    /**< Waiting for 2nd wand target.   */
    GAME_STATE_PROP_SHUFFLE_CONFIRM,    /**< Waiting to confirm shuffle.    */
    GAME_STATE_PROP_MOVES_CONFIRM,      /**< Waiting to confirm +5 moves.   */
    GAME_STATE_PROP_BUY_CONFIRM         /**< Waiting to confirm prop buy.   */
} GameState;

/**
 * @brief Gem colour identifiers.
 *
 * Values 0‥MAX_GEM_TYPES-1 are valid coloured gems.
 * GEM_EMPTY (255) marks a vacant cell.
 */
typedef enum {
    GEM_RED      = 0,
    GEM_MINT     = 1,
    GEM_BLUE     = 2,
    GEM_AMBER    = 3,
    GEM_LAVENDER = 4,
    GEM_CORAL    = 5,
    GEM_WILDCARD = 6,   /**< Wildcard that matches any color. */
    GEM_EMPTY    = 255  /**< Empty cell sentinel. */
} GemColorType;

/**
 * @brief Special bomb/power-up types that can be assigned to a gem.
 */
typedef enum {
    BOMB_NONE   = 0, /**< Regular gem.               */
    BOMB_LINE_H,     /**< Clears entire row.         */
    BOMB_LINE_V,     /**< Clears entire column.      */
    BOMB_CROSS,      /**< Clears row + column.       */
    BOMB_RADIUS      /**< Clears 3×3 radius.         */
} BombType;

/* ================================================================
 *  Core data structures
 * ================================================================ */

/**
 * @brief Represents a single gem on the board.
 *
 * Pixel coordinates (screen_x/y, target_x/y) are maintained here so
 * the View can Lerp them each frame without an extra allocation.
 * The Model sets target_x/y; the View moves screen_x/y toward them.
 */
typedef struct {
    uint8_t row;      /**< Logical row index [0, BOARD_HEIGHT). */
    uint8_t col;      /**< Logical column index [0, BOARD_WIDTH). */
    uint8_t gem_type; /**< GemColorType value; GEM_EMPTY = vacant. */
    int     bomb_type;/**< BombType value. */
    int     next_bomb_type; /**< BombType to become after this elimination pass. */

    bool  is_stone;                  /**< True if this is an unmovable stone. */
    bool  has_ice;                   /**< True if this gem is covered in ice. */
    bool  is_marked_for_elimination; /**< Set by model_check_eliminations(). */
    float animation_progress;        /**< General-purpose anim progress 0‥1. */

    /* ---- Lerp animation state (updated by View each frame) ---- */
    float screen_x;  /**< Current rendered X position (pixels, centre).  */
    float screen_y;  /**< Current rendered Y position (pixels, centre).  */
    float target_x;  /**< Logical target X position (pixels, centre).    */
    float target_y;  /**< Logical target Y position (pixels, centre).    */
    float elim_scale;/**< Scale factor during elimination anim: 1.0→0.0. */
} Gem;

/**
 * @brief Full game state — board, score, FSM, and UI selection.
 *
 * Passed by pointer through every layer; the Controller mutates it,
 * the View reads it, and the Model owns the board sub-struct.
 */
typedef struct {
    Gem       board[BOARD_HEIGHT][BOARD_WIDTH]; /**< The 8×8 gem grid.      */
    uint32_t  score;               /**< Current session score.               */
    uint32_t  high_score;          /**< All-time high score (persisted).     */
    uint32_t  moves_remaining;     /**< Moves left before game-over.         */
    int       level;               /**< Current level (future use).          */
    int       difficulty;          /**< 0=Easy, 1=Normal, 2=Hard.           */

    /* ---- Difficulty configuration & in-game stats ---- */
    uint32_t  target_score;           /**< Score required to clear the level.   */
    uint8_t   max_props_per_game;     /**< Max total props usable per game.     */
    uint8_t   max_sandglass_per_game; /**< Max sandglass usable per game.       */
    float     hint_trigger_time;      /**< Seconds before showing a hint.       */
    float     wildcard_prob;          /**< Probability to spawn a wildcard.     */
    uint32_t  buy_prop_price;         /**< Coin cost to buy a prop in-game.     */
    uint8_t   used_props_total;       /**< Total props used this session.       */
    uint8_t   used_sandglass_count;   /**< Sandglass used this session.         */
    uint32_t  max_combo_this_game;    /**< Highest combo achieved this session. */
    uint8_t   stars_earned;           /**< Stars earned upon game over (1-3).   */

    GameState current_state;       /**< Active FSM state.                    */
    GameState previous_state;      /**< Used for pause/resume.              */

    uint8_t   selected_row;        /**< Row of first selected gem.           */
    uint8_t   selected_col;        /**< Column of first selected gem.        */
    bool      first_gem_selected;  /**< True when first gem has been picked. */

    uint8_t   hover_row;           /**< Row under mouse cursor.              */
    uint8_t   hover_col;           /**< Column under mouse cursor.           */

    int       highlighted_difficulty;   /**< Highlighted entry in diff menu. */
    int       highlighted_menu_option;  /**< Highlighted button in any menu. */

    float     state_timer;         /**< Seconds elapsed in current state.    */
    float     animation_duration;  /**< Expected duration of current anim.   */
    uint32_t  combo_multiplier;    /**< Cascade combo multiplier (starts 1). */
    bool      animations_settled;  /**< True when all gems reached targets.  */

    /* ---- Undo history (depth-1 ring buffer) ---- */
    uint8_t   undo_r1;             /**< Row of gem 1 in last swap.           */
    uint8_t   undo_c1;             /**< Col of gem 1 in last swap.           */
    uint8_t   undo_r2;             /**< Row of gem 2 in last swap.           */
    uint8_t   undo_c2;             /**< Col of gem 2 in last swap.           */
    bool      undo_available;      /**< True when an undo is possible.       */
    uint32_t  undo_score;          /**< Score before the last swap.          */
    uint32_t  undo_combo;          /**< Combo multiplier before last swap.   */

    /* ---- Visual FX ---- */
    float     combo_popup_timer;   /**< Countdown for combo pop-up text (s). */
    uint32_t  combo_popup_value;   /**< Multiplier value shown in pop-up.    */
    
    /* ---- Economy, Meta-Progression and Props ---- */
    uint32_t  total_coins;         /**< Persists across rounds (Economy).    */
    int       unlocked_difficulty; /**< Max difficulty unlocked (0-2).       */
    int       consecutive_fails_normal; /**< Fails in a row on Normal.       */
    int       consecutive_fails_hard;   /**< Fails in a row on Hard.         */
    uint8_t   prop_hammer_count;   /**< Hammer prop inventory count.         */
    uint8_t   prop_wand_count;     /**< Wand prop inventory count.           */
    uint8_t   prop_shuffle_count;  /**< Shuffle prop inventory count.        */
    uint8_t   prop_moves_count;    /**< Extra Moves prop inventory count.    */
    
    /* ---- Hint System ---- */
    float     idle_timer;          /**< Seconds elapsed without user input.  */
    bool      has_hint;            /**< True if a hint is available.         */
    uint8_t   hint_r;              /**< Row of the hint gem.                 */
    uint8_t   hint_c;              /**< Col of the hint gem.                 */
    uint8_t   hint_dir;            /**< 0=right, 1=down.                     */
} GameBoard;

/**
 * @brief Compact (row, col) pair used in EliminationSet.
 */
typedef struct {
    uint8_t row; /**< Board row. */
    uint8_t col; /**< Board column. */
} Position;

/**
 * @brief Set of positions marked for elimination in one pass.
 *
 * Size is bounded by the total number of cells on the board.
 */
typedef struct {
    Position positions[BOARD_WIDTH * BOARD_HEIGHT]; /**< Array of positions. */
    uint8_t  count;                                 /**< Number of entries.  */
} EliminationSet;

#endif /* MATCH3_TYPES_H */

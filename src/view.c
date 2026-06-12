#include "view.h"
#include "types.h"
#include "model.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ================================================================
 *  Colour palette  (Apple-inspired dark-mode Morandi system)
 * ================================================================ */

static const SDL_Color kBgColor             = {20,  15,  45,  255};
static const SDL_Color kPanelColor          = {35,  30,  65,  255};
static const SDL_Color kPanelBorderColor   = {60,  50,  100, 255};
static const SDL_Color kAccentBlueColor    = {0,   210, 255, 255};
static const SDL_Color kAccentGreenColor   = {0,   255, 150, 255};
static const SDL_Color kTextPrimaryColor   = {255, 255, 255, 255};
static const SDL_Color kTextSecondaryColor = {240, 230, 255, 255};
static const SDL_Color kTextHintColor      = {200, 190, 230, 255};
static const SDL_Color kMarkRingColor      = {255, 215, 0,   200};
static const SDL_Color kDangerColor         = {255, 80,  110, 255};

/* Morandi gem fill colours */
static const SDL_Color kGemFill[MAX_GEM_TYPES] = {
    {224, 108, 108, 255},  /* RED      */
    {86,  194, 155, 255},  /* MINT     */
    {85,  143, 220, 255},  /* BLUE     */
    {228, 177, 79,  255},  /* AMBER    */
    {162, 119, 200, 255},  /* LAVENDER */
    {229, 140, 93,  255},  /* CORAL    */
    {255, 255, 255, 255},  /* WILDCARD */
};

/* Specular highlight (upper-third gloss) */
static const SDL_Color kGemHighlight[MAX_GEM_TYPES] = {
    {255, 170, 170, 160},
    {160, 230, 210, 160},
    {160, 200, 245, 160},
    {255, 225, 150, 160},
    {210, 180, 235, 160},
    {255, 200, 160, 160},
    {255, 255, 255, 160},
};

/* Corner radii (pixels) */
#define GEM_CORNER_R   10
#define BTN_CORNER_R   14
#define PANEL_CORNER_R 18

/* ================================================================
 *  View module private state
 * ================================================================ */

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;

    /* Font sizes used in the UI */
    TTF_Font     *font_hint;    /* ~13 px  */
    TTF_Font     *font_body;    /* ~18 px  */
    TTF_Font     *font_medium;  /* ~24 px  */
    TTF_Font     *font_large;   /* ~36 px  */
    TTF_Font     *font_title;   /* ~52 px  */

    /* Sound effects */
    Mix_Chunk    *sfx_swap;
    Mix_Chunk    *sfx_match;
    Mix_Chunk    *sfx_error;
    Mix_Chunk    *sfx_combo;
    Mix_Chunk    *sfx_game_over;
    Mix_Chunk    *sfx_start;
    Mix_Chunk    *sfx_clear;
    Mix_Chunk    *sfx_pao;
    
    Mix_Music    *bgm_main;
    Mix_Music    *bgm_game;

    SDL_Texture  *tex_nuist_badge;
    SDL_Texture  *tex_gem[MAX_GEM_TYPES];
    
    SDL_Texture  *tex_prop_hammer;
    SDL_Texture  *tex_prop_wand;
    SDL_Texture  *tex_prop_shuffle;
    SDL_Texture  *tex_prop_moves;
    
    SDL_Texture  *tex_hammer;
    SDL_Texture  *tex_wand;
    SDL_Texture  *tex_sandglass;
    SDL_Texture  *tex_ice;
    SDL_Texture  *tex_dead_end;
    
    SDL_Texture  *tex_bg;
    SDL_Texture  *tex_bg_main;
    
    SDL_Texture  *tex_stone;

    bool          sdl_ok;
    bool          ttf_ok;
    bool          img_ok;
    bool          mix_ok;
    bool          should_close;
} ViewState;

static ViewState g_view;

/* ================================================================
 *  Particle System
 * ================================================================ */

#define MAX_PARTICLES 200

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    float max_life;
    SDL_Color color;
    bool active;
    float size;
} Particle;

static Particle g_particles[MAX_PARTICLES];

/* ================================================================
 *  Font search — tries several paths in order
 * ================================================================ */

static TTF_Font *load_font_any_path(int size)
{
    static const char *PATHS[] = {
        /* Bundled asset (preferred) */
        "assets/fonts/NotoSans-Regular.ttf",
        "assets/fonts/DejaVuSans.ttf",
        "../assets/fonts/NotoSans-Regular.ttf",
        "../assets/fonts/DejaVuSans.ttf",
        /* macOS */
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial Unicode.ttf",
        /* Linux */
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        /* Windows */
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        NULL
    };

    for (int i = 0; PATHS[i]; i++) {
        TTF_Font *f = TTF_OpenFont(PATHS[i], size);
        if (f)
            return f;
    }

    fprintf(stderr, "[view] WARNING: no font found for size %d — text disabled\n", size);
    return NULL;
}

/* ================================================================
 *  Primitive drawing helpers
 * ================================================================ */

static inline void set_color(SDL_Renderer *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static inline void set_blend(SDL_Renderer *r, SDL_BlendMode m)
{
    SDL_SetRenderDrawBlendMode(r, m);
}

/**
 * @brief Fill a circle using horizontal scanlines (Bresenham).
 */
static void fill_circle(SDL_Renderer *r, int cx, int cy, int radius,
                         SDL_Color color)
{
    set_color(r, color);
    set_blend(r, SDL_BLENDMODE_BLEND);

    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}


/**
 * @brief Draw a quarter circle outline using the midpoint algorithm.
 * @param quad  1=bottom-right, 2=bottom-left, 3=top-left, 4=top-right
 */
static void draw_quarter_circle_outline(SDL_Renderer *r, int cx, int cy, int radius,
                                 SDL_Color color, int thickness, int quad)
{
    set_color(r, color);
    set_blend(r, SDL_BLENDMODE_BLEND);

    for (int t = 0; t < thickness; t++) {
        int rad = radius - t;
        int x   = rad, y = 0, err = 0;
        while (x >= y) {
            switch (quad) {
                case 1:
                    SDL_RenderDrawPoint(r, cx + x, cy + y);
                    SDL_RenderDrawPoint(r, cx + y, cy + x);
                    break;
                case 2:
                    SDL_RenderDrawPoint(r, cx - x, cy + y);
                    SDL_RenderDrawPoint(r, cx - y, cy + x);
                    break;
                case 3:
                    SDL_RenderDrawPoint(r, cx - x, cy - y);
                    SDL_RenderDrawPoint(r, cx - y, cy - x);
                    break;
                case 4:
                    SDL_RenderDrawPoint(r, cx + x, cy - y);
                    SDL_RenderDrawPoint(r, cx + y, cy - x);
                    break;
            }
            y++;
            if (err <= 0)      { err += 2 * y + 1; }
            if (err > 0)       { x--; err -= 2 * x + 1; }
        }
    }
}

/**
 * @brief Fill one quarter-circle.
 * @param quad  1=bottom-right, 2=bottom-left, 3=top-left, 4=top-right
 */
static void fill_quarter_circle(SDL_Renderer *r, int cx, int cy, int radius,
                                  int quad)
{
    for (int dy = 0; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius * radius - dy * dy));
        int x1, x2, y;
        switch (quad) {
            case 1: x1 = cx;      x2 = cx + dx; y = cy + dy; break;
            case 2: x1 = cx - dx; x2 = cx;      y = cy + dy; break;
            case 3: x1 = cx - dx; x2 = cx;      y = cy - dy; break;
            case 4: x1 = cx;      x2 = cx + dx; y = cy - dy; break;
            default: return;
        }
        SDL_RenderDrawLine(r, x1, y, x2, y);
    }
}

/**
 * @brief Fill a rounded rectangle (no border).
 */
static void fill_rounded_rect(SDL_Renderer *r, int x, int y, int w, int h,
                               int cr, SDL_Color color)
{
    set_color(r, color);
    set_blend(r, SDL_BLENDMODE_BLEND);

    if (cr < 1 || w < 2 * cr || h < 2 * cr) {
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderFillRect(r, &rect);
        return;
    }

    /* Centre + left + right strips */
    SDL_Rect rects[3] = {
        {x + cr, y,      w - 2 * cr, h     },
        {x,      y + cr, cr,          h - 2 * cr},
        {x + w - cr, y + cr, cr,      h - 2 * cr},
    };
    SDL_RenderFillRects(r, rects, 3);

    /* Four corner quadrants */
    fill_quarter_circle(r, x + cr,     y + cr,     cr, 3);
    fill_quarter_circle(r, x + w - cr, y + cr,     cr, 4);
    fill_quarter_circle(r, x + cr,     y + h - cr, cr, 2);
    fill_quarter_circle(r, x + w - cr, y + h - cr, cr, 1);
}

/**
 * @brief Draw a rounded rectangle outline.
 */
static void draw_rounded_rect_outline(SDL_Renderer *r, int x, int y,
                                       int w, int h, int cr,
                                       SDL_Color color, int thickness)
{
    set_color(r, color);
    set_blend(r, SDL_BLENDMODE_BLEND);

    for (int t = 0; t < thickness; t++) {
        int xi = x + t, yi = y + t, wi = w - 2 * t, hi = h - 2 * t;
        int ri = cr - t;
        if (ri < 1) ri = 1;

        /* Four straight edges */
        SDL_RenderDrawLine(r, xi + ri, yi, xi + wi - ri, yi);
        SDL_RenderDrawLine(r, xi + ri, yi + hi, xi + wi - ri, yi + hi);
        SDL_RenderDrawLine(r, xi, yi + ri, xi, yi + hi - ri);
        SDL_RenderDrawLine(r, xi + wi, yi + ri, xi + wi, yi + hi - ri);

        /* Four corners (arcs only — no fill) */
        draw_quarter_circle_outline(r, xi + ri,      yi + ri,      ri, color, 1, 3);
        draw_quarter_circle_outline(r, xi + wi - ri, yi + ri,      ri, color, 1, 4);
        draw_quarter_circle_outline(r, xi + ri,      yi + hi - ri, ri, color, 1, 2);
        draw_quarter_circle_outline(r, xi + wi - ri, yi + hi - ri, ri, color, 1, 1);
    }
}



void view_spawn_particles(float cx, float cy, uint8_t gem_type)
{
    if (gem_type >= MAX_GEM_TYPES) return;
    
    SDL_Color c = kGemFill[gem_type];
    
    for (int i = 0; i < 8; i++) { /* spawn 8 particles */
        for (int j = 0; j < MAX_PARTICLES; j++) {
            if (!g_particles[j].active) {
                g_particles[j].active = true;
                g_particles[j].x = cx + (rand() % 20 - 10);
                g_particles[j].y = cy + (rand() % 20 - 10);
                g_particles[j].vx = (rand() % 100 - 50) * 1.5f;
                g_particles[j].vy = (rand() % 100 - 50) * 1.5f;
                g_particles[j].max_life = 0.3f + (rand() % 30) / 100.0f;
                g_particles[j].life = g_particles[j].max_life;
                g_particles[j].size = 6.0f + (rand() % 5);
                g_particles[j].color = c;
                break;
            }
        }
    }
}

/* ================================================================
 *  Global getters===
 *  Text rendering
 * ================================================================ */

static void draw_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                      int x, int y, SDL_Color color)
{
    if (!font || !text || text[0] == '\0')
        return;

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf)
        return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void draw_text_centered(SDL_Renderer *r, TTF_Font *font,
                                const char *text, int cx, int cy,
                                SDL_Color color)
{
    if (!font || !text)
        return;
    int tw, th;
    TTF_SizeUTF8(font, text, &tw, &th);
    
    /* Simple drop shadow for better readability on complex backgrounds */
    SDL_Color shadow = {0, 0, 0, 150};
    draw_text(r, font, text, cx - tw / 2 + 2, cy - th / 2 + 2, shadow);
    
    draw_text(r, font, text, cx - tw / 2, cy - th / 2, color);
}

/* ================================================================
 *  Lerp helper
 * ================================================================ */

static inline float lerp(float cur, float tgt, float speed, float dt)
{
    /* Premium framerate-independent exponential ease-out */
    return tgt + (cur - tgt) * expf(-speed * dt);
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

bool view_init_window(void)
{
    memset(&g_view, 0, sizeof(g_view));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[view] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    g_view.sdl_ok = true;

    g_view.window = SDL_CreateWindow(
        "Match-3 Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!g_view.window) {
        fprintf(stderr, "[view] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_view.renderer = SDL_CreateRenderer(
        g_view.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!g_view.renderer) {
        /* Fallback: software renderer */
        g_view.renderer = SDL_CreateRenderer(g_view.window, -1,
                                             SDL_RENDERER_SOFTWARE);
        if (!g_view.renderer) {
            fprintf(stderr, "[view] SDL_CreateRenderer failed: %s\n", SDL_GetError());
            return false;
        }
    }

    SDL_RenderSetLogicalSize(g_view.renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); /* bilinear filtering */

    /* SDL2_ttf */
    if (TTF_Init() == 0) {
        g_view.ttf_ok = true;
    } else {
        fprintf(stderr, "[view] TTF_Init failed: %s\n", TTF_GetError());
    }

    /* SDL2_image */
    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) {
        g_view.img_ok = true;
    } else {
        fprintf(stderr, "[view] IMG_Init failed: %s\n", IMG_GetError());
    }

    /* SDL2_mixer */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
        g_view.mix_ok = true;
    } else {
        fprintf(stderr, "[view] Mix_OpenAudio failed: %s\n", Mix_GetError());
    }

    return true;
}

void view_destroy_window(void)
{
    if (g_view.sfx_swap)      { Mix_FreeChunk(g_view.sfx_swap);      g_view.sfx_swap      = NULL; }
    if (g_view.sfx_match)     { Mix_FreeChunk(g_view.sfx_match);     g_view.sfx_match     = NULL; }
    if (g_view.sfx_error)     { Mix_FreeChunk(g_view.sfx_error);     g_view.sfx_error     = NULL; }
    if (g_view.sfx_combo)     { Mix_FreeChunk(g_view.sfx_combo);     g_view.sfx_combo     = NULL; }
    if (g_view.sfx_game_over) { Mix_FreeChunk(g_view.sfx_game_over); g_view.sfx_game_over = NULL; }
    if (g_view.sfx_start)     { Mix_FreeChunk(g_view.sfx_start);     g_view.sfx_start     = NULL; }
    if (g_view.sfx_clear)     { Mix_FreeChunk(g_view.sfx_clear);     g_view.sfx_clear     = NULL; }
    if (g_view.sfx_pao)       { Mix_FreeChunk(g_view.sfx_pao);       g_view.sfx_pao       = NULL; }

    if (g_view.bgm_main) { Mix_FreeMusic(g_view.bgm_main); g_view.bgm_main = NULL; }
    if (g_view.bgm_game) { Mix_FreeMusic(g_view.bgm_game); g_view.bgm_game = NULL; }

    if (g_view.tex_nuist_badge) { SDL_DestroyTexture(g_view.tex_nuist_badge); g_view.tex_nuist_badge = NULL; }
    for (int i = 0; i < MAX_GEM_TYPES; i++) {
        if (g_view.tex_gem[i]) {
            SDL_DestroyTexture(g_view.tex_gem[i]);
            g_view.tex_gem[i] = NULL;
        }
    }
    if (g_view.img_ok) {
        IMG_Quit();
        g_view.img_ok = false;
    }
    if (g_view.mix_ok) {
        Mix_HaltMusic();
        Mix_CloseAudio();
        g_view.mix_ok = false;
    }
    if (g_view.ttf_ok) {
        TTF_Quit();
        g_view.ttf_ok = false;
    }
    if (g_view.renderer) {
        SDL_DestroyRenderer(g_view.renderer);
        g_view.renderer = NULL;
    }
    if (g_view.window) {
        SDL_DestroyWindow(g_view.window);
        g_view.window = NULL;
    }
    if (g_view.sdl_ok) {
        SDL_Quit();
        g_view.sdl_ok = false;
    }
}

static Mix_Chunk *load_wav_fallback(const char *path)
{
    Mix_Chunk *c = Mix_LoadWAV(path);
    if (!c) {
        char buf[256];
        snprintf(buf, sizeof(buf), "../%s", path);
        c = Mix_LoadWAV(buf);
    }
    return c;
}

static Mix_Music *load_mus_fallback(const char *path)
{
    Mix_Music *m = Mix_LoadMUS(path);
    if (!m) {
        char buf[256];
        snprintf(buf, sizeof(buf), "../%s", path);
        m = Mix_LoadMUS(buf);
    }
    return m;
}

static SDL_Texture *load_tex_fallback(const char *path)
{
    SDL_Texture *tex = IMG_LoadTexture(g_view.renderer, path);
    if (!tex) {
        char buf[256];
        snprintf(buf, sizeof(buf), "../%s", path);
        tex = IMG_LoadTexture(g_view.renderer, buf);
    }
    if (!tex) {
        fprintf(stderr, "[view] failed to load texture %s: %s\n", path, IMG_GetError());
    }
    return tex;
}

bool view_load_assets(void)
{
    /* Fonts — best-effort; NULL fonts cause text to be silently skipped */
    if (g_view.ttf_ok) {
        g_view.font_hint   = load_font_any_path(13);
        g_view.font_body   = load_font_any_path(18);
        g_view.font_medium = load_font_any_path(24);
        g_view.font_large  = load_font_any_path(36);
        g_view.font_title  = TTF_OpenFont("assets/fonts/SmileySans.ttf", 64);
    }

    /* Sound effects — missing files are silently ignored */
    if (g_view.mix_ok) {
        g_view.sfx_swap      = load_wav_fallback("assets/sounds/swap.wav");
        g_view.sfx_match     = load_wav_fallback("assets/sounds/match.mp3");
        g_view.sfx_error     = load_wav_fallback("assets/sounds/combo_break.wav");
        g_view.sfx_combo     = load_wav_fallback("assets/sounds/combo.wav");
        g_view.sfx_game_over = load_wav_fallback("assets/sounds/game_over.wav");
        g_view.sfx_start     = load_wav_fallback("assets/sounds/start.mp3");
        g_view.sfx_clear     = load_wav_fallback("assets/sounds/clear.wav");
        g_view.sfx_pao       = load_wav_fallback("assets/sounds/pao.wav");

        g_view.bgm_main      = load_mus_fallback("assets/sounds/bgm_main.mp3");
        g_view.bgm_game      = load_mus_fallback("assets/sounds/bgm_game.mp3");
    }

    if (g_view.img_ok) {
        g_view.tex_nuist_badge = load_tex_fallback("assets/images/nuist_badge.png");
        
        g_view.tex_gem[0] = load_tex_fallback("assets/images/1.png");
        g_view.tex_gem[1] = load_tex_fallback("assets/images/2.png");
        g_view.tex_gem[2] = load_tex_fallback("assets/images/3.png");
        g_view.tex_gem[3] = load_tex_fallback("assets/images/4.png");
        g_view.tex_gem[4] = load_tex_fallback("assets/images/5.png");
        g_view.tex_gem[5] = NULL; /* Not used if we only have 5 regular colors */
        g_view.tex_gem[GEM_WILDCARD] = load_tex_fallback("assets/images/6.png");

        g_view.tex_prop_hammer  = load_tex_fallback("assets/images/prop_hammer.png");
        g_view.tex_prop_wand    = load_tex_fallback("assets/images/prop_wand.png");
        g_view.tex_prop_shuffle = load_tex_fallback("assets/images/prop_shuffle.png");
        g_view.tex_prop_moves   = load_tex_fallback("assets/images/prop_moves.png");
        g_view.tex_sandglass    = load_tex_fallback("assets/images/prop_sandglass.png");
        g_view.tex_ice          = load_tex_fallback("assets/images/ice.png");
        g_view.tex_dead_end     = load_tex_fallback("assets/images/dead_end.png");
        
        g_view.tex_bg           = load_tex_fallback("assets/images/bg_game.png");
        g_view.tex_bg_main      = load_tex_fallback("assets/images/bg_main.png");
        
        g_view.tex_stone        = load_tex_fallback("assets/images/stone.png");
    }

    return true; /* always succeed — missing assets degrade gracefully */
}

bool view_has_badge(void)
{
    return g_view.tex_nuist_badge != NULL;
}

void view_unload_assets(void)
{
    if (g_view.font_hint)   { TTF_CloseFont(g_view.font_hint);   g_view.font_hint   = NULL; }
    if (g_view.font_body)   { TTF_CloseFont(g_view.font_body);   g_view.font_body   = NULL; }
    if (g_view.font_medium) { TTF_CloseFont(g_view.font_medium); g_view.font_medium = NULL; }
    if (g_view.font_large)  { TTF_CloseFont(g_view.font_large);  g_view.font_large  = NULL; }
    if (g_view.font_title)  { TTF_CloseFont(g_view.font_title);  g_view.font_title  = NULL; }

    if (g_view.sfx_swap)      { Mix_FreeChunk(g_view.sfx_swap);      g_view.sfx_swap      = NULL; }
    if (g_view.sfx_match)     { Mix_FreeChunk(g_view.sfx_match);     g_view.sfx_match     = NULL; }
    if (g_view.sfx_error)     { Mix_FreeChunk(g_view.sfx_error);     g_view.sfx_error     = NULL; }
    if (g_view.sfx_combo)     { Mix_FreeChunk(g_view.sfx_combo);     g_view.sfx_combo     = NULL; }
    if (g_view.sfx_game_over) { Mix_FreeChunk(g_view.sfx_game_over); g_view.sfx_game_over = NULL; }

    if (g_view.tex_prop_hammer)  { SDL_DestroyTexture(g_view.tex_prop_hammer);  g_view.tex_prop_hammer  = NULL; }
    if (g_view.tex_prop_wand)    { SDL_DestroyTexture(g_view.tex_prop_wand);    g_view.tex_prop_wand    = NULL; }
    if (g_view.tex_prop_shuffle) { SDL_DestroyTexture(g_view.tex_prop_shuffle); g_view.tex_prop_shuffle = NULL; }
    if (g_view.tex_prop_moves)   { SDL_DestroyTexture(g_view.tex_prop_moves);   g_view.tex_prop_moves   = NULL; }
    if (g_view.tex_bg)           { SDL_DestroyTexture(g_view.tex_bg);           g_view.tex_bg           = NULL; }
    if (g_view.tex_bg_main)      { SDL_DestroyTexture(g_view.tex_bg_main);      g_view.tex_bg_main      = NULL; }
    if (g_view.tex_sandglass)    { SDL_DestroyTexture(g_view.tex_sandglass);    g_view.tex_sandglass    = NULL; }
    if (g_view.tex_ice)          { SDL_DestroyTexture(g_view.tex_ice);          g_view.tex_ice          = NULL; }
    if (g_view.tex_dead_end)     { SDL_DestroyTexture(g_view.tex_dead_end);     g_view.tex_dead_end     = NULL; }
    if (g_view.tex_stone)        { SDL_DestroyTexture(g_view.tex_stone);        g_view.tex_stone        = NULL; }
}

/* ================================================================
 *  Audio
 * ================================================================ */

void view_play_sound_effect(const char *sound_name)
{
    if (!g_view.mix_ok || !sound_name)
        return;

    Mix_Chunk *chunk = NULL;
    if      (strcmp(sound_name, "swap")      == 0) chunk = g_view.sfx_swap;
    else if (strcmp(sound_name, "match")     == 0) chunk = g_view.sfx_match;
    else if (strcmp(sound_name, "error")     == 0) chunk = g_view.sfx_error;
    else if (strcmp(sound_name, "combo")     == 0) chunk = g_view.sfx_combo;
    else if (strcmp(sound_name, "game_over") == 0) chunk = g_view.sfx_game_over;
    else if (strcmp(sound_name, "start")     == 0) chunk = g_view.sfx_start;
    else if (strcmp(sound_name, "clear")     == 0) chunk = g_view.sfx_clear;
    else if (strcmp(sound_name, "pao")       == 0) chunk = g_view.sfx_pao;

    if (chunk)
        Mix_PlayChannel(-1, chunk, 0);
}

void view_set_bgm(int state)
{
    if (!g_view.mix_ok)
        return;
    
    Mix_Music *target = (state == 0) ? g_view.bgm_main : g_view.bgm_game;
    
    if (target) {
        Mix_PlayMusic(target, -1);
    } else {
        Mix_HaltMusic();
    }
}

/* ================================================================
 *  Utility
 * ================================================================ */

void view_set_window_title(const char *title)
{
    if (g_view.window && title)
        SDL_SetWindowTitle(g_view.window, title);
}

bool view_should_close_window(void)
{
    return g_view.should_close;
}

bool view_all_gems_settled(const GameBoard *board)
{
    return board ? board->animations_settled : true;
}

/* ================================================================
 *  Animation update  (Lerp each gem toward its target)
 * ================================================================ */

void view_update_animations(GameBoard *board, float dt)
{
    if (!board)
        return;

    float speed = LERP_SPEED_GRAVITY;
    if (board->current_state == GAME_STATE_SWAP_ANIMATING ||
        board->current_state == GAME_STATE_SWAP_FAIL_ANIMATING)
        speed = LERP_SPEED_SWAP;
    else if (board->current_state == GAME_STATE_REFILL)
        speed = LERP_SPEED_REFILL;

    bool all_settled = true;

    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            Gem *g = &board->board[r][c];
            if (g->gem_type == (uint8_t)GEM_EMPTY)
                continue;

            float dx = g->target_x - g->screen_x;
            float dy = g->target_y - g->screen_y;

            if (fabsf(dx) > ANIM_SETTLE_THRESH) {
                g->screen_x = lerp(g->screen_x, g->target_x, speed, dt);
                all_settled = false;
            } else {
                g->screen_x = g->target_x;
            }

            if (fabsf(dy) > ANIM_SETTLE_THRESH) {
                g->screen_y = lerp(g->screen_y, g->target_y, speed, dt);
                all_settled = false;
            } else {
                g->screen_y = g->target_y;
            }

            /* Elimination shrink-to-zero */
            if (g->is_marked_for_elimination) {
                if (g->elim_scale > 0.01f) {
                    g->elim_scale = lerp(g->elim_scale, 0.0f, 10.0f, dt);
                    all_settled   = false;
                } else {
                    g->elim_scale = 0.0f;
                }
            }
        }
    }

    board->animations_settled = all_settled;

    /* Tick down combo popup timer */
    if (board->combo_popup_timer > 0.0f) {
        board->combo_popup_timer -= dt;
        if (board->combo_popup_timer < 0.0f)
            board->combo_popup_timer = 0.0f;
    }

    /* Update particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].active) {
            g_particles[i].x += g_particles[i].vx * dt;
            g_particles[i].y += g_particles[i].vy * dt;
            g_particles[i].life -= dt;
            if (g_particles[i].life <= 0.0f) {
                g_particles[i].active = false;
            }
        }
    }
}

/* ================================================================
 *  Gem rendering
 * ================================================================ */

static void draw_gem(const Gem *g, bool is_selected)
{
    if (!g || g->gem_type >= MAX_GEM_TYPES)
        return;

    float scale = (g->elim_scale > 0.0f) ? g->elim_scale : 1.0f;
    if (g->is_marked_for_elimination && g->elim_scale < 0.05f)
        return;

    SDL_Renderer *r = g_view.renderer;

    int half = (int)(GEM_SIZE / 2 * scale);
    int cx   = (int)g->screen_x;
    int cy   = (int)g->screen_y;
    int x    = cx - half + 3;
    int y    = cy - half + 3;
    int w    = half * 2 - 6;
    int h    = half * 2 - 6;
    int cr   = (int)(GEM_CORNER_R * scale);
    if (cr < 1) cr = 1;

    if (g->is_stone) {
        if (g_view.tex_stone) {
            SDL_Rect dst = {x, y, w, h};
            SDL_RenderCopy(r, g_view.tex_stone, NULL, &dst);
        } else {
            fill_rounded_rect(r, x, y, w, h, cr, (SDL_Color){128, 128, 128, 255});
            draw_rounded_rect_outline(r, x, y, w, h, cr, (SDL_Color){100, 100, 100, 255}, 2);
        }
    } else if (g_view.tex_gem[g->gem_type]) {
        SDL_Rect dst = {x, y, w, h};
        SDL_RenderCopy(r, g_view.tex_gem[g->gem_type], NULL, &dst);
    } else {
        /* Gem body fallback */
        fill_rounded_rect(r, x, y, w, h, cr, kGemFill[g->gem_type]);

        /* Specular highlight (upper-third ellipse) */
        if (scale > 0.5f) {
            SDL_Color hi_color = kGemHighlight[g->gem_type];
            int hw = w / 4;
            int hh = h / 6;
            fill_circle(r, cx, y + hh + 2, hw > 2 ? hw : 2, hi_color);
        }
    }

    /* Ice Overlay */
    if (g->has_ice) {
        if (g_view.tex_ice) {
            /* Make the ice slightly larger than the gem to wrap it completely */
            SDL_Rect dst = {x - 6, y - 6, w + 12, h + 12};
            /* Lower transparency further to let the gem color show through clearly */
            SDL_SetTextureBlendMode(g_view.tex_ice, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(g_view.tex_ice, 130);
            SDL_RenderCopy(r, g_view.tex_ice, NULL, &dst);
        } else {
            fill_rounded_rect(r, x, y, w, h, cr, (SDL_Color){173, 216, 230, 128});
            draw_rounded_rect_outline(r, x, y, w, h, cr, (SDL_Color){255, 255, 255, 180}, 1);
        }
    }

    /* Bomb indicator */
    if (g->bomb_type != BOMB_NONE && scale > 0.5f) {
        set_color(r, (SDL_Color){255, 255, 255, 200});
        int b_cx = cx, b_cy = cy;
        int b_size = w / 3;
        
        if (g->bomb_type == BOMB_LINE_H || g->bomb_type == BOMB_CROSS) {
            SDL_RenderDrawLine(r, b_cx - b_size, b_cy, b_cx + b_size, b_cy);
            SDL_RenderDrawLine(r, b_cx - b_size, b_cy - 1, b_cx + b_size, b_cy - 1);
            SDL_RenderDrawLine(r, b_cx - b_size, b_cy + 1, b_cx + b_size, b_cy + 1);
        }
        if (g->bomb_type == BOMB_LINE_V || g->bomb_type == BOMB_CROSS) {
            SDL_RenderDrawLine(r, b_cx, b_cy - b_size, b_cx, b_cy + b_size);
            SDL_RenderDrawLine(r, b_cx - 1, b_cy - b_size, b_cx - 1, b_cy + b_size);
            SDL_RenderDrawLine(r, b_cx + 1, b_cy - b_size, b_cx + 1, b_cy + b_size);
        }
        if (g->bomb_type == BOMB_RADIUS) {
            draw_rounded_rect_outline(r, b_cx - b_size/2, b_cy - b_size/2, b_size, b_size, b_size/2, (SDL_Color){255,255,255,200}, 2);
        }
    }

    /* Border & Selection Glow */
    if (is_selected) {
        float time_sec = (float)SDL_GetTicks64() / 1000.0f;
        uint8_t glow_alpha = (uint8_t)(100 + 155 * fabsf(sinf(time_sec * 6.0f)));
        SDL_Color glow_c = {255, 255, 255, glow_alpha};
        draw_rounded_rect_outline(r, x - 2, y - 2, w + 4, h + 4, cr + 1, glow_c, 3);
        draw_rounded_rect_outline(r, x, y, w, h, cr, (SDL_Color){255, 255, 255, 255}, 3);
    } else {
        draw_rounded_rect_outline(r, x, y, w, h, cr, kPanelBorderColor, 1);
        if (is_selected) {
            draw_rounded_rect_outline(r, x - 2, y - 2, w + 4, h + 4, cr, kMarkRingColor, 2);
        }
    }

    /* Elimination flash-ring */
    if (g->is_marked_for_elimination) {
        draw_rounded_rect_outline(r, x - 1, y - 1, w + 2, h + 2,
                                  cr, kMarkRingColor, 2);
    }
}

/* ================================================================
 *  Board background grid
 * ================================================================ */

static void draw_board_bg(void)
{
    /* The dark panel and glowing comet trail are removed
       so that the gems display directly on the beautiful background frame. */
}

/* ================================================================
 *  Info panel (right-hand side)
 * ================================================================ */

static void draw_info_panel(const GameBoard *board)
{
    SDL_Renderer *r = g_view.renderer;

    int px = BOARD_OFFSET_X + BOARD_WIDTH * GEM_SIZE + 20;
    int py = BOARD_OFFSET_Y;
    int pw = WINDOW_WIDTH - px - 10;

    /* Background removed so text draws directly on the beautiful game background */

    int tx = px + pw / 2;
    int ty = py + 36;

    draw_text_centered(r, g_view.font_large, "消消乐", tx, ty, kTextSecondaryColor);
    ty += 30;

    /* Divider */
    set_color(r, kPanelBorderColor);
    SDL_RenderDrawLine(r, px + 10, ty, px + pw - 10, ty);
    ty += 20;

    /* Score */
    char buf[64];
    draw_text_centered(r, g_view.font_hint, "分数", tx, ty, kTextHintColor);
    ty += 30;
    snprintf(buf, sizeof(buf), "%u", board->score);
    draw_text_centered(r, g_view.font_large, buf, tx, ty, kTextPrimaryColor);
    ty += 40;

    /* High score */
    draw_text_centered(r, g_view.font_hint, "最高分", tx, ty, kTextHintColor);
    ty += 25;
    snprintf(buf, sizeof(buf), "%u", board->high_score);
    draw_text_centered(r, g_view.font_medium, buf, tx, ty, kAccentBlueColor);
    ty += 32;

    /* Coins */
    draw_text_centered(r, g_view.font_hint, "金币", tx, ty, kTextHintColor);
    ty += 25;
    snprintf(buf, sizeof(buf), "%u", board->total_coins);
    draw_text_centered(r, g_view.font_medium, buf, tx, ty, kMarkRingColor);
    ty += 32;

    set_color(r, kPanelBorderColor);
    SDL_RenderDrawLine(r, px + 10, ty, px + pw - 10, ty);
    ty += 20;

    /* Moves */
    draw_text_centered(r, g_view.font_hint, "剩余步数", tx, ty, kTextHintColor);
    ty += 30;
    snprintf(buf, sizeof(buf), "%u", board->moves_remaining);
    SDL_Color moves_c = kTextPrimaryColor;
    if (board->moves_remaining <= 5) {
        if ((SDL_GetTicks64() / 250) % 2 == 0) moves_c = kDangerColor;
        else moves_c = (SDL_Color){150, 0, 0, 255};
    }
    draw_text_centered(r, g_view.font_large, buf, tx, ty, moves_c);
    ty += 36;

    /* Difficulty */
    static const char *DIFF_NAMES[] = {"简单", "普通", "困难"};
    int d = (board->difficulty >= 0 && board->difficulty <= 2) ? board->difficulty : 1;
    draw_text_centered(r, g_view.font_hint, "难度", tx, ty, kTextHintColor);
    ty += 25;
    draw_text_centered(r, g_view.font_medium, DIFF_NAMES[d], tx, ty, kAccentGreenColor);
    ty += 32;

    set_color(r, kPanelBorderColor);
    SDL_RenderDrawLine(r, px + 10, ty, px + pw - 10, ty);
    ty += 20;

    /* Key hints at bottom */
    draw_text_centered(r, g_view.font_hint, "S/L 存读  P/ESC 暂停", tx, ty, kTextHintColor);
    ty += 20;
    draw_text_centered(r, g_view.font_hint, "U 撤销    R 重来", tx, ty, kTextHintColor);
    ty += 28;

    /* Combo (rendered further down or elsewhere if needed) */
    if (board->combo_multiplier > 1) {
        snprintf(buf, sizeof(buf), "连击 x%u!", board->combo_multiplier);
        draw_text_centered(r, g_view.font_body, buf, tx, ty, kAccentBlueColor);
    }
    
    /* Props Row Under the Board */
    int props_y = BOARD_OFFSET_Y + BOARD_HEIGHT * GEM_SIZE + 24;
    int p_size = 50;
    int p_gap = 24;
    int start_x = BOARD_OFFSET_X + (BOARD_WIDTH * GEM_SIZE - (4 * p_size + 3 * p_gap)) / 2;
    
    struct { SDL_Texture* tex; uint8_t count; GameState state; } props[4] = {
        { g_view.tex_prop_hammer,  board->prop_hammer_count,  GAME_STATE_PROP_HAMMER_WAITING },
        { g_view.tex_prop_wand,    board->prop_wand_count,    GAME_STATE_PROP_WAND_FIRST_SEL },
        { g_view.tex_prop_shuffle, board->prop_shuffle_count, GAME_STATE_PROP_SHUFFLE_CONFIRM },
        { g_view.tex_prop_moves,   board->prop_moves_count,   GAME_STATE_PROP_MOVES_CONFIRM }
    };
    
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    int hovered_prop_idx = -1;

    for (int i = 0; i < 4; i++) {
        int dx = start_x + i * (p_size + p_gap);
        int dy = props_y;
        
        SDL_Rect pr = { dx, dy, p_size, p_size };
        
        if (mx >= dx && mx <= dx + p_size && my >= dy && my <= dy + p_size) {
            hovered_prop_idx = i;
        }
        
        bool locked = false;
        if (board->used_props_total >= board->max_props_per_game) locked = true;
        if (i == 3 && board->used_sandglass_count >= board->max_sandglass_per_game) locked = true;
        if (board->difficulty == 2) {
            if (i == 2 && board->used_sandglass_count > 0) locked = true;
            if (i == 3 && (board->level & 2)) locked = true;
        }

        if (board->current_state == props[i].state || 
           (i == 1 && board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL)) {
            SDL_SetRenderDrawColor(r, 255, 215, 0, 255);
            SDL_Rect hr = { dx-3, dy-3, p_size+6, p_size+6 };
            SDL_RenderFillRect(r, &hr);
        }
        
        fill_rounded_rect(r, dx, dy, p_size, p_size, 8, locked ? (SDL_Color){30, 30, 30, 255} : (SDL_Color){60, 50, 80, 255});
        if (props[i].tex) {
            if (locked) SDL_SetTextureColorMod(props[i].tex, 100, 100, 100);
            SDL_RenderCopy(r, props[i].tex, NULL, &pr);
            if (locked) SDL_SetTextureColorMod(props[i].tex, 255, 255, 255);
        }
        
        if (props[i].count > 0) {
            snprintf(buf, sizeof(buf), "x%u", props[i].count);
            draw_text_centered(r, g_view.font_hint, buf, dx + p_size/2, dy + p_size + 14, locked ? kTextHintColor : kTextPrimaryColor);
        } else {
            uint32_t price = 0;
            if (i == 0) price = 50;
            else if (i == 1) price = 80;
            else if (i == 2) price = 100;
            else if (i == 3) price = 150;
            snprintf(buf, sizeof(buf), "$%u", price);
            draw_text_centered(r, g_view.font_hint, buf, dx + p_size/2, dy + p_size + 14, (SDL_Color){255, 215, 0, 255});
        }
    }

    if (hovered_prop_idx != -1) {
        static const char *prop_desc[] = {
            "小木槌: 砸碎并消除单个方块",
            "魔法棒: 强制交换任意两个方块",
            "星空重置: 重新洗牌所有方块",
            "时光沙漏: 增加 5 步剩余步数"
        };
        const char *text = prop_desc[hovered_prop_idx];
        if (board->current_state == props[hovered_prop_idx].state || 
           (hovered_prop_idx == 1 && board->current_state == GAME_STATE_PROP_WAND_SECOND_SEL)) {
            if (hovered_prop_idx == 2 || hovered_prop_idx == 3) {
                text = "再次点击以确认使用该道具";
            } else {
                text = "请在棋盘上选择目标，或再次点击取消";
            }
        }
        draw_text_centered(r, g_view.font_hint, text, 
                           BOARD_OFFSET_X + (BOARD_WIDTH * GEM_SIZE)/2, 
                           props_y + p_size + 36, 
                           (SDL_Color){200, 200, 255, 255});
    }

}

/* ================================================================
 *  Combo pop-up overlay
 * ================================================================ */

/**
 * @brief Draw a fade-out "xN COMBO!" overlay in the board centre.
 *
 * The text alpha tracks combo_popup_timer linearly.
 * Runs for 1.5 s total; the final 0.5 s fades the text out.
 */
static void draw_combo_popup(const GameBoard *board)
{
    if (!board || board->combo_popup_timer <= 0.0f || board->combo_popup_value < 2)
        return;

    SDL_Renderer *rend = g_view.renderer;
    int cx = BOARD_OFFSET_X + BOARD_WIDTH  * GEM_SIZE / 2;
    int cy = BOARD_OFFSET_Y + BOARD_HEIGHT * GEM_SIZE / 2;

    /* alpha: full for first second, fade in last 0.5 s */
    float t     = board->combo_popup_timer;
    uint8_t alpha = (t > 0.5f) ? 255u : (uint8_t)(t / 0.5f * 255.0f);

    /* Draw a semi-transparent background pill */
    SDL_Color pill_bg = {10, 10, 14, (uint8_t)(alpha * 0.75f)};
    fill_rounded_rect(rend, cx - 110, cy - 36, 220, 64, 18, pill_bg);

    char buf[32];
    snprintf(buf, sizeof(buf), "连击 x%u!", board->combo_popup_value);

    SDL_Color text_col = {255, 214, 10, alpha};  /* amber */
    draw_text_centered(rend, g_view.font_large, buf, cx, cy, text_col);
}

/* ================================================================
 *  Full in-game UI
 * ================================================================ */

void view_draw_game_ui_complete(const GameBoard *board)
{
    draw_board_bg();

    /* Draw all gems */
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            const Gem *g = &board->board[row][col];
            uint8_t t = g->gem_type;
            if (t >= MAX_GEM_TYPES)
                continue;
            bool sel = board->first_gem_selected &&
                       row == board->selected_row &&
                       col == board->selected_col;
            
            bool hover = (board->current_state == GAME_STATE_WAITING_INPUT || board->current_state == GAME_STATE_FIRST_GEM_SELECT) &&
                         row == board->hover_row && col == board->hover_col;

            if (board->has_hint) {
                bool is_hint = (row == board->hint_r && col == board->hint_c) ||
                               (board->hint_dir == 0 && row == board->hint_r && col == board->hint_c + 1) ||
                               (board->hint_dir == 1 && row == board->hint_r + 1 && col == board->hint_c);
                if (is_hint && (SDL_GetTicks64() / 250) % 2 == 0) {
                    sel = true; /* Blink with selection ring */
                }
            }
            
            draw_gem(g, sel || hover);
        }
    }

    /* Draw particles */
    SDL_Renderer *r = g_view.renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].active) {
            float alpha_ratio = g_particles[i].life / g_particles[i].max_life;
            SDL_Color pc = g_particles[i].color;
            pc.a = (uint8_t)(255.0f * alpha_ratio);
            
            int p_size = (int)(g_particles[i].size * alpha_ratio);
            if (p_size < 1) p_size = 1;
            
            fill_rounded_rect(r, (int)g_particles[i].x, (int)g_particles[i].y, p_size, p_size, 1, pc);
        }
    }

    /* Combo pop-up overlay (renders above gems, below info panel) */
    draw_combo_popup(board);

    draw_info_panel(board);
}

/* ================================================================
 *  Menu button helper
 * ================================================================ */

static void draw_menu_button(int cx, int y, int w, int h,
                              const char *label, bool selected,
                              const char *sub_label, bool locked)
{
    SDL_Renderer *r = g_view.renderer;

    SDL_Color fill   = selected ? kAccentBlueColor : (locked ? (SDL_Color){20, 20, 20, 255} : kPanelColor);
    SDL_Color border = selected ? kAccentBlueColor : (locked ? (SDL_Color){40, 40, 40, 255} : kPanelBorderColor);
    int       bw     = selected ? 2 : 1;

    fill_rounded_rect(r, cx - w / 2, y, w, h, BTN_CORNER_R, fill);
    draw_rounded_rect_outline(r, cx - w / 2, y, w, h, BTN_CORNER_R, border, bw);

    int label_y = sub_label ? y + h / 2 - 14 : y + h / 2;
    SDL_Color text_col = locked ? (SDL_Color){100, 100, 100, 255} : kTextPrimaryColor;
    draw_text_centered(r, g_view.font_medium, label, cx, label_y, text_col);

    if (sub_label) {
        SDL_Color sc = selected ? (SDL_Color){200, 220, 255, 255} : (locked ? (SDL_Color){80, 80, 80, 255} : kTextSecondaryColor);
        draw_text_centered(r, g_view.font_hint, sub_label, cx, y + h / 2 + 10, sc);
    }
}

/* ================================================================
 *  Screen sub-renderers
 * ================================================================ */

void view_draw_main_menu(const GameBoard *board)
{
    SDL_Renderer *r = g_view.renderer;
    int cx = WINDOW_WIDTH / 2;

    if (g_view.tex_nuist_badge) {
        int w, h;
        SDL_QueryTexture(g_view.tex_nuist_badge, NULL, NULL, &w, &h);
        float scale = 1.0f;
        if (w > 120) scale = 120.0f / (float)w;
        if ((float)h * scale > 120.0f) scale = 120.0f / (float)h;
        SDL_Rect dst = {cx - (int)((float)w * scale) / 2, 40, (int)((float)w * scale), (int)((float)h * scale)};
        SDL_RenderCopy(r, g_view.tex_nuist_badge, NULL, &dst);
        
        draw_text_centered(r, g_view.font_title,  "消消乐",              cx, 200,  kTextPrimaryColor);
        draw_text_centered(r, g_view.font_medium, "按任意键或点击继续",
                           cx, 260, kTextSecondaryColor);

        static const char *LABELS[] = {"开始游戏", "游戏规则", "退出游戏"};
        for (int i = 0; i < 3; i++) {
            bool sel = (i == board->highlighted_menu_option);
            draw_menu_button(cx, 330 + i * 80, 280, 60, LABELS[i], sel, NULL, false);
        }
    } else {
        draw_text_centered(r, g_view.font_title,  "消消乐",              cx, 90,  kTextPrimaryColor);
        draw_text_centered(r, g_view.font_medium, "经典三消游戏体验",
                           cx, 160, kTextSecondaryColor);

        static const char *LABELS[] = {"开始游戏", "游戏规则", "退出游戏"};
        for (int i = 0; i < 3; i++) {
            bool sel = (i == board->highlighted_menu_option);
            draw_menu_button(cx, 250 + i * 90, 280, 64, LABELS[i], sel, NULL, false);
        }
    }

    draw_text_centered(r, g_view.font_hint,
                       "方向键选择  |  Enter确认  |  ESC退出",
                       cx, WINDOW_HEIGHT - 30, kTextHintColor);
}

void view_draw_difficulty_menu(const GameBoard *board)
{
    SDL_Renderer *r = g_view.renderer;
    int cx = WINDOW_WIDTH / 2;

    draw_text_centered(r, g_view.font_large, "选择难度", cx, 90,  kTextPrimaryColor);
    draw_text_centered(r, g_view.font_medium, "不同的挑战与奖励",
                       cx, 140, kTextSecondaryColor);

    static const char *NAMES[] = {"简单",   "普通", "困难"};
    static const char *MOVES[] = {"50 步","30 步","15 步"};
    for (int i = 0; i < 3; i++) {
        bool sel = (i == board->highlighted_difficulty);
        bool locked = false; /* Force unlock for testing */
        draw_menu_button(cx, 220 + i * 100, 300, 74, NAMES[i], sel, locked ? "未解锁" : MOVES[i], locked);
    }

    draw_text_centered(r, g_view.font_hint,
                       "方向键选择  |  Enter确认  |  ESC返回",
                       cx, WINDOW_HEIGHT - 30, kTextHintColor);
}

void view_draw_pause_menu(const GameBoard *board)
{
    SDL_Renderer *r = g_view.renderer;

    /* Dark overlay */
    set_color(r, (SDL_Color){10, 10, 14, 210});
    set_blend(r, SDL_BLENDMODE_BLEND);
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(r, &overlay);

    int cx = WINDOW_WIDTH / 2;
    draw_text_centered(r, g_view.font_title, "游戏暂停", cx, 120, kTextPrimaryColor);

    static const char *OPTS[] = {"继续游戏", "重新开始", "返回主菜单"};
    for (int i = 0; i < 3; i++) {
        bool sel = (i == board->highlighted_menu_option);
        draw_menu_button(cx, 220 + i * 90, 280, 64, OPTS[i], sel, NULL, false);
    }
}

void view_draw_game_over_screen(const GameBoard *board)
{
    SDL_Renderer *r = g_view.renderer;
    int cx = WINDOW_WIDTH / 2;

    const char *title_text = "游戏结束";
    SDL_Color title_color = kDangerColor;
    if (board->moves_remaining > 0 || model_is_deadlock((GameBoard *)board) == false) {
        /* Not a game over by fail, maybe reached target? */
        title_color = kAccentGreenColor;
    }

    draw_text_centered(r, g_view.font_title, title_text, cx, 80, title_color);
    
    char buf[64];
    
    // Stars
    snprintf(buf, sizeof(buf), "星级: %u / 3", board->stars_earned);
    draw_text_centered(r, g_view.font_large, buf, cx, 140, (SDL_Color){255, 215, 0, 255});

    // Score
    snprintf(buf, sizeof(buf), "最终得分: %u", board->score);
    draw_text_centered(r, g_view.font_medium, buf, cx, 190, kTextPrimaryColor);

    // Max Combo
    snprintf(buf, sizeof(buf), "最高连击: x%u", board->max_combo_this_game);
    draw_text_centered(r, g_view.font_body, buf, cx, 230, kTextSecondaryColor);

    // Props
    snprintf(buf, sizeof(buf), "使用道具: %u", board->used_props_total);
    draw_text_centered(r, g_view.font_body, buf, cx, 260, kTextSecondaryColor);

    static const char *OPTS[] = {"再来一局", "返回主菜单"};
    for (int i = 0; i < 2; i++) {
        bool sel = (i == board->highlighted_menu_option);
        draw_menu_button(cx, 330 + i * 80, 280, 64, OPTS[i], sel, NULL, false);
    }

    draw_text_centered(r, g_view.font_hint,
                       "方向键选择，回车确认",
                       cx, WINDOW_HEIGHT - 30, kTextHintColor);
}

/* ================================================================
 *  Main render entry point
 * ================================================================ */

bool view_render_frame(const GameBoard *board)
{
    if (!g_view.renderer || !board)
        return false;

    SDL_Renderer *r = g_view.renderer;

    /* Fill background */
    SDL_SetRenderDrawColor(r, kBgColor.r, kBgColor.g, kBgColor.b, kBgColor.a);
    SDL_RenderClear(r);
    
    /* Draw background image based on game state */
    if (board->current_state == GAME_STATE_MAIN_MENU || 
        board->current_state == GAME_STATE_DIFFICULTY_SELECTION) {
        if (g_view.tex_bg_main) {
            SDL_RenderCopy(r, g_view.tex_bg_main, NULL, NULL);
        }
    } else {
        if (g_view.tex_bg) {
            SDL_RenderCopy(r, g_view.tex_bg, NULL, NULL);
        }
    }

    switch (board->current_state) {
        case GAME_STATE_MAIN_MENU:
            view_draw_main_menu(board);
            break;
        case GAME_STATE_DIFFICULTY_SELECTION:
            view_draw_difficulty_menu(board);
            break;
        case GAME_STATE_PAUSED:
            view_draw_game_ui_complete(board);
            view_draw_pause_menu(board);
            break;
        case GAME_STATE_DEAD_END_ANIM:
            view_draw_game_ui_complete(board);
            if (g_view.tex_dead_end) {
                float alpha_f = (board->state_timer / board->animation_duration) * 255.0f;
                if (alpha_f > 255.0f) alpha_f = 255.0f;
                
                SDL_SetTextureBlendMode(g_view.tex_dead_end, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(g_view.tex_dead_end, (Uint8)alpha_f);
                
                int texW = 0, texH = 0;
                SDL_QueryTexture(g_view.tex_dead_end, NULL, NULL, &texW, &texH);
                
                int board_w = BOARD_WIDTH  * GEM_SIZE;
                int board_h = BOARD_HEIGHT * GEM_SIZE;
                
                /* Calculate scale to fit nicely (e.g. 80% of board width/height) */
                float scale_x = ((float)board_w * 0.8f) / (float)texW;
                float scale_y = ((float)board_h * 0.8f) / (float)texH;
                float scale = (scale_x < scale_y) ? scale_x : scale_y;
                if (scale > 1.0f) scale = 1.0f; /* Don't upscale if it's already small */
                
                int drawW = (int)((float)texW * scale);
                int drawH = (int)((float)texH * scale);
                
                int cx = BOARD_OFFSET_X + board_w / 2;
                int cy = BOARD_OFFSET_Y + board_h / 2;
                
                SDL_Rect dst = { cx - drawW / 2, cy - drawH / 2, drawW, drawH };
                SDL_RenderCopy(r, g_view.tex_dead_end, NULL, &dst);
            }
            break;
        case GAME_STATE_GAME_OVER:
            view_draw_game_over_screen(board);
            break;
        default:
            view_draw_game_ui_complete(board);
            break;
    }

    SDL_RenderPresent(g_view.renderer);
    return true;
}

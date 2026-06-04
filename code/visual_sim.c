#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fish_system.h"

#define MIN_FISH_COUNT 30
#define MAX_FISH_COUNT 60
#define VISUAL_MODE_NAME "RANDOM"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FPS_DELAY_MS 16
#define HEADER_HEIGHT 128
#define CONVEYOR_X 42
#define CONVEYOR_Y 208
#define CONVEYOR_WIDTH 850
#define CONVEYOR_HEIGHT 170
#define CLASSIFY_X 790.0f
#define ROUTE_X 875.0f
#define BIN_X 965
#define BIN_Y 162
#define BIN_WIDTH 255
#define BIN_HEIGHT 62
#define BIN_SPACING 10
#define LOG_X 955
#define LOG_Y 548
#define LOG_WIDTH 275
#define LOG_HEIGHT 132
#define LOG_LINES 5
#define BELT_SPEED 4.0f
#define ROUTE_LERP 0.12f
#define DEADLINE_MS 3.0f
#define RESTART_DELAY_MS 2000
#define INVALID_CHANCE_PERCENT 8
#define DEADLINE_MISS_CHANCE_PERCENT 8

typedef enum {
    PHASE_BELT = 0,
    PHASE_ROUTE = 1,
    PHASE_DONE = 2
} MotionPhase;

typedef struct {
    const char* name;
    SDL_Rect rect;
    SDL_Color color;
} BinVisual;

typedef struct {
    FishSample sample;
    float x;
    float y;
    float base_y;
    float target_x;
    float target_y;
    float wave_offset;
    float latency_ms;
    float highlight_timer;
    int width;
    int height;
    int deadline_miss;
    int active;
    int produced_logged;
    int classified_logged;
    int completed_logged;
    const char* destination;
    SDL_Color color;
    MotionPhase phase;
} VisualFish;

typedef struct {
    int total_fish;
    int deadline_ok_count;
    int deadline_miss_count;
    int invalid_data_count;
    float total_latency_ms;
    float max_latency_ms;
} VisualStats;

typedef struct {
    char lines[LOG_LINES][72];
    int count;
} VisualLogPanel;

typedef struct {
    VisualStats stats;
    VisualLogPanel log_panel;
    float sensor_activity;
    float classification_activity;
    float actuator_activity;
    float late_reject_flash;
    float belt_offset;
    int produced_count;
    int classified_count;
    int completed_count;
    int scenario_fish_count;
    int paused;
    int running;
    Uint32 completed_timestamp_ms;
} VisualState;

static const uint8_t GLYPH_SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
static const uint8_t GLYPH_A[7] = {14, 17, 17, 31, 17, 17, 17};
static const uint8_t GLYPH_B[7] = {30, 17, 17, 30, 17, 17, 30};
static const uint8_t GLYPH_C[7] = {14, 17, 16, 16, 16, 17, 14};
static const uint8_t GLYPH_D[7] = {30, 17, 17, 17, 17, 17, 30};
static const uint8_t GLYPH_E[7] = {31, 16, 16, 30, 16, 16, 31};
static const uint8_t GLYPH_F[7] = {31, 16, 16, 30, 16, 16, 16};
static const uint8_t GLYPH_G[7] = {14, 17, 16, 16, 19, 17, 15};
static const uint8_t GLYPH_H[7] = {17, 17, 17, 31, 17, 17, 17};
static const uint8_t GLYPH_I[7] = {31, 4, 4, 4, 4, 4, 31};
static const uint8_t GLYPH_J[7] = {7, 2, 2, 2, 18, 18, 12};
static const uint8_t GLYPH_K[7] = {17, 18, 20, 24, 20, 18, 17};
static const uint8_t GLYPH_L[7] = {16, 16, 16, 16, 16, 16, 31};
static const uint8_t GLYPH_M[7] = {17, 27, 21, 21, 17, 17, 17};
static const uint8_t GLYPH_N[7] = {17, 17, 25, 21, 19, 17, 17};
static const uint8_t GLYPH_O[7] = {14, 17, 17, 17, 17, 17, 14};
static const uint8_t GLYPH_P[7] = {30, 17, 17, 30, 16, 16, 16};
static const uint8_t GLYPH_Q[7] = {14, 17, 17, 17, 21, 18, 13};
static const uint8_t GLYPH_R[7] = {30, 17, 17, 30, 20, 18, 17};
static const uint8_t GLYPH_S[7] = {15, 16, 16, 14, 1, 1, 30};
static const uint8_t GLYPH_T[7] = {31, 4, 4, 4, 4, 4, 4};
static const uint8_t GLYPH_U[7] = {17, 17, 17, 17, 17, 17, 14};
static const uint8_t GLYPH_V[7] = {17, 17, 17, 17, 17, 10, 4};
static const uint8_t GLYPH_W[7] = {17, 17, 17, 21, 21, 21, 10};
static const uint8_t GLYPH_X[7] = {17, 17, 10, 4, 10, 17, 17};
static const uint8_t GLYPH_Y[7] = {17, 17, 10, 4, 4, 4, 4};
static const uint8_t GLYPH_Z[7] = {31, 1, 2, 4, 8, 16, 31};
static const uint8_t GLYPH_0[7] = {14, 17, 19, 21, 25, 17, 14};
static const uint8_t GLYPH_1[7] = {4, 12, 4, 4, 4, 4, 14};
static const uint8_t GLYPH_2[7] = {14, 17, 1, 2, 4, 8, 31};
static const uint8_t GLYPH_3[7] = {30, 1, 1, 6, 1, 1, 30};
static const uint8_t GLYPH_4[7] = {2, 6, 10, 18, 31, 2, 2};
static const uint8_t GLYPH_5[7] = {31, 16, 16, 30, 1, 1, 30};
static const uint8_t GLYPH_6[7] = {14, 16, 16, 30, 17, 17, 14};
static const uint8_t GLYPH_7[7] = {31, 1, 2, 4, 8, 8, 8};
static const uint8_t GLYPH_8[7] = {14, 17, 17, 14, 17, 17, 14};
static const uint8_t GLYPH_9[7] = {14, 17, 17, 15, 1, 1, 14};
static const uint8_t GLYPH_COLON[7] = {0, 4, 4, 0, 4, 4, 0};
static const uint8_t GLYPH_DOT[7] = {0, 0, 0, 0, 0, 12, 12};
static const uint8_t GLYPH_MINUS[7] = {0, 0, 0, 31, 0, 0, 0};
static const uint8_t GLYPH_UNDERSCORE[7] = {0, 0, 0, 0, 0, 0, 31};
static const uint8_t GLYPH_SLASH[7] = {1, 2, 4, 8, 16, 0, 0};

static const uint8_t* glyph_for_char(char c) {
    switch (c) {
        case 'A': return GLYPH_A;
        case 'B': return GLYPH_B;
        case 'C': return GLYPH_C;
        case 'D': return GLYPH_D;
        case 'E': return GLYPH_E;
        case 'F': return GLYPH_F;
        case 'G': return GLYPH_G;
        case 'H': return GLYPH_H;
        case 'I': return GLYPH_I;
        case 'J': return GLYPH_J;
        case 'K': return GLYPH_K;
        case 'L': return GLYPH_L;
        case 'M': return GLYPH_M;
        case 'N': return GLYPH_N;
        case 'O': return GLYPH_O;
        case 'P': return GLYPH_P;
        case 'Q': return GLYPH_Q;
        case 'R': return GLYPH_R;
        case 'S': return GLYPH_S;
        case 'T': return GLYPH_T;
        case 'U': return GLYPH_U;
        case 'V': return GLYPH_V;
        case 'W': return GLYPH_W;
        case 'X': return GLYPH_X;
        case 'Y': return GLYPH_Y;
        case 'Z': return GLYPH_Z;
        case '0': return GLYPH_0;
        case '1': return GLYPH_1;
        case '2': return GLYPH_2;
        case '3': return GLYPH_3;
        case '4': return GLYPH_4;
        case '5': return GLYPH_5;
        case '6': return GLYPH_6;
        case '7': return GLYPH_7;
        case '8': return GLYPH_8;
        case '9': return GLYPH_9;
        case ':': return GLYPH_COLON;
        case '.': return GLYPH_DOT;
        case '-': return GLYPH_MINUS;
        case '_': return GLYPH_UNDERSCORE;
        case '/': return GLYPH_SLASH;
        case ' ': return GLYPH_SPACE;
        default: return GLYPH_SPACE;
    }
}

static SDL_Color class_to_color(FishClass class_id) {
    switch (class_id) {
        case FISH_SMALL:
            return (SDL_Color) {70, 168, 255, 255};
        case FISH_MEDIUM:
            return (SDL_Color) {72, 205, 144, 255};
        case FISH_LARGE:
            return (SDL_Color) {255, 176, 67, 255};
        case FISH_INVALID:
            return (SDL_Color) {218, 87, 87, 255};
        default:
            return (SDL_Color) {180, 180, 180, 255};
    }
}

static void class_to_size(FishClass class_id, int* width, int* height) {
    switch (class_id) {
        case FISH_SMALL:
            *width = 52;
            *height = 24;
            break;
        case FISH_MEDIUM:
            *width = 74;
            *height = 34;
            break;
        case FISH_LARGE:
            *width = 104;
            *height = 44;
            break;
        case FISH_INVALID:
            *width = 60;
            *height = 28;
            break;
        default:
            *width = 68;
            *height = 30;
            break;
    }
}

static void push_log_event(VisualState* state, const char* fmt, ...) {
    va_list args;
    int i;

    if (state->log_panel.count < LOG_LINES) {
        state->log_panel.count++;
    }

    for (i = LOG_LINES - 1; i > 0; i--) {
        SDL_strlcpy(state->log_panel.lines[i], state->log_panel.lines[i - 1], sizeof(state->log_panel.lines[i]));
    }

    va_start(args, fmt);
    SDL_vsnprintf(state->log_panel.lines[0], sizeof(state->log_panel.lines[0]), fmt, args);
    va_end(args);
}

static void draw_text(SDL_Renderer* renderer, int x, int y, int scale, SDL_Color color, const char* text) {
    int cursor_x = x;
    int row;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    while (*text != '\0') {
        const uint8_t* glyph = glyph_for_char(*text);
        int col;

        for (row = 0; row < 7; row++) {
            for (col = 0; col < 5; col++) {
                if ((glyph[row] >> (4 - col)) & 1U) {
                    SDL_Rect pixel = {
                        cursor_x + col * scale,
                        y + row * scale,
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

        cursor_x += 6 * scale;
        text++;
    }
}

static int text_width(int scale, const char* text) {
    int width = 0;

    while (*text != '\0') {
        width += 6 * scale;
        text++;
    }

    return width > 0 ? width - scale : 0;
}

static int text_height(int scale) {
    return 7 * scale;
}

static void draw_text_centered(
    SDL_Renderer* renderer,
    SDL_Rect rect,
    int scale,
    SDL_Color color,
    const char* text
) {
    int x = rect.x + (rect.w - text_width(scale, text)) / 2;
    int y = rect.y + (rect.h - text_height(scale)) / 2;

    draw_text(renderer, x, y, scale, color, text);
}

static void draw_panel(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color fill, SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
}

static void draw_status_card(
    SDL_Renderer* renderer,
    int x,
    int y,
    int w,
    int h,
    const char* label,
    const char* value,
    SDL_Color accent
) {
    SDL_Rect rect = {x, y, w, h};
    SDL_Rect label_rect = {x + 12, y + 10, w - 24, 16};
    SDL_Rect value_rect = {x + 10, y + 30, w - 20, h - 34};
    int label_scale = text_width(2, label) > label_rect.w ? 1 : 2;
    int value_scale = text_width(3, value) > value_rect.w ? 2 : 3;

    draw_panel(renderer, rect, (SDL_Color) {20, 27, 33, 220}, (SDL_Color) {45, 58, 68, 255});
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect) {x, y, 5, h});
    draw_text_centered(renderer, label_rect, label_scale, (SDL_Color) {132, 149, 162, 255}, label);
    draw_text_centered(renderer, value_rect, value_scale, (SDL_Color) {232, 239, 242, 255}, value);
}

static void draw_activity_indicator(SDL_Renderer* renderer, int x, int y, const char* label, float activity) {
    SDL_Color dot = activity > 0.15f ? (SDL_Color) {72, 205, 144, 255} : (SDL_Color) {64, 74, 84, 255};
    SDL_Color outline = activity > 0.15f ? (SDL_Color) {103, 255, 186, 255} : (SDL_Color) {84, 96, 108, 255};

    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect) {x, y, 13, 13});
    SDL_SetRenderDrawColor(renderer, dot.r, dot.g, dot.b, 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect) {x + 2, y + 2, 9, 9});
    draw_text(renderer, x + 20, y - 1, 2, (SDL_Color) {208, 218, 224, 255}, label);
}

static void draw_filled_circle(SDL_Renderer* renderer, int center_x, int center_y, int radius) {
    int dy;

    for (dy = -radius; dy <= radius; dy++) {
        int dx = (int) SDL_sqrt((float) (radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, center_x - dx, center_y + dy, center_x + dx, center_y + dy);
    }
}

static void init_bins(BinVisual bins[5]) {
    int i;
    const char* names[5] = {"BIN_A", "BIN_B", "BIN_C", "REJECT_BIN", "LATE_REJECT"};
    SDL_Color colors[5] = {
        {70, 168, 255, 255},
        {72, 205, 144, 255},
        {255, 176, 67, 255},
        {218, 87, 87, 255},
        {255, 96, 96, 255}
    };

    for (i = 0; i < 5; i++) {
        bins[i].name = names[i];
        bins[i].rect.x = BIN_X;
        bins[i].rect.y = BIN_Y + i * (BIN_HEIGHT + BIN_SPACING);
        bins[i].rect.w = BIN_WIDTH;
        bins[i].rect.h = BIN_HEIGHT;
        bins[i].color = colors[i];
    }
}

static SDL_Rect find_bin_rect(const BinVisual bins[5], const char* name) {
    int i;

    for (i = 0; i < 5; i++) {
        if (SDL_strcmp(bins[i].name, name) == 0) {
            return bins[i].rect;
        }
    }

    return bins[3].rect;
}

static const char* resolve_destination(const FishSample* sample, int deadline_miss) {
    if (deadline_miss) {
        return "LATE_REJECT";
    }

    switch (sample->class_id) {
        case FISH_SMALL:
            return "BIN_A";
        case FISH_MEDIUM:
            return "BIN_B";
        case FISH_LARGE:
            return "BIN_C";
        case FISH_INVALID:
            return "REJECT_BIN";
        default:
            return "REJECT_BIN";
    }
}

static int random_int_range(int min_value, int max_value) {
    int span = max_value - min_value + 1;

    return min_value + (rand() % span);
}

static FishSample build_random_sample(uint32_t fish_id) {
    FishSample sample;
    FishClass target_class;
    int invalid_roll;

    sample.fish_id = fish_id;
    sample.timestamp_ns = 0;
    sample.class_id = FISH_INVALID;
    sample.error_code = ERR_NONE;
    invalid_roll = rand() % 100;
    if (invalid_roll < INVALID_CHANCE_PERCENT) {
        if ((rand() % 2) == 0) {
            sample.length_mm = 0;
            sample.weight_g = (uint16_t) random_int_range(120, 880);
        } else {
            sample.length_mm = (uint16_t) random_int_range(110, 460);
            sample.weight_g = 0;
        }
        return sample;
    }

    target_class = (FishClass) random_int_range((int) FISH_SMALL, (int) FISH_LARGE);
    switch (target_class) {
        case FISH_SMALL:
            sample.length_mm = (uint16_t) random_int_range(115, 195);
            sample.weight_g = (uint16_t) random_int_range(130, 245);
            break;
        case FISH_MEDIUM:
            sample.length_mm = (uint16_t) random_int_range(205, 345);
            sample.weight_g = (uint16_t) random_int_range(255, 690);
            break;
        case FISH_LARGE:
            if ((rand() % 2) == 0) {
                sample.length_mm = (uint16_t) random_int_range(350, 470);
                sample.weight_g = (uint16_t) random_int_range(420, 880);
            } else {
                sample.length_mm = (uint16_t) random_int_range(220, 345);
                sample.weight_g = (uint16_t) random_int_range(700, 900);
            }
            break;
        default:
            sample.length_mm = 0;
            sample.weight_g = 0;
            break;
    }

    return sample;
}

static int should_deadline_miss(const FishSample* sample) {
    if (sample->class_id == FISH_INVALID || sample->error_code == ERR_INVALID_DATA) {
        return 0;
    }

    return (rand() % 100) < DEADLINE_MISS_CHANCE_PERCENT;
}

static float compute_latency_ms(const VisualFish* fish) {
    float base = 0.55f + (float) random_int_range(0, 6) * 0.18f;

    if (fish->sample.error_code == ERR_INVALID_DATA || fish->sample.class_id == FISH_INVALID) {
        base += 0.55f;
    }
    if (fish->deadline_miss) {
        base += 4.6f;
    }

    return base;
}

static void init_fish(VisualFish fish_list[MAX_FISH_COUNT], VisualState* state, const BinVisual bins[5]) {
    int i;
    float spawn_offset = 0.0f;

    state->scenario_fish_count = random_int_range(MIN_FISH_COUNT, MAX_FISH_COUNT);

    for (i = 0; i < MAX_FISH_COUNT; i++) {
        SDL_memset(&fish_list[i], 0, sizeof(fish_list[i]));
    }

    for (i = 0; i < state->scenario_fish_count; i++) {
        FishSample sample = build_random_sample((uint32_t) (i + 1));
        SDL_Rect bin_rect;

        sample.class_id = classify_fish(sample.length_mm, sample.weight_g);
        if (!validate_fish_data(sample.length_mm, sample.weight_g)) {
            sample.error_code = ERR_INVALID_DATA;
        }

        fish_list[i].sample = sample;
        fish_list[i].active = 1;
        fish_list[i].deadline_miss = should_deadline_miss(&fish_list[i].sample);
        if (fish_list[i].deadline_miss) {
            fish_list[i].sample.error_code = ERR_DEADLINE_MISS;
        }

        fish_list[i].destination = resolve_destination(&fish_list[i].sample, fish_list[i].deadline_miss);
        fish_list[i].color = class_to_color(fish_list[i].sample.class_id);
        class_to_size(fish_list[i].sample.class_id, &fish_list[i].width, &fish_list[i].height);
        spawn_offset += (float) random_int_range(44, 96);
        fish_list[i].x = -150.0f - spawn_offset;
        fish_list[i].base_y = CONVEYOR_Y + 54.0f + (float) random_int_range(0, 74);
        fish_list[i].y = fish_list[i].base_y;
        fish_list[i].wave_offset = (float) random_int_range(0, 360);
        fish_list[i].phase = PHASE_BELT;
        fish_list[i].produced_logged = 0;
        fish_list[i].classified_logged = 0;
        fish_list[i].completed_logged = 0;
        fish_list[i].highlight_timer = 0.0f;
        fish_list[i].latency_ms = compute_latency_ms(&fish_list[i]);

        bin_rect = find_bin_rect(bins, fish_list[i].destination);
        fish_list[i].target_x = (float) (bin_rect.x + 18);
        fish_list[i].target_y = (float) (bin_rect.y + 14);

        printf(
            "[visual_sim] fish_id=%u class=%s destination=%s%s\n",
            fish_list[i].sample.fish_id,
            fish_class_to_string(fish_list[i].sample.class_id),
            fish_list[i].destination,
            fish_list[i].deadline_miss ? " deadline_miss" : ""
        );
    }
}

static void reset_visual_state(VisualState* state, VisualFish fish_list[MAX_FISH_COUNT], const BinVisual bins[5]) {
    int was_running = state->running;

    SDL_memset(state, 0, sizeof(*state));
    state->running = was_running;
    state->paused = 0;
    init_fish(fish_list, state, bins);
    push_log_event(state, "SYSTEM READY");
    push_log_event(state, "MODE=%s", VISUAL_MODE_NAME);
    push_log_event(state, "SCENARIO COUNT=%d", state->scenario_fish_count);
}

static void update_stats(VisualState* state, const VisualFish* fish) {
    state->stats.total_fish++;
    state->stats.total_latency_ms += fish->latency_ms;
    if (fish->latency_ms > state->stats.max_latency_ms) {
        state->stats.max_latency_ms = fish->latency_ms;
    }

    if (fish->deadline_miss) {
        state->stats.deadline_miss_count++;
    } else {
        state->stats.deadline_ok_count++;
    }

    if (fish->sample.class_id == FISH_INVALID || fish->sample.error_code == ERR_INVALID_DATA) {
        state->stats.invalid_data_count++;
    }
}

static void complete_fish(VisualState* state, VisualFish* fish) {
    if (fish->completed_logged) {
        return;
    }

    fish->completed_logged = 1;
    state->completed_count++;
    state->actuator_activity = 1.0f;
    fish->highlight_timer = fish->deadline_miss ? 1.2f : 0.0f;
    if (fish->deadline_miss) {
        state->late_reject_flash = 1.2f;
        push_log_event(state, "FISH_ID=%u DEADLINE_MISS", fish->sample.fish_id);
    } else {
        push_log_event(state, "FISH_ID=%u %s", fish->sample.fish_id, fish->destination);
    }
    update_stats(state, fish);

    if (state->completed_count == state->scenario_fish_count) {
        state->completed_timestamp_ms = SDL_GetTicks();
        push_log_event(state, "BATCH COMPLETE");
    }
}

static void update_fish(VisualState* state, VisualFish* fish, float frame_time) {
    if (!fish->active || fish->phase == PHASE_DONE) {
        return;
    }

    if (!fish->produced_logged && fish->x > 16.0f) {
        fish->produced_logged = 1;
        state->produced_count++;
        state->sensor_activity = 1.0f;
    }

    if (fish->phase == PHASE_BELT) {
        fish->x += BELT_SPEED * frame_time * 60.0f;
        fish->y = fish->base_y + SDL_sinf(state->belt_offset * 0.045f + fish->wave_offset) * 6.0f;

        if (!fish->classified_logged && fish->x >= CLASSIFY_X) {
            fish->classified_logged = 1;
            state->classified_count++;
            state->classification_activity = 1.0f;
            push_log_event(
                state,
                "FISH_ID=%u CLASS=%s",
                fish->sample.fish_id,
                fish_class_to_string(fish->sample.class_id)
            );
        }

        if (fish->x >= ROUTE_X) {
            fish->phase = PHASE_ROUTE;
        }
        return;
    }

    if (fish->phase == PHASE_ROUTE) {
        float dx = fish->target_x - fish->x;
        float dy = fish->target_y - fish->y;

        fish->x += dx * ROUTE_LERP * frame_time * 60.0f;
        fish->y += dy * ROUTE_LERP * frame_time * 60.0f;

        if (SDL_fabsf(dx) < 1.5f && SDL_fabsf(dy) < 1.5f) {
            fish->x = fish->target_x;
            fish->y = fish->target_y;
            fish->phase = PHASE_DONE;
            complete_fish(state, fish);
        }
    }
}

static void draw_background(SDL_Renderer* renderer, const BinVisual bins[5], const VisualState* state) {
    int i;
    SDL_Rect header = {20, 18, 1240, HEADER_HEIGHT};
    SDL_Rect belt_frame = {CONVEYOR_X - 10, CONVEYOR_Y - 10, CONVEYOR_WIDTH + 20, CONVEYOR_HEIGHT + 20};
    SDL_Rect belt = {CONVEYOR_X, CONVEYOR_Y, CONVEYOR_WIDTH, CONVEYOR_HEIGHT};
    SDL_Rect log_panel = {LOG_X, LOG_Y, LOG_WIDTH, LOG_HEIGHT};
    SDL_Rect actuator_label = {(int) ROUTE_X - 38, CONVEYOR_Y - 34, 170, 24};

    SDL_SetRenderDrawColor(renderer, 8, 12, 16, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 13, 20, 28, 255);
    for (i = 0; i < WINDOW_WIDTH; i += 48) {
        SDL_RenderDrawLine(renderer, i, 0, i, WINDOW_HEIGHT);
    }
    for (i = 0; i < WINDOW_HEIGHT; i += 48) {
        SDL_RenderDrawLine(renderer, 0, i, WINDOW_WIDTH, i);
    }

    draw_panel(renderer, header, (SDL_Color) {14, 20, 25, 230}, (SDL_Color) {36, 48, 58, 255});
    draw_panel(renderer, belt_frame, (SDL_Color) {26, 33, 41, 255}, (SDL_Color) {60, 72, 84, 255});
    draw_panel(renderer, belt, (SDL_Color) {55, 63, 72, 255}, (SDL_Color) {95, 107, 118, 255});

    SDL_SetRenderDrawColor(renderer, 86, 95, 106, 255);
    for (i = -1; i < 22; i++) {
        int stripe_x = CONVEYOR_X + ((i * 54) + (int) state->belt_offset) % (CONVEYOR_WIDTH + 54);
        SDL_Rect stripe = {stripe_x, CONVEYOR_Y + 42, 22, 86};
        SDL_RenderFillRect(renderer, &stripe);
    }

    SDL_SetRenderDrawColor(renderer, 24, 28, 33, 255);
    for (i = 0; i < 6; i++) {
        SDL_Rect roller = {CONVEYOR_X + 55 + i * 140, CONVEYOR_Y + CONVEYOR_HEIGHT - 22, 72, 10};
        SDL_RenderFillRect(renderer, &roller);
    }

    draw_panel(
        renderer,
        actuator_label,
        (SDL_Color) {16, 24, 30, 220},
        (SDL_Color) {70, 90, 104, 255}
    );
    draw_text_centered(
        renderer,
        actuator_label,
        2,
        (SDL_Color) {198, 212, 222, 255},
        "ACTUATOR ZONE"
    );

    for (i = 0; i < 5; i++) {
        SDL_Color fill = {18, 25, 32, 220};
        SDL_Color border = bins[i].color;
        if (SDL_strcmp(bins[i].name, "LATE_REJECT") == 0 && state->late_reject_flash > 0.0f) {
            fill.r = 60;
            fill.g = 18;
            fill.b = 18;
            border = (SDL_Color) {255, 86, 86, 255};
        }

        draw_panel(renderer, bins[i].rect, fill, border);
        SDL_SetRenderDrawColor(renderer, bins[i].color.r, bins[i].color.g, bins[i].color.b, 255);
        SDL_RenderFillRect(renderer, &(SDL_Rect) {bins[i].rect.x, bins[i].rect.y, 8, bins[i].rect.h});
        draw_text_centered(
            renderer,
            (SDL_Rect) {bins[i].rect.x + 10, bins[i].rect.y, bins[i].rect.w - 10, bins[i].rect.h},
            2,
            (SDL_Color) {235, 239, 242, 255},
            bins[i].name
        );
    }

    draw_panel(renderer, log_panel, (SDL_Color) {14, 20, 25, 232}, (SDL_Color) {36, 48, 58, 255});
}

static void draw_status_panel(SDL_Renderer* renderer, const VisualState* state) {
    char value_buffer[48];
    float average_latency_ms = 0.0f;
    int input_queue_count = state->produced_count - state->classified_count;
    int output_queue_count = state->classified_count - state->completed_count;

    if (state->stats.total_fish > 0) {
        average_latency_ms = state->stats.total_latency_ms / (float) state->stats.total_fish;
    }

    draw_status_card(renderer, 40, 34, 182, 74, "SYSTEM STATUS", "RUNNING", (SDL_Color) {72, 205, 144, 255});
    draw_status_card(renderer, 238, 34, 160, 74, "MODE", VISUAL_MODE_NAME, (SDL_Color) {70, 168, 255, 255});

    SDL_snprintf(value_buffer, sizeof(value_buffer), "%d", state->stats.deadline_ok_count);
    draw_status_card(renderer, 416, 34, 122, 74, "DL OK", value_buffer, (SDL_Color) {72, 205, 144, 255});

    SDL_snprintf(value_buffer, sizeof(value_buffer), "%d", state->stats.deadline_miss_count);
    draw_status_card(renderer, 552, 34, 122, 74, "DL MISS", value_buffer, (SDL_Color) {255, 96, 96, 255});

    SDL_snprintf(value_buffer, sizeof(value_buffer), "%d", state->stats.invalid_data_count);
    draw_status_card(renderer, 688, 34, 130, 74, "INVALID", value_buffer, (SDL_Color) {255, 170, 70, 255});

    SDL_snprintf(value_buffer, sizeof(value_buffer), "IN:%d OUT:%d", input_queue_count, output_queue_count);
    draw_status_card(renderer, 820, 34, 226, 74, "QUEUE STATUS", value_buffer, (SDL_Color) {118, 136, 153, 255});

    SDL_snprintf(value_buffer, sizeof(value_buffer), "%.2fMS", average_latency_ms);
    draw_status_card(renderer, 1054, 34, 94, 74, "AVG", value_buffer, (SDL_Color) {80, 180, 255, 255});

    SDL_snprintf(value_buffer, sizeof(value_buffer), "%.2fMS", state->stats.max_latency_ms);
    draw_status_card(renderer, 1160, 34, 98, 74, "MAX", value_buffer, (SDL_Color) {255, 176, 67, 255});

    draw_activity_indicator(renderer, 44, 112, "SENSOR THREAD ACTIVE", state->sensor_activity);
    draw_activity_indicator(renderer, 330, 112, "CLASSIFICATION THREAD ACTIVE", state->classification_activity);
    draw_activity_indicator(renderer, 742, 112, "ACTUATOR THREAD ACTIVE", state->actuator_activity);
}

static void draw_log_panel(SDL_Renderer* renderer, const VisualState* state) {
    int i;

    draw_text(renderer, LOG_X + 16, LOG_Y + 14, 2, (SDL_Color) {148, 164, 176, 255}, "RECENT EVENTS");
    for (i = 0; i < state->log_panel.count; i++) {
        SDL_Color color = {224, 231, 235, 255};
        if (SDL_strstr(state->log_panel.lines[i], "DEADLINE_MISS") != NULL) {
            color = (SDL_Color) {255, 112, 112, 255};
        }
        draw_text(renderer, LOG_X + 16, LOG_Y + 40 + i * 16, 2, color, state->log_panel.lines[i]);
    }
}

static void draw_fish(SDL_Renderer* renderer, const VisualFish* fish) {
    if (!fish->active) {
        return;
    }

    SDL_Rect body = {(int) fish->x, (int) fish->y, fish->width, fish->height};
    SDL_Point tail[4];
    int body_radius = fish->height / 2;
    int eye_x = body.x + fish->width - 12;
    int eye_y = body.y + fish->height / 2 - 3;

    if (fish->highlight_timer > 0.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 72, 72, 140);
        SDL_RenderFillRect(renderer, &(SDL_Rect) {
            body.x - 8, body.y - 8, body.w + 16, body.h + 16
        });
    }

    SDL_SetRenderDrawColor(renderer, fish->color.r, fish->color.g, fish->color.b, 255);
    SDL_RenderFillRect(renderer, &body);
    draw_filled_circle(renderer, body.x, body.y + body_radius, body_radius);
    draw_filled_circle(renderer, body.x + body.w, body.y + body_radius, body_radius);

    SDL_SetRenderDrawColor(renderer, 20, 26, 32, 220);
    SDL_RenderDrawRect(renderer, &(SDL_Rect) {body.x - 1, body.y - 1, body.w + 2, body.h + 2});

    tail[0].x = body.x - fish->width / 3;
    tail[0].y = body.y + body.h / 2;
    tail[1].x = body.x;
    tail[1].y = body.y + 2;
    tail[2].x = body.x;
    tail[2].y = body.y + body.h - 2;
    tail[3] = tail[0];
    SDL_SetRenderDrawColor(renderer, fish->color.r, fish->color.g, fish->color.b, 255);
    SDL_RenderDrawLines(renderer, tail, 4);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect) {eye_x, eye_y, 5, 5});
    SDL_SetRenderDrawColor(renderer, 12, 16, 20, 255);
    SDL_RenderFillRect(renderer, &(SDL_Rect) {eye_x + 2, eye_y + 2, 2, 2});
}

static void draw_route_guides(
    SDL_Renderer* renderer,
    const VisualFish fish_list[MAX_FISH_COUNT],
    int fish_count
) {
    int i;

    for (i = 0; i < fish_count; i++) {
        if (fish_list[i].phase == PHASE_ROUTE) {
            int start_x = (int) fish_list[i].x + fish_list[i].width + 8;
            int start_y = (int) fish_list[i].y + fish_list[i].height / 2;
            int end_x = (int) fish_list[i].target_x - 10;
            int end_y = (int) fish_list[i].target_y + fish_list[i].height / 2;
            SDL_Color guide = fish_list[i].deadline_miss
                ? (SDL_Color) {255, 92, 92, 170}
                : (SDL_Color) {119, 170, 212, 130};

            SDL_SetRenderDrawColor(renderer, guide.r, guide.g, guide.b, guide.a);
            SDL_RenderDrawLine(renderer, start_x, start_y, end_x, end_y);
            SDL_RenderDrawLine(renderer, end_x, end_y, end_x - 8, end_y - 5);
            SDL_RenderDrawLine(renderer, end_x, end_y, end_x - 8, end_y + 5);
        }
    }
}

static void decay_activity(VisualState* state, float frame_time) {
    float decay = frame_time * 1.6f;

    state->sensor_activity = SDL_max(0.0f, state->sensor_activity - decay);
    state->classification_activity = SDL_max(0.0f, state->classification_activity - decay);
    state->actuator_activity = SDL_max(0.0f, state->actuator_activity - decay);
    state->late_reject_flash = SDL_max(0.0f, state->late_reject_flash - decay);
}

int main(void) {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Event event;
    Uint64 previous_counter;
    BinVisual bins[5];
    VisualFish fish_list[MAX_FISH_COUNT];
    VisualState state = {0};
    int i;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "Industrial Realtime Fish Sorting Monitor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    srand((unsigned int) (SDL_GetPerformanceCounter() ^ SDL_GetTicks() ^ 0x5EED1234U));

    init_bins(bins);
    reset_visual_state(&state, fish_list, bins);
    state.running = 1;
    previous_counter = SDL_GetPerformanceCounter();

    printf("Controls: R=restart, P=pause/resume, ESC=exit\n");

    while (state.running) {
        Uint64 current_counter = SDL_GetPerformanceCounter();
        float frame_time = (float) (current_counter - previous_counter) / (float) SDL_GetPerformanceFrequency();
        previous_counter = current_counter;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                state.running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    state.running = 0;
                } else if (event.key.keysym.sym == SDLK_r) {
                    reset_visual_state(&state, fish_list, bins);
                    previous_counter = SDL_GetPerformanceCounter();
                } else if (event.key.keysym.sym == SDLK_p) {
                    state.paused = !state.paused;
                }
            }
        }

        if (!state.paused) {
            state.belt_offset += frame_time * 170.0f;
            decay_activity(&state, frame_time);

            for (i = 0; i < state.scenario_fish_count; i++) {
                update_fish(&state, &fish_list[i], frame_time);
            }

            if (state.completed_count == state.scenario_fish_count &&
                state.completed_timestamp_ms > 0 &&
                SDL_GetTicks() - state.completed_timestamp_ms >= RESTART_DELAY_MS) {
                reset_visual_state(&state, fish_list, bins);
                previous_counter = SDL_GetPerformanceCounter();
            }
        }

        draw_background(renderer, bins, &state);
        draw_status_panel(renderer, &state);
        draw_route_guides(renderer, fish_list, state.scenario_fish_count);

        for (i = 0; i < state.scenario_fish_count; i++) {
            if (fish_list[i].highlight_timer > 0.0f) {
                fish_list[i].highlight_timer = SDL_max(0.0f, fish_list[i].highlight_timer - frame_time * 1.4f);
            }
            draw_fish(renderer, &fish_list[i]);
        }

        draw_log_panel(renderer, &state);
        SDL_RenderPresent(renderer);
        SDL_Delay(FPS_DELAY_MS);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

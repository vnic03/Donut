#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <pthread.h>


#define WIDTH 1000
#define HEIGHT 800

#define BRIGHTNESS_SCALE 12

#define LOOKUP_SIZE 628 // 2 * PI * 100
float sin_lookup[LOOKUP_SIZE];
float cos_lookup[LOOKUP_SIZE];

#define K1_MIN 30.0f
#define K1_MAX 800.0f
#define K2_MIN 30.0f
#define K2_MAX 800.0f

#define DISTANCE_MIN -1.0f
#define DISTANCE_MAX 500.0f

#define COLOR_MIN 0.0f
#define COLOR_MAX 360.0f

#define SPEED_MIN 0.01f
#define SPEED_MAX 3.0f

#define SLIDER_HEIGHT 10
#define SLIDER_HANDLE_WIDTH 15
#define SLIDER_HANDLE_HEIGHT 20

#define BUTTON_WIDTH 50
#define BUTTON_HEIGHT 25
#define BUTTON_X (WIDTH - 100)


typedef struct {
    // Angle parameters for controlling the donut animation
    float A;             // Rotation angle around the X-axis
    float B;             // Rotation angle around the Y-axis

    // Scaling factors for the donut rendering
    float K1;            // Horizontal scaling factor
    float K2;            // Vertical scaling factor

    // Distance of the donut from the camera
    float DISTANCE;      // Distance of the donut from the viewing plane

    // Color value for rendering the donut
    float color_value;   // Color value in HSV color scale (0-360)

    // Renderer for drawing the donut
    SDL_Renderer* renderer;

    // Start and end lines for donut calculation (for parallelization)
    int start_y;         // Starting line of the donut section to be calculated
    int end_y;           // Ending line of the donut section to be calculated

    // Z-buffer for depth calculation
    float* z;            // Array for storing depth values

    // Brightness buffer for rendering calculations
    char* b;             // Array for storing brightness values (characters)
} DonutArgs;


const char brightness[] = ".,-~:;=!*#$@";


void init_lookup_tables() {
    for (int i = 0; i < LOOKUP_SIZE; i++) {
        float angle = i / 100.0f;
        sin_lookup[i] = sinf(angle);
        cos_lookup[i] = cosf(angle);
    }
}

void render_donut(float A, float B, SDL_Renderer* renderer, float K1, float K2, float DISTANCE, float color_value);

void handle_slider_event(SDL_Event* event, SDL_Rect slider, float* value, float min, float max, int* active_slider, int slider_id);

SDL_Rect create_slider_handle(SDL_Rect slider, float value, float min, float max);

void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y);

void render_button(SDL_Renderer* renderer, TTF_Font* font, SDL_Rect button, const char* text);

void hsv_to_rgb(float h, float s, float v, int* r, int* g, int* b);


int main(void) {
    SDL_Window* window = SDL_CreateWindow("donut.c", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (TTF_Init() != 0) {
        printf("TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("../font/DejaVuSans-Bold.ttf", 16);

    float A = 0, B = 0;
    float K1 = 200.0f, K2 = 200.0f, DISTANCE = 5.f, color_value = 300.f, speed = 0.18f;
    int running = 1;
    SDL_Event event;

    SDL_Rect k1_slider = {0, HEIGHT - 110, WIDTH, SLIDER_HEIGHT};
    SDL_Rect k2_slider = {0, HEIGHT - 80, WIDTH, SLIDER_HEIGHT};
    SDL_Rect color_slider = {0, HEIGHT - 50, WIDTH, SLIDER_HEIGHT};
    SDL_Rect speed_slider = {0, HEIGHT - 20, WIDTH, SLIDER_HEIGHT};

    SDL_Rect toggle_button = {BUTTON_X, 20, BUTTON_WIDTH, BUTTON_HEIGHT};
    SDL_Rect reset_button = {BUTTON_X, 60, BUTTON_WIDTH, BUTTON_HEIGHT};
    SDL_Rect rainbow_button = {BUTTON_X, 100, BUTTON_WIDTH, BUTTON_HEIGHT};
    int rainbow_mode = 0;
    float rainbow_color = 0.0f;

    int sliders_visible = 1;
    int active_slider = 0;

    int mouse_down = 0;
    int last_mouse_x, last_mouse_y;

    init_lookup_tables();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;

            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x = event.button.x;
                int y = event.button.y;
                if (x >= toggle_button.x && x <= toggle_button.x + toggle_button.w &&
                    y >= toggle_button.y && y <= toggle_button.y + toggle_button.h)
                {
                    sliders_visible = !sliders_visible;
                } else if (x >= reset_button.x && x <= reset_button.x + reset_button.w &&
                           y >= reset_button.y && y <= reset_button.y + reset_button.h)
                {
                    A = 0; B = 0;
                    K1 = K2 = 200.0f;
                    DISTANCE = 5.0f;
                    color_value = 300.0f;
                    speed = 0.18f;
                    rainbow_mode = 0;

                } else if (x >= rainbow_button.x && x <= rainbow_button.x + rainbow_button.w &&
                           y >= rainbow_button.y && y <= rainbow_button.y + rainbow_button.h)
                {
                    rainbow_mode = !rainbow_mode;
                }
                if (sliders_visible) {
                    handle_slider_event(&event, k1_slider, &K1, K1_MIN, K1_MAX, &active_slider, 1);
                    handle_slider_event(&event, k2_slider, &K2, K2_MIN, K2_MAX, &active_slider, 2);
                    handle_slider_event(&event, color_slider, &color_value, COLOR_MIN, COLOR_MAX, &active_slider, 3);
                    handle_slider_event(&event, speed_slider, &speed, SPEED_MIN, SPEED_MAX, &active_slider, 4);
                }
                if (event.button.button == SDL_BUTTON_LEFT && active_slider == 0) {
                    mouse_down = 1;
                    last_mouse_x = x;
                    last_mouse_y = y;
                }

            } else if (event.type == SDL_MOUSEBUTTONUP) {
                active_slider = 0;
                if (event.button.button == SDL_BUTTON_LEFT) mouse_down = 0;

            } else if (event.type == SDL_MOUSEMOTION) {
                if (active_slider != 0 && sliders_visible) {
                    mouse_down = 0;
                    if (active_slider == 1) {
                        handle_slider_event(&event, k1_slider, &K1, K1_MIN, K1_MAX, &active_slider, 1);
                    } else if (active_slider == 2) {
                        handle_slider_event(&event, k2_slider, &K2, K2_MIN, K2_MAX, &active_slider, 2);
                    } else if (active_slider == 3) {
                        handle_slider_event(&event, color_slider, &color_value, COLOR_MIN, COLOR_MAX, &active_slider, 3);
                    } else if (active_slider == 4) {
                        handle_slider_event(&event, speed_slider, &speed, SPEED_MIN, SPEED_MAX, &active_slider, 4);
                    }
                } else if (mouse_down) {
                    int mouse_x, mouse_y;
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    A += (mouse_y - last_mouse_y) * 0.01f;
                    B += (mouse_x - last_mouse_x) * 0.01f;
                    last_mouse_x = mouse_x;
                    last_mouse_y = mouse_y;
                }

            } else if (event.type == SDL_MOUSEWHEEL) {
                DISTANCE += event.wheel.y * 0.4f;
                if (DISTANCE < DISTANCE_MIN) DISTANCE = DISTANCE_MIN;
                if (DISTANCE > DISTANCE_MAX) DISTANCE = DISTANCE_MAX;
            }
        }

        if (rainbow_mode) {
            color_value = rainbow_color;
            rainbow_color += 0.4f;
            if (rainbow_color >= COLOR_MAX) rainbow_color = 0.0f;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        render_donut(A, B, renderer, K1, K2, DISTANCE, color_value);

        if (sliders_visible) {
            SDL_SetRenderDrawColor(renderer, 128, 0, 128, 255);
            SDL_RenderFillRect(renderer, &k1_slider);
            SDL_RenderFillRect(renderer, &k2_slider);
            SDL_RenderFillRect(renderer, &color_slider);
            SDL_RenderFillRect(renderer, &speed_slider);

            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

            SDL_Rect k1_handle = create_slider_handle(k1_slider, K1, K1_MIN, K1_MAX);
            SDL_Rect k2_handle = create_slider_handle(k2_slider, K2, K2_MIN, K2_MAX);
            SDL_Rect color_handle = create_slider_handle(color_slider, color_value, COLOR_MIN, COLOR_MAX);
            SDL_Rect speed_handle = create_slider_handle(speed_slider, speed, SPEED_MIN, SPEED_MAX);

            SDL_RenderFillRect(renderer, &k1_handle);
            SDL_RenderFillRect(renderer, &k2_handle);
            SDL_RenderFillRect(renderer, &color_handle);
            SDL_RenderFillRect(renderer, &speed_handle);

            render_text(renderer, font, "K1 (horizontal scl.)", k1_slider.x, k1_slider.y - 20);
            render_text(renderer, font, "K2 (vertical scl.)", k2_slider.x, k2_slider.y - 20);
            render_text(renderer, font, "COLOR", color_slider.x, color_slider.y - 20);
            render_text(renderer, font, "SPEED", speed_slider.x, speed_slider.y - 20);
        }

        render_button(renderer, font, toggle_button, sliders_visible ? "HIDE" : "SHOW");
        render_button(renderer, font, reset_button, "RESET");
        render_button(renderer, font, rainbow_button, "RAINBOW");

        SDL_RenderPresent(renderer);
        SDL_Delay(5);

        if (mouse_down == 0) {
            A += 0.05f * speed;
            B += 0.03f * speed;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void* render_donut_section(void* args) {
    DonutArgs *donut = (DonutArgs*)args;
    const float A = donut->A, B = donut->B, K1 = donut->K1, K2 = donut->K2, DISTANCE = donut->DISTANCE;
    const float sinA = sinf(A), cosA = cosf(A), cosB = cosf(B), sinB = sinf(B);
    float* z = donut->z;
    char* b = donut->b;

    for (int phi_i = 0; phi_i < LOOKUP_SIZE; phi_i += 7) {
        const float cosPhi = cos_lookup[phi_i], sinPhi = sin_lookup[phi_i];
        for (int theta_i = 0; theta_i < LOOKUP_SIZE; theta_i += 2) {
            const float sinTheta = sin_lookup[theta_i], cosTheta = cos_lookup[theta_i];
            const float cosPhi2 = cosPhi + 2.f;
            const float mess = 1.f / (sinTheta * cosPhi2 * sinA + sinPhi * cosA + DISTANCE);
            const float t = sinTheta * cosPhi2 * cosA - sinPhi * sinA;

            int x = (int)(WIDTH / 2 + K1 * mess * (cosTheta * cosPhi2 * cosB - t * sinB));
            int y = (int)(HEIGHT / 2 + K2 * mess * (cosTheta * cosPhi2 * sinB + t * cosB));
            int o = x + WIDTH * y;
            int N = (int)(BRIGHTNESS_SCALE * ((sinPhi * sinA - sinTheta * cosPhi * cosA) * cosB - sinTheta * cosPhi * sinA - sinPhi * cosA - cosTheta * cosPhi * sinB));

            if (y >= donut->start_y && y < donut->end_y && y > 0 && x > 0 && WIDTH > x && mess > z[o]) {
                z[o] = mess;
                b[o] = brightness[N > 0 ? N : 0];
            }
        }
    }
    return NULL;
}

void render_donut(float A, float B, SDL_Renderer* renderer, float K1, float K2, float DISTANCE, float color_value) {
    float z[WIDTH * HEIGHT];
    char b[WIDTH * HEIGHT];

    memset(b, 32, WIDTH * HEIGHT);
    memset(z, 0, WIDTH * HEIGHT * sizeof(float));

    const int num_threads = 4;
    pthread_t threads[num_threads];
    DonutArgs args[num_threads];

    const int section_height = HEIGHT / num_threads;
    for (int i = 0; i < num_threads; i++) {
        args[i] = (DonutArgs){A, B, K1, K2, DISTANCE, color_value, renderer, i * section_height, (i + 1) * section_height, z, b};
        pthread_create(&threads[i], NULL, render_donut_section, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    int r, g, b_col;
    for (int k = 0; k < WIDTH * HEIGHT; k++) {
        if (b[k] != 32) {
            int x = k % WIDTH;
            int y = k / WIDTH;

            int brightness_value = (b[k] - ' ') * 255 / 12;
            hsv_to_rgb(color_value, 1.0f, (float)brightness_value / 255.0f, &r, &g, &b_col);
            SDL_SetRenderDrawColor(renderer, r, g, b_col, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

void handle_slider_event(SDL_Event* event, SDL_Rect slider, float* value, float min, float max, int* active_slider, int slider_id) {
    int x = event->button.x;
    int y = event->button.y;
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (x >= slider.x && x <= slider.x + slider.w && y >= slider.y && y <= slider.y + slider.h) {
            *value = min + (float)(x - slider.x) / slider.w * (max - min);
            *active_slider = slider_id;
        }
    } else if (event->type == SDL_MOUSEMOTION && *active_slider == slider_id) {
        *value = min + (float)(x - slider.x) / slider.w * (max - min);
    }
}

SDL_Rect create_slider_handle(SDL_Rect slider, float value, float min, float max) {
    SDL_Rect handle;
    handle.x = slider.x + (int)((value - min) / (max - min) * slider.w) - 7;
    handle.y = slider.y - 5;
    handle.w = SLIDER_HANDLE_WIDTH;
    handle.h = SLIDER_HANDLE_HEIGHT;

    if (handle.x < slider.x) handle.x = slider.x;
    if (handle.x + handle.w > slider.x + slider.w) handle.x = slider.x + slider.w - handle.w;

    return handle;
}

void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y) {
    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = {x, y, surface->w, surface->h};

    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void render_button(SDL_Renderer* renderer, TTF_Font* font, SDL_Rect button, const char* text) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderFillRect(renderer, &button);
    render_text(renderer, font, text, button.x + 5, button.y + 5);
}

void hsv_to_rgb(float h, float s, float v, int* r, int* g, int* b) {
    float p, q, t, ff;
    long i;
    float r1, g1, b1;

    h = fmod(h, COLOR_MAX);
    if (h < 0.f) h += COLOR_MAX;

    s = s < 0.f ? 0.f : s > 1.f ? 1.f : s;
    v = v < 0.f ? 0.f : v > 1.f ? 1.f : v;

    if (s == 0.f) {
        *r = *g = *b = (int)(v * 255.f);
        return;
    }

    ff = h / 60.0f;
    i = (long)ff;
    ff -= (float)i;
    p = v * (1.f - s);
    q = v * (1.f - (s * ff));
    t = v * (1.f - (s * (1.f - ff)));

    switch (i) {
        case 0:
            r1 = v;
            g1 = t;
            b1 = p;
            break;
        case 1:
            r1 = q;
            g1 = v;
            b1 = p;
            break;
        case 2:
            r1 = p;
            g1 = v;
            b1 = t;
            break;
        case 3:
            r1 = p;
            g1 = q;
            b1 = v;
            break;
        case 4:
            r1 = t;
            g1 = p;
            b1 = v;
            break;
        default:
            r1 = v;
            g1 = p;
            b1 = q;
            break;
    }

    *r = (int)(r1 * 255.f);
    *g = (int)(g1 * 255.f);
    *b = (int)(b1 * 255.f);
}

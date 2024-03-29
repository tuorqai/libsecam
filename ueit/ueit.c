
//------------------------------------------------------------------------------
// cc -lm -lSDL2 -o ueit ueit.c
//------------------------------------------------------------------------------
// Usage:
// ./ueit
// ./ueit image.bmp
//------------------------------------------------------------------------------
// Controls:
// Up/Down Arrow: select option
// Left/Right Arrow: control option
// Space: Pause/unpause filtering
// Enter: Filter for one frame (if paused)
// Escape: exit
//------------------------------------------------------------------------------

#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>

#define LIBSECAM_IMPLEMENTATION
#include "../libsecam.h"

//------------------------------------------------------------------------------

enum option
{
    OPTION_LUMA_NOISE_FACTOR,
    OPTION_CHROMA_NOISE_FACTOR,
    OPTION_CHROMA_FIRE_FACTOR,
    OPTION_ECHO_OFFSET,
    OPTION_SKEW,
    OPTION_HORIZONTAL_INSTABILITY,
    TOTAL_OPTIONS,
};

static char const *optionNames[TOTAL_OPTIONS] = {
    "LUMA_NOISE",
    "CHROMA_NOISE",
    "CHROMA_FIRE",
    "ECHO",
    "SKEW",
    "WOBBLE",
};

static int pause = 0;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Surface *ueitSurface = NULL;
static SDL_Texture *ueitTexture = NULL;
static libsecam_t *libsecam = NULL;
static libsecam_options_t *options = NULL;
static enum option currentOption = 0;
static int pingUpdate = 0;
static char optionText[256];

static void updateWindowTitle(void);

//------------------------------------------------------------------------------

static SDL_Surface *loadImage(char const *path)
{
    SDL_Surface *src, *conv;

    if ((src = IMG_Load(path)) == NULL) {
        return NULL;
    }

    conv = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_XBGR8888, 0);
    SDL_FreeSurface(src);

    return conv;
}

static void fillTexture(SDL_Texture *texture, unsigned char const *pixels, int width, int height)
{
    unsigned char *data;
    int pitch;

    SDL_LockTexture(texture, NULL, (void **) &data, &pitch);

    for (int i = 0; i < height; i++) {
        unsigned char const *row = &pixels[i * width * 4];
        memcpy(&data[i * pitch], row, width * 4);
    }

    SDL_UnlockTexture(texture);
}

static int initialize(char const *path)
{
    if (SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }

    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE, "letterbox");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    if (SDL_CreateWindowAndRenderer(720, 576, SDL_WINDOW_RESIZABLE, &window, &renderer) == -1) {
        return 1;
    }

    SDL_SetWindowTitle(window, "UEIT");
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_RenderSetVSync(renderer, 1);
    SDL_RenderSetLogicalSize(renderer, 720, 576);

    if ((ueitSurface = loadImage(path ? path : "ueit.bmp")) == NULL) {
        return 1;
    }

    if ((ueitTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, ueitSurface->w, ueitSurface->h)) == NULL) {
        return 1;
    }

    libsecam = libsecam_init(ueitSurface->w, ueitSurface->h);
    options = libsecam_options(libsecam);

    return 0;
}

static void terminate(void)
{
    libsecam_close(libsecam);
    SDL_DestroyTexture(ueitTexture);
    SDL_FreeSurface(ueitSurface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

static void decrementOption(double mul)
{
    switch (currentOption) {
    case OPTION_LUMA_NOISE_FACTOR:
        options->luma_noise -= 0.01 * mul;
        break;
    case OPTION_CHROMA_NOISE_FACTOR:
        options->chroma_noise -= 0.01 * mul;
        break;
    case OPTION_CHROMA_FIRE_FACTOR:
        options->chroma_fire -= 0.01 * mul;
        break;
    case OPTION_ECHO_OFFSET:
        options->echo -= 1;
        break;
    case OPTION_SKEW:
        options->skew -= 1;
        break;
    case OPTION_HORIZONTAL_INSTABILITY:
        options->wobble -= 1;
        break;
    }
}

static void incrementOption(double mul)
{
    switch (currentOption) {
    case OPTION_LUMA_NOISE_FACTOR:
        options->luma_noise += 0.01 * mul;
        break;
    case OPTION_CHROMA_NOISE_FACTOR:
        options->chroma_noise += 0.01 * mul;
        break;
    case OPTION_CHROMA_FIRE_FACTOR:
        options->chroma_fire += 0.01 * mul;
        break;
    case OPTION_ECHO_OFFSET:
        options->echo += 1;
        break;
    case OPTION_SKEW:
        options->skew += 1;
        break;
    case OPTION_HORIZONTAL_INSTABILITY:
        options->wobble += 1;
        break;
    }
}

static void printOption(void)
{
    char buffer[64];

    switch (currentOption) {
    case OPTION_LUMA_NOISE_FACTOR:
        snprintf(buffer, sizeof(buffer) - 1, "%.3f", options->luma_noise);
        break;
    case OPTION_CHROMA_NOISE_FACTOR:
        snprintf(buffer, sizeof(buffer) - 1, "%.3f", options->chroma_noise);
        break;
    case OPTION_CHROMA_FIRE_FACTOR:
        snprintf(buffer, sizeof(buffer) - 1, "%.3f", options->chroma_fire);
        break;
    case OPTION_ECHO_OFFSET:
        snprintf(buffer, sizeof(buffer) - 1, "%d", options->echo);
        break;
    case OPTION_SKEW:
        snprintf(buffer, sizeof(buffer) - 1, "%d", options->skew);
        break;
    case OPTION_HORIZONTAL_INSTABILITY:
        snprintf(buffer, sizeof(buffer) - 1, "%d", options->wobble);
        break;
    }

    snprintf(optionText, sizeof(optionText) - 1, "%s: %s",
        optionNames[currentOption], buffer);
    
    updateWindowTitle();
}

static void keyInput(SDL_Scancode key, unsigned int mod)
{
    switch (key) {
    case SDL_SCANCODE_SPACE:
        pause = !pause;
        break;
    case SDL_SCANCODE_RETURN:
        pingUpdate = 1;
        break;
    case SDL_SCANCODE_UP:
        if (currentOption > 0) {
            currentOption--;
            printOption();
        }
        break;
    case SDL_SCANCODE_DOWN:
        if (currentOption < (TOTAL_OPTIONS - 1)) {
            currentOption++;
            printOption();
        }
        break;
    case SDL_SCANCODE_LEFT:
        decrementOption((mod & KMOD_SHIFT) ? 10.0 : ((mod & KMOD_CTRL) ? 0.1 : 1.0));
        printOption();
        pingUpdate = 1;
        break;
    case SDL_SCANCODE_RIGHT:
        incrementOption((mod & KMOD_SHIFT) ? 10.0 : ((mod & KMOD_CTRL) ? 0.1 : 1.0));
        printOption();
        pingUpdate = 1;
        break;
    case SDL_SCANCODE_BACKSPACE:
        options->luma_noise = LIBSECAM_DEFAULT_LUMA_NOISE;
        options->chroma_noise = LIBSECAM_DEFAULT_CHROMA_NOISE;
        options->chroma_fire = LIBSECAM_DEFAULT_CHROMA_FIRE;
        options->echo = LIBSECAM_DEFAULT_ECHO;
        options->skew = LIBSECAM_DEFAULT_SKEW;
        options->wobble = LIBSECAM_DEFAULT_WOBBLE;
        printOption();
        break;
    case SDL_SCANCODE_0:
        options->luma_noise = 0.0;
        options->chroma_noise = 0.0;
        options->chroma_fire = 0.0;
        options->echo = 0;
        options->skew = 0;
        options->wobble = 0;
        printOption();
        break;
    default:
        break;
    }
}

static int process(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            return 1;
        case SDL_KEYDOWN:
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                return 1;
            } else {
                keyInput(event.key.keysym.scancode, event.key.keysym.mod);
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

static int framerateCounter = 0;
static unsigned int framerateLastCheck = 0;
static int framerateValue = -1;
static float framerateAvg = -1.f;
static char windowTitleBuffer[256];

static void updateWindowTitle(void)
{
    snprintf(windowTitleBuffer, sizeof(windowTitleBuffer) - 1,
        "UEIT [%d fps, %.2f ms avg] (%s)",
        framerateValue, framerateAvg, optionText);

    SDL_SetWindowTitle(window, windowTitleBuffer);
}

static void loop(void)
{
    unsigned int ticks = SDL_GetTicks();

    if ((ticks - framerateLastCheck) > 1000) {
        framerateValue = framerateCounter;
        framerateAvg = (ticks - framerateLastCheck) / (float) framerateCounter;
        updateWindowTitle();
        framerateLastCheck = ticks;
        framerateCounter = 0;
    }

    if (pause == 0 || pingUpdate) {
        SDL_UnlockSurface(ueitSurface);
        unsigned char const *filtered = libsecam_filter(libsecam, ueitSurface->pixels);
        SDL_LockSurface(ueitSurface);

        fillTexture(ueitTexture, filtered, ueitSurface->w, ueitSurface->h);

        if (pingUpdate) {
            pingUpdate = 0;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, ueitTexture, NULL, NULL);
    SDL_RenderPresent(renderer);

    framerateCounter++;
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if (initialize((argc < 2) ? NULL : argv[1])) {
        return EXIT_FAILURE;
    }

    atexit(terminate);

    printOption();

    while (process() == 0) {
        loop();
    }

    return EXIT_SUCCESS;
}

//------------------------------------------------------------------------------

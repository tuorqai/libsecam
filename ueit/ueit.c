
//------------------------------------------------------------------------------
// cc -lm -lSDL2 -o ueit ueit.c
//------------------------------------------------------------------------------

#include <SDL2/SDL.h>

#define LIBSECAM_IMPLEMENTATION
#include "../libsecam.h"

//------------------------------------------------------------------------------

static int pause = 0;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Surface *ueitSurface = NULL;
static SDL_Texture *ueitTexture = NULL;
static libsecam_t *libsecam = NULL;

//------------------------------------------------------------------------------

static SDL_Surface *loadBMP(char const *path)
{
    SDL_Surface *src, *conv;

    if ((src = SDL_LoadBMP(path)) == NULL) {
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

    SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE, "overscan");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    if (SDL_CreateWindowAndRenderer(720, 576, SDL_WINDOW_RESIZABLE, &window, &renderer) == -1) {
        return 1;
    }

    SDL_SetWindowTitle(window, "UEIT");
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_RenderSetVSync(renderer, 1);
    SDL_RenderSetLogicalSize(renderer, 720, 576);

    if ((ueitSurface = loadBMP(path ?: "ueit.bmp")) == NULL) {
        return 1;
    }

    if ((ueitTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 720, 576)) == NULL) {
        return 1;
    }

    libsecam = libsecam_init(720, 576);

    return 0;
}

static void terminate(void)
{
    libsecam_close(libsecam);
    SDL_DestroyTexture(ueitTexture);
    SDL_FreeSurface(ueitSurface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
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
            } else if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                pause = !pause;
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

static void loop(void)
{
    if (pause == 0) {
        SDL_UnlockSurface(ueitSurface);
        unsigned char const *filtered = libsecam_filter(libsecam, ueitSurface->pixels);
        SDL_LockSurface(ueitSurface);

        fillTexture(ueitTexture, filtered, 720, 576);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, ueitTexture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if (initialize((argc < 2) ? NULL : argv[1])) {
        return EXIT_FAILURE;
    }

    atexit(terminate);

    while (process() == 0) {
        loop();
    }

    return EXIT_SUCCESS;
}

//------------------------------------------------------------------------------

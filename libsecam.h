//------------------------------------------------------------------------------
// Copyright (c) 2023 tuorqai
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//------------------------------------------------------------------------------
// libsecam: SECAM fire video filter
//
// Input and output: array of width * height pixels.
// Each pixel is a series of 4 bytes: red, green, blue and unused byte (XRGB).
// Output is the same format.
// Width should be divisible by 8, height should be divisible by 2.
//
// Version history:
//      3.5a    2024.02.14  Minor updates
//      3.5     2024.02.14  Update echo effect (no more dark image)
//      3.4     2024.02.13  Separate stable shift option
//      3.3     2023.11.20  apply_fire() updated
//      3.2     2023.11.20  New fixes:
//                          * Fires on cyan and yellow backgrounds
//                          * Static shift: bright parts of image get shifted
//      3.1     2023.11.20  New, more realistic chroma noise
//      3.0     2023.11.20  Update 3:
//                          * Add fires on luma edges
//                          * Add fires on cyan background
//                          * Removed features: luma fire, luma/chrome loss,
//                            chroma shift
//                          * Corresponding options made obsolete
//      2.5     2023.11.19  Remove obsolete code, replace unnecessary bilerp()
//                          by lerp(), better comments.
//      2.4     2023.11.19  Const chroma_loss, randomized chroma shift,
//                          no negative chroma fire sign
//      2.3     2023.11.18  Update 2, Revision 3 (less color loss on RGB-YUV)
//      2.2     2023.11.18  Update 2, Revision 2 (added _filter_to_buffer())
//      2.1     2023.11.18  Update 2, Revision 1
//      2.0     2023.11.18  Update 2
//      1.0     2023.02.08  Initial release
//------------------------------------------------------------------------------

#ifndef TUORQAI_LIBSECAM_H
#define TUORQAI_LIBSECAM_H

//------------------------------------------------------------------------------

#include <stdbool.h>

//------------------------------------------------------------------------------

#define LIBSECAM_DEFAULT_LUMA_NOISE                 0.07
#define LIBSECAM_DEFAULT_CHROMA_NOISE               0.25
#define LIBSECAM_DEFAULT_CHROMA_FIRE                0.04
#define LIBSECAM_DEFAULT_ECHO                       4
#define LIBSECAM_DEFAULT_SKEW                       2
#define LIBSECAM_DEFAULT_WOBBLE                     0

//------------------------------------------------------------------------------

typedef struct libsecam_s libsecam_t;

//------------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct libsecam_options
{
    double luma_noise;              // range: 0.0 to 1.0
    double chroma_noise;            // range: 0.0 to 1.0
    double chroma_fire;             // range: 0.0 to 1.0
    int echo;                       // range: 0 to whatever
    int skew;                       // range: 0 to whatever
    int wobble;                     // range: 0 to whatever
} libsecam_options_t;

libsecam_t *libsecam_init(int width, int height);
void libsecam_close(libsecam_t *self);
void libsecam_filter_to_buffer(libsecam_t *self, unsigned char const *src, unsigned char *dst);
unsigned char const *libsecam_filter(libsecam_t *self, unsigned char const *src);
libsecam_options_t *libsecam_options(libsecam_t *self);

#if defined(__cplusplus)
}
#endif

//------------------------------------------------------------------------------

#ifdef LIBSECAM_IMPLEMENTATION

//------------------------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------

#if !defined(LIBSECAM_MALLOC)
#define LIBSECAM_MALLOC(s)          malloc(s)
#endif

#if !defined(LIBSECAM_FREE)
#define LIBSECAM_FREE(p)            free(p)
#endif

#if !defined(LIBSECAM_RAND_MAX)
#define LIBSECAM_RAND_MAX           RAND_MAX
#endif

#if !defined(LIBSECAM_SRAND)
#define LIBSECAM_SRAND(s)           srand(s)
#endif

#if !defined(LIBSECAM_RAND)
#define LIBSECAM_RAND()             rand()
#endif

//------------------------------------------------------------------------------

struct libsecam_s
{
    libsecam_options_t options;

    int width;
    int height;

    double *luma;
    double *chroma;
    double *stable_shift_buffer;
    double *vert;
    unsigned char *chroma_buffer;

    unsigned char *output;

    int frame_count;
};

//------------------------------------------------------------------------------

/**
 * Get random number in range [0.0, 1.0].
 */
static inline double libsecam_frand(void)
{
    return LIBSECAM_RAND() / (double) LIBSECAM_RAND_MAX;
}

/**
 * Get random integer number.
 */
static inline int libsecam_irand(int a, int b)
{
    return a + LIBSECAM_RAND() % (b - a);
}

/**
 * Returns true. Sometimes.
 */
static inline int libsecam_chance(double chance)
{
    return libsecam_frand() < (chance / 100.0);
}

/**
 * Self-explanatory.
 */
static inline int libsecam_clamp(int a, int b, int x)
{
    return x < a ? a : (x >= b ? b : x);
}

/**
 * Linear interpolation.
 */
static inline double libsecam_lerp(double a, double b, float x)
{
    return a + (b - a) * x;
}

/**
 * Bilinear interpolation.
 */
static inline double libsecam_bilerp(double a, double b, double c, double d, float x, float y)
{
    return libsecam_lerp(libsecam_lerp(a, b, x), libsecam_lerp(c, d, x), y);
}

/**
 * Interpolate line.
 */
static void libsecam_lerp_line(double *line, int length, int step)
{
    for (int i = 0; i < length; i++) {
        if ((i % step) == 0) {
            continue;
        }

        double const a = line[((i / step) * step) % length];
        double const b = line[((i / step) * step + step) % length];
        double const x = (i % step) / (double) step;

        line[i] = libsecam_lerp(a, b, x);
    }
}

/**
 * Converts three-byte RGB array to Y' (luma) value.
 */
static unsigned char libsecam_rgb_to_luma(unsigned char const *src)
{
    return libsecam_clamp(0, 255, 16.0
        + (65.7380 * src[0] / 256.0)
        + (129.057 * src[1] / 256.0)
        + (25.0640 * src[2] / 256.0));
}

/**
 * Converts three-byte RGB array to Cb (chroma blue) value.
 */
static unsigned char libsecam_rgb_to_cb(unsigned char const *src, int loss)
{
    unsigned int r = src[0];
    unsigned int g = src[1];
    unsigned int b = src[2];

    if (loss > 1) {
        for (int i = 1; i < loss; i++) {
            r += src[4 * i + 0];
            g += src[4 * i + 1];
            b += src[4 * i + 2];
        }

        r /= loss;
        g /= loss;
        b /= loss;
    }

    return libsecam_clamp(0, 255, 128.0
        - (37.9450 * r / 256.0)
        - (74.4940 * g / 256.0)
        + (112.439 * b / 256.0));
}

/**
 * Converts three-byte RGB array to Cr (chroma red) value.
 */
static unsigned char libsecam_rgb_to_cr(unsigned char const *src, int loss)
{
    unsigned int r = src[0];
    unsigned int g = src[1];
    unsigned int b = src[2];

    if (loss > 1) {
        for (int i = 1; i < loss; i++) {
            r += src[4 * i + 0];
            g += src[4 * i + 1];
            b += src[4 * i + 2];
        }

        r /= loss;
        g /= loss;
        b /= loss;
    }

    return libsecam_clamp(0, 255, 128.0
        + (112.439 * r / 256.0)
        - (94.1540 * g / 256.0)
        - (18.2850 * b / 256.0));
}

/**
 * Converts Y'CbCr values to 32-bit XRGB.
 */
static void libsecam_ycbcr_to_rgb(unsigned char *dst, unsigned char luma, unsigned char cb, unsigned char cr)
{
    dst[0] = libsecam_clamp(0, 255, 0.0
        + (298.082 * luma / 256.0)
        + (408.583 * cr / 256.0)
        - 222.921);

    dst[1] = libsecam_clamp(0, 255, 0.0
        + (298.082 * luma / 256.0)
        - (100.291 * cb / 256.0)
        - (208.120 * cr / 256.0)
        + 135.576);

    dst[2] = libsecam_clamp(0, 255, 0.0
        + (298.082 * luma / 256.0)
        + (516.412 * cb / 256.0)
        - 276.836);
    
    dst[3] = 255;
}

/**
 * Converts XRGB image to "internal representation", which is:
 *   per every line: array of Y' values [0.0 - 1.0],
 *   per even line: array of Cb values [0.0 - 1.0],
 *   per odd line: array of Cr values [0.0 - 1.0].
 */
static void libsecam_convert_frame(libsecam_t *self, unsigned char const *src)
{
    int shift = 0;

    for (int y = 0; y < self->height; y++) {
        unsigned char const *rgb = &src[y * self->width * 4];
        double *luma = &self->luma[y * self->width];
        double *chroma = &self->chroma[y * self->width];
        double brightness = 0.0;

        for (int x = 0; x < self->width; x++) {
            int xs = x - shift;

            luma[x] = libsecam_rgb_to_luma(&rgb[xs * 4]) / 255.0;

            if (y % 2 == 0) {
                chroma[x] = libsecam_rgb_to_cb(&rgb[xs * 4], 1) / 255.0;
            } else {
                chroma[x] = libsecam_rgb_to_cr(&rgb[xs * 4], 1) / 255.0;
            }

            brightness += luma[x];
        }

        brightness /= self->width;
    }
}

static double libsecam_value_with_loss(double const *line, int width, int x, int loss)
{
    if (loss <= 1) {
        return line[x];
    }

    // FIXME: this yields quite good result (but not yet accurate), though
    // it seems to be too slow

    double v = (1.0 / loss) * line[x];

    for (int i = (-loss + 1); i <= (loss - 1); i++) {
        if (i == 0) {
            continue;
        }

        int n = x + i;

        if (n < 0) {
            n = 0;
        } else if (n > (width - 1)) {
            n = width - 1;
        }

        v += (1.0 / (2 * loss)) * line[n];
    }

    return v;

/*
    double f = 1.0 / loss;
    double v = 0.0;

    for (int i = 0; i < loss; i++) {
        int n = x - i;
        v += f * (n >= 0 ? line[n] : line[0]);
    }

    return v;
*/
}

/**
 * Converts stored "internal representation" of an image back to
 * normal RGB image, which is stored in the 'buffer' array.
 */
static void libsecam_revert_frame(libsecam_t *self, unsigned char *out)
{
    // Simulate low luminance resolution: 240 TVL
    int luma_loss = (int) ceil(self->width / 240.0);

    // Simulate low chrominance resolution: 60 TVL
    int chroma_loss = (int) ceil(self->width / 60.0);

    double const *prev_cb = self->chroma;
    double const *prev_cr = self->chroma;

    memset(self->chroma_buffer, 128, self->width);

    for (int y = 0; y < self->height; y++) {
        unsigned char *rgb = &out[y * self->width * 4];
        double const *luma = &self->luma[y * self->width];
        double const *chroma = &self->chroma[y * self->width];

        if (y % 2 == 0) {
            prev_cb = chroma;
        } else {
            prev_cr = chroma;
        }

        for (int x = 0; x < self->width; x++) {
            double luma_val = libsecam_value_with_loss(luma, self->width, x, luma_loss);
            double chroma_val = libsecam_value_with_loss(chroma, self->width, x, chroma_loss);

            unsigned char c0 = libsecam_clamp(0, 255, luma_val * 255.0);
            unsigned char c1 = libsecam_clamp(0, 255, chroma_val * 255.0);
            unsigned char c2 = self->chroma_buffer[x];

            if (y % 2 == 0) {
                libsecam_ycbcr_to_rgb(&rgb[4 * x], c0, c1, c2);
            } else {
                libsecam_ycbcr_to_rgb(&rgb[4 * x], c0, c2, c1);
            }

            self->chroma_buffer[x] = c1;
        }
    }
}

/**
 * Applies echo (ghosting) effect on a scanline.
 */
static void libsecam_apply_echo(double *line, int width, int shift, int echo)
{
    if (echo == 0) {
        return;
    }

    int x0 = (shift < 0) ? 0 : shift;
    int x1 = x0 + echo;
    int x2 = x0 + (echo * 2);

    for (int x = x0; x < width; x++) {
        if (x < x1) {
            line[x] = ((x - x0) / (double) echo) * line[x1];
        } else if (x >= x2) {
            double u = line[x - echo];
            double v = line[x];
            line[x] = line[x] - u * 0.5 + v * 0.5;
        }
    }
}

/**
 * Applies noise on the scanline.
 */
static void libsecam_apply_noise(double *line, int width, double amplitude)
{
    for (int i = 0; i < width; i++) {
        line[i] += (libsecam_frand() - 0.5) * amplitude;
    }
}

/**
 * Shifts scanline.
 */
static void libsecam_apply_shift(double *line, int width, int shift, double fill)
{
    if (shift == 0) {
        return;
    }

    if (shift > 0) {
        for (int x = width - 1; x >= 0; x--) {
            if (x < shift) {
                line[x] = fill;
            } else {
                line[x] = line[x - shift];
            }
        }
    } else if (shift < 0) {
        int margin = width + shift;

        for (int x = 0; x < width; x++) {
            if (x < margin) {
                line[x] = line[x - shift];
            } else {
                line[x] = fill;
            }
        }
    }
}

/**
 * Simulates "fire" effect.
 */
static void libsecam_apply_fire(double *c0, double *c1, int width, double factor)
{
    double gain = 0;
    double fall = 0;
    double sign = 0;

    double const threshold = 0.48;

    for (int i = 0; i < width; i++) {
        double d = fabsf(c1[i] - c0[i]);

        if (d < 0.4) {
            d = 0.0;
        }

        double actual_factor = factor * (0.01 + (0.99 * d));

        double r = libsecam_frand();

        if (r < actual_factor) {
            // Start new fire.
            double force = 0.25 + (libsecam_frand() * 0.5);
            int length = (int) floor(force * (width / 10.0));

            gain = force;
            fall = force / length;
            sign = (c0[i] > 0.75) ? -1.0 : 1.0;

            c0[i] += gain / 2.0 * sign;
        } else if (gain > 0.0) {
            // Continue drawing previously started fire.
            gain -= fall;

            if (gain < 0.0) {
                gain = 0.0;
                fall = 0.0;
            }

            c0[i] += gain * sign;
        }
    }
}

//------------------------------------------------------------------------------

libsecam_t *libsecam_init(int width, int height)
{
    libsecam_t *self = LIBSECAM_MALLOC(sizeof(libsecam_t));

    if (!self) {
        return NULL;
    }

    memset(self, 0, sizeof(*self));

    self->width = width;
    self->height = height;

    self->luma = LIBSECAM_MALLOC(sizeof(*self->luma) * self->width * self->height);
    self->chroma = LIBSECAM_MALLOC(sizeof(*self->chroma) * self->width * self->height);

    if (!self->luma || !self->chroma) {
        libsecam_close(self);
        return NULL;
    }

    self->stable_shift_buffer = LIBSECAM_MALLOC(sizeof(*self->stable_shift_buffer) * height);
    self->vert = LIBSECAM_MALLOC(sizeof(double) * height);
    self->chroma_buffer = LIBSECAM_MALLOC(width);

    self->options.luma_noise = LIBSECAM_DEFAULT_LUMA_NOISE;
    self->options.chroma_noise = LIBSECAM_DEFAULT_CHROMA_NOISE;
    self->options.chroma_fire = LIBSECAM_DEFAULT_CHROMA_FIRE;
    self->options.echo = LIBSECAM_DEFAULT_ECHO;
    self->options.skew = LIBSECAM_DEFAULT_SKEW;
    self->options.wobble = LIBSECAM_DEFAULT_WOBBLE;

    self->output = NULL; // Will be initialized later if used.

    if (!self->stable_shift_buffer || !self->vert || !self->chroma_buffer) {
        libsecam_close(self);
        return NULL;
    }

    self->frame_count = 0;

    return self;
}

void libsecam_close(libsecam_t *self)
{
    LIBSECAM_FREE(self->chroma_buffer);
    LIBSECAM_FREE(self->stable_shift_buffer);
    LIBSECAM_FREE(self->vert);
    LIBSECAM_FREE(self->chroma);
    LIBSECAM_FREE(self->luma);
    LIBSECAM_FREE(self->output);
    LIBSECAM_FREE(self);
}

libsecam_options_t *libsecam_options(libsecam_t *self)
{
    return &self->options;
}

void libsecam_filter_to_buffer(libsecam_t *self, unsigned char const *src, unsigned char *dst)
{
    libsecam_convert_frame(self, src);

    if (self->options.wobble > 0) {
        for (int i = 0; i < self->height; i += 8) {
            self->vert[i] = libsecam_frand();
        }

        libsecam_lerp_line(self->vert, self->height, 8);
    }

    if (self->options.skew) {
        for (int y = 0; y < self->height; y += 8) {
            double *luma = &self->luma[y * self->width];
            double chunk_brightness = 0.0;

            for (int y1 = y; y1 < (y + 8); y1++) {
                double line_brightness = 0.0;

                for (int x = 0; x < self->width; x++) {
                    line_brightness += luma[x];
                }

                line_brightness /= (double) self->width;
                chunk_brightness += line_brightness;
            }

            chunk_brightness /= 8.0;
            self->stable_shift_buffer[y] = chunk_brightness;
        }

        libsecam_lerp_line(self->stable_shift_buffer, self->height, 8);
    }

    double *c1 = self->chroma;

    for (int y = 0; y < self->height; y++) {
        double *luma = &self->luma[y * self->width];
        double *c0 = &self->chroma[y * self->width];

        int shift = 0;

        // Skew (constant shift)
        if (self->options.skew > 0) {
            shift += self->stable_shift_buffer[y] * self->options.skew * 4;
        }

        // Unstable shift.
        if (self->options.wobble > 0) {
            shift += self->vert[y] * self->options.wobble;
        }

        if (shift > 0) {
            libsecam_apply_shift(luma, self->width, shift, 0.0);
            libsecam_apply_shift(c0, self->width, shift, 0.5);
        }

        // Luminance echo+noise.
        libsecam_apply_echo(luma, self->width, shift, self->options.echo);
        libsecam_apply_noise(luma, self->width, self->options.luma_noise);

        // Chroma noise+fire.
        libsecam_apply_fire(c0, c1, self->width, self->options.chroma_fire);
        libsecam_apply_noise(c0, self->width, self->options.chroma_noise);

        c1 = c0;

        luma[0] = luma[self->width - 1] = 0.0;
        c0[0] = c0[self->width - 1] = 0.5;
    }

    libsecam_revert_frame(self, dst);

    self->frame_count++;
}

unsigned char const *libsecam_filter(libsecam_t *self, unsigned char const *src)
{
    if (!self->output) {
        self->output = LIBSECAM_MALLOC(self->width * self->height * 4);

        if (!self->output) {
            return NULL;
        }
    }

    libsecam_filter_to_buffer(self, src, self->output);

    return self->output;
}

//------------------------------------------------------------------------------

#endif // LIBSECAM_IMPLEMENTATION

//------------------------------------------------------------------------------

#endif // TUORQAI_LIBSECAM_H


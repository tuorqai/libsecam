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

#define LIBSECAM_DEFAULT_LUMA_NOISE_FACTOR          0.07
#define LIBSECAM_DEFAULT_LUMA_FIRE_FACTOR           0.005   /* unused */
#define LIBSECAM_DEFAULT_LUMA_LOSS_CHANCE           0.02    /* unused */
#define LIBSECAM_DEFAULT_CHROMA_SHIFT_CHANCE        9.0     /* unused */
#define LIBSECAM_DEFAULT_CHROMA_NOISE_FACTOR        0.25
#define LIBSECAM_DEFAULT_CHROMA_FIRE_FACTOR         4.0
#define LIBSECAM_DEFAULT_CHROMA_LOSS_CHANCE         0.03    /* unused */
#define LIBSECAM_DEFAULT_ECHO_OFFSET                4
#define LIBSECAM_DEFAULT_STABLE_SHIFT               2
#define LIBSECAM_DEFAULT_HORIZONTAL_INSTABILITY     0

//------------------------------------------------------------------------------

typedef struct libsecam_s libsecam_t;

//------------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct libsecam_options
{
    double luma_noise_factor;       /* range: 0.0 to 1.0 */
    double luma_fire_factor;        /* unused */
    double luma_loss_chance;        /* unused */
    double chroma_shift_chance;     /* unused */
    double chroma_noise_factor;     /* range: 0.0 to 1.0 */
    double chroma_fire_factor;      /* range: 0.0 to 100.0 */
    double chroma_loss_chance;      /* unused */
    int echo_offset;                /* range: 0 to whatever */
    int stable_shift;               /* range: 0 to whatever */
    int horizontal_instability;     /* range: 0 to whatever */
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

    unsigned char *buffer;

    double *luma;
    double *chroma;
    double *stable_shift_buffer;
    double *vert;
    unsigned char *chroma_buffer;

    int luma_loss;
    int chroma_loss;
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
    int const luma_width = self->width / self->luma_loss;
    int const chroma_width = self->width / self->chroma_loss;

    for (int y = 0; y < self->height; y++) {
        unsigned char const *rgb = &src[y * self->width * 4];
        double *luma = &self->luma[y * luma_width];
        double *chroma = &self->chroma[y * chroma_width];

        for (int x_luma = 0; x_luma < luma_width; x_luma++) {
            luma[x_luma] = libsecam_rgb_to_luma(&rgb[4 * self->luma_loss * x_luma]) / 255.0;
        }

        for (int x_chroma = 0; x_chroma < chroma_width; x_chroma++) {
            if (y % 2 == 0) {
                chroma[x_chroma] = libsecam_rgb_to_cb(&rgb[4 * self->chroma_loss * x_chroma], self->chroma_loss) / 255.0;
            } else {
                chroma[x_chroma] = libsecam_rgb_to_cr(&rgb[4 * self->chroma_loss * x_chroma], self->chroma_loss) / 255.0;
            }
        }
    }
}

/**
 * Converts stored "internal representation" of an image back to
 * normal RGB image, which is stored in the 'buffer' array.
 */
static void libsecam_revert_frame(libsecam_t *self, unsigned char *out)
{
    int const luma_width = self->width / self->luma_loss;
    int const chroma_width = self->width / self->chroma_loss;

    double const *prev_cb = self->chroma;
    double const *prev_cr = self->chroma;

    for (int y = 0; y < self->height; y++) {
        unsigned char *rgb = &out[y * self->width * 4];
        double const *luma = &self->luma[y * luma_width];
        double const *chroma = &self->chroma[y * chroma_width];

        if (y % 2 == 0) {
            prev_cb = chroma;
        } else {
            prev_cr = chroma;
        }

        for (int x = 0; x < self->width; x++) {
            int x0_luma = libsecam_clamp(0, luma_width - 1, x / self->luma_loss);
            int x1_luma = libsecam_clamp(0, luma_width - 1, x0_luma + 1);
            double x_luma = (1.0 / self->luma_loss) + (x % self->luma_loss) / (double) self->luma_loss;

            double luma_value = 255.0 * libsecam_lerp(
                luma[x0_luma], luma[x1_luma],
                x_luma
            );

            unsigned char yuv_y = libsecam_clamp(0, 255, luma_value);

            int x0_chroma = libsecam_clamp(0, chroma_width - 1, x / self->chroma_loss);
            int x1_chroma = libsecam_clamp(0, chroma_width - 1, x0_chroma + 1);
            double x_chroma = (1.0 / self->chroma_loss) + (x % self->chroma_loss) / (double) self->chroma_loss;

            double chroma_value = 255.0 * libsecam_lerp(
                chroma[x0_chroma], chroma[x1_chroma],
                x_chroma
            );

            unsigned char yuv_cb, yuv_cr;

            if (y % 2 == 0) {
                yuv_cb = libsecam_clamp(0, 255, chroma_value);
                yuv_cr = self->chroma_buffer[x];
                self->chroma_buffer[x] = yuv_cb;
            } else {
                yuv_cb = self->chroma_buffer[x];
                yuv_cr = libsecam_clamp(0, 255, chroma_value);
                self->chroma_buffer[x] = yuv_cr;
            }

            libsecam_ycbcr_to_rgb(&rgb[4 * x], yuv_y, yuv_cb, yuv_cr);
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
 * Also simulates chroma noise using the same method as fires.
 */
static void libsecam_apply_fire(double *luma, int luma_width, double *chroma, double *prev_chroma, int chroma_width, double factor, double noise_factor)
{
    double gain = 0;
    double fall = 0;
    double sign = 0;
    double const force = 0.25 + (libsecam_frand() * 0.5);

    double noise_gain = 0;
    double noise_fall = 0;
    double noise_sign = 0;

    double const threshold = 0.48;
    int const ratio = luma_width / chroma_width;

    for (int i = 0; i < chroma_width; i++) {
        double min = luma[i * ratio + 0];
        double max = luma[i * ratio + 0];

        for (int j = 0; j < ratio; j++) {
            if (luma[i * ratio + j] < min) {
                min = luma[i * ratio + j];
            }

            if (luma[i * ratio + j] > max) {
                max = luma[i * ratio + j];
            }
        }

        double luma_delta = max - min;
        double chroma_delta = fabs(prev_chroma[i] - chroma[i]);

        if (chroma_delta < 0.4) {
            chroma_delta = 0.0;
        }

        double const random_factor = 0.01;
        double const luma_factor = 0.07;
        double const chroma_factor = 0.92;

        double actual_factor = factor * (
            + random_factor
            + luma_delta * luma_factor
            + chroma_delta * chroma_factor);

        double r = libsecam_frand() * 100.0;

        if (r < actual_factor) {
            // Start new fire.
            int const length = (int) floor(force * 32.0);

            gain = force;
            fall = force / length;
            sign = (chroma[i] > 0.75) ? -1.0 : 1.0;

            chroma[i] += gain / 2.0 * sign;
        } else if (gain > 0.0) {
            // Continue drawing previously started fire.
            gain -= fall;

            if (gain < 0.0) {
                gain = 0.0;
                fall = 0.0;
            }

            chroma[i] += gain * sign;
        }

        if (r < 18.0) {
            // Start noise fire.
            int const length = 4;
            
            noise_gain = noise_factor;
            noise_fall = noise_factor / length;
            noise_sign = ((int) floor(r) % 2) ? -1.0 : 1.0;

            chroma[i] += noise_gain / 2.0 * noise_sign;
        } else if (noise_gain > 0.0) {
            // Continue drawing previously started noise.
            noise_gain -= noise_fall;

            if (noise_gain < 0.0) {
                noise_gain = 0.0;
                noise_fall = 0.0;
            }

            chroma[i] += noise_gain * noise_sign;
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

    int const luma_loss = 1; // used to be 1 or 2 depending on horizontal resolution
    int const chroma_loss = 4; // used to be 4 or 8 depending on vertical resolution

    self->width = width;
    self->height = height;
    self->buffer = NULL; // Will be initialized later if used.

    self->luma = LIBSECAM_MALLOC(sizeof(double) * width * height / luma_loss);
    self->chroma = LIBSECAM_MALLOC(sizeof(double) * width * height / chroma_loss);
    self->stable_shift_buffer = LIBSECAM_MALLOC(sizeof(*self->stable_shift_buffer) * height);
    self->vert = LIBSECAM_MALLOC(sizeof(double) * height);
    self->chroma_buffer = LIBSECAM_MALLOC(width);

    self->options.luma_noise_factor = LIBSECAM_DEFAULT_LUMA_NOISE_FACTOR;
    self->options.chroma_noise_factor = LIBSECAM_DEFAULT_CHROMA_NOISE_FACTOR;
    self->options.chroma_fire_factor = LIBSECAM_DEFAULT_CHROMA_FIRE_FACTOR;
    self->options.echo_offset = LIBSECAM_DEFAULT_ECHO_OFFSET;
    self->options.stable_shift = LIBSECAM_DEFAULT_STABLE_SHIFT;
    self->options.horizontal_instability = LIBSECAM_DEFAULT_HORIZONTAL_INSTABILITY;

    self->luma_loss = luma_loss;
    self->chroma_loss = chroma_loss;

    if (!self->luma || !self->chroma || !self->stable_shift_buffer || !self->vert || !self->chroma_buffer) {
        libsecam_close(self);
        return NULL;
    }

    return self;
}

void libsecam_close(libsecam_t *self)
{
    LIBSECAM_FREE(self->chroma_buffer);
    LIBSECAM_FREE(self->stable_shift_buffer);
    LIBSECAM_FREE(self->vert);
    LIBSECAM_FREE(self->chroma);
    LIBSECAM_FREE(self->luma);
    LIBSECAM_FREE(self->buffer);
    LIBSECAM_FREE(self);
}

libsecam_options_t *libsecam_options(libsecam_t *self)
{
    return &self->options;
}

void libsecam_filter_to_buffer(libsecam_t *self, unsigned char const *src, unsigned char *dst)
{
    libsecam_convert_frame(self, src);

    if (self->options.horizontal_instability > 0) {
        for (int i = 0; i < self->height; i += 8) {
            self->vert[i] = libsecam_frand();
        }

        libsecam_lerp_line(self->vert, self->height, 8);
    }

    int const luma_width = self->width / self->luma_loss;

    if (self->options.stable_shift) {
        for (int y = 0; y < self->height; y += 8) {
            double *luma = &self->luma[y * luma_width];
            double chunk_brightness = 0.0;

            for (int y1 = y; y1 < (y + 8); y1++) {
                double line_brightness = 0.0;

                for (int x = 0; x < luma_width; x++) {
                    line_brightness += luma[x];
                }

                line_brightness /= (double) luma_width;
                chunk_brightness += line_brightness;
            }

            chunk_brightness /= 8.0;
            self->stable_shift_buffer[y] = chunk_brightness;
        }

        libsecam_lerp_line(self->stable_shift_buffer, self->height, 8);
    }

    int const chroma_width = self->width / self->chroma_loss;
    double *prev_chroma = self->chroma;

    memset(self->chroma_buffer, 128, self->width);

    for (int y = 0; y < self->height; y++) {
        double *luma = &self->luma[y * luma_width];
        double *chroma = &self->chroma[y * chroma_width];

        int shift = 0;

        // Stable shift.
        if (self->options.stable_shift > 0) {
            shift += self->stable_shift_buffer[y] * self->options.stable_shift * 4;
        }

        // Unstable shift.
        if (self->options.horizontal_instability > 0) {
            shift += self->vert[y] * self->options.horizontal_instability;
        }

        if (shift > 0) {
            libsecam_apply_shift(luma, luma_width, shift / self->luma_loss, 0.0);
            libsecam_apply_shift(chroma, chroma_width, shift / self->chroma_loss, 0.5);
        }

        // Luminance echo+noise.
        libsecam_apply_echo(luma, luma_width, shift / self->luma_loss, self->options.echo_offset);
        libsecam_apply_noise(luma, luma_width, self->options.luma_noise_factor);

        // [Hack] Despite having name "prev_chroma", this pointer
        // points to the next chroma line. Why? Because previous line
        // was already altered with fire effect and the next line
        // is still clean. _apply_fire() function needs two clean lines
        // to work correctly, otherwise the image will become too pink
        // (resonance-like effect?).
        prev_chroma = (y == self->height - 1) ? chroma : chroma + chroma_width;

        // Chroma noise+fire.
        libsecam_apply_fire(luma, luma_width,
            chroma, prev_chroma, chroma_width,
            self->options.chroma_fire_factor,
            self->options.chroma_noise_factor);
    }

    libsecam_revert_frame(self, dst);
}

unsigned char const *libsecam_filter(libsecam_t *self, unsigned char const *src)
{
    if (!self->buffer) {
        self->buffer = LIBSECAM_MALLOC(self->width * self->height * 4);

        if (!self->buffer) {
            return NULL;
        }
    }

    libsecam_filter_to_buffer(self, src, self->buffer);

    return self->buffer;
}

//------------------------------------------------------------------------------

#endif // LIBSECAM_IMPLEMENTATION

//------------------------------------------------------------------------------

#endif // TUORQAI_LIBSECAM_H


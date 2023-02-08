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
//      1.0         Initial release
//------------------------------------------------------------------------------

#ifndef TUORQAI_LIBSECAM_H
#define TUORQAI_LIBSECAM_H

//------------------------------------------------------------------------------

#include <stdbool.h>

//------------------------------------------------------------------------------

typedef struct libsecam_s libsecam_t;

//------------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif

libsecam_t *libsecam_init(int width, int height);
void libsecam_close(libsecam_t *self);
unsigned char const *libsecam_filter(libsecam_t *self, unsigned char const *src);

bool libsecam_get_line_variability(libsecam_t *self);
double libsecam_get_luma_shift_chance(libsecam_t *self);
double libsecam_get_luma_noise_factor(libsecam_t *self);
double libsecam_get_luma_fire_factor(libsecam_t *self);
double libsecam_get_luma_loss_chance(libsecam_t *self);
double libsecam_get_chroma_shift_chance(libsecam_t *self);
double libsecam_get_chroma_noise_factor(libsecam_t *self);
double libsecam_get_chroma_fire_factor(libsecam_t *self);
double libsecam_get_chroma_loss_chance(libsecam_t *self);
int libsecam_get_echo_offset(libsecam_t *self);

void libsecam_set_line_variability(libsecam_t *self, bool line_variability);
void libsecam_set_luma_shift_chance(libsecam_t *self, double luma_shift_chance);
void libsecam_set_luma_noise_factor(libsecam_t *self, double luma_noise_factor);
void libsecam_set_luma_fire_factor(libsecam_t *self, double luma_fire_factor);
void libsecam_set_luma_loss_chance(libsecam_t *self, double luma_loss_chance);
void libsecam_set_chroma_shift_chance(libsecam_t *self, double chroma_shift_chance);
void libsecam_set_chroma_noise_factor(libsecam_t *self, double chroma_noise_factor);
void libsecam_set_chroma_fire_factor(libsecam_t *self, double chroma_fire_factor);
void libsecam_set_chroma_loss_chance(libsecam_t *self, double chroma_loss_chance);
void libsecam_set_echo_offset(libsecam_t *self, int echo_offset);

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

// old default values:
// .06, .28, .001, .0002
// .09, .40, .007, .0003

#define DEFAULT_LUMA_SHIFT_CHANCE       0.03
#define DEFAULT_LUMA_NOISE_FACTOR       0.07
#define DEFAULT_LUMA_FIRE_FACTOR        0.0001
#define DEFAULT_LUMA_LOSS_CHANCE        0.0002
#define DEFAULT_CHROMA_SHIFT_CHANCE     0.09
#define DEFAULT_CHROMA_NOISE_FACTOR     0.25
#define DEFAULT_CHROMA_FIRE_FACTOR      0.001
#define DEFAULT_CHROMA_LOSS_CHANCE      0.0003

//------------------------------------------------------------------------------

struct libsecam_s
{
    int width;
    int height;

    unsigned char *buffer;

    double *luma;
    double *chroma;
    double *vert;

    unsigned char *chroma_buffer;
    double *echo_buffer;

    bool line_variability;
    double luma_shift_chance;
    double luma_noise_factor;
    double luma_fire_factor;
    double luma_loss_chance;
    double chroma_shift_chance;
    double chroma_noise_factor;
    double chroma_fire_factor;
    double chroma_loss_chance;
    int echo_offset;

    int luma_loss;
    int chroma_loss;
};

//------------------------------------------------------------------------------

/**
 * Self-explanatory.
 */
static int libsecam_clamp(int a, int b, int x)
{
    return x < a ? a : (x >= b ? b : x);
}

/**
 * Linear interpolation.
 */
static double libsecam_lerp(double a, double b, float x)
{
    return a + (b - a) * x;
}

/**
 * Bilinear interpolation.
 */
static double libsecam_bilerp(double a, double b, double c, double d, float x, float y)
{
    return libsecam_lerp(libsecam_lerp(a, b, x), libsecam_lerp(c, d, x), y);
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
static unsigned char libsecam_rgb_to_cb(unsigned char const *src)
{
    return libsecam_clamp(0, 255, 128.0
        - (37.9450 * src[0] / 256.0)
        - (74.4940 * src[1] / 256.0)
        + (112.439 * src[2] / 256.0));
}

/**
 * Converts three-byte RGB array to Cr (chroma red) value.
 */
static unsigned char libsecam_rgb_to_cr(unsigned char const *src)
{
    return libsecam_clamp(0, 255, 128.0
        + (112.439 * src[0] / 256.0)
        - (94.1540 * src[1] / 256.0)
        - (18.2850 * src[2] / 256.0));
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
                chroma[x_chroma] = libsecam_rgb_to_cb(&rgb[4 * self->chroma_loss * x_chroma]) / 255.0;
            } else {
                chroma[x_chroma] = libsecam_rgb_to_cr(&rgb[4 * self->chroma_loss * x_chroma]) / 255.0;
            }
        }
    }
}

/**
 * Converts stored "internal representation" of an image back to
 * normal RGB image, which is stored in the 'buffer' array.
 */
static void *libsecam_revert_frame(libsecam_t *self)
{
    int const luma_width = self->width / self->luma_loss;
    int const chroma_width = self->width / self->chroma_loss;

    double const *prev_luma = self->luma;
    double const *prev_cb = self->chroma;
    double const *prev_cr = self->chroma;

    for (int y = 0; y < self->height; y++) {
        unsigned char *rgb = &self->buffer[y * self->width * 4];
        double const *luma = &self->luma[y * luma_width];
        double const *chroma = &self->chroma[y * chroma_width];
        double const *prev_chroma;

        if (y % 2 == 0) {
            prev_chroma = prev_cb;
            prev_cb = chroma;
        } else {
            prev_chroma = prev_cr;
            prev_cr = chroma;
        }

        for (int x = 0; x < self->width; x++) {
            int x0_luma = libsecam_clamp(0, luma_width - 1, x / self->luma_loss);
            int x1_luma = libsecam_clamp(0, luma_width - 1, x0_luma + 1);
            double x_luma = (1.0 / self->luma_loss) + (x % self->luma_loss) / (double) self->luma_loss;

            double luma_value = 255.0 * libsecam_bilerp(
                prev_luma[x0_luma], prev_luma[x1_luma],
                luma[x0_luma], luma[x1_luma],
                x_luma, 0.5
            );

            unsigned char yuv_y = libsecam_clamp(0, 255, luma_value);

            int x0_chroma = libsecam_clamp(0, chroma_width - 1, x / self->chroma_loss);
            int x1_chroma = libsecam_clamp(0, chroma_width - 1, x0_chroma + 1);
            double x_chroma = (1.0 / self->chroma_loss) + (x % self->chroma_loss) / (double) self->chroma_loss;
            double y_chroma = 0.5 + ((y % 2) / 2.0);

            double chroma_value = 255.0 * libsecam_bilerp(
                prev_chroma[x0_chroma], prev_chroma[x1_chroma],
                chroma[x0_chroma], chroma[x1_chroma],
                x_chroma, y_chroma
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

        prev_luma = luma;
    }
    
    return self->buffer;
}

/**
 * Applies echo (ghosting) effect on a scanline.
 * FIXME: Makes the picture oversaturated.
 */
static void libsecam_apply_echo(double *line, double *buffer, int width, int offset, double intensity)
{
    if (offset == 0) {
        return;
    }

    for (int x = offset; x < width; x++) {
        line[x] += (1.0 - buffer[x - offset]) * intensity;
    }
}

/**
 * Applies noise on the scanline.
 */
static void libsecam_apply_noise(double *line, int width, double amplitude)
{
    for (int i = 0; i < width; i++) {
        line[i] += ((rand() / (double) RAND_MAX) - 0.5) * amplitude;
    }
}

/**
 * Shifts scanline.
 */
static void libsecam_apply_shift(double *line, int width, int range, double fill)
{
    if (range == 0) {
        return;
    }

    for (int x = width - 1; x >= 0; x--) {
        if ((x - range) >= 0) {
            line[x] = line[x - range];
        } else {
            line[x] = fill;
        }
    }
}

/**
 * Simulates "fire" effect.
 */
static void libsecam_apply_fire(double *line, size_t length, double chance)
{
    double gain = 0;
    double fall = 0;
    double sign = 0;
    int force = 35;

    for (int i = 0; i < length; i++) {
        double r = rand() / (double) RAND_MAX;

        if (r < chance) {
            gain = force / 100.0;
            fall = 1.0 / ((force / 2.0) + (rand() % (force / 2)));
            sign = (rand() % 2 == 0) ? 1 : -1;

            if (i > 0) {
                line[i - 1] += (gain / 2.0) * sign;
            }
        }

        if (gain > 0.0) {
            gain -= fall;

            if (gain < 0.0) {
                gain = 0.0;
                fall = 0.0;
            }
        }

        line[i] += gain * sign;
    }
}

//------------------------------------------------------------------------------

libsecam_t *libsecam_init(int width, int height)
{
    libsecam_t *self = malloc(sizeof(libsecam_t));

    if (!self) {
        return NULL;
    }

    int const luma_loss = 1; // used to be 1 or 2 depending on horizontal resolution
    int const chroma_loss = (height < 480) ? 4 : 8;

    *self = (libsecam_t) {
        .width = width,
        .height = height,
        .buffer = malloc(width * height * 4),
        .luma = malloc(sizeof(double) * width * height / luma_loss),
        .chroma = malloc(sizeof(double) * width * height / chroma_loss),
        .vert = malloc(sizeof(double) * height),
        .chroma_buffer = malloc(width),
        .echo_buffer = malloc(sizeof(double) * width / luma_loss),
        .line_variability = true,
        .luma_shift_chance = DEFAULT_LUMA_SHIFT_CHANCE,
        .luma_noise_factor = DEFAULT_LUMA_NOISE_FACTOR,
        .luma_fire_factor = DEFAULT_LUMA_FIRE_FACTOR,
        .luma_loss_chance = DEFAULT_LUMA_LOSS_CHANCE,
        .chroma_shift_chance = DEFAULT_CHROMA_SHIFT_CHANCE,
        .chroma_noise_factor = DEFAULT_CHROMA_NOISE_FACTOR,
        .chroma_fire_factor = DEFAULT_CHROMA_FIRE_FACTOR,
        .chroma_loss_chance = DEFAULT_CHROMA_LOSS_CHANCE,
        .echo_offset = 0,
        .luma_loss = luma_loss,
        .chroma_loss = chroma_loss,
    };

    if (!self->buffer || !self->luma || !self->chroma || !self->vert || !self->chroma_buffer || !self->echo_buffer) {
        libsecam_close(self);
        return NULL;
    }

    return self;
}

void libsecam_close(libsecam_t *self)
{
    free(self->echo_buffer);
    free(self->chroma_buffer);
    free(self->vert);
    free(self->chroma);
    free(self->luma);
    free(self->buffer);
    free(self);
}

unsigned char const *libsecam_filter(libsecam_t *self, unsigned char const *src)
{
    libsecam_convert_frame(self, src);

    for (int i = 0; i < self->height / 8; i++) {
        self->vert[i] = self->line_variability ? (rand() / (double) RAND_MAX) : 1.0;
    }

    int const luma_width = self->width / self->luma_loss;
    int const chroma_width = self->width / self->chroma_loss;

    memset(self->chroma_buffer, 128, self->width);

    for (int y = 0; y < self->height; y++) {
        double v0 = self->vert[(y / 8) % (self->height / 8)];
        double v1 = self->vert[((y + 1) / 8) % (self->height / 8)];
        double v = libsecam_lerp(v0, v1, (y % 8) / 8.0);

        double *luma = &self->luma[y * luma_width];
        double *chroma = &self->chroma[y * chroma_width];

        memcpy(self->echo_buffer, luma, sizeof(double) * luma_width);

        if ((rand() / (double) RAND_MAX) < self->luma_loss_chance) {
            libsecam_apply_noise(luma, luma_width, 0.5);
        } else {
            if ((rand() / (double) RAND_MAX) < self->luma_shift_chance) {
                int range = 1 + rand() % 2;
                libsecam_apply_shift(luma, luma_width, range, 0.0);
            }

            libsecam_apply_echo(luma, self->echo_buffer, luma_width, self->echo_offset, 0.0625);
            libsecam_apply_noise(luma, luma_width, self->luma_noise_factor * v);
            libsecam_apply_fire(luma, luma_width, self->luma_fire_factor * v);
        }

        for (int x = 0; x < luma_width; x++) {
            luma[x] = 0.0625 + (luma[x] * 0.9);
        }

        if ((rand() / (double) RAND_MAX) < self->chroma_loss_chance) {
            libsecam_apply_noise(chroma, chroma_width, 0.5);
        } else {
            if ((rand() / (double) RAND_MAX) < self->chroma_shift_chance) {
                int range = 1 + (rand() % (self->chroma_loss / 4));
                libsecam_apply_shift(chroma, chroma_width, range, 0.5);
            }

            libsecam_apply_noise(chroma, chroma_width, self->chroma_noise_factor * v);
            libsecam_apply_fire(chroma, chroma_width, self->chroma_fire_factor * v);
        }
    }

    return libsecam_revert_frame(self);
}

bool libsecam_get_line_variability(libsecam_t *self)
{
    return self->line_variability;
}

double libsecam_get_luma_shift_chance(libsecam_t *self)
{
    return self->luma_shift_chance;
}

double libsecam_get_luma_noise_factor(libsecam_t *self)
{
    return self->luma_noise_factor;
}

double libsecam_get_luma_fire_factor(libsecam_t *self)
{
    return self->luma_fire_factor;
}

double libsecam_get_luma_loss_chance(libsecam_t *self)
{
    return self->luma_loss_chance;
}

double libsecam_get_chroma_shift_chance(libsecam_t *self)
{
    return self->chroma_shift_chance;
}

double libsecam_get_chroma_noise_factor(libsecam_t *self)
{
    return self->chroma_noise_factor;
}

double libsecam_get_chroma_fire_factor(libsecam_t *self)
{
    return self->chroma_fire_factor;
}

double libsecam_get_chroma_loss_chance(libsecam_t *self)
{
    return self->chroma_loss_chance;
}

int libsecam_get_echo_offset(libsecam_t *self)
{
    return self->echo_offset;
}

void libsecam_set_line_variability(libsecam_t *self, bool line_variability)
{
    self->line_variability = line_variability;
}

void libsecam_set_luma_shift_chance(libsecam_t *self, double luma_shift_chance)
{
    self->luma_shift_chance = (luma_shift_chance < 0.0) ? DEFAULT_LUMA_SHIFT_CHANCE : luma_shift_chance;
}

void libsecam_set_luma_noise_factor(libsecam_t *self, double luma_noise_factor)
{
    self->luma_noise_factor = (luma_noise_factor < 0.0) ? DEFAULT_LUMA_NOISE_FACTOR : luma_noise_factor;
}

void libsecam_set_luma_fire_factor(libsecam_t *self, double luma_fire_factor)
{
    self->luma_fire_factor = (luma_fire_factor < 0.0) ? DEFAULT_LUMA_FIRE_FACTOR : luma_fire_factor;
}

void libsecam_set_luma_loss_chance(libsecam_t *self, double luma_loss_chance)
{
    self->luma_loss_chance = (luma_loss_chance < 0.0) ? DEFAULT_LUMA_LOSS_CHANCE : luma_loss_chance;
}

void libsecam_set_chroma_shift_chance(libsecam_t *self, double chroma_shift_chance)
{
    self->chroma_shift_chance = (chroma_shift_chance < 0.0) ? DEFAULT_CHROMA_SHIFT_CHANCE : chroma_shift_chance;
}

void libsecam_set_chroma_noise_factor(libsecam_t *self, double chroma_noise_factor)
{
    self->chroma_noise_factor = (chroma_noise_factor < 0.0) ? DEFAULT_CHROMA_NOISE_FACTOR : chroma_noise_factor;
}

void libsecam_set_chroma_fire_factor(libsecam_t *self, double chroma_fire_factor)
{
    self->chroma_fire_factor = (chroma_fire_factor < 0.0) ? DEFAULT_CHROMA_FIRE_FACTOR : chroma_fire_factor;
}

void libsecam_set_chroma_loss_chance(libsecam_t *self, double chroma_loss_chance)
{
    self->chroma_loss_chance = (chroma_loss_chance < 0.0) ? DEFAULT_CHROMA_LOSS_CHANCE : chroma_loss_chance;
}

void libsecam_set_echo_offset(libsecam_t *self, int echo_offset)
{
    self->echo_offset = echo_offset;
}

//------------------------------------------------------------------------------

#endif // LIBSECAM_IMPLEMENTATION

//------------------------------------------------------------------------------

#endif // TUORQAI_LIBSECAM_H

//------------------------------------------------------------------------------

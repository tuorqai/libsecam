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
//------------------------------------------------------------------------------

#ifndef TUORQAI_LIBSECAM_H
#define TUORQAI_LIBSECAM_H

//------------------------------------------------------------------------------

#include <stdbool.h>

#ifdef LIBSECAM_USE_THREADS
#   ifdef _WIN32
#       define WIN32_LEAN_AND_MEAN
#       include <windows.h>
#   else
#       include <pthread.h>
#   endif
#endif

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

struct libsecam_params
{
    double noise;
};

void libsecam_perform(void *dst, void const *src, int width, int height,
    struct libsecam_params params);

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
// Legacy stuff (to be removed soon)

struct libsecam_s
{
    int unused;
};

libsecam_t *libsecam_init(int width, int height)
{
    return NULL;
}

void libsecam_close(libsecam_t *self)
{
}

libsecam_options_t *libsecam_options(libsecam_t *self)
{
    return NULL;
}

void libsecam_filter_to_buffer(libsecam_t *self, unsigned char const *src, unsigned char *dst)
{
}

unsigned char const *libsecam_filter(libsecam_t *self, unsigned char const *src)
{
    return NULL;
}

//------------------------------------------------------------------------------
// Utility functions

static int libsecam__clamp(int x, int a, int b)
{
    return x < a ? a : (x >= b ? b : x);
}

//------------------------------------------------------------------------------
// YCbCr color space

static unsigned char libsecam__rgb_to_ycbcr_y(float r, float g, float b)
{
    return 16 + (65.7380 * r) + (129.057 * g) + (25.0640 * b);
}

static unsigned char libsecam__rgb_to_ycbcr_u(float r, float g, float b)
{
    return 128 - (37.9450 * r) - (74.4940 * g) + (112.439 * b);
}

static unsigned char libsecam__rgb_to_ycbcr_v(float r, float g, float b)
{
    return 128 + (112.439 * r) - (94.1540 * g) - (18.2850 * b);
}

static void libsecam__ycbcr_to_rgb(unsigned char *rgb, float y, float u, float v)
{
    rgb[0] = libsecam__clamp((298.082 * y) + (408.583 * v) - 222.921, 0, 255);
    rgb[1] = libsecam__clamp((298.082 * y) - (100.291 * u) - (208.120 * v) + 135.576, 0, 255);
    rgb[2] = libsecam__clamp((298.082 * y) + (516.412 * u) - 276.836, 0, 255);
}

//------------------------------------------------------------------------------
// YDbDr color space

static unsigned char libsecam__rgb_to_ydbdr_y(float r, float g, float b)
{
    return (76.25 * r) + (149.68 * g) + (29.07 * b);
}

static unsigned char libsecam__rgb_to_ydbdr_u(float r, float g, float b)
{
    return 128 - (43.00 * r) - (86.500 * g) + (127.5 * b);
}

static unsigned char libsecam__rgb_to_ydbdr_v(float r, float g, float b)
{
    return 128 - (127.5 * r) + (106.70 * g) + (20.80 * b);
}

static void libsecam__ydbdr_to_rgb(unsigned char *rgb, float y, float u, float v)
{
    rgb[0] = libsecam__clamp(255.0 * y + 0.06300 * u - 357.531 * v + 178.734, 0, 255);
    rgb[1] = libsecam__clamp(255.0 * y - 87.7880 * u + 182.126 * v - 47.1690, 0, 255);
    rgb[2] = libsecam__clamp(255.0 * y + 451.869 * u - 0.05400 * v - 225.908, 0, 255);
}

//------------------------------------------------------------------------------

static void libsecam__copy_pair_as_yuv(
    unsigned char *dst_even, unsigned char *dst_odd,
    unsigned char const *src_even, unsigned char const *src_odd,
    int width)
{
    int i;

    for (i = 0; i < width; i += 2) {
        float r0_even = src_even[4 * (i + 0) + 0] / 255.0;
        float g0_even = src_even[4 * (i + 0) + 1] / 255.0;
        float b0_even = src_even[4 * (i + 0) + 2] / 255.0;

        float r1_even = src_even[4 * (i + 1) + 0] / 255.0;
        float g1_even = src_even[4 * (i + 1) + 1] / 255.0;
        float b1_even = src_even[4 * (i + 1) + 2] / 255.0;

        float r0_odd = src_odd[4 * (i + 0) + 0] / 255.0;
        float g0_odd = src_odd[4 * (i + 0) + 1] / 255.0;
        float b0_odd = src_odd[4 * (i + 0) + 2] / 255.0;

        float r1_odd = src_odd[4 * (i + 1) + 0] / 255.0;
        float g1_odd = src_odd[4 * (i + 1) + 1] / 255.0;
        float b1_odd = src_odd[4 * (i + 1) + 2] / 255.0;

        float r_even = (r0_even + r1_even) / 2.f;
        float g_even = (g0_even + g1_even) / 2.f;
        float b_even = (b0_even + b1_even) / 2.f;

        float r_odd = (r0_odd + r1_odd) / 2.f;
        float g_odd = (g0_odd + g1_odd) / 2.f;
        float b_odd = (b0_odd + b1_odd) / 2.f;

        unsigned char y0_even = libsecam__rgb_to_ydbdr_y(r0_even, g0_even, b0_even);
        unsigned char y1_even = libsecam__rgb_to_ydbdr_y(r1_even, g1_even, b1_even);

        unsigned char y0_odd = libsecam__rgb_to_ydbdr_y(r0_odd, g0_odd, b0_odd);
        unsigned char y1_odd = libsecam__rgb_to_ydbdr_y(r1_odd, g1_odd, b1_odd);

        unsigned char u = libsecam__rgb_to_ydbdr_u(r_odd, g_odd, b_odd);
        unsigned char v = libsecam__rgb_to_ydbdr_v(r_even, g_even, b_even);

        dst_even[4 * (i + 0) + 0] = y0_even;
        dst_even[4 * (i + 0) + 1] = v;
        dst_even[4 * (i + 0) + 3] = src_even[4 * (i + 0) + 3];

        dst_even[4 * (i + 1) + 0] = y1_even;
        dst_even[4 * (i + 1) + 1] = v;
        dst_even[4 * (i + 1) + 3] = src_even[4 * (i + 1) + 3];

        dst_odd[4 * (i + 0) + 0] = y0_odd;
        dst_odd[4 * (i + 0) + 1] = u;
        dst_odd[4 * (i + 0) + 3] = src_odd[4 * (i + 0) + 3];

        dst_odd[4 * (i + 1) + 0] = y1_odd;
        dst_odd[4 * (i + 1) + 1] = u;
        dst_odd[4 * (i + 1) + 3] = src_odd[4 * (i + 1) + 3];
    }
}

static int libsecam__is_blue_area(int u, int v)
{
    return (u > 28 && v < -28) ? (u - v) : 0;
}

static int libsecam__is_red_area(int u, int v)
{
    return (v > 56 && u < -14) ? (v - u) : 0;
}

static double frand(void)
{
    double r = -1.0 + (rand() / (double) RAND_MAX) * 2.0;

    return r * r * r * r * r;
}

static unsigned long libsecam__juice(int j)
{
    j ^= j << 13;
    j ^= j >> 17;
    j ^= j << 5;

    return j;
}

static void libsecam__filter_pair(unsigned char *even, unsigned char *odd,
    int width, struct libsecam_params const *params)
{
    int i;

    int blue_fire = 0;
    int blue_attenuation = 0;

    int red_fire = 0;
    int red_attenuation = 0;

    int noise = params->noise * 448;
    int color_noise = params->noise * 512;
    int rare_event = (params->noise > 0) ? (8192 / params->noise) : 0;

    int echo = 4;

    unsigned long r_even = rand();
    unsigned long r_odd = rand();

    int u_prev = odd[4 * i + 1] - 128;
    int v_prev = even[4 * i + 1] - 128;

    for (i = 0; i < (width - 1); i++) {
        int y_even = even[4 * i + 0];
        int y_odd = odd[4 * i + 0];
        int u = odd[4 * i + 1] - 128;
        int v = even[4 * i + 1] - 128;
        int u0, v0;

        r_even = libsecam__juice(r_even);
        r_odd = libsecam__juice(r_odd);

        // Oversaturation: increase color intensity by 25%.
        u += u / 4;
        v += v / 4;

        // Noise.
        if (noise) {
            int n_even = (r_even % noise) - (noise / 2);
            int n_odd = (r_odd % noise) - (noise / 2);
            int n_u = (r_even % color_noise) - (color_noise / 2);
            int n_v = (r_odd % color_noise) - (color_noise / 2);

            y_even += n_even;
            y_odd += n_odd;
            u += n_u;
            v += n_v;
        }

        // Echo.
        if (echo && i >= echo) {
            y_even += (y_even - even[4 * (i - echo)]) / 2;
            y_odd += (y_odd - odd[4 * (i - echo)]) / 2;
        }

        u0 = u;
        v0 = v;
#if 0
        if (blue_fire > 0) {
            u += blue_fire;
        } else {
            if ((u - u_prev) > 108) {
                blue_fire = 256 + (r_odd % 64);
                blue_attenuation = 4 + (r_odd % 8);
            }
        }
#endif
        if (red_fire > 0) {
            v -= red_fire;
        } else {
            if (abs(v - v_prev) * (y_even / 256.0) > 108) {
                red_fire = 256 + (r_even % 64);
                red_attenuation = 4 + (r_even % 8);
            }
        }

        blue_fire -= blue_attenuation;
        red_fire -= red_attenuation;

        even[4 * i + 0] = libsecam__clamp(y_even, 16, 235);
        even[4 * i + 1] = libsecam__clamp(v + 128, 16, 240);

        odd[4 * i + 0] = libsecam__clamp(y_odd, 16, 235);
        odd[4 * i + 1] = libsecam__clamp(u + 128, 16, 240);

        u_prev = u0;
        v_prev = v0;

        if (i % 4 == 0) {

        }
    }
}

static void libsecam__convert_pair_to_rgb(unsigned char *even,
    unsigned char *odd, int width)
{
    int i, j;

    int luma_loss = 4;
    int chroma_loss = 8;

    even[4 * (width - 1) + 0] = 0;
    even[4 * (width - 1) + 1] = 128;
    odd[4 * (width - 1) + 0] = 0;
    odd[4 * (width - 1) + 1] = 128;

    for (i = 0; i < width; i++) {
        float y_even = 0.f;
        float y_odd = 0.f;
        float u = 0.f;
        float v = 0.f;

        for (j = 0; j < luma_loss; j++) {
            int idx = libsecam__clamp(i + j, 0, width - 1);
            y_even += even[4 * idx + 0];
            y_odd += odd[4 * idx + 0];
        }

        for (j = 0; j < chroma_loss; j++) {
            int idx = libsecam__clamp(i + j, 0, width - 1);
            u += odd[4 * idx + 1];
            v += even[4 * idx + 1];
        }

        y_even /= 255.f * luma_loss;
        y_odd /= 255.f * luma_loss;
        u /= 255.f * chroma_loss;
        v /= 255.f * chroma_loss;

        libsecam__ydbdr_to_rgb(&even[4 * i], y_even, u, v);
        libsecam__ydbdr_to_rgb(&odd[4 * i], y_odd, u, v);
    }
}

void libsecam_perform(void *dst, void const *src, int width, int height,
    struct libsecam_params params)
{
    unsigned char *out = dst;
    unsigned char const *in = src;

    for (int y = 0; y < height; y += 2) {
        int even = 4 * width * (y + 0);
        int odd = 4 * width * (y + 1);

        libsecam__copy_pair_as_yuv(&out[even], &out[odd], &in[even], &in[odd], width);
        libsecam__filter_pair(&out[even], &out[odd], width, &params);
        libsecam__convert_pair_to_rgb(&out[even], &out[odd], width);
    }
}

//------------------------------------------------------------------------------

#endif // LIBSECAM_IMPLEMENTATION

//------------------------------------------------------------------------------

#endif // TUORQAI_LIBSECAM_H

//------------------------------------------------------------------------------
//
// Version history:
//      4.0     2024.03.30  Almost completely rewritten, added multithreading
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

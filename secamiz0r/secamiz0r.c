
// #define ENABLE_TIME_TEST
#define LIBSECAM_IMPLEMENTATION

#include "frei0r.h"
#include "libsecam.h"

struct secamiz0r
{
    libsecam_t *libsecam;
    unsigned int width;
    unsigned int height;
    double intensity;
};

static void update_libsecam_options(libsecam_t *libsecam, double intensity)
{
    libsecam_options_t *options = libsecam_options(libsecam);

    // Deprecated options
    options->luma_fire_factor = 0.0;
    options->luma_loss_chance = 0.0;
    options->chroma_shift_chance = 0.0;
    options->chroma_loss_chance = 0.0;

    if (intensity < 0.25) {
        double const x = intensity / 0.25;

        options->luma_noise_factor = 0.00;
        options->chroma_noise_factor = 0.125;
        options->chroma_fire_factor = 1.0 * x; /* 0.0 to 1.0 */
        options->echo_offset = 2;
        options->horizontal_instability = (x < 0.25) ? 0 : 2; /* 0 or 2 */
    } else {
        double const x = (intensity - 0.25) / 0.75;
        double const xs = x * x;

        options->luma_noise_factor = 0.5 * xs; /* 0.07 to 0.5, non-linear */
        options->chroma_noise_factor = 0.125;
        options->chroma_fire_factor = 1.0 + (19.0 * x); /* 1.0 to 20.0 */
        options->echo_offset = (int) ceilf(2.0 + (6.0 * xs)); /* 2 to 8, non-linear */
        options->horizontal_instability = (int) ceilf(2.0 + (6.0 * xs)); /* 2 to 8, non-linear */
    }
}

int f0r_init(void)
{
    return 1;
}

void f0r_deinit(void)
{
}

void f0r_get_plugin_info(f0r_plugin_info_t *info)
{
    info->name = "secamiz0r";
    info->author = "tuorqai";
    info->plugin_type = F0R_PLUGIN_TYPE_FILTER;
    info->color_model = F0R_COLOR_MODEL_RGBA8888;
    info->frei0r_version = FREI0R_MAJOR_VERSION;
    info->major_version = 1;
    info->minor_version = 0;
    info->num_params = 1;
    info->explanation = "SECAM Fire effect";
}

void f0r_get_param_info(f0r_param_info_t *info, int index)
{
    switch (index) {
    case 0:
        info->name = "Intensity";
        info->type = F0R_PARAM_DOUBLE;
        info->explanation = NULL;
        break;
    default:
        break;
    }
}

f0r_instance_t f0r_construct(unsigned int width, unsigned int height)
{
    struct secamiz0r *instance = calloc(1, sizeof(*instance));

    if (!instance) {
        return 0;
    }

    instance->libsecam = libsecam_init(width, height);

    if (!instance->libsecam) {
        free(instance);
        return 0;
    }

    instance->width = width;
    instance->height = height;
    instance->intensity = 0.25;

    update_libsecam_options(instance->libsecam, instance->intensity);

    return instance;
}

void f0r_destruct(f0r_instance_t instance)
{
    struct secamiz0r *secamiz0r = instance;

    libsecam_close(secamiz0r->libsecam);
    free(secamiz0r);
}

void f0r_set_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *secamiz0r = instance;

    switch (index) {
    case 0:
        secamiz0r->intensity = *((double *) param);
        break;
    }

    update_libsecam_options(secamiz0r->libsecam, secamiz0r->intensity);
}

void f0r_get_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *secamiz0r = instance;

    switch (index) {
    case 0:
        *((double *) param) = secamiz0r->intensity;
        break;
    }
}

void f0r_update(f0r_instance_t instance, double time, uint32_t const *input, uint32_t *output)
{
    struct secamiz0r *secamiz0r = instance;

#ifdef ENABLE_TIME_TEST
    secamiz0r->intensity = fmod(time, 10000.0) / 10000.0;
    update_libsecam_options(secamiz0r->libsecam, secamiz0r->intensity);
#endif

    libsecam_filter_to_buffer(secamiz0r->libsecam,
        (unsigned char const *) input,
        (unsigned char *) output);

#ifdef ENABLE_TIME_TEST
    int w = floor(secamiz0r->intensity * secamiz0r->width);
    for (int x = 0; x < w; x++) {
        for (int y = (secamiz0r->height - 8); y < secamiz0r->height; y++) {
            output[y * secamiz0r->width + x] = 0xffff00ff;
        }
    }
#endif

}



// #define ENABLE_TIME_TEST
#define LIBSECAM_IMPLEMENTATION

#include "frei0r.h"
#include "libsecam.h"

struct secamiz0r
{
    unsigned int width;
    unsigned int height;
    double intensity;
};

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

    instance->width = width;
    instance->height = height;
    instance->intensity = 0.25;

    return instance;
}

void f0r_destruct(f0r_instance_t instance)
{
    free(instance);
}

void f0r_set_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *secamiz0r = instance;

    switch (index) {
    case 0:
        secamiz0r->intensity = *((double *) param);
        break;
    }
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

    struct libsecam_params params = {
        .noise = secamiz0r->intensity,
    };

#ifdef ENABLE_TIME_TEST
    double d = fmod(time, 10000.0) / 10000.0;
    params.noise = d;
    // secamiz0r->intensity = fmod(time, 10000.0) / 10000.0;
    // update_libsecam_options(secamiz0r->libsecam, secamiz0r->intensity);
#endif

    // libsecam_filter_to_buffer(secamiz0r->libsecam,
    //     (unsigned char const *) input,
    //     (unsigned char *) output);
    
    libsecam_perform(output, input, secamiz0r->width, secamiz0r->height, params);

#ifdef ENABLE_TIME_TEST
    int w = floor(d * secamiz0r->width);
    for (int x = 0; x < w; x++) {
        output[(secamiz0r->height - 8) * secamiz0r->width + x] = 0xffffffff;
        for (int y = (secamiz0r->height - 7); y < secamiz0r->height; y++) {
            output[y * secamiz0r->width + x] = 0xffff0000;
        }
    }
#endif

}


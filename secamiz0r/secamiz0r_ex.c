
#define LIBSECAM_IMPLEMENTATION

#include <stdio.h>
#include "frei0r.h"
#include "libsecam.h"

#define INTEGER_STRING_LENGTH           32

struct secamiz0r
{
    libsecam_t *libsecam;
    unsigned int width;
    unsigned int height;

    char echo[INTEGER_STRING_LENGTH];
    char skew[INTEGER_STRING_LENGTH];
    char wobble[INTEGER_STRING_LENGTH];
};

static int parse_number(int *value, char const *str)
{
    if (sscanf(str, "%d", value) == -1) {
        return -1;
    }

    return 0;
}

static void store_option(char *str, int value)
{
    snprintf(str, INTEGER_STRING_LENGTH - 1, "%d", value);
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
    info->name = "secamiz0r_ex";
    info->author = "tuorqai";
    info->plugin_type = F0R_PLUGIN_TYPE_FILTER;
    info->color_model = F0R_COLOR_MODEL_RGBA8888;
    info->frei0r_version = FREI0R_MAJOR_VERSION;
    info->major_version = 1;
    info->minor_version = 0;
    info->num_params = 6;
    info->explanation = "SECAM Fire effect (Extended)";
}

void f0r_get_param_info(f0r_param_info_t *info, int index)
{
    if (index < 0 || index >= 6) {
        return;
    }

    char const *names[] = {
        "Luma noise",
        "Chroma noise",
        "Chroma fire",
        "Echo",
        "Skew",
        "Wobble",
    };

    int types[] = {
        F0R_PARAM_DOUBLE,
        F0R_PARAM_DOUBLE,
        F0R_PARAM_DOUBLE,
        F0R_PARAM_STRING,
        F0R_PARAM_STRING,
        F0R_PARAM_STRING,
    };

    char const *descriptions[] = {
        "Intensity of luminance (brightness) noise",
        "Intensity of chrominance (color) noise",
        "Intensity of fire effect",
        "Force of echo effect (in pixels)",
        "Amount of horizontal skew (in pixels)",
        "Amount of horizontal wobble (in pixels)",
    };

    info->name = names[index];
    info->type = types[index];
    info->explanation = descriptions[index];
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

    store_option(instance->echo, instance->libsecam->options.echo);
    store_option(instance->skew, instance->libsecam->options.skew);
    store_option(instance->wobble, instance->libsecam->options.wobble);

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
    int value;

    switch (index) {
    case 0:
        secamiz0r->libsecam->options.luma_noise = *((double *) param);
        break;
    case 1:
        secamiz0r->libsecam->options.chroma_noise = *((double *) param);
        break;
    case 2:
        secamiz0r->libsecam->options.chroma_fire = *((double *) param);
        break;
    case 3:
        if (parse_number(&value, *(f0r_param_string *) param) == 0) {
            secamiz0r->libsecam->options.echo = value;
            store_option(secamiz0r->echo, value);
        }
        break;
    case 4:
        if (parse_number(&value, *(f0r_param_string *) param) == 0) {
            secamiz0r->libsecam->options.skew = value;
            store_option(secamiz0r->skew, value);
        }
        break;
    case 5:
        if (parse_number(&value, *(f0r_param_string *) param) == 0) {
            secamiz0r->libsecam->options.wobble = value;
            store_option(secamiz0r->wobble, value);
        }
        break;
    default:
        break;
    }
}

void f0r_get_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *secamiz0r = instance;

    switch (index) {
    case 0:
        *((double *) param) = secamiz0r->libsecam->options.luma_noise;
        break;
    case 1:
        *((double *) param) = secamiz0r->libsecam->options.chroma_noise;
        break;
    case 2:
        *((double *) param) = secamiz0r->libsecam->options.chroma_fire;
        break;
    case 3:
        *((f0r_param_string *) param) = secamiz0r->echo;
        break;
    case 4:
        *((f0r_param_string *) param) = secamiz0r->skew;
        break;
    case 5:
        *((f0r_param_string *) param) = secamiz0r->wobble;
        break;
    default:
        break;
    }
}

void f0r_update(f0r_instance_t instance, double time, uint32_t const *input, uint32_t *output)
{
    struct secamiz0r *secamiz0r = instance;

    libsecam_filter_to_buffer(secamiz0r->libsecam,
        (unsigned char const *) input,
        (unsigned char *) output);
}

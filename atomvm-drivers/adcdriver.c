
#include "adcdriver.h"

#include <stdbool.h>
#include <string.h>

#include <hardware/adc.h>

#include "defaultatoms.h"
#include "memory.h"
#include "portnifloader.h"
#include "term.h"
#include "trace.h"

#define ADC_PIN_MIN 26
#define ADC_PIN_MAX 28
#define ADC_TEMP_CHANNEL 4

static bool adc_initialized = false;

static void ensure_adc_init(void)
{
    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }
}

static term nif_adc_init(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    VALIDATE_VALUE(argv[0], term_is_integer);
    uint pin = (uint) term_to_int(argv[0]);

    if (UNLIKELY(pin < ADC_PIN_MIN || pin > ADC_PIN_MAX)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    ensure_adc_init();

    adc_gpio_init(pin);

    return OK_ATOM;
}

static term nif_adc_read(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    VALIDATE_VALUE(argv[0], term_is_integer);
    uint pin = (uint) term_to_int(argv[0]);

    if (UNLIKELY(pin < ADC_PIN_MIN || pin > ADC_PIN_MAX)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    uint channel = pin - ADC_PIN_MIN;
    adc_select_input(channel);
    uint16_t raw = adc_read();

    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    term result = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(result, 0, OK_ATOM);
    term_put_tuple_element(result, 1, term_from_int(raw));

    return result;
}

static term nif_adc_read_temp(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    ensure_adc_init();

    adc_set_temp_sensor_enabled(true);
    adc_select_input(ADC_TEMP_CHANNEL);
    uint16_t raw = adc_read();

    adc_set_temp_sensor_enabled(false);

    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    term result = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(result, 0, OK_ATOM);
    term_put_tuple_element(result, 1, term_from_int(raw));

    return result;
}

static const struct Nif adc_init_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_adc_init
};
static const struct Nif adc_read_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_adc_read
};
static const struct Nif adc_read_temp_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_adc_read_temp
};

const struct Nif *adc_nif_get_nif(const char *nifname)
{
    if (strcmp("adc:init/1", nifname) == 0
        || strcmp("Elixir.ADC:init/1", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &adc_init_nif;
    }
    if (strcmp("adc:read/1", nifname) == 0
        || strcmp("Elixir.ADC:read/1", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &adc_read_nif;
    }
    if (strcmp("adc:read_temp/0", nifname) == 0
        || strcmp("Elixir.ADC:read_temp/0", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &adc_read_temp_nif;
    }
    return NULL;
}

REGISTER_NIF_COLLECTION(adc, NULL, NULL, adc_nif_get_nif)

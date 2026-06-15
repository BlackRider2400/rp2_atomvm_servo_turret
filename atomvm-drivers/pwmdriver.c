
#include "pwmdriver.h"

#include <stdbool.h>
#include <string.h>

#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include "defaultatoms.h"
#include "memory.h"
#include "portnifloader.h"
#include "term.h"
#include "trace.h"

// RP2040 system clock = 125 MHz
#define SYS_CLOCK_HZ 125000000UL
// wrap (TOP) value — rozdzielczość 16-bit
#define PWM_WRAP 65535

// Erlang:  pwm:init(Pin, FreqHz) -> ok | {error, badarg}
// Elixir:  PWM.init(pin, freq_hz) -> :ok | {:error, :badarg}

static term nif_pwm_init(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    VALIDATE_VALUE(argv[0], term_is_integer);
    VALIDATE_VALUE(argv[1], term_is_integer);

    uint pin = (uint) term_to_int(argv[0]);
    uint freq_hz = (uint) term_to_int(argv[1]);

    if (UNLIKELY(pin >= NUM_BANK0_GPIOS)) {
        RAISE_ERROR(BADARG_ATOM);
    }
    if (UNLIKELY(freq_hz == 0)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(pin);

    uint32_t div16 = (uint32_t) (SYS_CLOCK_HZ * 16UL / ((uint64_t) freq_hz * (PWM_WRAP + 1)));

    if (UNLIKELY(div16 < 16)) {
        div16 = 16;
    }
    if (UNLIKELY(div16 > 0xFFF)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac(&cfg, div16 >> 4, div16 & 0xF);
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(slice, &cfg, true);

    pwm_set_gpio_level(pin, 0);

    return OK_ATOM;
}

static term nif_pwm_set_duty(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    VALIDATE_VALUE(argv[0], term_is_integer);
    VALIDATE_VALUE(argv[1], term_is_integer);

    uint pin = (uint) term_to_int(argv[0]);
    uint duty = (uint) term_to_int(argv[1]);

    if (UNLIKELY(pin >= NUM_BANK0_GPIOS)) {
        RAISE_ERROR(BADARG_ATOM);
    }
    if (UNLIKELY(duty > PWM_WRAP)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    pwm_set_gpio_level(pin, duty);

    return OK_ATOM;
}

static term nif_pwm_deinit(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    VALIDATE_VALUE(argv[0], term_is_integer);
    uint pin = (uint) term_to_int(argv[0]);

    if (UNLIKELY(pin >= NUM_BANK0_GPIOS)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    gpio_set_function(pin, GPIO_FUNC_SIO);

    return OK_ATOM;
}

static const struct Nif pwm_init_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_pwm_init
};
static const struct Nif pwm_set_duty_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_pwm_set_duty
};
static const struct Nif pwm_deinit_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_pwm_deinit
};

const struct Nif *pwm_nif_get_nif(const char *nifname)
{
    if (strcmp("pwm:init/2", nifname) == 0
        || strcmp("Elixir.PWM:init/2", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &pwm_init_nif;
    }
    if (strcmp("pwm:set_duty/2", nifname) == 0
        || strcmp("Elixir.PWM:set_duty/2", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &pwm_set_duty_nif;
    }
    if (strcmp("pwm:deinit/1", nifname) == 0
        || strcmp("Elixir.PWM:deinit/1", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &pwm_deinit_nif;
    }
    return NULL;
}

REGISTER_NIF_COLLECTION(pwm, NULL, NULL, pwm_nif_get_nif)

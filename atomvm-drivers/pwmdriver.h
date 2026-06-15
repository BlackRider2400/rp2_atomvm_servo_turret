
#ifndef _PWM_DRIVER_H_
#define _PWM_DRIVER_H_

#include "context.h"
#include "nifs.h"

#ifdef __cplusplus
extern "C" {
#endif

const struct Nif *pwm_nif_get_nif(const char *nifname);

#ifdef __cplusplus
}
#endif

#endif

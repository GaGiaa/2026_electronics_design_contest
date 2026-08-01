#include <string.h>

#include "app_k230_service.h"

bool h723_k230_service_get_snapshot(app_k230_sample_t *sample,
                                    uint32_t now_ms,
                                    uint32_t *sample_age_ms)
{
    (void)now_ms;
    if (sample != NULL) {
        (void)memset(sample, 0, sizeof(*sample));
    }
    if (sample_age_ms != NULL) {
        *sample_age_ms = UINT32_MAX;
    }
    return false;
}

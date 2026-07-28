#ifndef APP_STARTUP_H
#define APP_STARTUP_H

#include <stdint.h>

typedef struct {
    uint32_t stacked_r0;
    uint32_t stacked_r1;
    uint32_t stacked_r2;
    uint32_t stacked_r3;
    uint32_t stacked_r12;
    uint32_t stacked_lr;
    uint32_t stacked_pc;
    uint32_t stacked_xpsr;
    uint32_t stacked_sp;
    uint32_t exception_return;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t icsr;
    uint32_t active;
} app_hardfault_snapshot_t;

extern volatile app_hardfault_snapshot_t g_hardfault_snapshot;

void app_startup(void);

#endif

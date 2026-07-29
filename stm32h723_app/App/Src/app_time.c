#include "app_time.h"

#include "cmsis_os2.h"

uint32_t h723_app_time_now_ms(void)
{
    return osKernelGetTickCount();
}

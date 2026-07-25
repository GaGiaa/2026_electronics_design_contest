#include <FreeRTOS.h>
#include <task.h>

#include "app/app_startup.h"
#include "ti_msp_dl_config.h"

int main(void)
{
    SYSCFG_DL_init();
    app_startup();
    vTaskStartScheduler();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

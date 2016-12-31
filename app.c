#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <ssid_config.h>

#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp/gpio.h>
#include <string.h>

#include "player.h"
#include "eq.h"

#define GPIO_LED 16
int isEqStart = 0;
void output_callback(void *data, int len)
{
    if(isEqStart == 0) {
        eq_start();
        isEqStart = 1;
    }
    eq_update(data, len);
}

void user_init(void) {

    uart_set_baud(0, 115200);
    printf("SDK version: %s, free heap %u\n", sdk_system_get_sdk_version(), xPortGetFreeHeapSize());

    eq_init();
    player_init(44100, output_callback);
    player_play("happy-new-year.mp3");

}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <ssd1306/ssd1306.h>
#include <i2c/i2c.h>
#include <math.h>
#include "kiss_fft.h"

#include "eq.h"
#define PI 3.14159265

#define SCL_PIN  5
#define SDA_PIN  4

#define BAR_COUNT 16

#define DISPLAY_WIDTH 128

#define FFT_SIZE (128)
#define FFT_HAFT_SIZE (FFT_SIZE/2)
#define BAR_DIV (FFT_SIZE/BAR_COUNT/2)

static uint8_t buffer[128 * 64 / 8];
static uint8_t bar_val[BAR_COUNT];
TaskHandle_t xTaskEqNotify;
int eq_run = 0;

/* Declare device descriptor */
static const ssd1306_t dev = {
    .protocol = SSD1306_PROTO_I2C,
    .addr     = SSD1306_I2C_ADDR_0,
    .width    = 128,
    .height   = 64,
    .screen = SH1106_SCREEN
};
const font_info_t *font = NULL;


char *data_in_buffer = NULL;
kiss_fft_cpx *fft_in = NULL;
kiss_fft_cpx *fft_out = NULL;
float *fft_magnitude = NULL;
int isAvailabe = 0;

inline float scale(short val)
{
    return val < 0 ? val * (1 / 32768.0f) : val * (1 / 32767.0f);
}

void eq_update_bar()
{
    uint8_t bar_width = DISPLAY_WIDTH / BAR_COUNT;
    int i;

    memset(buffer, 0xFF, sizeof(buffer));
    for(i = 0; i < BAR_COUNT; i++) {

        ssd1306_fill_rectangle(&dev, buffer, i * bar_width, 0, bar_width, 64 - bar_val[i], OLED_COLOR_BLACK);
    }
    ssd1306_draw_string(&dev, buffer, font_builtin_fonts[FONT_FACE_BITOCRA_6X11], 0, 0, "Happy New Year 2017", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_load_frame_buffer(&dev, buffer);
}
void eq_task(void *pv)
{
    int i,j;
    kiss_fft_cfg *fft = NULL;
    short *data_ptr;
    while(eq_run) {
        printf("wait eq...%d\n", xPortGetFreeHeapSize());
        if(fft == NULL)
            fft = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);
        if(data_in_buffer == NULL)
            data_in_buffer = malloc(FFT_SIZE);
        if(fft_in == NULL)
            fft_in = malloc(FFT_SIZE * sizeof(kiss_fft_cpx));
        if(fft_out == NULL)
            fft_out = malloc(FFT_SIZE * sizeof(kiss_fft_cpx));
        if(fft_magnitude == NULL)
            fft_magnitude = malloc((FFT_SIZE/2) * sizeof(float));
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(isAvailabe) {


            // // convert data to real and apply window func
            // // http://stackoverflow.com/questions/3555318/implement-hann-window

            data_ptr = (short*)data_in_buffer;

            for(i = 0; i < FFT_SIZE; i++) {
                fft_in[i].i = 0;
                fft_in[i].r = (0.5 * (1 - cos(2 * PI * i / (FFT_SIZE - 1)))) * scale(*data_ptr);
                data_ptr ++;
            }
            //do fft
            kiss_fft(fft, fft_in, fft_out);
            //calc fft_magnitude
            j = 0;
            for(i = 0; i < FFT_SIZE / 2; i++) {
                fft_magnitude[i] = sqrt(fft_out[i].i * fft_out[i].i + fft_out[i].r * fft_out[i].r)*8;
                if(i % BAR_DIV == 0) {
                    bar_val[i / BAR_DIV] = fft_magnitude[i];
                }

            }
            eq_update_bar();
            isAvailabe = 0;
        }

    }
    vTaskDelete(NULL);
}

void eq_init()
{
    eq_run = 1;
    font = font_builtin_fonts[FONT_FACE_TERMINUS_BOLD_8X14_ISO8859_1];

    i2c_init(SCL_PIN, SDA_PIN);

    while(ssd1306_init(&dev) != 0)
    {
        printf("%s: failed to init SSD1306 lcd\n", __func__);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ssd1306_set_whole_display_lighting(&dev, false);
    ssd1306_set_scan_direction_fwd(&dev, false);
    memset(buffer, 0, sizeof(buffer));
    ssd1306_load_frame_buffer(&dev, buffer);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    /* Display ESP8266 */
    ssd1306_fill_rectangle(&dev, buffer, 0, 0, 128, 64 / 2, OLED_COLOR_BLACK);
    ssd1306_draw_string(&dev, buffer, font_builtin_fonts[FONT_FACE_BITOCRA_7X13], 4, 10, "Espressif Systems", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_draw_string(&dev, buffer, font_builtin_fonts[FONT_FACE_TERMINUS_BOLD_16X32_ISO8859_1], 8, 30, "ESP8266", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_load_frame_buffer(&dev, buffer);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    memset(buffer, 0, sizeof(buffer));

    /* Display Happy new year */
    ssd1306_fill_rectangle(&dev, buffer, 0, 0, 128, 64 / 2, OLED_COLOR_BLACK);
    ssd1306_draw_string(&dev, buffer, font_builtin_fonts[FONT_FACE_TERMINUS_BOLD_8X14_ISO8859_1], 5, 10, "Happy New Year", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_draw_string(&dev, buffer, font_builtin_fonts[FONT_FACE_TERMINUS_BOLD_16X32_ISO8859_1], 30, 30, "2017", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_load_frame_buffer(&dev, buffer);

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    memset(bar_val, 0, sizeof(bar_val));
    eq_update_bar();
    printf("finish init EQ\n");
}

void eq_start()
{
    printf("Start EQ, mem=%d\n", xPortGetFreeHeapSize());
    if(xTaskCreate(&eq_task, "eq_task", 256, NULL, 5, &xTaskEqNotify) == pdFALSE) {
        printf("Error init player_task\n");
    }
}
void eq_stop()
{
    eq_run = 0;
}

int copied_size = 0;
void eq_update(void *data, int len)
{
    if(data_in_buffer == NULL)
        return;
    int need_copy = FFT_SIZE - copied_size;
    if(len < need_copy) {
        need_copy = len;
    }
    memcpy(data_in_buffer + copied_size, data, need_copy);
    copied_size += need_copy;
    if(copied_size >= FFT_SIZE) {
        copied_size = 0;
        isAvailabe = 1;
    }

    xTaskNotifyGive(xTaskEqNotify);
}

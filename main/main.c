/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

// This file contains a simple Hello World app which you can base you own
// native Badge apps on.

#include "main.h"
#include "sdcard.h"

static pax_buf_t buf;
xQueueHandle buttonQueue;

#include <esp_log.h>
static const char *TAG = "bahorn-test";

#define VIDEO_WIDTH 160
#define VIDEO_HEIGHT 120
struct funargs {
    int t;
    char frame[2400];
} fun;

// Updates the screen with the latest buffer.
void disp_flush() {
    ili9341_write(get_ili9341(), buf.buf);
}

// Exits the app, returning to the launcher.
void exit_to_launcher() {
    REG_WRITE(RTC_CNTL_STORE0_REG, 0);
    esp_restart();
}

pax_col_t my_shader_callback(pax_col_t tint, int x, int y, float u, float v, void *args) {
    struct funargs *funa = (struct funargs *)args;
    // now do a lookup
    int value = 0;
    int new_x = x/2;
    int new_y = y/2;
    char bit = 1 << (7 - ((x/2) % 8));
    if ((funa->frame[(VIDEO_WIDTH/8)*new_y + new_x/8] & bit) > 0) {
        value = 1;
    }
    
    return pax_col_rgb(value*255, value*255, value*255);
}

pax_shader_t my_shader = (pax_shader_t) {
    .callback          = my_shader_callback,
    .callback_args     = &fun,
    .alpha_promise_0   = false,
    .alpha_promise_255 = true
};

void app_main() {

    esp_err_t res  = mount_sd(GPIO_SD_CMD, GPIO_SD_CLK, GPIO_SD_D0, GPIO_SD_PWR, "/sd", false, 5);
    if(res != ESP_OK) ESP_LOGE(TAG, "could not mount SD card");


    FILE *fd = fopen("/sd/out.bin", "rb");

    fread(&(fun.frame), 2400, 1, fd);

    
    ESP_LOGI(TAG, "Welcome to the template app!");

    // Initialize the screen, the I2C and the SPI busses.
    bsp_init();

    // Initialize the RP2040 (responsible for buttons, etc).
    bsp_rp2040_init();
    
    // This queue is used to receive button presses.
    buttonQueue = get_rp2040()->queue;
    
    // Initialize graphics for the screen.
    pax_buf_init(&buf, NULL, 320, 240, PAX_BUF_16_565RGB);
    
    // Initialize NVS.
    nvs_flash_init();
    
    // Initialize WiFi. This doesn't connect to Wifi yet.
    wifi_init();

    fun.t = 128;
    
    while (1) {
        fun.t += 10;

        if (fread(&(fun.frame), 2400, 1, fd) != 1) {
            rewind(fd);
            fread(&(fun.frame), 2400, 1, fd);
        }

        pax_shade_rect(&buf, -1, &my_shader, NULL, 0, 0, 360, 240);
        char text[256];
        sprintf(text, "%i", fun.t);
        // Pick the font (Saira is the only one that looks nice in this size).
        const pax_font_t *font = pax_font_saira_condensed;

        // Determine how the text dimensions so we can display it centered on
        // screen.
        //pax_vec1_t        dims = pax_text_size(font, font->default_size, text);

        // Draw the centered text.
        /*
        pax_draw_text(
            &buf, // Buffer to draw to.
            0xff000000, // color
            font, font->default_size, // Font and size to use.
            // Position (top left corner) of the app.
            (buf.width  - dims.x) / 2.0,
            (buf.height - dims.y) / 2.0,
            // The text to be rendered.
            text
        );
        */

        // Draws the entire graphics buffer to the screen.
        disp_flush();
    }
}

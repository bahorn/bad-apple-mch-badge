/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

// This file contains a simple Hello World app which you can base you own
// native Badge apps on.

#include "main.h"
#include "pax_gfx.h"
#include "sdcard.h"

#include <string.h>

static pax_buf_t buf;
xQueueHandle buttonQueue;

#include <esp_log.h>
static const char *TAG = "bahorn-test";

#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240

#define FRAME_SIZE 9600
#define BATCH_SIZE 32
#define BATCH_COUNT 8

struct funargs {
    FILE *fd;
    bool requested;
    bool reset;
    int t;
    int next_batch;
    int remaining;
    char * buffer;
} fun;


void draw_frame() {
    //memset(buf.buf_16bpp, 0, VIDEO_WIDTH * VIDEO_HEIGHT * sizeof(uint16_t));
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            char bit = 1 << (7 - ((x) % 8));
            char frame = fun.buffer[
                FRAME_SIZE * fun.t + (VIDEO_WIDTH/8) * y + x / 8
            ];
            uint16_t col = 0xffff * ((frame & bit) > 0);
            buf.buf_16bpp[y * VIDEO_WIDTH + x] = col;
        }

    }

}

void load_batch(bool initial) {
    // only load more every BATCH_SIZE
    int res;

    // need to copy two buffers in at once.
    if (initial) {
        ESP_LOGI(TAG, "Setting up! %i bbp @ %i x %i", buf.bpp, buf.width, buf.height);
        res = fread(
            fun.buffer,
            FRAME_SIZE,
            BATCH_SIZE * BATCH_COUNT,
            fun.fd
        );
        ESP_LOGI(TAG, "Res: %i", res);
        fun.t = 0;
        fun.next_batch = 0;
        fun.remaining = res;
        fun.requested = false;
        fun.reset = false;
        return;
    }

    /* Load more frames */
    ESP_LOGI(TAG, "Loading more for batch: %i, remaining %i", fun.next_batch, fun.remaining);
    res = fread(
        &(fun.buffer[fun.next_batch * BATCH_SIZE * FRAME_SIZE]),
        FRAME_SIZE,
        BATCH_SIZE,
        fun.fd
    );
    fun.remaining += res;
    fun.next_batch += 1;
    fun.next_batch %= BATCH_COUNT;
    fun.requested = false;
    fun.reset = false;

    // if we are out of frames, go back to the start.
    if (fun.remaining == 0) {
        ESP_LOGI(TAG, "Rewind");
        fun.reset = true;
    }
}

bool read_frame() {
    if (!fun.requested && fun.remaining < ((BATCH_SIZE * (BATCH_COUNT - 1))))
    {
        fun.requested = true;
    }
    if (fun.remaining <= 0) {
        ESP_LOGI(TAG, "DELAY!");
        return false;
    }
    draw_frame();
    fun.t += 1;
    fun.t %= (BATCH_SIZE * BATCH_COUNT);
    fun.remaining -= 1;

    return true;
}

// Updates the screen with the latest buffer.
void disp_flush() {
    ili9341_write(get_ili9341(), buf.buf);
}

// Exits the app, returning to the launcher.
void exit_to_launcher() {
    REG_WRITE(RTC_CNTL_STORE0_REG, 0);
    esp_restart();
}

void loadData(void * pvParameters)
{
    while (true) {
        if (fun.requested) {
            if (fun.reset && fun.remaining == 0) {
                rewind(fun.fd);
                load_batch(true);
            } else {
                load_batch(false);
            }
        }
    }
}


void app_main() {
    esp_err_t res  = mount_sd(
        GPIO_SD_CMD,
        GPIO_SD_CLK,
        GPIO_SD_D0,
        GPIO_SD_PWR,
        "/sd",
        false,
        5
    );

    if(res != ESP_OK) ESP_LOGE(TAG, "could not mount SD card");

    ESP_LOGI(TAG, "Starting bad apple!!");

    bsp_init();
    bsp_rp2040_init();
    pax_buf_init(&buf, NULL, 320, 240, PAX_BUF_16_565RGB);
    nvs_flash_init();

    fun.fd = fopen("/sd/out.bin", "rb");
    fun.buffer = malloc(FRAME_SIZE * BATCH_SIZE * BATCH_COUNT);
    
    load_batch(true);
    
    /* Start the second thread */
    TaskHandle_t xDataLoader = NULL;
    xTaskCreatePinnedToCore(
        loadData,
        "SDFETCH",
        8192,
        NULL,
        32,
        &xDataLoader,
        1
    );

    while (true) {
        if (read_frame()) {
            disp_flush();
        }
    }
}

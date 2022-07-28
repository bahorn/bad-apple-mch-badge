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
ILI9341 *disp;

#include <esp_log.h>
static const char *TAG = "bahorn-test";

#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240

#define FRAME_SIZE 9600
#define BATCH_SIZE 32
#define BATCH_COUNT 2

#define DRAW_BUFFERS 16

int idx;
uint16_t *buffers[DRAW_BUFFERS];
int avaliable;
struct funargs {
    FILE *fd;
    bool requested;
    bool reset;
    int t;
    int next_batch;
    int remaining;
    char * buffer;
} fun;


void draw_frame(int buf_idx) {
    for (int y = 0; y < VIDEO_HEIGHT; y++) {
        for (int x = 0; x < VIDEO_WIDTH; x++) {
            char bit = 1 << (7 - ((x) % 8));
            char frame = fun.buffer[
                FRAME_SIZE * fun.t + (VIDEO_WIDTH/8) * y + x / 8
            ];
            uint16_t col = 0xffff * ((frame & bit) > 0);
            buffers[buf_idx][y * VIDEO_WIDTH + x] = col;
        }
    }
}

void load_batch(bool initial) {
    // only load more every BATCH_SIZE
    int res;

    // need to copy two buffers in at once.
    if (initial) {
        ESP_LOGI(TAG, "Setting up! %i bbp @ %i x %i", 16, VIDEO_WIDTH, VIDEO_HEIGHT);
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

bool read_frame(int frame_idx) {
    if (!fun.requested && fun.remaining < ((BATCH_SIZE * (BATCH_COUNT - 1))))
    {
        fun.requested = true;
    }
    if (fun.remaining <= 0) {
        ESP_LOGI(TAG, "DELAY!");
        return false;
    }
    draw_frame(frame_idx);
    fun.t += 1;
    fun.t %= (BATCH_SIZE * BATCH_COUNT);
    fun.remaining -= 1;

    return true;
}

// Updates the screen with the latest buffer.
void disp_flush() {
    ili9341_write(disp, (uint8_t*) buffers[idx]);
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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void renderLoop(void * pvParameters)
{
    avaliable = 0;
    int k = 0;
    while (true) {
        if (avaliable < DRAW_BUFFERS) {
            read_frame(k);
            avaliable += 1;
            k += 1;
            k %= DRAW_BUFFERS;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main() {
    ESP_LOGI(TAG, "Starting bad apple!!");

    bsp_init();
    bsp_rp2040_init();

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

    idx = 0;
    
    disp = get_ili9341();
    // allocate buffers 
    for (int i = 0; i < DRAW_BUFFERS; i++) {
        buffers[i] = malloc(sizeof(uint16_t) * VIDEO_WIDTH * VIDEO_HEIGHT);
    }

    nvs_flash_init();

    fun.fd = fopen("/sd/out.bin", "rb");
    fun.buffer = malloc(FRAME_SIZE * BATCH_SIZE * BATCH_COUNT);
    
    load_batch(true);
    
    /* Thread to load frames */
    TaskHandle_t xDataLoader = NULL;
    xTaskCreatePinnedToCore(
        loadData,
        "SDFETCH",
        8192,
        NULL,
        24,
        &xDataLoader,
        1
    );
    /* Thread to render frames */
    TaskHandle_t xRender = NULL;
    xTaskCreatePinnedToCore(
        renderLoop,
        "RENDER",
        8192,
        NULL,
        8,
        &xRender,
        1
    );

    while (true) {
        if (avaliable > 0) {
            disp_flush();
            idx += 1;
            idx %= DRAW_BUFFERS;
            avaliable -= 1;
        }
    }
}

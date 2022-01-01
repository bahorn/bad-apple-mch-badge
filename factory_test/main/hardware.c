#include "hardware.h"
#include <driver/spi_master.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include "managed_i2c.h"
#include "logo.h"

static const char *TAG = "hardware";

PCA9555 dev_pca9555 = {0};
BNO055  dev_bno055   = {0};
ILI9341 dev_ili9341 = {0};
ICE40   dev_ice40 = {0};

// Wrapper functions for linking the ICE40 component to the PCA9555 component
esp_err_t ice40_get_done_wrapper(bool* done) { return pca9555_get_gpio_value(&dev_pca9555, PCA9555_PIN_FPGA_CDONE, done); }
esp_err_t ice40_set_reset_wrapper(bool reset) { return pca9555_set_gpio_value(&dev_pca9555, PCA9555_PIN_FPGA_RESET, reset); }

esp_err_t hardware_init() {
    esp_err_t res;
    
    // Interrupts
    res = gpio_install_isr_service(0);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing ISR service failed");
        return res;
    }
    
    // System I2C bus
    res = i2c_init(I2C_BUS_SYS, GPIO_I2C_SYS_SDA, GPIO_I2C_SYS_SCL, I2C_SPEED_SYS, false, false);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing system I2C bus failed");
        return res;
    }
    
    // User I2C bus
    res = i2c_init(I2C_BUS_EXT, GPIO_I2C_EXT_SDA, GPIO_I2C_EXT_SCL, I2C_SPEED_EXT, false, false);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing user I2C bus failed");
        return res;
    }

    // SPI bus
    spi_bus_config_t busConfiguration = {0};
    busConfiguration.mosi_io_num     = GPIO_SPI_MOSI;
    busConfiguration.miso_io_num     = GPIO_SPI_MISO;
    busConfiguration.sclk_io_num     = GPIO_SPI_CLK;
    busConfiguration.quadwp_io_num   = -1;
    busConfiguration.quadhd_io_num   = -1;
    busConfiguration.max_transfer_sz = SPI_MAX_TRANSFER_SIZE;
    res = spi_bus_initialize(SPI_BUS, &busConfiguration, SPI_DMA_CHANNEL);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing SPI bus failed");
        return res;
    }
    
    // PCA9555 IO expander on system I2C bus
    res = pca9555_init(&dev_pca9555, I2C_BUS_SYS, PCA9555_ADDR, GPIO_INT_PCA9555);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing PCA9555 failed");
        return res;
    }
    
    res = pca9555_set_gpio_direction(&dev_pca9555, PCA9555_PIN_FPGA_RESET, true);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Setting the FPGA reset pin on the PCA9555 to output failed");
        return res;
    }
    
    res = pca9555_set_gpio_value(&dev_pca9555, PCA9555_PIN_FPGA_RESET, false);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Setting the FPGA reset pin on the PCA9555 to low failed");
        return res;
    }
    
    // FPGA
    dev_ice40.spi_bus = SPI_BUS;
    dev_ice40.pin_cs = GPIO_SPI_CS_FPGA;
    dev_ice40.pin_done = -1;
    dev_ice40.pin_reset = -1;
    dev_ice40.pin_int = GPIO_INT_FPGA;
    dev_ice40.spi_speed = 60000000; // 60MHz
    dev_ice40.spi_max_transfer_size = SPI_MAX_TRANSFER_SIZE;
    dev_ice40.get_done = ice40_get_done_wrapper;
    dev_ice40.set_reset = ice40_set_reset_wrapper;

    res = ice40_init(&dev_ice40);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing FPGA failed");
        return res;
    }

    // LCD display
    dev_ili9341.spi_bus   = SPI_BUS;
    dev_ili9341.pin_cs    = GPIO_SPI_CS_LCD;
    dev_ili9341.pin_dcx   = GPIO_SPI_DC_LCD;
    dev_ili9341.pin_reset = -1;
    dev_ili9341.rotation  = 1;
    dev_ili9341.color_mode = true; // Blue and red channels are swapped
    dev_ili9341.spi_speed = 60000000; // 60MHz
    dev_ili9341.spi_max_transfer_size = SPI_MAX_TRANSFER_SIZE;
    dev_ili9341.callback = NULL; // Callback for changing LCD mode between ESP32 and FPGA
    
    res = ili9341_init(&dev_ili9341);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing LCD failed");
        return res;
    }
    
    // Hack: show logo while the other hardware components initialize
    res = ili9341_write(&dev_ili9341, logo);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write logo to LCD");
        return res;
    }
    
    // BNO055 sensor on system I2C bus
    
    res = bno055_init(&dev_bno055, I2C_BUS_SYS, BNO055_ADDR, GPIO_INT_BNO055, true);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing BNO055 failed");
        return res;
    }

    return res;
}

PCA9555* get_pca9555() {
    return &dev_pca9555;
}

BNO055* get_bno055() {
    return &dev_bno055;
}

ILI9341* get_ili9341() {
    return &dev_ili9341;
}

ICE40* get_ice40() {
    return &dev_ice40;
}

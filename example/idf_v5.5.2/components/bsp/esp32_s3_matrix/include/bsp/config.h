#pragma once

#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_AUDIO          1
#define BSP_CAPS_SDCARD         1
#define BSP_CAPS_BUTTONS        1
#define BSP_CAPS_WIFI           1
#define BSP_CAPS_I2C            1

typedef enum {
    BSP_BUTTON_MAIN = 0,
    BSP_BUTTON_NUM
} bsp_button_t;

/**************************************************************************************************
 *  ESP32-S3-Matrix-BSP pinout
 **************************************************************************************************/
#define BSP_I2C_SCL           (GPIO_NUM_48)
#define BSP_I2C_SDA           (GPIO_NUM_47)

#define BSP_I2S_SCLK          (GPIO_NUM_43)
#define BSP_I2S_MCLK          (GPIO_NUM_12)
#define BSP_I2S_LCLK          (GPIO_NUM_38)
#define BSP_I2S_DOUT          (GPIO_NUM_21)
#define BSP_I2S_DSIN          (GPIO_NUM_39)
#define BSP_POWER_AMP_IO      (GPIO_NUM_11)

#define BSP_BUTTON_MAIN_IO    (GPIO_NUM_0)

#define BSP_SD_D0             (GPIO_NUM_17)
#define BSP_SD_D1             (GPIO_NUM_NC) 
#define BSP_SD_D2             (GPIO_NUM_NC)
#define BSP_SD_D3             (GPIO_NUM_NC)
#define BSP_SD_CMD            (GPIO_NUM_44)
#define BSP_SD_CLK            (GPIO_NUM_1)

#define BSP_SD_SPI_MISO       (GPIO_NUM_NC)
#define BSP_SD_SPI_MOSI       (GPIO_NUM_NC)
#define BSP_SD_SPI_CLK        (GPIO_NUM_NC)
#define BSP_SD_SPI_CS         (GPIO_NUM_14)
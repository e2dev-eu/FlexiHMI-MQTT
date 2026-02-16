#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "bsp/config.h"
#include "bsp/display.h"
#include "bsp/touch.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "lvgl.h"
#endif

#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        0
#define BSP_CAPS_AUDIO          0
#define BSP_CAPS_AUDIO_SPEAKER  0
#define BSP_CAPS_AUDIO_MIC      0
#define BSP_CAPS_SDCARD         0
#define BSP_CAPS_IMU            0

#define BSP_I2C_SCL             (GPIO_NUM_8)
#define BSP_I2C_SDA             (GPIO_NUM_7)

#define BSP_I2C_NUM             (I2C_NUM_1)

#define BSP_LCD_BACKLIGHT       (GPIO_NUM_23)
#define BSP_LCD_RST             (GPIO_NUM_0)
#define BSP_LCD_TOUCH_RST       (GPIO_NUM_22)
#define BSP_LCD_TOUCH_INT       (GPIO_NUM_21)

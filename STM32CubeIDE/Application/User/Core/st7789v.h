/*
 * st7789v.h
 * ST7789V LCD Driver for LVGL
 */

#ifndef APPLICATION_USER_CORE_ST7789V_H_
#define APPLICATION_USER_CORE_ST7789V_H_

#include "stm32g4xx_hal.h"
#include "lvgl.h"

extern SPI_HandleTypeDef hspi2;
#define BUS_SPI1_POLL_TIMEOUT 1000

// ST7789V Commands
#define ST7789_SWRESET         0x01
#define ST7789_SLPOUT          0x11
#define ST7789_NORON           0x13
#define ST7789_INVOFF          0x20
#define ST7789_DISPON          0x29
#define ST7789_CASET           0x2A
#define ST7789_RASET           0x2B
#define ST7789_RAM_WR          0x2C
#define ST7789_MADCTL          0x36
#define ST7789_COLMOD          0x3A
#define ST7789_PORCTRL         0xB2
#define ST7789_GCTRL           0xB7
#define ST7789_VCOMS           0xBB
#define ST7789_LCMCTRL         0xC0
#define ST7789_VDVVRHEN        0xC2
#define ST7789_VRHS            0xC3
#define ST7789_VDVS            0xC4
#define ST7789_FRCTRL2         0xC6
#define ST7789_PWCTRL1         0xD0
#define ST7789_PVGAMCTRL       0xE0
#define ST7789_NVGAMCTRL       0xE1

// Display Parameters (Landscape: 320x240)
#define DISPLAY_WIDTH          320
#define DISPLAY_HEIGHT         240

// RGB565 Color Definitions
#define COLOR565_BLACK   0x0000
#define COLOR565_WHITE   0xFFFF
#define COLOR565_RED     0xF800
#define COLOR565_GREEN   0x07E0
#define COLOR565_BLUE    0x001F

// Function Declarations
void ST7789V_Init(void);
void ST7789V_SetWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);
void ST7789V_Flush_Cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void ST7789V_SetDisplay(lv_disp_t *disp);
lv_disp_t* ST7789V_GetDisplay(void);

extern volatile int my_disp_bus_busy;

#endif /* APPLICATION_USER_CORE_ST7789V_H_ */

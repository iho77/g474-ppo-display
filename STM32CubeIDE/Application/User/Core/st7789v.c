#include "st7789v.h"
#include "main.h"
#include "lvgl.h"

volatile int my_disp_bus_busy = 0;
static lv_disp_t *g_disp = NULL;


static void TFT_DC_SET(uint8_t state)
{
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, (state == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

HAL_StatusTypeDef ST7789V_SendCmd(uint8_t cmd, const uint8_t *param, size_t param_size)
{
  //  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  //  HAL_SPI_Init(&hspi1);

    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

    TFT_DC_SET(0);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);

    if (status != HAL_OK)
    {
        HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
        return status;
    }

    if (param_size > 0)
    {
        TFT_DC_SET(1);
        status = HAL_SPI_Transmit(&hspi2, (uint8_t*)param, (uint16_t)param_size, HAL_MAX_DELAY);
    }

    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
    return status;
}

void ST7789V_Init(void)
{
    uint8_t param_madctl[] = { 0xA0 };  // Landscape: MY | MV
    uint8_t param_colmod[] = { 0x55 };  // 16-bit RGB565
    uint8_t param_porctrl[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
    uint8_t param_gctrl[] = { 0x35 };
    uint8_t param_vcoms[] = { 0x1F };
    uint8_t param_lcmctrl[] = { 0x2C };
    uint8_t param_vdvvrhen[] = { 0x01 };
    uint8_t param_vrhs[] = { 0x12 };
    uint8_t param_vdvs[] = { 0x20 };
    uint8_t param_frctrl2[] = { 0x0F };
    uint8_t param_pwctrl1[] = { 0xA4, 0xA1 };
    uint8_t param_pvgam[] = {
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23
    };
    uint8_t param_nvgam[] = {
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23
    };

    // Hardware reset
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(120);

    ST7789V_SendCmd(ST7789_SWRESET, NULL, 0);
    HAL_Delay(150);
    ST7789V_SendCmd(ST7789_SLPOUT, NULL, 0);
    HAL_Delay(120);
    ST7789V_SendCmd(ST7789_MADCTL, param_madctl, 1);
    ST7789V_SendCmd(ST7789_COLMOD, param_colmod, 1);
    HAL_Delay(10);
    ST7789V_SendCmd(ST7789_PORCTRL, param_porctrl, 5);
    ST7789V_SendCmd(ST7789_GCTRL, param_gctrl, 1);
    ST7789V_SendCmd(ST7789_VCOMS, param_vcoms, 1);
    ST7789V_SendCmd(ST7789_LCMCTRL, param_lcmctrl, 1);
    ST7789V_SendCmd(ST7789_VDVVRHEN, param_vdvvrhen, 1);
    ST7789V_SendCmd(ST7789_VRHS, param_vrhs, 1);
    ST7789V_SendCmd(ST7789_VDVS, param_vdvs, 1);
    ST7789V_SendCmd(ST7789_FRCTRL2, param_frctrl2, 1);
    ST7789V_SendCmd(ST7789_PWCTRL1, param_pwctrl1, 2);
    ST7789V_SendCmd(ST7789_PVGAMCTRL, param_pvgam, 14);
    ST7789V_SendCmd(ST7789_NVGAMCTRL, param_nvgam, 14);
    ST7789V_SendCmd(ST7789_INVOFF, NULL, 0);
    ST7789V_SendCmd(ST7789_NORON, NULL, 0);
    HAL_Delay(10);
    ST7789V_SendCmd(ST7789_DISPON, NULL, 0);
    HAL_Delay(120);
}

void ST7789V_SetWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    uint8_t caset_data[4] = {
        (uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF),
        (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF)
    };
    ST7789V_SendCmd(ST7789_CASET, caset_data, 4);

    uint8_t raset_data[4] = {
        (uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF),
        (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF)
    };
    ST7789V_SendCmd(ST7789_RASET, raset_data, 4);
    ST7789V_SendCmd(ST7789_RAM_WR, NULL, 0);
}

void ST7789V_SetDisplay(lv_disp_t *disp)
{
    g_disp = disp;
}

lv_disp_t* ST7789V_GetDisplay(void)
{
    return g_disp;
}

void ST7789V_Flush_Cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)disp_drv;
   // while (my_disp_bus_busy);

    ST7789V_SetWindow(area->x1, area->y1, area->x2, area->y2);

    int32_t width = area->x2-area->x1+1;
    int32_t height = area->y2-area->y1+1;
    size_t byte_count = (size_t)width * (size_t)height * 2U;

  //  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
   // HAL_SPI_Init(&hspi1);

    uint8_t ram_wr_cmd = ST7789_RAM_WR;
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2, &ram_wr_cmd, 1, BUS_SPI1_POLL_TIMEOUT);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    //  my_disp_bus_busy = 1;
    // Cast color_p to uint8_t* for DMA
    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)color_p, (uint16_t)byte_count);
}

#include "w25q128.h"
#include "main.h"   /* SPI2_CS_Pin, SPI2_CS_GPIO_Port */
#include <string.h>

/* ── CS control ── */
#define CS_LOW()   HAL_GPIO_WritePin(FL_CS_GPIO_Port, FL_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(FL_CS_GPIO_Port, FL_CS_Pin, GPIO_PIN_SET)

/* ── SPI command opcodes ── */
#define CMD_JEDEC_ID      0x9FU
#define CMD_WRITE_EN      0x06U
#define CMD_READ_SR1      0x05U
#define CMD_READ          0x03U
#define CMD_PAGE_PROG     0x02U
#define CMD_SECTOR_ERASE  0x20U
#define SR1_BUSY_BIT      0x01U

/* ── Private helpers ── */

static w25q128_status_t spi_tx(SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len)
{
    if (HAL_SPI_Transmit(hspi, buf, len, W25Q128_SPI_TIMEOUT) != HAL_OK) {
        return W25Q128_ERROR_SPI;
    }
    return W25Q128_OK;
}

static w25q128_status_t spi_rx(SPI_HandleTypeDef *hspi, uint8_t *buf, uint16_t len)
{
    if (HAL_SPI_Receive(hspi, buf, len, W25Q128_SPI_TIMEOUT) != HAL_OK) {
        return W25Q128_ERROR_SPI;
    }
    return W25Q128_OK;
}

static w25q128_status_t w25q128_wait_busy(SPI_HandleTypeDef *hspi, uint32_t timeout_ms)
{
    uint8_t cmd = CMD_READ_SR1;
    uint8_t sr1;
    uint32_t start = HAL_GetTick();

    CS_LOW();
    if (spi_tx(hspi, &cmd, 1U) != W25Q128_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    do {
        if (spi_rx(hspi, &sr1, 1U) != W25Q128_OK) {
            CS_HIGH();
            return W25Q128_ERROR_SPI;
        }
        if ((sr1 & SR1_BUSY_BIT) == 0U) {
            CS_HIGH();
            return W25Q128_OK;
        }
    } while ((HAL_GetTick() - start) < timeout_ms);

    CS_HIGH();
    return W25Q128_ERROR_TIMEOUT;
}

static w25q128_status_t w25q128_write_enable(SPI_HandleTypeDef *hspi)
{
    uint8_t cmd = CMD_WRITE_EN;
    CS_LOW();
    w25q128_status_t st = spi_tx(hspi, &cmd, 1U);
    CS_HIGH();
    return st;
}

/* ── Public API ── */

w25q128_status_t w25q128_init(SPI_HandleTypeDef *hspi)
{
    CS_HIGH();                /* deassert — was held LOW since GPIO init */
    HAL_Delay(3U);            /* power-up stabilisation */

    /* Software reset — clears any partial command state from power-on glitches
     * or spurious SCK edges during SPI peripheral initialisation (CS was LOW). */
    uint8_t cmd[2];
	cmd[0] = 0x66U;     /* Enable Reset */
    cmd[1] = 0x99U;    /* Reset Device */

    CS_LOW();
    spi_tx(hspi,&cmd[0],1U);
    CS_HIGH();
    HAL_Delay(3U);

    CS_LOW();
    spi_tx(hspi,&cmd[1],1U);
    CS_HIGH();
    HAL_Delay(3U);


    uint8_t mfr, type, cap;
    w25q128_status_t st = w25q128_read_jedec_id(hspi, &mfr, &type, &cap);
    if (st != W25Q128_OK) {
        return st;
    }
    if ((mfr != W25Q128_JEDEC_MFR) ||
        (type != W25Q128_JEDEC_TYPE) ||
        (cap  != W25Q128_JEDEC_CAP)) {
        return W25Q128_ERROR_ID;
    }
    return W25Q128_OK;
}

w25q128_status_t w25q128_read_jedec_id(SPI_HandleTypeDef *hspi,
                                         uint8_t *mfr, uint8_t *type, uint8_t *cap)
{
    uint8_t tx[4] = { CMD_JEDEC_ID, 0x00U, 0x00U, 0x00U };
    uint8_t rx[4] = { 0U };

    CS_LOW();
    if (HAL_SPI_TransmitReceive(hspi, tx, rx, 4U, W25Q128_SPI_TIMEOUT) != HAL_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    CS_HIGH();

    *mfr  = rx[1];
    *type = rx[2];
    *cap  = rx[3];
    return W25Q128_OK;
}

w25q128_status_t w25q128_read(SPI_HandleTypeDef *hspi,
                               uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t cmd[4] = {
        CMD_READ,
        (uint8_t)((addr >> 16U) & 0xFFU),
        (uint8_t)((addr >>  8U) & 0xFFU),
        (uint8_t)( addr         & 0xFFU),
    };

    CS_LOW();
    if (spi_tx(hspi, cmd, 4U) != W25Q128_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    if (HAL_SPI_Receive(hspi, buf, (uint16_t)len, W25Q128_SPI_TIMEOUT) != HAL_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    CS_HIGH();
    return W25Q128_OK;
}

w25q128_status_t w25q128_write_page(SPI_HandleTypeDef *hspi,
                                      uint32_t addr, const uint8_t *data, uint16_t len)
{
    /* Validate: len ≤ 256, write must not cross page boundary */
    if ((len > W25Q128_PAGE_SIZE) || (((addr & 0xFFU) + (uint32_t)len) > W25Q128_PAGE_SIZE)) {
        return W25Q128_ERROR_SPI;
    }

    uint8_t cmd[4] = {
        CMD_PAGE_PROG,
        (uint8_t)((addr >> 16U) & 0xFFU),
        (uint8_t)((addr >>  8U) & 0xFFU),
        (uint8_t)( addr         & 0xFFU),
    };

    w25q128_status_t st = w25q128_write_enable(hspi);
    if (st != W25Q128_OK) {
        return st;
    }

    CS_LOW();
    if (spi_tx(hspi, cmd, 4U) != W25Q128_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    /* data is const; HAL_SPI_Transmit takes uint8_t* — cast away const (transmit only) */
    if (HAL_SPI_Transmit(hspi, (uint8_t *)(uintptr_t)data, len, W25Q128_SPI_TIMEOUT) != HAL_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    CS_HIGH();

    return w25q128_wait_busy(hspi, W25Q128_TIMEOUT_PAGE_WRITE);
}

w25q128_status_t w25q128_erase_sector(SPI_HandleTypeDef *hspi, uint32_t addr)
{
    /* Address must be 4KB-aligned */
    if ((addr & 0xFFFU) != 0U) {
        return W25Q128_ERROR_SPI;
    }

    uint8_t cmd[4] = {
        CMD_SECTOR_ERASE,
        (uint8_t)((addr >> 16U) & 0xFFU),
        (uint8_t)((addr >>  8U) & 0xFFU),
        (uint8_t)( addr         & 0xFFU),
    };

    w25q128_status_t st = w25q128_write_enable(hspi);
    if (st != W25Q128_OK) {
        return st;
    }

    CS_LOW();
    if (spi_tx(hspi, cmd, 4U) != W25Q128_OK) {
        CS_HIGH();
        return W25Q128_ERROR_SPI;
    }
    CS_HIGH();

    return w25q128_wait_busy(hspi, W25Q128_TIMEOUT_SECTOR_ERASE);
}

w25q128_status_t w25q128_diagnostic(SPI_HandleTypeDef *hspi, w25q128_diag_t *result)
{
    memset(result, 0, sizeof(*result));

    /* 1. JEDEC ID */
    w25q128_status_t st = w25q128_read_jedec_id(hspi,
        &result->manufacturer, &result->memory_type, &result->capacity);
    if (st == W25Q128_OK) {
        result->id_ok = (result->manufacturer == W25Q128_JEDEC_MFR) &&
                        (result->memory_type  == W25Q128_JEDEC_TYPE) &&
                        (result->capacity     == W25Q128_JEDEC_CAP);
    }

    if (!result->id_ok) {
        return W25Q128_ERROR_ID;
    }

    /* 2. Erase sector 0 */
    result->erase_ok = (w25q128_erase_sector(hspi, 0x000000UL) == W25Q128_OK);

    /* 3. Write page 0 with pattern[i] = i */
    uint8_t pattern[W25Q128_PAGE_SIZE];
    for (uint16_t i = 0U; i < W25Q128_PAGE_SIZE; i++) {
        pattern[i] = (uint8_t)i;
    }
    result->write_ok = (w25q128_write_page(hspi, 0x000000UL, pattern, W25Q128_PAGE_SIZE) == W25Q128_OK);

    /* 4. Read back and verify */
    uint8_t readback[W25Q128_PAGE_SIZE];
    if (w25q128_read(hspi, 0x000000UL, readback, W25Q128_PAGE_SIZE) == W25Q128_OK) {
        result->verify_ok = (memcmp(pattern, readback, W25Q128_PAGE_SIZE) == 0);
    }

    return W25Q128_OK;
}

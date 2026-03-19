#ifndef W25Q128_H
#define W25Q128_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── JEDEC identity (W25Q128JV) ── */
#define W25Q128_JEDEC_MFR   0xEFU   /* Winbond */
#define W25Q128_JEDEC_TYPE  0x40U   /* SPI NOR */
#define W25Q128_JEDEC_CAP   0x18U   /* 128 Mbit */
#define W25Q128_SIZE_BYTES  (16UL * 1024UL * 1024UL)

/* ── Geometry ── */
#define W25Q128_PAGE_SIZE     256U
#define W25Q128_SECTOR_SIZE   4096U

/* ── Timeouts (ms) ── */
#define W25Q128_TIMEOUT_SECTOR_ERASE  400U
#define W25Q128_TIMEOUT_PAGE_WRITE      5U
#define W25Q128_SPI_TIMEOUT            HAL_MAX_DELAY//10U

typedef enum {
    W25Q128_OK = 0,
    W25Q128_ERROR_SPI,
    W25Q128_ERROR_TIMEOUT,
    W25Q128_ERROR_ID,
} w25q128_status_t;

typedef struct {
    bool    id_ok;        /* JEDEC matched EF 40 18 */
    uint8_t manufacturer; /* Raw JEDEC byte 0 */
    uint8_t memory_type;  /* Raw JEDEC byte 1 */
    uint8_t capacity;     /* Raw JEDEC byte 2 */
    bool    erase_ok;     /* Sector 0 erase succeeded */
    bool    write_ok;     /* Page 0 write succeeded */
    bool    verify_ok;    /* Read-back matched written pattern */
} w25q128_diag_t;

w25q128_status_t w25q128_init        (SPI_HandleTypeDef *hspi);
w25q128_status_t w25q128_read_jedec_id(SPI_HandleTypeDef *hspi,
                                        uint8_t *mfr, uint8_t *type, uint8_t *cap);
w25q128_status_t w25q128_read        (SPI_HandleTypeDef *hspi,
                                        uint32_t addr, uint8_t *buf, uint32_t len);
w25q128_status_t w25q128_write_page  (SPI_HandleTypeDef *hspi,
                                        uint32_t addr, const uint8_t *data, uint16_t len);
w25q128_status_t w25q128_erase_sector(SPI_HandleTypeDef *hspi, uint32_t addr);
w25q128_status_t w25q128_diagnostic  (SPI_HandleTypeDef *hspi, w25q128_diag_t *result);

#endif /* W25Q128_H */

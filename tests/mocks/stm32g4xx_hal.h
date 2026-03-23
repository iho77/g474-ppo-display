/**
 * @file stm32g4xx_hal.h
 * @brief Forwarding stub — redirects all HAL includes to mock_stm32.h
 *
 * Placed in tests/ so that -I. makes this file shadow the real HAL header
 * when compiling production .c files for the native test host.
 */
#pragma once
#include "mock_stm32.h"

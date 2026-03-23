# =============================================================================
# g474_port Unit Test Makefile
# Native x86 GCC build — no ARM cross-compiler needed
#
# Prerequisites:
#   Windows: Install MSYS2 (https://www.msys2.org/) then:
#     pacman -S mingw-w64-ucrt-x86_64-gcc
#   Or via winget:
#     winget install MSYS2.MSYS2
#   Then open "UCRT64" or "MINGW64" terminal and run make from there.
#
# Usage:
#   make          — build test binary
#   make test     — build and run all tests
#   make clean    — remove built artifacts
# =============================================================================

# On this machine, native GCC runs via WSL (Debian 12, GCC 12).
# Run make via STM32CubeIDE's bundled make.exe:
#   "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.100.202601091506/tools/bin/make.exe" test
# Or from an MSYS2/MinGW terminal: set CC=gcc and run make normally.
CC      = wsl gcc
CFLAGS  = -Wall -Werror -Wextra -std=c11 -g -O0 -DUNIT_TEST \
          -I../STM32CubeIDE/Application/User/Core \
          -I../Core/Inc \
          -Imocks \
          -I. \
          -Wno-unused-parameter \
          -Wno-unused-function

# Production source directory
SRC_DIR = ../STM32CubeIDE/Application/User/Core

# Test source files
TEST_SRCS = \
    test_runner.c \
    test_calibration.c \
    test_drift_tracker.c \
    test_flash_utils.c \
    test_warning.c \
    test_settings.c \
    test_pressure_sensor.c \
    test_ms5837.c

# Mock support file
MOCK_SRCS = \
    mocks/mock_sensor_data.c \
    mocks/stubs.c

# Production modules under test (pure-logic, no HAL side-effects)
PROD_SRCS = \
    $(SRC_DIR)/calibration.c \
    $(SRC_DIR)/drift_tracker.c \
    $(SRC_DIR)/flash_utils.c \
    $(SRC_DIR)/warning.c \
    $(SRC_DIR)/settings_storage.c \
    $(SRC_DIR)/pressure_sensor.c \
    $(SRC_DIR)/ms5837.c \
    $(SRC_DIR)/sensor.c

# NOTE: sensor_diagnostics.c excluded — calls flash_calibration_iterate()
#       which is SPI/HAL-dependent (no pure-logic path to test).

ALL_SRCS = $(TEST_SRCS) $(MOCK_SRCS) $(PROD_SRCS)

TARGET = test_runner

# ── Default target ─────────────────────────────────────────────────────────
$(TARGET): $(ALL_SRCS)
	$(CC) $(CFLAGS) $(ALL_SRCS) -o $(TARGET) -lm

# ── Run tests ───────────────────────────────────────────────────────────────
# The binary is a Linux ELF compiled by WSL GCC — invoke it through WSL.
test: $(TARGET)
	wsl ./$(TARGET)

# ── Clean ───────────────────────────────────────────────────────────────────
clean:
	rm -f $(TARGET) $(TARGET).exe *.o

.PHONY: test clean

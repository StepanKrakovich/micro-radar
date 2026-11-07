#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <algorithm>
#include "LGFX.h"

class RGBDisplay
{
    // Align the app canvas with the panel's visible area.
    static constexpr int VerticalImageOffset = 0;

    static constexpr uint8_t ExpanderAddress = 0x20;
    static constexpr uint8_t ExpanderOutputRegister = 0x01;
    static constexpr uint8_t ExpanderConfigRegister = 0x03;
    static constexpr uint8_t LcdResetBit = 0;
    static constexpr uint8_t LcdChipSelectBit = 2;
    static constexpr gpio_num_t BacklightPin = GPIO_NUM_6;

    spi_device_handle_t _commandDevice = nullptr;
    esp_lcd_panel_handle_t _panel = nullptr;
    uint16_t* _presentBuffer = nullptr;

    static bool writeExpanderRegister(uint8_t reg, uint8_t value)
    {
        Wire.beginTransmission(ExpanderAddress);
        Wire.write(reg);
        Wire.write(value);
        return Wire.endTransmission() == 0;
    }

    static bool readExpanderRegister(uint8_t reg, uint8_t& value)
    {
        Wire.beginTransmission(ExpanderAddress);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0 || Wire.requestFrom(ExpanderAddress, 1) != 1) {
            return false;
        }
        value = Wire.read();
        return true;
    }

    bool writePanelByte(bool isData, uint8_t value)
    {
        spi_transaction_t transaction = {};
        transaction.cmd = isData ? 1 : 0;
        transaction.addr = value;
        const esp_err_t result = spi_device_transmit(_commandDevice, &transaction);
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: ST7701 SPI transfer failed: %s\n", esp_err_to_name(result));
            return false;
        }
        return true;
    }

    bool sendPanelCommands()
    {
        // ST7701's board-specific setup: each command is followed by its data-byte count and payload.
        static constexpr uint8_t commands[] = {
            0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
            0xEF, 1, 0x08,
            0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
            0xC0, 2, 0x3B, 0x00,
            0xC1, 2, 0x10, 0x0C,
            0xC2, 2, 0x07, 0x0A,
            0xC7, 1, 0x00,
            0xCC, 1, 0x10,
            0xCD, 1, 0x08,
            0xB0, 16, 0x05, 0x12, 0x98, 0x0E, 0x0F, 0x07, 0x07, 0x09,
                     0x09, 0x23, 0x05, 0x52, 0x0F, 0x67, 0x2C, 0x11,
            0xB1, 16, 0x0B, 0x11, 0x97, 0x0C, 0x12, 0x06, 0x06, 0x08,
                     0x08, 0x22, 0x03, 0x51, 0x11, 0x66, 0x2B, 0x0F,
            0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
            0xB0, 1, 0x5D,
            0xB1, 1, 0x3E,
            0xB2, 1, 0x81,
            0xB3, 1, 0x80,
            0xB5, 1, 0x4E,
            0xB7, 1, 0x85,
            0xB8, 1, 0x20,
            0xC1, 1, 0x78,
            0xC2, 1, 0x78,
            0xD0, 1, 0x88,
            0xE0, 3, 0x00, 0x00, 0x02,
            0xE1, 11, 0x06, 0x30, 0x08, 0x30, 0x05, 0x30,
                      0x07, 0x30, 0x00, 0x33, 0x33,
            0xE2, 12, 0x11, 0x11, 0x33, 0x33, 0xF4, 0x00,
                      0x00, 0x00, 0xF4, 0x00, 0x00, 0x00,
            0xE3, 4, 0x00, 0x00, 0x11, 0x11,
            0xE4, 2, 0x44, 0x44,
            0xE5, 16, 0x0D, 0xF5, 0x30, 0xF0, 0x0F, 0xF7, 0x30, 0xF0,
                      0x09, 0xF1, 0x30, 0xF0, 0x0B, 0xF3, 0x30, 0xF0,
            0xE6, 4, 0x00, 0x00, 0x11, 0x11,
            0xE7, 2, 0x44, 0x44,
            0xE8, 16, 0x0C, 0xF4, 0x30, 0xF0, 0x0E, 0xF6, 0x30, 0xF0,
                      0x08, 0xF0, 0x30, 0xF0, 0x0A, 0xF2, 0x30, 0xF0,
            0xE9, 2, 0x36, 0x01,
            0xEB, 7, 0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40,
            0xED, 16, 0xFF, 0x10, 0xAF, 0x76, 0x54, 0x2B, 0xCF, 0xFF,
                      0xFF, 0xFC, 0xB2, 0x45, 0x67, 0xFA, 0x01, 0xFF,
            0xEF, 6, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54,
            0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
            0x11, 0x80, 120,
            0x3A, 1, 0x66,
            0x36, 1, 0x00,
            0x35, 1, 0x00,
            0x29, 0,
            0xFF, 0xFF,
        };

        size_t index = 0;
        while (index + 1 < sizeof(commands)) {
            const uint8_t command = commands[index++];
            const uint8_t dataLength = commands[index++];
            if (command == 0xFF && dataLength == 0xFF) {
                break;
            }

            if (!writePanelByte(false, command)) {
                return false;
            }
            if (dataLength == 0x80) {
                delay(commands[index++]);
                continue;
            }

            for (uint8_t i = 0; i < dataLength; ++i) {
                if (!writePanelByte(true, commands[index++])) {
                    return false;
                }
            }
        }
        return true;
    }

    bool initializeST7701()
    {
        uint8_t outputs = 0;
        if (!writeExpanderRegister(ExpanderOutputRegister, outputs)) {
            Serial.println("[display] ERROR: LCD reset assertion failed");
            return false;
        }
        delay(10);

        outputs |= static_cast<uint8_t>(1U << LcdResetBit);
        if (!writeExpanderRegister(ExpanderOutputRegister, outputs)) {
            Serial.println("[display] ERROR: LCD reset release failed");
            return false;
        }
        delay(50);

        outputs &= static_cast<uint8_t>(~(1U << LcdChipSelectBit));
        if (!writeExpanderRegister(ExpanderOutputRegister, outputs)) {
            Serial.println("[display] ERROR: LCD chip-select assertion failed");
            return false;
        }
        delay(10);

        spi_bus_config_t busConfig = {};
        busConfig.mosi_io_num = 1;
        busConfig.miso_io_num = -1;
        busConfig.sclk_io_num = 2;
        busConfig.quadwp_io_num = -1;
        busConfig.quadhd_io_num = -1;
        busConfig.max_transfer_sz = 64;

        esp_err_t result = spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO);
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: ST7701 SPI bus initialization failed: %s\n", esp_err_to_name(result));
            return false;
        }

        spi_device_interface_config_t deviceConfig = {};
        deviceConfig.command_bits = 1;
        deviceConfig.address_bits = 8;
        deviceConfig.mode = SPI_MODE0;
        deviceConfig.clock_speed_hz = 40'000'000;
        deviceConfig.spics_io_num = -1;
        deviceConfig.queue_size = 1;
        result = spi_bus_add_device(SPI2_HOST, &deviceConfig, &_commandDevice);
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: ST7701 SPI device initialization failed: %s\n", esp_err_to_name(result));
            return false;
        }

        if (!sendPanelCommands()) {
            return false;
        }

        outputs |= static_cast<uint8_t>(1U << LcdChipSelectBit);
        if (!writeExpanderRegister(ExpanderOutputRegister, outputs)) {
            Serial.println("[display] ERROR: LCD chip-select deassertion failed");
            return false;
        }
        delay(10);
        return true;
    }

    esp_err_t createRGBPanel()
    {
        esp_lcd_rgb_panel_config_t config = {};
        config.clk_src = LCD_CLK_SRC_PLL240M;
        config.timings.pclk_hz = 18'000'000;
        config.timings.h_res = DisplayConfig::Width;
        config.timings.v_res = DisplayConfig::Height;
        config.timings.hsync_pulse_width = 8;
        config.timings.hsync_back_porch = 10;
        config.timings.hsync_front_porch = 50;
        config.timings.vsync_pulse_width = 2;
        config.timings.vsync_back_porch = 18;
        config.timings.vsync_front_porch = 8;
        config.timings.flags.pclk_active_neg = 0;
        config.data_width = 16;
        config.bits_per_pixel = 16;
        config.num_fbs = 1;
        config.bounce_buffer_size_px = DisplayConfig::Width * 20;
        config.dma_burst_size = 64;
        config.hsync_gpio_num = 38;
        config.vsync_gpio_num = 39;
        config.de_gpio_num = 40;
        config.pclk_gpio_num = 41;
        config.disp_gpio_num = -1;
        const int dataPins[16] = {5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17};
        for (size_t i = 0; i < 16; ++i) {
            config.data_gpio_nums[i] = dataPins[i];
        }
        config.flags.fb_in_psram = 1;

        return esp_lcd_new_rgb_panel(&config, &_panel);
    }

public:
    bool initBoardDisplay()
    {
        Serial.println("[display] Starting ESP-IDF RGB panel initialization");
        if (!Wire.begin(15, 7)) {
            Serial.println("[display] ERROR: I2C initialization failed (SDA=15, SCL=7)");
            return false;
        }
        if (!writeExpanderRegister(ExpanderConfigRegister, 0x00)) {
            Serial.println("[display] ERROR: TCA9554 at 0x20 did not acknowledge configuration");
            return false;
        }
        if (!writeExpanderRegister(ExpanderOutputRegister, 0x00)) {
            Serial.println("[display] ERROR: TCA9554 output register write failed");
            return false;
        }

        uint8_t expanderOutputs = 0;
        if (readExpanderRegister(ExpanderOutputRegister, expanderOutputs)) {
            Serial.printf("[display] TCA9554 output register: 0x%02X\n", expanderOutputs);
        } else {
            Serial.println("[display] WARNING: TCA9554 output register read failed");
        }

        if (!initializeST7701()) {
            return false;
        }

        esp_err_t result = createRGBPanel();
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: RGB panel creation failed: %s\n", esp_err_to_name(result));
            return false;
        }
        constexpr size_t pixelCount = DisplayConfig::Width * DisplayConfig::Height;
        _presentBuffer = static_cast<uint16_t*>(
            heap_caps_malloc(pixelCount * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (_presentBuffer == nullptr) {
            Serial.println("[display] ERROR: Could not allocate the RGB565 presentation buffer");
            return false;
        }

        result = esp_lcd_panel_reset(_panel);
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: RGB panel reset failed: %s\n", esp_err_to_name(result));
            return false;
        }
        result = esp_lcd_panel_init(_panel);
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: RGB panel initialization failed: %s\n", esp_err_to_name(result));
            return false;
        }
        if (!ledcAttach(BacklightPin, 20'000, 10)
            || !ledcWrite(BacklightPin, 1024)) {
            Serial.println("[display] ERROR: Backlight PWM initialization failed");
            return false;
        }

        Serial.printf("[display] ESP-IDF ST7701/RGB init: OK (%dx%d)\n",
            DisplayConfig::Width, DisplayConfig::Height);
        Serial.printf("[display] PSRAM: %s, free: %u bytes\n",
            psramFound() ? "found" : "not found",
            static_cast<unsigned int>(ESP.getFreePsram()));
        return true;
    }

    bool present(const void* rgb565Frame)
    {
        const auto* source = static_cast<const uint16_t*>(rgb565Frame);
        std::fill_n(_presentBuffer, DisplayConfig::Width * DisplayConfig::Height, 0);
        for (int sourceY = 0; sourceY < DisplayConfig::Height; ++sourceY) {
            const int targetY = sourceY + VerticalImageOffset;
            if (targetY < 0 || targetY >= DisplayConfig::Height) {
                continue;
            }

            const size_t rowOffset = static_cast<size_t>(targetY) * DisplayConfig::Width;
            const size_t sourceRowOffset = static_cast<size_t>(sourceY) * DisplayConfig::Width;
            for (int x = 0; x < DisplayConfig::Width; ++x) {
                const uint16_t pixel = source[sourceRowOffset + x];
                // The RGB panel bus expects the two RGB565 bytes in the opposite order.
                _presentBuffer[rowOffset + x] = static_cast<uint16_t>((pixel << 8) | (pixel >> 8));
            }
        }

        const esp_err_t result = esp_lcd_panel_draw_bitmap(
            _panel, 0, 0, DisplayConfig::Width, DisplayConfig::Height, _presentBuffer);
        if (result != ESP_OK) {
            Serial.printf("[display] ERROR: Frame transfer failed: %s\n", esp_err_to_name(result));
            return false;
        }
        return true;
    }
};

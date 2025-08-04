#pragma once

#include <esp_err.h>
#include <driver/gpio.h>
#include <lvgl.h>
#include "esp_lvgl_port.h"

class lhs154kc
{
public:
    static lhs154kc &instance()
    {
        static lhs154kc _instance;
        return _instance;
    }

    void operator=(lhs154kc const &) = delete;
    lhs154kc(lhs154kc const &) = delete;

public:
    typedef struct {
        uint8_t reg;
        uint8_t data[16];
        uint8_t len;
    } seq_t;

private:
    lhs154kc() = default;

public:
    esp_err_t init();
    static void set_backlight(bool on);
    esp_err_t send_sequence(const lhs154kc::seq_t *seq, size_t seq_cnt);
    static esp_err_t lock(uint32_t timeout_ms = 0);
    static void unlock();

private:
    lv_display_t *display = nullptr;
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel_handle = nullptr;

private:
    static const constexpr gpio_num_t DISP_IO_CS = GPIO_NUM_7;
    static const constexpr gpio_num_t DISP_IO_SCLK = GPIO_NUM_6;
    static const constexpr gpio_num_t DISP_IO_DATA = GPIO_NUM_5;
    static const constexpr gpio_num_t DISP_IO_RST = GPIO_NUM_4;
    static const constexpr gpio_num_t DISP_IO_DC = GPIO_NUM_15;
    static const constexpr gpio_num_t DISP_IO_BL = GPIO_NUM_16;
    static const constexpr spi_host_device_t DISP_SPI_HOST = SPI2_HOST;
    static const constexpr char TAG[] = "lhs154kc";
    static const constexpr lvgl_port_cfg_t LVGL_CFG = {
            .task_priority = 4,         /* LVGL task priority */
            .task_stack = 8192,         /* LVGL task stack size */
            .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
            .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
            .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };

    static const constexpr lhs154kc::seq_t INIT_SEQ[] = {
            {0x36, {0x00}, 1},
            {0x3A, {0x05}, 1},
            {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},
            {0xB7, {0x35}, 1},
            {0xBB, {0x32}, 1}, // VCOM for LHS154KC is 1.35v!!!
            {0xC2, {0x01}, 1},
            {0xC3, {0x15}, 1}, // GVDD for LHS154KC is 4.8v!!
            {0xC4, {0x20}, 1},
            {0xC6, {0x0F}, 1},
            {0xD0, {0xA4, 0xA1}, 2},
            {0xE0, {
                    0xD0,0x08,0x0E,0x09,
                    0x09,0x05,0x31,0x33,
                    0x48,0x17,0x14,0x15,
                    0x31,0x34,}, 14},
            {0xE1, {
                       0xD0,0x08,0x0E,0x09,
                       0x09,0x15,0x31,0x33,
                       0x48,0x17,0x14,0x15,
                       0x31,0x34}, 14},
            {0x21, {}, 0},
            {0x29, {}, 0},
    };
};

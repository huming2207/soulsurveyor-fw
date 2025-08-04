#include <esp_log.h>
#include <esp_lcd_panel_st7789.h>
#include "lhs154kc.hpp"
#include "esp_lvgl_port.h"

esp_err_t lhs154kc::init()
{
    gpio_config_t pwr_io_cfg = {
            .pin_bit_mask = (1ULL << DISP_IO_BL),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
    };

    auto ret = gpio_config(&pwr_io_cfg);
    ret = ret ?: gpio_set_level((gpio_num_t)DISP_IO_BL, 0);
    ret = ret ?: gpio_reset_pin((gpio_num_t)DISP_IO_CS);
    ret = ret ?: gpio_reset_pin((gpio_num_t)DISP_IO_DC);
    ret = ret ?: gpio_reset_pin((gpio_num_t)DISP_IO_DATA);
    ret = ret ?: gpio_reset_pin((gpio_num_t)DISP_IO_RST);
    ret = ret ?: gpio_reset_pin((gpio_num_t)DISP_IO_SCLK);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO setup failed: 0x%x", ret);
        return ret;
    }

    spi_bus_config_t spi_bus_cfg = {};
    spi_bus_cfg.sclk_io_num = DISP_IO_SCLK;
    spi_bus_cfg.mosi_io_num = DISP_IO_DATA;
    spi_bus_cfg.miso_io_num = GPIO_NUM_NC;
    spi_bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    spi_bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    ret = spi_bus_initialize(DISP_SPI_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI Bus init failed: 0x%x", ret);
        return ret;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num = DISP_IO_DC;
    io_cfg.cs_gpio_num = DISP_IO_CS;

    io_cfg.pclk_hz = SPI_MASTER_FREQ_26M;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.spi_mode = 0;
    io_cfg.trans_queue_depth = 10;
    io_cfg.user_ctx = this;
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DISP_SPI_HOST, &io_cfg, &panel_io);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI dev init failed: 0x%x", ret);
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = DISP_IO_RST;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.bits_per_pixel = 16;
    ret = esp_lcd_new_panel_st7789(panel_io, &panel_cfg, &panel_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel create failed: 0x%x", ret);
        return ret;
    }

    set_backlight(false);
    ret = ret ?: esp_lcd_panel_reset(panel_handle);
    ret = ret ?: esp_lcd_panel_init(panel_handle);
    ret = ret ?: esp_lcd_panel_swap_xy(panel_handle, false);
    ret = ret ?: send_sequence(INIT_SEQ, sizeof(INIT_SEQ) / sizeof(lhs154kc::seq_t));
    ret = ret ?: esp_lcd_panel_disp_on_off(panel_handle, true);
    ret = ret ?: esp_lcd_panel_invert_color(panel_handle, true);
    set_backlight(true);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP LCD init failed, 0x%x %s", ret, esp_err_to_name(ret));
        return ret;
    }


    ret = lvgl_port_init(&LVGL_CFG);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP LVGL hack init failed, 0x%x %s", ret, esp_err_to_name(ret));
        return ret;
    }

    lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = panel_io,
            .panel_handle = panel_handle,
            .control_handle = nullptr,
            .buffer_size = 240 * 240,
            .double_buffer = true,
            .trans_size = 0,
            .hres = 240,
            .vres = 240,
            .monochrome = false,
            .rotation = {
                    .swap_xy = false,
                    .mirror_x = false,
                    .mirror_y = false,
            },
            .color_format = LV_COLOR_FORMAT_RGB565,

            .flags = {
                    .buff_dma = true,
                    .buff_spiram = true,
                    .sw_rotate = false,
                    .swap_bytes = true,
                    .full_refresh = false,
                    .direct_mode = false,
            }
    };

    display = lvgl_port_add_disp(&disp_cfg);
    if (display == nullptr) {
        ESP_LOGE(TAG, "Can't add display!");
        return ESP_ERR_INVALID_STATE;
    }

    return ret;
}

void lhs154kc::set_backlight(bool on)
{
    gpio_set_level(DISP_IO_BL, on ? 1 : 0);
}

esp_err_t lhs154kc::send_sequence(const lhs154kc::seq_t *seq, size_t seq_cnt)
{
    esp_err_t ret = ESP_OK;

    for (size_t idx = 0; idx < seq_cnt; idx += 1) {
        ret = ret ?: esp_lcd_panel_io_tx_param(panel_io, seq[idx].reg, (seq[idx].len > 0 ? seq[idx].data : nullptr), seq[idx].len);
    }

    return ret;
}

esp_err_t lhs154kc::lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms) ? ESP_OK : ESP_FAIL;
}

void lhs154kc::unlock()
{
    return lvgl_port_unlock();
}

#include <cstring>
#include <string>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include "uart_handler.hpp"

#include <algorithm>

#include "esp_lvgl_port.h"

esp_err_t uart_handler::init()
{
    gpio_reset_pin(GPIO_NUM_9);

    uart_config_t uart_config = {};
    uart_config.baud_rate = 921600;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_APB;

    auto ret = uart_driver_install(port, 10240, 512, 32, &uart_evt_queue, 0);
    ret = ret ?: uart_param_config(port, &uart_config);
    ret = ret ?: uart_set_pin(port, GPIO_NUM_NC, GPIO_NUM_9, GPIO_NUM_NC, GPIO_NUM_NC);
    ret = ret ?: uart_enable_pattern_det_baud_intr(port, '>', 3, 9, 0, 0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART setup failed");
        return ret;
    }

    lvgl_port_lock(pdMS_TO_TICKS(100));

    if (base_obj != nullptr) {
        lv_obj_delete(base_obj);
    }

    base_obj = lv_obj_create(lv_scr_act());
    if (base_obj == nullptr) {
        ESP_LOGE(TAG, "Can't draw base object!!");
        return ESP_ERR_INVALID_STATE;
    }

    lv_obj_set_size(base_obj, 240, 240);
    lv_obj_set_scrollbar_mode(base_obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(base_obj, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(base_obj, 0, 0);
    lv_obj_set_style_border_width(base_obj, 0, 0);
    lv_obj_set_style_radius(base_obj, 0, 0);
    lv_obj_set_style_pad_all(base_obj, 0, 0);
    lv_obj_set_style_outline_width(base_obj, 0, 0);

    lvgl_port_unlock();

    xTaskCreateWithCaps(uart_evt_task_func, "uart_rx_task", 32768, this, 6, nullptr, MALLOC_CAP_SPIRAM);
    return ret;
}

void uart_handler::uart_evt_task_func(void *_ctx)
{
    auto *ctx = static_cast<uart_handler *>(_ctx);
    uint8_t rx_buf[16384] = { 0 };
    while (true) {
        uart_event_t event = {};
        if (xQueueReceive(ctx->uart_evt_queue, &event, portMAX_DELAY) != pdTRUE) {
            vTaskDelay(1);
            continue;
        }

        switch (event.type) {
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
            case UART_PARITY_ERR:
            case UART_FRAME_ERR: {
                uart_flush(ctx->port);
                xQueueReset(ctx->uart_evt_queue);
                break;
            }

            case UART_PATTERN_DET: {
                int pos = uart_pattern_pop_pos(ctx->port);
                if (pos == -1 || pos > sizeof(rx_buf)) {
                    // Invalid pattern position
                    uart_flush_input(ctx->port);
                    ESP_LOGE(TAG, "uart_read: got garbage data? pos=%d", pos);
                    break;
                }

                int preamble_len = pos + 3;
                int read_len = uart_read_bytes(ctx->port, rx_buf, preamble_len, pdMS_TO_TICKS(100));

                if (read_len != preamble_len) {
                    ESP_LOGW(TAG, "Unexpected preamble read length");
                    uart_flush_input(ctx->port);
                    break;
                }

                memset(rx_buf, 0, sizeof(rx_buf));

                int payload_len = 0;
                while (payload_len < sizeof(rx_buf) - 1) {
                    uint8_t byte = 0;
                    int len = uart_read_bytes(ctx->port, &byte, 1, pdMS_TO_TICKS(50));
                    if (len > 0) {
                        if (byte == '\r') break;
                        rx_buf[payload_len++] = byte;
                    } else {
                        break; // Timeout
                    }
                }

                rx_buf[payload_len] = '\0';
                ESP_LOGI(TAG, "Got: %s len=%u", rx_buf, payload_len);
                ctx->handle_uart_read(rx_buf, payload_len);

                memset(rx_buf, 0, sizeof(rx_buf));
                break;
            }

            default: {
                break;
            }

        }
    }
}

void uart_handler::handle_uart_read(uint8_t *buf, const size_t len)
{
    ESP_LOGI(TAG, "uart_read: len=%u", len);
    auto uart_str = std::string((char *)buf, len);

    if (strstr((const char *)buf, "TIMEOUT") != nullptr) {
        screen_draw_error();
        uart_flush(port);
        return;
    }

    if (strstr((const char *)buf, "CLEAR") != nullptr) {
        // screen_clear(); // Doesn't work, don't clear
        return;
    }

    size_t comma_pos = uart_str.find(',');
    if (comma_pos == std::string_view::npos) {
        screen_draw_error();
    }

    std::string rssi = uart_str.substr(0, comma_pos);
    std::erase(rssi, '>');
    std::string snr = uart_str.substr(comma_pos + 1);
    std::erase(snr, '>');

    screen_draw_readings(rssi, snr);
}

void uart_handler::screen_draw_error()
{
    if (!lvgl_port_lock(pdMS_TO_TICKS(1000))) {
        return;
    }
    if (base_obj != nullptr) {
        lv_obj_delete(base_obj);
        base_obj = nullptr;
    }

    base_obj = lv_obj_create(lv_scr_act());
    if (base_obj == nullptr) {
        ESP_LOGE(TAG, "Can't draw base object!!");
        return;
    }

    lv_obj_set_size(base_obj, 240, 240);
    lv_obj_set_scrollbar_mode(base_obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(base_obj, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(base_obj, 0, 0);
    lv_obj_set_style_border_width(base_obj, 0, 0);
    lv_obj_set_style_radius(base_obj, 0, 0);
    lv_obj_set_style_pad_all(base_obj, 0, 0);
    lv_obj_set_style_outline_width(base_obj, 0, 0);

    lv_obj_t *obj = lv_label_create(base_obj);
    lv_obj_set_pos(obj, LV_PCT(15), LV_PCT(30));
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(obj, "ERROR!!!");

    lvgl_port_unlock();
}

void uart_handler::screen_draw_readings(const std::string &rssi, const std::string &snr)
{
    if (!lvgl_port_lock(pdMS_TO_TICKS(1000))) {
        return;
    }

    if (base_obj != nullptr) {
        lv_obj_delete(base_obj);
        base_obj = nullptr;
    }

    base_obj = lv_obj_create(lv_scr_act());
    if (base_obj == nullptr) {
        ESP_LOGE(TAG, "Can't draw base object!!");
        return;
    }

    lv_obj_set_size(base_obj, 240, 240);
    lv_obj_set_scrollbar_mode(base_obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(base_obj, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(base_obj, 0, 0);
    lv_obj_set_style_border_width(base_obj, 0, 0);
    lv_obj_set_style_radius(base_obj, 0, 0);
    lv_obj_set_style_pad_all(base_obj, 0, 0);
    lv_obj_set_style_outline_width(base_obj, 0, 0);


    lv_obj_t *rssi_obj = lv_label_create(base_obj);
    lv_obj_set_pos(rssi_obj, LV_PCT(3), LV_PCT(10));
    lv_obj_set_size(rssi_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(rssi_obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_fmt(rssi_obj, "RSSI: %s", rssi.c_str());

    lv_obj_t *snr_obj = lv_label_create(base_obj);
    lv_obj_set_pos(snr_obj, LV_PCT(3), LV_PCT(30));
    lv_obj_set_size(snr_obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(snr_obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text_fmt(snr_obj, "SNR: %s", snr.c_str());

    lvgl_port_unlock();
}

void uart_handler::screen_clear()
{
    if (!lvgl_port_lock(pdMS_TO_TICKS(1000))) {
        return;
    }

    if (base_obj != nullptr) {
        lv_obj_delete(base_obj);
        base_obj = nullptr;
    }

    lvgl_port_unlock();
}

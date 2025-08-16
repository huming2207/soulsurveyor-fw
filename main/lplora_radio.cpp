#include <esp_err.h>
#include <esp_log.h>
#include <driver/uart.h>
#include "lplora_radio.hpp"

void lplora_radio::uart_evt_task_func(void *_ctx)
{
    uint8_t rx_buf[8192] = { 0 };
    size_t rx_idx = 0;

    auto *ctx = static_cast<lplora_radio *>(_ctx);
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

            case UART_DATA: {
                uint8_t rx_byte = 0;
                int read_len = uart_read_bytes(ctx->port, &rx_byte, 1, pdMS_TO_TICKS(200));

                if (read_len < 1) {
                    break;
                }

                switch (rx_byte) {
                    case SLIP_START: {
                        rx_idx = 0;
                        memset(rx_buf, 0, sizeof(rx_buf));
                        break;
                    }

                    case SLIP_END: {
                        if (rx_idx < 1) {
                            break;
                        }

                        if (xRingbufferSend(ctx->rx_rb, rx_buf, rx_idx, pdMS_TO_TICKS(1000)) != pdTRUE) {
                            ESP_LOGW(TAG, "Rx Ringbuffer probably full!");
                        }

                        rx_idx = 0;
                        memset(rx_buf, 0, sizeof(rx_buf));
                        break;
                    }

                    case SLIP_ESC: {
                        read_len = uart_read_bytes(ctx->port, &rx_byte, 1, pdMS_TO_TICKS(200));

                        if (read_len < 1) {
                            ESP_LOGE(TAG, "slip_decode: expecting next SLIP ESC but got nothing");
                            break;
                        }

                        switch (rx_byte) {
                            case SLIP_ESC_ESC: {
                                rx_buf[rx_idx++] = SLIP_ESC;
                                break;
                            }

                            case SLIP_ESC_END: {
                                rx_buf[rx_idx++] = SLIP_END;
                                break;
                            }

                            case SLIP_ESC_START: {
                                rx_buf[rx_idx++] = SLIP_START;
                                break;
                            }

                            default: {
                                ESP_LOGE(TAG, "slip_decode: expecting next SLIP ESC but got 0x%02x", rx_byte);
                                break;
                            }
                        }

                        break;
                    }

                    default: {
                        rx_buf[rx_idx++] = rx_byte;
                        break;
                    }
                }
            }

            default: {
                break;
            }
        }

        vTaskDelay(1);
    }
}

esp_err_t lplora_radio::init()
{
    uart_config_t uart_config = {};
    uart_config.baud_rate = 9600;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_APB;

    auto ret = uart_driver_install(port, 8192, 3072, 16, &uart_evt_queue, 0);
    ret = ret ?: uart_param_config(port, &uart_config);
    ret = ret ?: uart_set_pin(port, tx_pin, rx_pin, GPIO_NUM_NC, GPIO_NUM_NC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART init failed");
        return ret;
    }

    auto task_ret = xTaskCreateWithCaps(uart_evt_task_func, "lplora_radio", 32768, this, 6, &uart_task, MALLOC_CAP_SPIRAM);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "UART task init failed");
        return ESP_ERR_NO_MEM;
    }

    return ret;
}

esp_err_t lplora_radio::send_internal(lplora_radio::uart_packet_header *header, uint8_t *payload, size_t len)
{
    return 0;
}


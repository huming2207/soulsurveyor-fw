#include <esp_err.h>
#include <esp_log.h>
#include <driver/uart.h>
#include "lplora_radio.hpp"
#include "lplora_utils.hpp"

void lplora_radio::uart_evt_task_func(void *_ctx)
{
    uint8_t rx_buf[768] = { 0 };
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
                if (rx_idx >= sizeof(rx_buf)) {
                    ESP_LOGW(TAG, "Rx index overflow!");
                    rx_idx = 0;
                    memset(rx_buf, 0, sizeof(rx_buf));
                }

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
                        if (rx_idx < sizeof(uart_packet_header)) {
                            rx_idx = 0;
                            memset(rx_buf, 0, sizeof(rx_buf));
                            break;
                        }

                        uint16_t expected_crc = ((rx_buf[rx_idx - 1] << 8) | (rx_buf[rx_idx - 2]));
                        uint16_t actual_crc = lplora_utils::calc_crc16(rx_buf, rx_idx - 2);
                        if (expected_crc != actual_crc) {
                            ESP_LOGE(TAG, "Rx: CRC mismatch! 0x%x vs 0x%x", expected_crc, actual_crc);
                        } else {
                            if (xRingbufferSend(ctx->rx_rb, rx_buf, rx_idx, pdMS_TO_TICKS(1000)) != pdTRUE) {
                                ESP_LOGW(TAG, "Rx Ringbuffer probably full!");
                            }
                        }

                        rx_idx = 0;
                        memset(rx_buf, 0, sizeof(rx_buf));
                        break;
                    }

                    case SLIP_ESC: {
                        read_len = uart_read_bytes(ctx->port, &rx_byte, 1, pdMS_TO_TICKS(200));

                        if (read_len < 1) {
                            break; // Start over
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
                                rx_idx = 0;
                                memset(rx_buf, 0, sizeof(rx_buf));
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

esp_err_t lplora_radio::init(uint32_t malloc_cap, size_t task_size)
{
    uart_config_t uart_config = {};
    uart_config.baud_rate = 9600;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_APB;

    auto ret = uart_driver_install(port, 3072, 3072, 16, &uart_evt_queue, 0);
    ret = ret ?: uart_param_config(port, &uart_config);
    ret = ret ?: uart_set_pin(port, tx_pin, rx_pin, GPIO_NUM_NC, GPIO_NUM_NC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART init failed");
        return ret;
    }

    rx_rb = xRingbufferCreateWithCaps(16384, RINGBUF_TYPE_NOSPLIT, malloc_cap);

    if (rx_rb == nullptr) {
        ESP_LOGE(TAG, "Can't create Tx lock");
        return ESP_ERR_NO_MEM;
    }

    tx_lock = xSemaphoreCreateBinary();
    if (tx_lock == nullptr) {
        ESP_LOGE(TAG, "Can't create Tx lock");
        return ESP_ERR_NO_MEM;
    } else {
        xSemaphoreGive(tx_lock);
    }

    auto task_ret = xTaskCreateWithCaps(uart_evt_task_func, "lplora_uart", task_size, this, 6, &uart_task, malloc_cap);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "UART task init failed");
        return ESP_ERR_NO_MEM;
    }

    task_ret = xTaskCreateWithCaps(packet_dispatcher_task_func, "lplora_rxpkt", task_size, this, 3, &uart_task, malloc_cap);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "UART task init failed");
        return ESP_ERR_NO_MEM;
    }


    return ret;
}

esp_err_t lplora_radio::send_packet(lplora_radio::uart_packet_type pkt_type, uint8_t *payload, size_t len, uint32_t wait_ticks)
{
    if (xSemaphoreTake(tx_lock, wait_ticks) != pdTRUE) {
        ESP_LOGE(TAG, "Can't take Tx lock!");
        return ESP_ERR_TIMEOUT;
    }

    int tx_len = uart_write_bytes(port, &SLIP_START, 1);
    if (tx_len < 1) {
        ESP_LOGE(TAG, "Can't send SLIP_START!");
        xSemaphoreGive(tx_lock);
        return ESP_FAIL;
    }

    uint16_t crc = lplora_utils::calc_crc16((uint8_t *)&pkt_type, 1);
    auto ret = slip_tx((uint8_t *)&pkt_type, 1, wait_ticks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't send packet type!");
        xSemaphoreGive(tx_lock);
        return ESP_FAIL;
    }

    uint8_t len_bytes[2] = { (uint8_t)(len & 0xff), (uint8_t)(len >> 8) };
    crc = lplora_utils::calc_crc16(len_bytes, sizeof(len_bytes), crc);
    ret = slip_tx(len_bytes, sizeof(len_bytes), wait_ticks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't send payload length!");
        xSemaphoreGive(tx_lock);
        return ESP_FAIL;
    }

    if (payload != nullptr && len > 0) {
        crc = lplora_utils::calc_crc16(payload, len, crc);
        ret = slip_tx(payload, len, wait_ticks);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Can't send payload!");
            xSemaphoreGive(tx_lock);
            return ESP_FAIL;
        }
    }

    uint8_t crc_bytes[2] = { (uint8_t)(crc & 0xff), (uint8_t)(crc >> 8) };
    ret = slip_tx(crc_bytes, sizeof(crc_bytes), wait_ticks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Can't send CRC!");
        xSemaphoreGive(tx_lock);
        return ESP_FAIL;
    }

    tx_len = uart_write_bytes(port, &SLIP_END, 1);
    if (tx_len < 1) {
        ESP_LOGE(TAG, "Can't send SLIP_END!");
        xSemaphoreGive(tx_lock);
        return ESP_FAIL;
    }

    xSemaphoreGive(tx_lock);
    return uart_wait_tx_done(port, wait_ticks);
}

esp_err_t lplora_radio::slip_tx(uint8_t *buf, size_t len, uint32_t wait_ticks)
{
    if (buf == nullptr || len < 1) {
        return ESP_ERR_INVALID_ARG;
    }

    int tx_len = ESP_OK;
    for (size_t idx = 0; idx < len; idx += 1) {
        switch (buf[idx]) {
            case SLIP_ESC: {
                const uint8_t bytes[2] = { SLIP_ESC, SLIP_ESC_ESC };
                tx_len = uart_write_bytes(port, bytes, sizeof(bytes));
                break;
            }

            case SLIP_END: {
                const uint8_t bytes[2] = { SLIP_ESC, SLIP_ESC_END };
                tx_len = uart_write_bytes(port, bytes, sizeof(bytes));
                break;
            }

            case SLIP_START: {
                const uint8_t bytes[2] = { SLIP_ESC, SLIP_ESC_START };
                tx_len = uart_write_bytes(port, bytes, sizeof(bytes));
                break;
            }

            default: {
                tx_len = uart_write_bytes(port, &buf[idx], 1);
                break;
            }
        }
    }

    esp_err_t ret = tx_len > 0 ? ESP_OK : ESP_FAIL;
    if (wait_ticks > 0) {
        ret = ret ?: uart_wait_tx_done(port, wait_ticks);
    }

    return ret;
}

void lplora_radio::packet_dispatcher_task_func(void *_ctx)
{


}



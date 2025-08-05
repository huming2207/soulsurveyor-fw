#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include "uart_handler.hpp"

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

    auto ret = uart_driver_install(port, 8192, 8192, 32, &uart_evt_queue, 0);
    ret = ret ?: uart_param_config(port, &uart_config);
    ret = ret ?: uart_set_pin(port, GPIO_NUM_NC, GPIO_NUM_9, GPIO_NUM_NC, GPIO_NUM_NC);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART setup failed");
        return ret;
    }

    xTaskCreateWithCaps(uart_evt_task_func, "uart_rx_task", 16384, this, 6, nullptr, MALLOC_CAP_INTERNAL);
    return ret;
}

void uart_handler::uart_evt_task_func(void *_ctx)
{
    auto *ctx = (uart_handler *)_ctx;
    while (true) {
        uart_event_t event = {};
        if (xQueueReceive(ctx->uart_evt_queue, &event, portMAX_DELAY) != pdTRUE) {
            vTaskDelay(1);
            continue;
        }

        switch (event.type) {
            case UART_DATA: {
                // TODO read here
                break;
            }

            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
            case UART_PARITY_ERR:
            case UART_FRAME_ERR: {
                uart_flush(ctx->port);
                break;
            }

            default: {
                break;
            }

        }
    }
}

#pragma once

#include <string>
#include <driver/uart.h>
#include <esp_err.h>

#include "misc/lv_types.h"

class uart_handler
{
public:
    static uart_handler &instance()
    {
        static uart_handler _instance;
        return _instance;
    }

    void operator=(uart_handler const &) = delete;
    uart_handler(uart_handler const &) = delete;

private:
    uart_handler() = default;

public:
    esp_err_t init();
    void handle_uart_read(uint8_t *buf, size_t len);
    void screen_draw_error();
    void screen_draw_readings(const std::string &rssi, const std::string &snr);

private:
    static void uart_evt_task_func(void *_ctx);

    uart_port_t port = UART_NUM_1;
    QueueHandle_t uart_evt_queue = nullptr;
    lv_obj_t *base_obj = nullptr;

private:
    static const constexpr char TAG[] = "uart_handler";

};


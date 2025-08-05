#pragma once

#include <driver/uart.h>
#include <esp_err.h>

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

private:
    static void uart_evt_task_func(void *_ctx);

private:
    uart_port_t port = UART_NUM_1;
    QueueHandle_t uart_evt_queue = nullptr;

private:
    static const constexpr char TAG[] = "uart_handler";

};


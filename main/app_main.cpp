/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <cstdio>
#include "lhs154kc.hpp"
#include "uart_handler.hpp"


extern "C" void app_main(void)
{
    auto &lcd = lhs154kc::instance();

    ESP_ERROR_CHECK(lcd.init());
    uart_handler::instance().init();

    vTaskDelay(portMAX_DELAY);
}

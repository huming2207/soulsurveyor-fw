/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <cstdio>
#include "lhs154kc.hpp"


extern "C" void app_main(void)
{
    auto &lcd = lhs154kc::instance();

    ESP_ERROR_CHECK(lcd.init());

    lhs154kc::lock();
    auto *label = lv_label_create(lv_scr_act());

    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(label, "Privyet!");
    lhs154kc::unlock();

    vTaskDelay(portMAX_DELAY);
}

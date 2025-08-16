idf_component_register(SRCS "app_main.cpp"
                            "lhs154kc.cpp" "lhs154kc.hpp"
                            "uart_handler.cpp" "uart_handler.hpp"
                            "lplora_radio.cpp" "lplora_radio.hpp"
                       REQUIRES driver lvgl esp_lvgl_port esp_lcd
                       INCLUDE_DIRS ".")

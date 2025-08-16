#pragma once

#include <cstdint>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include "freertos/queue.h"
#include <freertos/task.h>
#include "hal/uart_types.h"
#include "soc/gpio_num.h"


class lplora_radio
{
public:
    // Packet types
    enum class uart_packet_type : uint8_t {
        // Request from host
        ping = 0x00,
        radio_phy_config = 0x10,
        radio_freq_config = 0x11,
        radio_lora_config = 0x12,
        radio_gfsk_config = 0x13,
        enter_sleep_stop2 = 0x20,
        radio_go_sleep = 0x40,
        radio_go_idle = 0x41,
        radio_send = 0x42,
        radio_recv_start = 0x43,
        restart = 0x7f,

        // Reply from module
        pong = 0x80,
        ack = 0x83,
        nack = 0x84,
        radio_received_packet = 0xC1,
    };

    // LoRa spreading factor
    enum class lora_sf : uint8_t {
        sf5 = 0x05,
        sf6 = 0x06,
        sf7 = 0x07,
        sf8 = 0x08,
        sf9 = 0x09,
        sf10 = 0x0A,
        sf11 = 0x0B,
        sf12 = 0x0C,
    };

    // LoRa bandwidth
    enum class lora_bw : uint8_t {
        bw7 = 0x00,
        bw10 = 0x08,
        bw15 = 0x01,
        bw20 = 0x09,
        bw31 = 0x02,
        bw41 = 0x0A,
        bw62 = 0x03,
        bw125 = 0x04,
        bw250 = 0x05,
        bw500 = 0x06,
    };

    // LoRa coding rate
    enum class lora_cr : uint8_t {
        cr44 = 0x00,
        cr45 = 0x01,
        cr46 = 0x02,
        cr47 = 0x03,
        cr48 = 0x04,
    };


    // Packet header
    struct __attribute__((packed)) uart_packet_header {
        uart_packet_type packet_type;
        uint16_t payload_len;
    };

    // RadioPhyConfig payload
    struct __attribute__((packed)) radio_phy_config {
        uint8_t pa_duty_cycle;
        uint8_t hp_max;
        uint8_t pa_sel; // 0 for LP, 1 for HP
        int8_t power;
        uint8_t ramp_time;
        uint8_t rx_boost; // 0 for false, 1 for true
    };

    // RadioFreqConfig payload
    struct __attribute__((packed)) radio_freq_config {
        uint32_t freq_hz;
    };

    // RadioLoraConfig payload
    struct __attribute__((packed)) radio_lora_config {
        uint16_t preamble_len;
        uint8_t header_type; // 0 for fixed, 1 for variable
        uint8_t payload_len;
        uint8_t crc_en; // 0 for false, 1 for true
        uint8_t invert_iq; // 0 for false, 1 for true
        lora_sf sf;
        lora_bw bw;
        lora_cr cr;
        uint8_t ldro_en; // 0 for false, 1 for true
        uint8_t sync_word[2];
    };

    // RadioGfskConfig payload
    struct __attribute__((packed)) radio_gfsk_config {
        uint16_t preamble_len;
        uint8_t preamble_detection;
        uint8_t sync_word_len;
        uint8_t addr_comp;
        uint8_t header_type;
        uint8_t payload_len;
        uint8_t crc_type;
        uint8_t whiten_enable;
        uint32_t bitrate;
        uint8_t pulse_shape;
        uint8_t bandwidth;
        uint32_t fdev;
        uint8_t sync_word[8];
    };

public:
    lplora_radio(gpio_num_t tx, gpio_num_t rx) : tx_pin(tx), rx_pin(rx) {};
    esp_err_t init();


private:
    static void uart_evt_task_func(void *_ctx);
    esp_err_t send_internal(uart_packet_header *header, uint8_t *payload, size_t len);

private:
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    uart_port_t port = UART_NUM_2;
    QueueHandle_t uart_evt_queue = nullptr;
    TaskHandle_t uart_task = nullptr;
    RingbufHandle_t rx_rb = nullptr;

private:
    static const constexpr char TAG[] = "lplora_host";
    static const constexpr uint8_t SLIP_START = 0xA5;
    static const constexpr uint8_t SLIP_END = 0xC0;
    static const constexpr uint8_t SLIP_ESC = 0xDB;
    static const constexpr uint8_t SLIP_ESC_END = 0xDC;
    static const constexpr uint8_t SLIP_ESC_ESC = 0xDD;
    static const constexpr uint8_t SLIP_ESC_START = 0xDE;
};


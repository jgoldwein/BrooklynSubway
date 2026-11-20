#pragma once
#include <stdint.h>
using esp_err_t=int;
using gpio_num_t=int;
constexpr esp_err_t ESP_OK=0, ESP_ERR_INVALID_STATE=1, ESP_ERR_INVALID_SIZE=2;
constexpr int GPIO_MODE_OUTPUT=1;
using rmt_channel_t=int;
constexpr rmt_channel_t RMT_CHANNEL_0=0;
constexpr int RMT_MODE_TX=0;
struct rmt_item32_t {uint32_t val;};
struct rmt_config_t {int pin; rmt_channel_t channel; uint8_t clk_div,mem_block_num;};
#define RMT_DEFAULT_CONFIG_TX(pin,channel) rmt_config_t{pin,channel,80,1}
esp_err_t rmt_config(const rmt_config_t *);
esp_err_t rmt_driver_install(rmt_channel_t, unsigned, int);
esp_err_t rmt_get_counter_clock(rmt_channel_t,uint32_t *);
esp_err_t rmt_set_gpio(rmt_channel_t,int,gpio_num_t,bool);
esp_err_t rmt_write_items(rmt_channel_t,const rmt_item32_t *,int,bool);
esp_err_t rmt_wait_tx_done(rmt_channel_t,uint32_t);
esp_err_t rmt_tx_stop(rmt_channel_t);
esp_err_t gpio_set_direction(gpio_num_t,int);
esp_err_t gpio_set_level(gpio_num_t,int);
const char *esp_err_to_name(esp_err_t);

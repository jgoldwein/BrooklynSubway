#pragma once
#include <stdint.h>
uint32_t ledcSetup(uint8_t channel, uint32_t frequency, uint8_t resolution);
void ledcAttachPin(uint8_t pin, uint8_t channel);
void ledcWrite(uint8_t channel, uint32_t duty);
void ledcDetachPin(uint8_t pin);

#pragma once
#include <stdint.h>
#include <stddef.h>
constexpr int LOW=0, HIGH=1, pdPASS=1;
using TaskHandle_t=void *;
#define pdMS_TO_TICKS(x) (x)
struct FakeSerial { void println(const char *); template<class... T> void printf(const char *, T...) {} };
extern FakeSerial Serial;
int digitalRead(uint8_t);
int xTaskCreatePinnedToCore(void (*)(void *), const char *, uint32_t, void *, int, TaskHandle_t *, int);
void vTaskDelete(void *);
void vTaskDelay(uint32_t);

void delay(uint32_t);

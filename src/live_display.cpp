#include "live_display.h"

#if defined(BROOKLYN_TARGET_PCB) && BROOKLYN_ALLOW_LIVE_LED_OUTPUT == 1
#include "board_pins.h"
#include "led_backend.h"
#include "wifi_policy.h"
#include "test_button_policy.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace brooklyn::display::live {
namespace {
SemaphoreHandle_t mutex = nullptr;
TaskHandle_t task = nullptr;
Controller controller(true);
bool recoveryRequested = false;
bool recoveryPending = false;
controls::TestButton testButton;
bool testRequested = false;
uint16_t diagnosticDepth = 0;
bool arm(void *) { return led::backend::armLiveOutput(); }
bool transmit(void *, const led::DisplayState *frame, uint16_t count) {
  return led::backend::transmitLiveOutput(frame, count);
}
void stop(void *) { led::backend::stopLiveOutput(); }
const Operations operations{nullptr, arm, transmit, stop};
void run(void *) {
  wifi::HoldDetector recoveryHold;
  for (;;) {
    const uint32_t nowMs = millis();
    const bool recovery = recoveryHold.update(digitalRead(pins::ROTARY_PUSH) == LOW, nowMs);
    xSemaphoreTake(mutex, portMAX_DELAY);
    const bool testReleased = testButton.update(digitalRead(pins::BUTTON_TEST) == LOW, nowMs);
    if (testReleased && diagnosticDepth == 0) testRequested = true;
    if (recovery) recoveryRequested = recoveryPending = true;
    if (!recoveryRequested && digitalRead(pins::ROTARY_PUSH) != LOW) recoveryPending = false;
    const bool wasEnabled = controller.enabled();
    controller.service(nowMs, digitalRead(pins::BUTTON_POWER) == LOW,
                       recoveryPending, operations);
    if (wasEnabled && !controller.enabled()) Serial.println(controller.stats().powerCanResume
        ? "Live display paused by POWER; press POWER again to resume"
        : "Live display stopped; GPIO1 LOW; enable again from web");
    if (!wasEnabled && controller.enabled()) Serial.println("Live display started/resumed after checks");
    xSemaphoreGive(mutex);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}
bool begin() {
  if (task) return true;
  mutex = xSemaphoreCreateMutex();
  if (!mutex) {
    Serial.println("Live display unavailable: mutex allocation failed; outputs OFF");
    return false;
  }
  if (xTaskCreatePinnedToCore(run, "brooklyn-led", 8192, nullptr, 2, &task, 1) != pdPASS) {
    vSemaphoreDelete(mutex); mutex = nullptr; task = nullptr;
    Serial.println("Live display unavailable: task allocation failed; outputs OFF");
    return false;
  }
  Serial.println("Live display ready: GPIO1 enable; GPIO4/5/6/7/10/11/12/13 data; auto-start after healthy fresh data; cap 16/255; POWER toggles");
  return true;
}
bool available() { return task != nullptr; }
void publish(const Snapshot &snapshot) {
  if (!available()) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  controller.publish(snapshot);
  xSemaphoreGive(mutex);
}
bool enable() {
  if (!available()) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const bool result = !recoveryPending && digitalRead(pins::BUTTON_POWER) != LOW && controller.enable(millis());
  xSemaphoreGive(mutex);
  Serial.println(result ? "Live display enabled explicitly; brightness capped at 16/255" : "Live display refused: need healthy fresh data, released POWER and no recovery");
  return result;
}
void disable() {
  if (!available()) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  controller.disable(operations);
  xSemaphoreGive(mutex);
}
bool enabled() {
  if (!available()) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const bool result = controller.enabled();
  xSemaphoreGive(mutex);
  return result;
}
bool consumeRecoveryRequest() {
  if (!available()) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const bool result = recoveryRequested;
  recoveryRequested = false;
  xSemaphoreGive(mutex);
  return result;
}
OutputStats stats() {
  if (!available()) return {};
  xSemaphoreTake(mutex, portMAX_DELAY);
  const auto result = controller.stats();
  xSemaphoreGive(mutex);
  return result;
}
void restoreAfterSuccessfulTest(bool start) {
  if (!available()) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (!recoveryPending) {
    if (start) controller.requestAutomaticStart();
    else controller.allowPowerResume();
  }
  xSemaphoreGive(mutex);
}
bool consumeTestRequest() {
  if (!available()) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const bool result = testRequested;
  testRequested = false;
  xSemaphoreGive(mutex);
  return result;
}
void beginDiagnostic() {
  if (!available()) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  ++diagnosticDepth;
  testRequested = false;
  xSemaphoreGive(mutex);
}
void endDiagnostic() {
  if (!available()) return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (diagnosticDepth) --diagnosticDepth;
  if (!diagnosticDepth) {
    testButton.reset(digitalRead(pins::BUTTON_TEST) == LOW, millis());
    testRequested = false;
    // A test stays OFF until restored by its caller or a new POWER press.
    // Fresh-data and driver guards still apply to that explicit restart.
    if (!recoveryPending) controller.allowPowerResume();
  }
  xSemaphoreGive(mutex);
}
}  // namespace brooklyn::display::live
#else
namespace brooklyn::display::live {
bool begin() { return false; }
bool available() { return false; }
void publish(const Snapshot &) {}
bool enable() { return false; }
void disable() {}
bool enabled() { return false; }
OutputStats stats() { return {}; }
void restoreAfterSuccessfulTest(bool) {}
bool consumeTestRequest() { return false; }
void beginDiagnostic() {}
void endDiagnostic() {}
bool consumeRecoveryRequest() { return false; }
}
#endif

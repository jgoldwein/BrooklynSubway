#include "led_map.h"

namespace brooklyn::led::map {
namespace {

constexpr uint8_t CHANNEL_COUNTS[CHANNEL_COUNT] = {37, 38, 37, 37, 42, 37, 38, 40};

// PCB designators follow data/led_chain_map.csv in global physical order.
constexpr uint16_t DESIGNATORS[TOTAL_LED_COUNT] = {
    42, 41, 40, 229, 228, 39, 227, 38, 100, 206, 18, 168, 99, 169, 19, 20, 173,
    101, 172, 21, 22, 26, 27, 28, 102, 174, 23, 24, 25, 6, 103, 104, 171, 29, 30,
    31, 32,
    208, 115, 207, 114, 113, 289, 61, 233, 60, 59, 58, 57, 43, 44, 106, 105, 33,
    34, 107, 45, 46, 47, 48, 49, 111, 116, 175, 112, 110, 109, 108, 35, 36, 37,
    167, 166, 165, 7,
    226, 146, 147, 250, 225, 145, 224, 144, 143, 223, 222, 142, 141, 221, 196, 195,
    302, 301, 300, 183, 180, 140, 181, 304, 182, 303, 290, 185, 184, 51, 52, 53, 231,
    54, 55, 56, 232,
    220, 139, 230, 50, 138, 219, 137, 218, 17, 292, 249, 248, 98, 97, 247, 156, 155,
    119, 164, 162, 161, 157, 136, 158, 159, 163, 160, 191, 217, 215, 135, 216, 214,
    134, 177, 133, 178,
    190, 293, 154, 153, 118, 152, 179, 10, 213, 132, 209, 117, 294, 311, 310, 309,
    308, 188, 291, 285, 286, 255, 256, 257, 258, 259, 234, 62, 235, 236, 63, 297, 9,
    82, 81, 80, 295, 96, 79, 78, 95, 187,
    194, 176, 210, 124, 123, 122, 121, 288, 120, 287, 189, 16, 15, 14, 13, 12, 11,
    298, 64, 237, 238, 65, 240, 66, 239, 192, 67, 296, 68, 241, 193, 86, 170, 254,
    85, 84, 83,
    198, 197, 199, 200, 201, 90, 128, 129, 130, 212, 131, 245, 246, 77, 76, 75, 74,
    73, 244, 243, 72, 151, 71, 150, 148, 149, 299, 127, 126, 211, 125, 69, 242, 70,
    89, 88, 87, 253,
    252, 251, 306, 307, 284, 283, 281, 282, 279, 280, 278, 277, 305, 276, 275, 274,
    273, 272, 271, 269, 270, 268, 267, 266, 265, 264, 263, 262, 261, 260, 186, 205,
    204, 203, 91, 202, 92, 93, 94, 8};

constexpr uint16_t CHANNEL_OFFSETS[CHANNEL_COUNT] = {0, 37, 75, 112, 149, 191, 228, 266};

constexpr uint16_t STATUS_DESIGNATORS[4] = {252, 251, 306, 307};

uint8_t channelNumber(PhysicalChannel channel) {
  return static_cast<uint8_t>(channel);
}

}  // namespace

uint8_t channelLedCount(PhysicalChannel channel) {
  const uint8_t number = channelNumber(channel);
  if (number < 1 || number > CHANNEL_COUNT) {
    return 0;
  }
  return CHANNEL_COUNTS[number - 1];
}

bool toGlobalIndex(PhysicalAddress address, GlobalPhysicalIndex &result) {
  const uint8_t number = channelNumber(address.channel);
  if (number < 1 || number > CHANNEL_COUNT ||
      address.index >= CHANNEL_COUNTS[number - 1]) {
    return false;
  }
  result.value = CHANNEL_OFFSETS[number - 1] + address.index;
  return true;
}

bool toPhysicalAddress(GlobalPhysicalIndex index, PhysicalAddress &result) {
  if (index.value >= TOTAL_LED_COUNT) {
    return false;
  }
  for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
    const uint16_t start = CHANNEL_OFFSETS[channel];
    const uint16_t end = start + CHANNEL_COUNTS[channel];
    if (index.value >= start && index.value < end) {
      result.channel = static_cast<PhysicalChannel>(channel + 1);
      result.index = static_cast<uint8_t>(index.value - start);
      return true;
    }
  }
  return false;
}

bool physicalLed(GlobalPhysicalIndex index, PhysicalLed &result) {
  PhysicalAddress address{};
  if (!toPhysicalAddress(index, address)) {
    return false;
  }
  result.address = address;
  result.globalIndex = index;
  result.designator.value = DESIGNATORS[index.value];
  result.role = (index.value >= 266 && index.value <= 269) ? LedRole::Status
                                                           : LedRole::Map;
  return true;
}

bool statusLedAddress(StatusLed status, PhysicalAddress &result) {
  const uint8_t index = static_cast<uint8_t>(status);
  if (index >= 4) {
    return false;
  }
  result.channel = PhysicalChannel::CH8;
  result.index = index;
  return true;
}

bool statusLedDesignator(StatusLed status, BoardLedDesignator &result) {
  const uint8_t index = static_cast<uint8_t>(status);
  if (index >= 4) {
    return false;
  }
  result.value = STATUS_DESIGNATORS[index];
  return true;
}

}  // namespace brooklyn::led::map

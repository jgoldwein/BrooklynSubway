#pragma once
#include <stdint.h>
#include <string.h>
#include "check.h"
#define U8G2_R0 nullptr
#define U8X8_PIN_NONE 255
#define U8X8_MSG_BYTE_INIT 0
#define U8X8_MSG_BYTE_SET_DC 1
#define U8X8_MSG_BYTE_START_TRANSFER 2
#define U8X8_MSG_BYTE_SEND 3
#define U8X8_MSG_BYTE_END_TRANSFER 4
struct u8x8_t { uint8_t (*byte_cb)(u8x8_t *, uint8_t, uint8_t, void *)=nullptr; uint8_t address=0; };
inline uint8_t u8x8_GetI2CAddress(u8x8_t *s) { return s->address; }
const uint8_t u8g2_font_6x12_tf[]={0}, u8g2_font_8x13_tf[]={0};
extern int frames, titles, contrastCalls, powerCommands;
extern uint8_t lastContrast;
struct U8G2_SSD1306_128X64_NONAME_F_HW_I2C {
 u8x8_t d;
 U8G2_SSD1306_128X64_NONAME_F_HW_I2C(const void *,uint8_t reset,uint8_t scl=255,uint8_t sda=255) { OLED_CHECK(reset==255 && scl==255 && sda==255); }
 void clearBuffer() {} void setFont(const uint8_t *) {}
 int getStrWidth(const char *s) {return strlen(s)*8;}
 void drawStr(int,int,const char *s) {if(strcmp(s,"Brooklyn Subway")==0)++titles;}
 void transfer() { uint8_t b=0; d.byte_cb(&d,2,0,nullptr); d.byte_cb(&d,3,1,&b); d.byte_cb(&d,4,0,nullptr); }
 void sendBuffer() { ++frames; for(int i=0;i<16;++i)transfer(); }
 void setI2CAddress(uint8_t a) {d.address=a;}
 void setBusClock(uint32_t) {}
 void setPowerSave(uint8_t value) { OLED_CHECK(value<=1); ++powerCommands; transfer(); }
 void setContrast(uint8_t value) { ++contrastCalls; lastContrast=value; transfer(); }
 u8x8_t *getU8x8(){return &d;}
 void begin() { d.byte_cb(&d,0,0,nullptr); for(int i=0;i<16;++i)transfer(); }
};

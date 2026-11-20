#include "led_transport.h"
#include "led_waveform.h"
#include "board_pins.h"
#include <Arduino.h>
#include <driver/rmt.h>
#include <esp_intr_alloc.h>
#include <soc/gpio_sig_map.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace brooklyn::led;
namespace pins=brooklyn::pins;
FakeSerial Serial;
const char *scenario="ok";
int checks=0, installs=0, writes=0, waits=0, stops=0, routes=0;
int routed=-1, enable=1;
DisplayState frame[306]{};
#define CHECK(x) do {++checks; if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
bool failing(const char *name) {return strcmp(scenario,name)==0;}
void digitalWrite(uint8_t pin,int level) {CHECK(pin==1 && level==0);enable=level;}
int xPortGetCoreID(){return failing("core")?0:1;}
const char *esp_err_to_name(esp_err_t){return "fake";}
esp_err_t gpio_set_direction(gpio_num_t,int mode){CHECK(mode==GPIO_MODE_OUTPUT);return ESP_OK;}
esp_err_t gpio_set_level(gpio_num_t,int level){CHECK(level==0);return ESP_OK;}
void esp_rom_gpio_connect_out_signal(uint32_t pin,uint32_t signal,bool invert,bool oen) {
  CHECK(signal==SIG_GPIO_OUT_IDX && !invert && !oen);
  if(int(pin)==routed) routed=-1;
}
esp_err_t rmt_config(const rmt_config_t *c) {
  CHECK(c->pin==4 && c->channel==0 && c->clk_div==2 && c->mem_block_num==4);
  return failing("configure")?1:0;
}
esp_err_t rmt_driver_install(rmt_channel_t channel,unsigned rx,int flags) {
  CHECK(channel==0 && !rx && flags==(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL3));
  ++installs;return failing("install")?1:0;
}
esp_err_t rmt_get_counter_clock(rmt_channel_t,uint32_t *hz) {
  *hz=failing("clock")?20000000:40000000;return 0;
}
esp_err_t rmt_set_gpio(rmt_channel_t channel,int mode,gpio_num_t pin,bool invert) {
  CHECK(channel==0 && mode==RMT_MODE_TX && !invert && routed==-1);
  CHECK(pin==pins::LED_DATA[routes%8]);
  ++routes;routed=pin;
  return failing("route") && routes==3?1:0;
}
esp_err_t rmt_write_items(rmt_channel_t,const rmt_item32_t *items,int count,bool wait) {
  const int ch=writes%8;
  CHECK(routed==pins::LED_DATA[ch] && !wait);
  CHECK(count==pins::LED_COUNT[ch]*24+1);
  int offset=0;for(int c=0;c<ch;++c)offset+=pins::LED_COUNT[c];
  int used=0;
  for(int p=0;p<pins::LED_COUNT[ch];++p) {
    const auto rgb=frame[offset+p].color;
    const uint8_t expected[]={rgb.green,rgb.red,rgb.blue};
    for(uint8_t b:expected) for(uint8_t bit=128;bit;bit>>=1) {
      const auto word=items[used++].val;
      CHECK((word&0x8000u) && !(word&0x80000000u));
      CHECK((word&0x7fffu)==((b&bit)?32u:16u));
      CHECK(((word>>16)&0x7fffu)==((b&bit)?18u:34u));
    }
  }
  CHECK(items[used].val==(6000u|(6000u<<16)));
  ++writes;return failing("write") && writes==3?1:0;
}
esp_err_t rmt_wait_tx_done(rmt_channel_t,uint32_t ticks) {
  CHECK(ticks==100);++waits;return failing("wait") && waits==3?1:0;
}
esp_err_t rmt_tx_stop(rmt_channel_t){++stops;return 0;}
int main(int argc,char **argv) {
  if(argc>1)scenario=argv[1];
  for(int i=0;i<306;++i)frame[i].color={uint8_t(i*31),uint8_t(i*53),uint8_t(i*97)};
  CHECK(!transport::transmit(frame,306));
  const bool initialized=transport::initialize();
  const bool initFailure=failing("configure")||failing("install")||failing("core")||failing("clock");
  CHECK(initialized!=initFailure);
  if(initialized) {
    CHECK(!transport::transmit(nullptr,306));CHECK(!transport::transmit(frame,305));
    const bool sent=transport::transmit(frame,306);
    CHECK(sent==failing("ok"));
    CHECK(routed==-1);
    if(sent) {
      CHECK(writes==8 && waits==8 && stops==0 && installs==1);
      CHECK(transport::initialize());CHECK(installs==1);
      CHECK(transport::transmit(frame,306));CHECK(writes==16 && waits==16 && installs==1);
      rmt_item32_t out[waveform::kMaxItems+1]{};out[0].val=123;
      CHECK(!waveform::encode(frame,43,out,waveform::kMaxItems));
      CHECK(!waveform::encode(frame,42,out,waveform::kMaxItems-1));
      CHECK(!waveform::encode(frame,0,out,waveform::kMaxItems));
      CHECK(out[0].val==123);
    } else CHECK(writes<=3 && stops==1 && enable==0);
  }
  if(!failing("ok")) {
    CHECK(enable==0 && !transport::initialize());
    const int previous=writes;
    CHECK(!transport::transmit(frame,306));CHECK(writes==previous);
  }
  printf("LED transport %s: %d checks passed\n",scenario,checks);
}

#include "oled_clock.h"
#include <Arduino.h>
#include <Wire.h>
#include "check.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
jmp_buf iterationEnd;
FakeSerial Serial; FakeWire Wire;
int frames=0,titles=0,ends=0,starts=0,deletes=0,delays=0;
int retryDelays=0;
int scenario=0; bool open=false; uint8_t lastAddress=0;
void (*worker)(void *)=nullptr;
void FakeSerial::println(const char *) {}
int digitalRead(uint8_t pin){OLED_CHECK(pin==8||pin==9); return scenario==1?LOW:HIGH;}
int xTaskCreatePinnedToCore(void (*f)(void *),const char *,uint32_t stack,void *,int priority,TaskHandle_t *handle,int core){
 OLED_CHECK(stack==4096 && priority==1 && core==1);
 if(scenario==6)return 0; worker=f; *handle=reinterpret_cast<void *>(1);return pdPASS;
}
void delay(uint32_t ms){OLED_CHECK(ms==100);++retryDelays;}
void vTaskDelete(void *) {++deletes;}
void vTaskDelay(uint32_t ms) { OLED_CHECK(ms==1000); ++delays; longjmp(iterationEnd,1); }
void FakeWire::setTimeOut(uint16_t ms){OLED_CHECK(ms==50);}
void FakeWire::setClock(uint32_t hz){OLED_CHECK(hz==100000);}
void FakeWire::beginTransmission(uint8_t a){OLED_CHECK(!open); OLED_CHECK(a==0x38||a==0x3c||a==0x3d);lastAddress=a;open=true;++starts;}
uint8_t FakeWire::endTransmission(){OLED_CHECK(open);open=false;++ends;
 if(scenario==2)return 2;
 if(scenario==7 && lastAddress==0x38)return 2;
 if(scenario==9 && ends<=3)return 2;
 if(scenario==3 && ends>=2)return 5;
 if(scenario==5 && ends>17)return 5;
 if(scenario==10 && ends>33)return 5;
 return 0;
}
size_t FakeWire::write(uint8_t *,size_t n){OLED_CHECK(open);return scenario==4?0:n;}
int main(int argc,char **argv){OLED_CHECK(argc==2);scenario=atoi(argv[1]);
 if(scenario==8){brooklyn::oled::begin(0x07); OLED_CHECK(!worker && starts==0); puts("OLED invalid address rejected PASS"); return 0;}
 const uint8_t preferred=scenario==11?0x3c:0x38;
 brooklyn::oled::begin(preferred);OLED_CHECK(starts==0 && frames==0); // Startup must return without any I2C work.
 if(scenario==6){OLED_CHECK(worker==nullptr);puts("OLED task allocation failure isolated PASS");return 0;}
 OLED_CHECK(worker);brooklyn::oled::begin(preferred);
 if(setjmp(iterationEnd)==0) worker(nullptr);
 OLED_CHECK(!open);
 if(scenario==0){OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::Ready); OLED_CHECK(frames==2 && titles==2 && delays==1 && deletes==0);}
 if(scenario==7){OLED_CHECK(lastAddress==0x3c && frames==2 && titles==2 && delays==1);
 OLED_CHECK(brooklyn::oled::status().activeAddress==0x3c && brooklyn::oled::status().requestedAddress==0x38); }
 if(scenario==9){OLED_CHECK(retryDelays==1 && frames==2 && brooklyn::oled::status().state==brooklyn::oled::State::Ready); }
 if(scenario==1){OLED_CHECK(starts==0 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::BusLow); }
 if(scenario==2){OLED_CHECK(ends==6 && retryDelays==1 && frames==0 && deletes==1);
 OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::NoAck && brooklyn::oled::status().error==2); }
 if(scenario==3 || scenario==4){OLED_CHECK(ends==2 && frames==0 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::InitFailed); }
 if(scenario==5){OLED_CHECK(ends==18 && frames==1 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::InitFailed); }
 if(scenario==10){OLED_CHECK(ends==34 && frames==2 && deletes==1); OLED_CHECK(brooklyn::oled::status().state==brooklyn::oled::State::TransferFailed && brooklyn::oled::status().error==5);}
 if(scenario==11){OLED_CHECK(frames==2 && brooklyn::oled::status().requestedAddress==0x3c && brooklyn::oled::status().activeAddress==0x3c);}
 printf("OLED runtime scenario %d PASS\n",scenario);
}

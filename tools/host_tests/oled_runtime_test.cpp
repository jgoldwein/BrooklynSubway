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
int powerCommands=0;
int retryDelays=0, contrastCalls=0; uint8_t lastContrast=207;
int scenario=0; bool open=false; uint8_t lastAddress=0;
void (*worker)(void *)=nullptr;
void FakeSerial::println(const char *text) {
 if(scenario>=17 && strcmp(text,"OLED clock: initialization and first frame complete")==0) {
   brooklyn::oled::setEnabled(false);
   brooklyn::oled::showBrightness(scenario==18,5,scenario==18?255:16);
 }
 if (scenario>=15 && strcmp(text,"OLED clock: initialization and first frame complete")==0) brooklyn::oled::setEnabled(false);
 if ((scenario==12 || scenario==14) && strcmp(text,"OLED clock: initialization and first frame complete")==0)
   brooklyn::oled::setContrast(5);
}
int digitalRead(uint8_t pin){OLED_CHECK(pin==8||pin==9); return scenario==1?LOW:HIGH;}
int xTaskCreatePinnedToCore(void (*f)(void *),const char *,uint32_t stack,void *,int priority,TaskHandle_t *handle,int core){
 OLED_CHECK(stack==4096 && priority==1 && core==1);
 if(scenario==6)return 0; worker=f; *handle=reinterpret_cast<void *>(1);return pdPASS;
}
void delay(uint32_t ms){OLED_CHECK(ms==100);++retryDelays;}
void vTaskDelete(void *) {++deletes;}
void vTaskDelay(uint32_t ms) { OLED_CHECK(ms==1000); ++delays; if(scenario==19 && delays<5) return; if(scenario==20 && delays==1) { brooklyn::oled::clearBrightnessFeedback(); return; } if(scenario==16 && delays==1) { brooklyn::oled::setEnabled(true); return; } longjmp(iterationEnd,1); }
void FakeWire::setTimeOut(uint16_t ms){OLED_CHECK(ms==50);}
void FakeWire::setClock(uint32_t hz){OLED_CHECK(hz==100000);}
void FakeWire::beginTransmission(uint8_t a){OLED_CHECK(!open); OLED_CHECK(a==0x38||a==0x3c||a==0x3d);lastAddress=a;open=true;++starts;}
uint8_t FakeWire::endTransmission(){OLED_CHECK(open);open=false;++ends;
 if(scenario==2)return 2;
 if(scenario==7 && lastAddress==0x38)return 2;
 if(scenario==9 && ends<=3)return 2;
 if(scenario==13 && ends>17)return 5;
 if(scenario==14 && ends>33)return 5;
 if(scenario==3 && ends>=2)return 5;
 if(scenario==5 && ends>17)return 5;
 if(scenario==10 && ends>33)return 5;
 return 0;
}
size_t FakeWire::write(uint8_t *,size_t n){OLED_CHECK(open);return scenario==4?0:n;}
int main(int argc,char **argv){OLED_CHECK(argc==2);scenario=atoi(argv[1]);
 if(scenario==8){brooklyn::oled::begin(0x07); OLED_CHECK(!worker && starts==0); puts("OLED invalid address rejected PASS"); return 0;}
 const uint8_t preferred=scenario==11?0x3c:0x38;
 brooklyn::oled::begin(preferred,scenario==12||scenario==13?32:207);OLED_CHECK(starts==0 && frames==0); // Startup must return without any I2C work.
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
 if(scenario==12){OLED_CHECK(contrastCalls==2 && lastContrast==5 && frames==2 && brooklyn::oled::status().state==brooklyn::oled::State::Ready);}
 if(scenario==13){OLED_CHECK(frames==0 && deletes==1 && brooklyn::oled::status().state==brooklyn::oled::State::InitFailed);}
 if(scenario==14){OLED_CHECK(frames==1 && deletes==1 && brooklyn::oled::status().state==brooklyn::oled::State::TransferFailed);}
 if(scenario==17||scenario==18) OLED_CHECK(powerCommands==0 && frames==2 && titles==1);
 if(scenario==19) OLED_CHECK(powerCommands==1 && frames==4 && titles==1 && delays==5);
 if(scenario==20) OLED_CHECK(powerCommands==1 && frames==2 && titles==1 && delays==2);
 if(scenario==15) OLED_CHECK(powerCommands==1 && frames==1 && deletes==0);
 if(scenario==16) OLED_CHECK(powerCommands==2 && frames==2 && deletes==0);
 printf("OLED runtime scenario %d PASS\n",scenario);
}

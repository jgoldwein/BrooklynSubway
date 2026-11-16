#include "service_sound_policy.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);exit(1);}}while(0)
int main() {
  using namespace brooklyn::audio;
  ServicePolicy p;
  CHECK(p.observe(0,false,true,false)==ServiceEvent::None);
  CHECK(p.observe(100000,false,true,false)==ServiceEvent::None);
  p.observe(100001,true,true,false);p.observe(100002,false,true,false);
  CHECK(p.observe(160001,false,true,false)==ServiceEvent::None);
  CHECK(p.observe(160002,false,true,false)==ServiceEvent::Lost);
  // Merely queuing a tone is not proof that it played.
  CHECK(p.observe(160003,true,true,false)==ServiceEvent::None);
  p.observe(170000,false,true,false);CHECK(p.observe(230000,false,true,false)==ServiceEvent::Lost);
  p.lossAnnounced();CHECK(p.observe(900000,false,true,false)==ServiceEvent::None);
  CHECK(p.observe(900001,true,true,false)==ServiceEvent::Restored);
  CHECK(p.observe(900002,true,true,false)==ServiceEvent::None);
  for(unsigned i=0;i<10;++i){p.observe(1000000+i*60000,false,true,false);CHECK(p.observe(1059999+i*60000,false,true,false)==ServiceEvent::None);p.observe(1059999+i*60000,true,true,false);}
  const uint32_t start=UINT32_MAX-30000;p.observe(start,true,true,false);p.observe(start,false,true,false);
  CHECK(p.observe(start+59999u,false,true,false)==ServiceEvent::None);
  CHECK(p.observe(start+60000u,false,true,false)==ServiceEvent::Lost);p.lossAnnounced();
  p.observe(start+60001u,false,true,true);CHECK(p.observe(start+60002u,true,true,false)==ServiceEvent::None);
  p.observe(1,false,true,false);p.observe(60001,false,true,false);p.lossAnnounced();p.observe(60002,false,false,false);
  CHECK(p.observe(60003,true,true,false)==ServiceEvent::None);
  puts("Service audio: startup silence, 60-second boundary, short outages, acknowledged restoration, no repeats, quiet/disable and rollover PASS");
}

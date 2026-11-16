#include "factory_reset.h"
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x);exit(1); } } while (0)
#include <string.h>
#include <stdio.h>
struct Fake {
  bool pending=false,readOk=true,writeOk=true;
  int failAt=-1,count=0;
  const char *cleared[3]={};
};
bool read(void *p,bool &pending) {auto &f=*static_cast<Fake *>(p);pending=f.pending;return f.readOk;}
bool write(void *p,bool pending) {auto &f=*static_cast<Fake *>(p);if(!f.writeOk)return false;f.pending=pending;return true;}
bool clear(void *p,const char *name) {
  auto &f=*static_cast<Fake *>(p);
  if(f.count==f.failAt)return false;
  CHECK(f.count<3);f.cleared[f.count++]=name;return true;
}
int main() {
  using namespace brooklyn::defaults;
  Fake f;Store store{&f,read,write,clear};
  CHECK(completePending(store)==Result::NotRequested && !f.count);
  f.readOk=false;CHECK(completePending(store)==Result::Failed && !f.count);
  f.readOk=true;f.pending=true;
  CHECK(completePending(store)==Result::Complete && !f.pending && f.count==3);
  CHECK(!strcmp(f.cleared[0],"bkmap_wifi"));CHECK(!strcmp(f.cleared[1],"bkmap_cfg"));CHECK(!strcmp(f.cleared[2],"otacfg"));
  // Each partial erase leaves the marker committed, so a reboot retries all stores.
  for(int fail=0;fail<3;++fail) {
    f=Fake{};f.pending=true;f.failAt=fail;
    CHECK(completePending(store)==Result::Failed && f.pending);
    f.count=0;f.failAt=-1;CHECK(completePending(store)==Result::Complete && !f.pending);
  }
  f=Fake{};f.pending=true;f.writeOk=false;
  CHECK(completePending(store)==Result::Failed && f.pending);
  f.count=0;f.writeOk=true;CHECK(completePending(store)==Result::Complete && !f.pending);
  CHECK(completePending(store)==Result::NotRequested && f.count==3);
  puts("Firmware defaults: owned namespaces only, no-op, failures and interrupted-reset retry PASS");
}

#include "tls_memory.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
FakeSerial Serial;void *(*allocFn)(size_t,size_t)=nullptr;void(*freeFn)(void *)=nullptr;
int installs=0,allocCalls=0,externalCalls=0,internalCalls=0;bool failExternal=false,failInternal=false;
int mbedtls_platform_set_calloc_free(void *(*a)(size_t,size_t),void(*f)(void *)){++installs;if(installs==1)return -1;allocFn=a;freeFn=f;return 0;}
void *heap_caps_calloc(size_t n,size_t size,unsigned caps){++allocCalls;if(caps&MALLOC_CAP_SPIRAM){++externalCalls;if(failExternal)return nullptr;}else{CHECK(caps&MALLOC_CAP_INTERNAL);++internalCalls;if(failInternal)return nullptr;}CHECK(caps&MALLOC_CAP_8BIT);return calloc(n,size);}
void heap_caps_free(void *p){free(p);}
int main(){
 using namespace brooklyn::tls;CHECK(!configureMemory());CHECK(configureMemory());CHECK(configureMemory() && installs==2);
 void *small=allocFn(1,64);CHECK(small && internalCalls==1 && externalCalls==0);freeFn(small);
 void *large=allocFn(2,8192);CHECK(large && externalCalls==1);for(unsigned i=0;i<16384;++i)CHECK(static_cast<unsigned char *>(large)[i]==0);freeFn(large);
 void *medium=allocFn(1,1024);CHECK(medium && externalCalls==2 && internalCalls==1);freeFn(medium);
 failExternal=true;large=allocFn(1,4096);CHECK(large && internalCalls==2);freeFn(large);
 failInternal=true;CHECK(!allocFn(1,16384));CHECK(!allocFn(1,32));
 const int before=allocCalls;CHECK(!allocFn(SIZE_MAX,2));CHECK(allocCalls==before);
 freeFn(nullptr);const auto stats=memoryStats();CHECK(stats.externalAllocations==2 && stats.internalFallbacks==2 && stats.allocationFailures==3);
 puts("TLS allocator: one-time install/retry, small internal, large zeroed PSRAM, internal fallback, allocation failure, overflow and compatible free PASS");
}

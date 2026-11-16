#include "feed_stream_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct FakeReadClock {uint32_t now=0,step=1000,waits=0;uint32_t nowMs(){return now;}void wait(){now+=step;++waits;}};
struct Stream {
  int avail=16384,got=-99,reads=0;bool up=true;
  int available(){return avail;}
  int read(uint8_t *out,size_t n){++reads;memset(out,42,n);return got==-99?static_cast<int>(n):got;}
  int readBytes(uint8_t *,size_t){CHECK(false);return 0;} // Never use byte-wise SDK path.
  bool connected(){return up;}
};
int main() {
  using namespace brooklyn::transport;
  FeedStreamReader reader;FakeReadClock clock;Stream stream;uint8_t data[16384]{};
  reader.reset(0,0,16384,8000,12000);
  CHECK(reader.read(stream,clock,data,sizeof(data))==16384 && stream.reads==1);
  CHECK(reader.bytes==16384 && reader.chunks==1 && reader.stop==BodyStop::Complete && !clock.waits);
  reader.reset(0,0,7,8000,12000);CHECK(reader.read(stream,clock,data,7)==7 && reader.bytes==7);
  // Data keeps moving beyond the idle limit; only the total bound stops it.
  stream.avail=1;reader.reset(0,0,4,3000,12000);
  for(unsigned i=0;i<4;++i){clock.now=i*2000;CHECK(reader.read(stream,clock,data,1)==1);}
  CHECK(reader.stop==BodyStop::Complete && reader.maxGapMs==2000);
  clock.now=12000;CHECK(reader.read(stream,clock,data,1)==kReadTimeout && reader.stop==BodyStop::Deadline);
  clock=FakeReadClock{};stream.avail=0;reader.reset(0,0,10,8000,12000);
  CHECK(reader.read(stream,clock,data,10)==kReadTimeout && clock.now==8000 && reader.stop==BodyStop::Idle && reader.maxGapMs==8000);
  clock=FakeReadClock{};clock.now=11000;reader.reset(0,11000,10,8000,12000);
  CHECK(reader.read(stream,clock,data,10)==kReadTimeout && clock.now==12000 && reader.stop==BodyStop::Deadline);
  clock=FakeReadClock{};stream.up=false;reader.reset(0,0,10,8000,12000);
  CHECK(reader.read(stream,clock,data,10)==kReadEof && reader.stop==BodyStop::Eof && !reader.bytes);
  stream.up=true;stream.avail=-1;CHECK(reader.read(stream,clock,data,10)==kReadError);
  stream.up=false;CHECK(reader.read(stream,clock,data,10)==kReadEof);stream.up=true;
  stream.avail=10;stream.got=-1;CHECK(reader.read(stream,clock,data,10)==kReadError);
  stream.got=11;CHECK(reader.read(stream,clock,data,10)==kReadError);
  stream.got=0;reader.reset(0,0,10,8000,12000);CHECK(reader.read(stream,clock,data,10)==kReadTimeout);
  clock=FakeReadClock{};stream.got=-99;const uint32_t start=UINT32_MAX-50;clock.now=start;
  reader.reset(start,start,30,80,100);CHECK(reader.read(stream,clock,data,10)==10);
  clock.now=start+79;CHECK(reader.read(stream,clock,data,10)==10);
  clock.now=start+100;CHECK(reader.read(stream,clock,data,10)==kReadTimeout && reader.stop==BodyStop::Deadline);
  reader.reset(0,0,kUnknownLength,8000,12000);clock.now=0;stream.avail=10;CHECK(reader.read(stream,clock,data,10)==10);
  stream.avail=0;stream.up=false;CHECK(reader.read(stream,clock,data,10)==kReadEof && reader.bytes==10);
  CHECK(reader.read(stream,clock,nullptr,10)==kReadError);
  CHECK(reader.read(stream,clock,data,0)==kReadError);
  // Slow headers do not consume the body allowance; progress still has bounds.
  reader.reset(0,11000,100,8000,30000,12000);stream.up=true;stream.avail=1;clock.now=11000;
  CHECK(reader.read(stream,clock,data,1)==1);clock.now=18000;CHECK(reader.read(stream,clock,data,1)==1);
  clock.now=22999;CHECK(reader.read(stream,clock,data,1)==1);clock.now=23000;
  CHECK(reader.read(stream,clock,data,1)==kReadTimeout && reader.stop==BodyStop::BodyDeadline);
  reader.reset(0,25000,100,8000,30000,12000);clock.now=30000;
  CHECK(reader.read(stream,clock,data,1)==kReadTimeout && reader.stop==BodyStop::Deadline);
  puts("Feed bulk reader: one block read, capacity bound, progress idle reset, hard deadline with data available, EOF/error/zero progress and rollover PASS");
}

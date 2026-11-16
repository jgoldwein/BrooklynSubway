#pragma once
#include "feed_transport.h"
namespace brooklyn::transport {
enum class BodyStop : uint8_t { NotRead, Complete, Eof, Idle, Deadline, Error, BodyDeadline };
inline const char *bodyStopName(BodyStop stop) {
  switch(stop) {
    case BodyStop::Complete:return "complete";
    case BodyStop::Eof:return "eof";
    case BodyStop::Idle:return "idle-timeout";
    case BodyStop::Deadline:return "request-deadline";
    case BodyStop::BodyDeadline:return "body-deadline";
    case BodyStop::Error:return "read-error";
    default:return "not-read";
  }
}
// Read decrypted bytes in blocks. Stream::readBytes() in the pinned SDK loops
// over single-byte TLS reads; do not use it for feed bodies.
class FeedStreamReader {
 public:
  void reset(uint32_t started,uint32_t headers,int32_t expected,uint32_t idle,uint32_t deadline,uint32_t bodyDeadline=UINT32_MAX) {
    started_=started;lastProgress_=headers;expected_=expected;idle_=idle;deadline_=deadline;
    headers_=headers;bodyDeadline_=bodyDeadline;
    bytes=chunks=maxGapMs=0;stop=BodyStop::NotRead;
  }
  template<class Stream,class Clock>
  int32_t read(Stream &stream,Clock &clock,uint8_t *dest,size_t maxLen) {
    if(!dest || !maxLen){stop=BodyStop::Error;return kReadError;}
    for(;;) {
      const uint32_t now=clock.nowMs();
      const uint32_t silent=now-lastProgress_;if(silent>maxGapMs)maxGapMs=silent;
      if(uint32_t(now-started_)>=deadline_){stop=BodyStop::Deadline;return kReadTimeout;}
      if(uint32_t(now-headers_)>=bodyDeadline_){stop=BodyStop::BodyDeadline;return kReadTimeout;}
      if(uint32_t(now-lastProgress_)>=idle_){stop=BodyStop::Idle;return kReadTimeout;}
      const int available=stream.available();
      if(available<0) {
        // The SDK stops its socket when TLS close-notify is observed. Preserve
        // EOF semantics; Content-Length validation still rejects truncated feeds.
        if(!stream.connected()){stop=BodyStop::Eof;return kReadEof;}
        stop=BodyStop::Error;return kReadError;
      }
      if(available>0) {
        const size_t want=static_cast<size_t>(available)<maxLen?static_cast<size_t>(available):maxLen;
        const int got=stream.read(dest,want); // Bulk virtual TLS read, NOT readBytes().
        if(got<0 || static_cast<size_t>(got)>want){stop=BodyStop::Error;return kReadError;}
        if(got>0) {
          const uint32_t at=clock.nowMs(),gap=at-lastProgress_;
          if(gap>maxGapMs)maxGapMs=gap;
          lastProgress_=at;bytes+=static_cast<uint32_t>(got);++chunks;
          if(expected_>=0 && bytes==static_cast<uint32_t>(expected_))stop=BodyStop::Complete;
          return got;
        }
      } else if(!stream.connected()){stop=BodyStop::Eof;return kReadEof;}
      clock.wait();
    }
  }
  uint32_t bytes=0,chunks=0,maxGapMs=0;
  BodyStop stop=BodyStop::NotRead;
 private:
  uint32_t started_=0,lastProgress_=0,idle_=0,deadline_=0;
  uint32_t headers_=0,bodyDeadline_=UINT32_MAX;
  int32_t expected_=kUnknownLength;
};
}

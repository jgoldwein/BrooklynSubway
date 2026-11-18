#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace fakeNvs {
struct Blob {
  unsigned char data_[1024]{};size_t n=0;
  size_t size() const {return n;}
  unsigned char *data() {return data_;}
  unsigned char &operator[](size_t at) {return data_[at];}
  void assign(const unsigned char *a,const unsigned char *b) {n=b-a;memcpy(data_,a,n);}
  void clear() {n=0;}
  void pop_back() {--n;}
  bool operator==(const Blob &b) const {return n==b.n && memcmp(data_,b.data_,n)==0;}
};
inline Blob &bytes() {static Blob b;return b;}
inline Blob &extraBytes() {static Blob b;return b;}
inline Blob &forKey(const char *key) {return strcmp(key,"extras_v1")==0?extraBytes():bytes();}
inline uint32_t &alertWord() {static uint32_t v=0;return v;}
inline bool &failWrite() {static bool b=false;return b;}
inline bool &failOpen() {static bool b=false;return b;}
}
class Preferences {
 public:
  bool begin(const char *name,bool) {return strcmp(name,"bkmap_cfg")==0 && !fakeNvs::failOpen();}
  void end() {}
  uint32_t getUInt(const char *key,uint32_t fallback) {return strcmp(key,"station_alerts")==0 && fakeNvs::alertWord()?fakeNvs::alertWord():fallback;}
  size_t putUInt(const char *key,uint32_t value) {if(fakeNvs::failWrite() || strcmp(key,"station_alerts")!=0)return 0;fakeNvs::alertWord()=value;return 4;}
  size_t getBytesLength(const char *key) {return (strcmp(key,"personal_v1")==0 || strcmp(key,"extras_v1")==0)?fakeNvs::forKey(key).size():0;}
  size_t getBytes(const char *key,void *out,size_t n) {if(n!=fakeNvs::forKey(key).size())return 0;memcpy(out,fakeNvs::forKey(key).data(),n);return n;}
  size_t putBytes(const char *key,const void *data,size_t n) {
    if(fakeNvs::failWrite() || (strcmp(key,"personal_v1")!=0 && strcmp(key,"extras_v1")!=0))return 0;
    const auto *p=static_cast<const unsigned char *>(data);fakeNvs::forKey(key).assign(p,p+n);return n;
  }
};

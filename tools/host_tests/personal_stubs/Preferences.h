#pragma once
#include <stddef.h>
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
inline bool &failWrite() {static bool b=false;return b;}
inline bool &failOpen() {static bool b=false;return b;}
}
class Preferences {
 public:
  bool begin(const char *name,bool) {return strcmp(name,"bkmap_cfg")==0 && !fakeNvs::failOpen();}
  void end() {}
  size_t getBytesLength(const char *key) {return strcmp(key,"personal_v1")==0?fakeNvs::bytes().size():0;}
  size_t getBytes(const char *,void *out,size_t n) {if(n!=fakeNvs::bytes().size())return 0;memcpy(out,fakeNvs::bytes().data(),n);return n;}
  size_t putBytes(const char *key,const void *data,size_t n) {
    if(fakeNvs::failWrite() || strcmp(key,"personal_v1")!=0)return 0;
    const auto *p=static_cast<const unsigned char *>(data);fakeNvs::bytes().assign(p,p+n);return n;
  }
};

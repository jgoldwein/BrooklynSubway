// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

// Host tests for the signed OTA package installer and upload authorization.
// Packages are produced by tools/sign_firmware.py with real ECDSA P-256 keys;
// signatures are verified through a Python oracle over the exact bytes the
// installer builds. Usage: ota_test <fixtures> <python> <oracle.py> <pubhex>

#include "admin_auth.h"
#include "ota_package.h"
#include "ota_upload_gate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

using namespace brooklyn::ota;

namespace {

int checks = 0;
int failures = 0;

#define CHECK(condition)                                                     \
  do {                                                                       \
    ++checks;                                                                \
    if (!(condition)) {                                                      \
      ++failures;                                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);            \
    }                                                                        \
  } while (0)

using Bytes = std::vector<uint8_t>;

std::string gDir, gPython, gOracle, gPubHex;
int gOracleCalls = 0;

Bytes readFile(const std::string &name) {
  Bytes out;
  FILE *f = fopen((gDir + "/" + name).c_str(), "rb");
  if (f == nullptr) {
    printf("FAIL cannot read fixture %s\n", name.c_str());
    ++failures;
    return out;
  }
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
  fclose(f);
  return out;
}

void writeFile(const std::string &path, const uint8_t *data, size_t length) {
  FILE *f = fopen(path.c_str(), "wb");
  if (f == nullptr) return;
  fwrite(data, 1, length, f);
  fclose(f);
}

// ---- portable SHA-256 for the host ----
class SoftSha256 : public Sha256Stream {
 public:
  void begin() override {
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(h_, init, sizeof(h_));
    length_ = 0;
    fill_ = 0;
  }
  void update(const uint8_t *data, size_t length) override {
    length_ += length;
    while (length-- != 0) {
      block_[fill_++] = *data++;
      if (fill_ == 64) {
        compress();
        fill_ = 0;
      }
    }
  }
  void finish(uint8_t out[kDigestSize]) override {
    const uint64_t bits = length_ * 8;
    block_[fill_++] = 0x80;
    if (fill_ > 56) {
      while (fill_ < 64) block_[fill_++] = 0;
      compress();
      fill_ = 0;
    }
    while (fill_ < 56) block_[fill_++] = 0;
    for (int i = 7; i >= 0; --i) block_[fill_++] = static_cast<uint8_t>(bits >> (i * 8));
    compress();
    for (int i = 0; i < 8; ++i) {
      out[i * 4] = static_cast<uint8_t>(h_[i] >> 24);
      out[i * 4 + 1] = static_cast<uint8_t>(h_[i] >> 16);
      out[i * 4 + 2] = static_cast<uint8_t>(h_[i] >> 8);
      out[i * 4 + 3] = static_cast<uint8_t>(h_[i]);
    }
  }

 private:
  static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  void compress() {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(block_[i * 4]) << 24) | (static_cast<uint32_t>(block_[i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(block_[i * 4 + 2]) << 8) | block_[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
    for (int i = 0; i < 64; ++i) {
      const uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
  }
  uint32_t h_[8];
  uint8_t block_[64];
  size_t fill_ = 0;
  uint64_t length_ = 0;
};

class OracleVerifier : public SignatureVerifier {
 public:
  bool verify(const uint8_t publicKey[kPublicKeySize], const uint8_t *message,
              size_t messageLength,
              const uint8_t signature[kSignatureSize]) override {
    ++gOracleCalls;
    char hex[kPublicKeySize * 2 + 1];
    for (size_t i = 0; i < kPublicKeySize; ++i) snprintf(hex + i * 2, 3, "%02x", publicKey[i]);
    const std::string msgPath = gDir + "/_msg.bin";
    const std::string sigPath = gDir + "/_sig.bin";
    writeFile(msgPath, message, messageLength);
    writeFile(sigPath, signature, kSignatureSize);
    std::string command = "\"" + gPython + "\" \"" + gOracle + "\" " + hex +
                          " \"" + msgPath + "\" \"" + sigPath + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    return system(command.c_str()) == 0;
  }
};

class MockWriter : public ImageWriter {
 public:
  size_t capacityBytes = 3342336;
  bool failBegin = false;
  bool failWriteAfter = false;
  size_t failWriteAtByte = 0;
  bool failFinalize = false;
  std::vector<std::string> events;
  Bytes written;
  bool open = false;
  bool bootable = false;
  int abortCalls = 0;
  int finalizeCalls = 0;
  int beginCalls = 0;

  size_t capacity() override { return capacityBytes; }
  bool begin(uint32_t) override {
    ++beginCalls;
    events.push_back("begin");
    if (failBegin) return false;
    open = true;
    return true;
  }
  bool write(const uint8_t *data, size_t length) override {
    if (events.empty() || events.back() != "write") events.push_back("write");
    if (failWriteAfter && written.size() + length > failWriteAtByte) return false;
    written.insert(written.end(), data, data + length);
    return true;
  }
  bool finalizeAndActivate() override {
    ++finalizeCalls;
    events.push_back("finalize");
    if (failFinalize) return false;
    open = false;
    bootable = true;
    return true;
  }
  void abort() override {
    ++abortCalls;
    events.push_back("abort");
    open = false;
    bootable = false;
  }
};

TrustedKey gKeys[2];

void setupKeys() {
  memset(gKeys, 0, sizeof(gKeys));
  gKeys[0].id = 1;
  for (size_t i = 0; i < kPublicKeySize; ++i) {
    gKeys[0].publicKey[i] = static_cast<uint8_t>(strtoul(gPubHex.substr(i * 2, 2).c_str(), nullptr, 16));
  }
  gKeys[1].id = 2;  // unprovisioned
}

struct Run {
  Result result = Result::Ok;
  bool succeeded = false;
  bool downgrade = false;
  bool reboot = false;
  MockWriter writer;
  SoftSha256 sha;
  OracleVerifier verifier;
};

// Streams a package in chunks, mimicking the web layer.
void runPackage(Run &run, Target target, const Bytes &package, size_t declaredLength,
                size_t chunk, const TrustedKey *keys = gKeys, size_t keyCount = 2) {
  const InstallerConfig config{target, keys, keyCount, "1.0.0"};
  Installer installer(config, run.sha, run.verifier, run.writer);
  Result r = installer.start(declaredLength);
  size_t offset = 0;
  while (r == Result::Ok && offset < package.size()) {
    const size_t take = package.size() - offset < chunk ? package.size() - offset : chunk;
    r = installer.feed(package.data() + offset, take);
    offset += take;
  }
  if (r == Result::Ok) r = installer.finish();
  run.result = r;
  run.succeeded = installer.succeeded();
  run.downgrade = installer.downgrade();
  run.reboot = installer.rebootPermitted();
}

void expectRejected(const char *name, Result expected, Run &run, bool flashTouched) {
  if (run.result != expected) {
    printf("  %s: got '%s'\n", name, resultName(run.result));
  }
  CHECK(run.result == expected);
  CHECK(!run.succeeded);
  CHECK(!run.reboot);
  CHECK(!run.writer.bootable);
  CHECK(run.writer.finalizeCalls == 0 || expected == Result::WriterFinalize);
  if (!flashTouched) CHECK(run.writer.beginCalls == 0);
  if (run.writer.beginCalls > 0) CHECK(run.writer.abortCalls >= 1 || !run.writer.open);
}

Bytes mutate(Bytes package, size_t index, uint8_t xorMask) {
  package[index] ^= xorMask;
  return package;
}

constexpr size_t kPrefix = kHeaderSize + kSignatureSize;

void testValidPackages() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  const Bytes brooklyn = readFile("brooklyn_ok.bkota");
  const Bytes payload(xiao.begin() + kPrefix, xiao.end());

  for (size_t chunk : {size_t(1), size_t(7), size_t(1436), xiao.size()}) {
    Run run;
    runPackage(run, Target::Xiao, xiao, xiao.size(), chunk);
    CHECK(run.result == Result::Ok);
    CHECK(run.succeeded && run.reboot);
    CHECK(run.writer.bootable);
    CHECK(run.writer.written == payload);
    CHECK(!run.downgrade);
    // authorized -> target verified -> signature -> begin -> stream -> finalize
    CHECK(run.writer.events.size() == 3);
    CHECK(run.writer.events[0] == "begin" && run.writer.events[1] == "write" &&
          run.writer.events[2] == "finalize");
    CHECK(run.writer.abortCalls == 0);
  }
  {
    Run run;
    runPackage(run, Target::BrooklynProduction, brooklyn, brooklyn.size(), 1436);
    CHECK(run.result == Result::Ok && run.reboot && run.writer.bootable);
  }
  {
    Run run;
    const Bytes old = readFile("xiao_old.bkota");
    runPackage(run, Target::Xiao, old, old.size(), 1436);
    CHECK(run.result == Result::Ok && run.downgrade);  // signed downgrade allowed with warning
  }
  CHECK(compareVersions("1.2.3", "1.2.3") == 0);
  CHECK(compareVersions("1.10.0", "1.9.9") == 1);
  CHECK(compareVersions("0.9", "1.0.0") == -1);
  CHECK(compareVersions("Phase 4V", "1.0.0") == 2);
  CHECK(compareVersions("1.0.0-rc1", "1.0.0") == 2);
  CHECK(Installer::scratchBytes() == 148);
}

void testCrossTarget() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  const Bytes brooklyn = readFile("brooklyn_ok.bkota");
  Run a;
  runPackage(a, Target::BrooklynProduction, xiao, xiao.size(), 1436);
  expectRejected("xiao->prod", Result::WrongTarget, a, false);
  Run b;
  runPackage(b, Target::Xiao, brooklyn, brooklyn.size(), 1436);
  expectRejected("prod->xiao", Result::WrongTarget, b, false);
  CHECK(a.writer.events.empty() && b.writer.events.empty());
}

void testSignatureAndDigest() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  {
    Run run;
    const Bytes bad = mutate(xiao, kHeaderSize + 5, 0x01);  // signature byte
    runPackage(run, Target::Xiao, bad, bad.size(), 1436);
    expectRejected("bad signature", Result::BadSignature, run, false);
  }
  {
    Run run;
    const Bytes bad = mutate(xiao, 30, 0x80);  // digest byte inside signed header
    runPackage(run, Target::Xiao, bad, bad.size(), 1436);
    expectRejected("tampered header digest", Result::BadSignature, run, false);
  }
  {
    Run run;
    const Bytes bad = mutate(xiao, 56, 0x01);  // version byte inside signed header
    runPackage(run, Target::Xiao, bad, bad.size(), 1436);
    expectRejected("tampered version", Result::BadSignature, run, false);
  }
  {
    Run run;
    const Bytes bad = mutate(xiao, kPrefix + 100, 0x01);  // corrupted firmware byte
    runPackage(run, Target::Xiao, bad, bad.size(), 1436);
    expectRejected("corrupted payload", Result::DigestMismatch, run, true);
    CHECK(run.writer.abortCalls == 1 && run.writer.finalizeCalls == 0);
  }
  {
    Run run;
    const Bytes other = readFile("wrong_key.bkota");
    runPackage(run, Target::Xiao, other, other.size(), 1436);
    expectRejected("signed by other key", Result::BadSignature, run, false);
  }
  {
    Run run;
    const Bytes other = readFile("unknown_key.bkota");
    runPackage(run, Target::Xiao, other, other.size(), 1436);
    expectRejected("unknown key id", Result::UnknownKey, run, false);
  }
  {
    Run run;
    const Bytes other = readFile("wrong_digest.bkota");
    runPackage(run, Target::Xiao, other, other.size(), 1436);
    expectRejected("signed wrong digest", Result::DigestMismatch, run, true);
  }
  {
    Run run;
    const Bytes other = readFile("not_image.bkota");
    runPackage(run, Target::Xiao, other, other.size(), 1436);
    expectRejected("payload not an image", Result::BadImageMagic, run, true);
  }
  // Unsigned raw firmware is never accepted.
  {
    Run run;
    const Bytes raw(xiao.begin() + kPrefix, xiao.end());
    runPackage(run, Target::Xiao, raw, raw.size(), 1436);
    expectRejected("unsigned raw", Result::BadMagic, run, false);
  }
  CHECK(gOracleCalls > 0);
}

void testMalformed() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  struct Case {
    const char *name;
    size_t index;
    uint8_t mask;
    Result expected;
  } cases[] = {
      {"magic", 0, 0x01, Result::BadMagic},
      {"format version", 8, 0x02, Result::UnsupportedFormat},
      {"target id", 12, 0x04, Result::WrongTarget},
      {"flags network bit cleared", 13, 0x01, Result::BadFlags},
      {"flags physical bit set", 13, 0x02, Result::BadFlags},
      {"reserved nonzero", 14, 0x01, Result::BadFlags},
      {"version space", 53, 0x20, Result::BadVersionString},  // '.' -> 0x0e control
      {"version trailing junk", 83, 0x41, Result::BadVersionString},
      {"payload size grows", 16, 0x01, Result::LengthMismatch},
  };
  for (const Case &c : cases) {
    Run run;
    const Bytes bad = mutate(xiao, c.index, c.mask);
    runPackage(run, Target::Xiao, bad, bad.size(), 1436);
    if (run.result != c.expected) printf("  case '%s': got '%s'\n", c.name, resultName(run.result));
    CHECK(run.result == c.expected);
    CHECK(!run.succeeded && !run.writer.bootable && run.writer.beginCalls == 0);
  }
  {
    Run run;  // empty payload size
    Bytes bad = xiao;
    bad[16] = bad[17] = bad[18] = bad[19] = 0;
    runPackage(run, Target::Xiao, bad, bad.size(), 1436);
    CHECK(run.result == Result::EmptyPayload && run.writer.beginCalls == 0);
  }
  {
    Run run;  // zero-length upload
    runPackage(run, Target::Xiao, Bytes(), 0, 1436);
    CHECK(run.result == Result::EmptyPayload && !run.succeeded);
  }
  {
    Run run;  // header only, no signature or payload
    const Bytes header(xiao.begin(), xiao.begin() + kHeaderSize);
    runPackage(run, Target::Xiao, header, header.size(), 1436);
    CHECK(run.result == Result::Truncated && !run.succeeded);
  }
}

void testLengthAndTruncation() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  {
    Run run;  // truncated stream, full declared length
    const Bytes cut(xiao.begin(), xiao.end() - 10);
    runPackage(run, Target::Xiao, cut, xiao.size(), 1436);
    expectRejected("truncated payload", Result::Truncated, run, true);
    CHECK(run.writer.abortCalls == 1);
  }
  {
    Run run;  // truncated inside signature
    const Bytes cut(xiao.begin(), xiao.begin() + kHeaderSize + 20);
    runPackage(run, Target::Xiao, cut, xiao.size(), 1436);
    expectRejected("truncated signature", Result::Truncated, run, false);
  }
  {
    Run run;  // declared length shorter than header says
    runPackage(run, Target::Xiao, xiao, xiao.size() - 1, 1436);
    expectRejected("declared too short", Result::LengthMismatch, run, false);
  }
  {
    Run run;  // extra bytes after the payload
    Bytes extra = xiao;
    extra.push_back(0);
    runPackage(run, Target::Xiao, extra, xiao.size(), 1436);
    expectRejected("excess data", Result::ExcessData, run, true);
    CHECK(run.writer.abortCalls == 1);
  }
}

void testWriterAndKeys() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  {
    Run run;  // oversized for the slot
    run.writer.capacityBytes = 1000;
    runPackage(run, Target::Xiao, xiao, xiao.size(), 1436);
    expectRejected("oversized", Result::PayloadTooLarge, run, false);
  }
  {
    Run run;
    run.writer.failBegin = true;
    runPackage(run, Target::Xiao, xiao, xiao.size(), 1436);
    CHECK(run.result == Result::WriterBegin && !run.writer.bootable && !run.reboot);
  }
  {
    Run run;
    run.writer.failWriteAfter = true;
    run.writer.failWriteAtByte = 2000;
    runPackage(run, Target::Xiao, xiao, xiao.size(), 1436);
    CHECK(run.result == Result::WriterWrite && !run.writer.bootable && !run.reboot);
    CHECK(run.writer.abortCalls == 1 && run.writer.finalizeCalls == 0);
  }
  {
    Run run;
    run.writer.failFinalize = true;
    runPackage(run, Target::Xiao, xiao, xiao.size(), 1436);
    CHECK(run.result == Result::WriterFinalize && !run.writer.bootable && !run.reboot);
    CHECK(run.writer.abortCalls == 1);
  }
  {
    Run run;  // no provisioned key: nothing is trusted
    TrustedKey none[2] = {{1, {0}}, {2, {0}}};
    runPackage(run, Target::Xiao, xiao, xiao.size(), 1436, none, 2);
    expectRejected("no keys", Result::NoTrustedKeys, run, false);
  }
  {
    Run run;  // an all-zero or non-uncompressed slot is never trusted
    TrustedKey bad[2] = {gKeys[0], {2, {0}}};
    bad[0].publicKey[0] = 0x02;
    runPackage(run, Target::Xiao, xiao, xiao.size(), 1436, bad, 2);
    expectRejected("malformed key slot", Result::NoTrustedKeys, run, false);
  }
  {
    Run run;  // a failure must stay failed even if fed more data
    const InstallerConfig config{Target::Xiao, gKeys, 2, "1.0.0"};
    Installer installer(config, run.sha, run.verifier, run.writer);
    CHECK(installer.start(xiao.size()) == Result::Ok);
    Bytes bad = mutate(xiao, 0, 1);
    CHECK(installer.feed(bad.data(), bad.size()) == Result::BadMagic);
    CHECK(installer.feed(xiao.data(), xiao.size()) == Result::BadMagic);
    CHECK(installer.finish() == Result::BadMagic);
    CHECK(!installer.rebootPermitted());
  }
  {
    Run run;  // an aborted (client disconnect) upload never activates
    const InstallerConfig config{Target::Xiao, gKeys, 2, "1.0.0"};
    Installer installer(config, run.sha, run.verifier, run.writer);
    CHECK(installer.start(xiao.size()) == Result::Ok);
    CHECK(installer.feed(xiao.data(), kPrefix + 500) == Result::Ok);
    installer.abort();
    CHECK(run.writer.abortCalls == 1 && !run.writer.bootable);
    CHECK(installer.finish() != Result::Ok && !installer.rebootPermitted());
  }
}

// ---- authorization (real Phase 4V manager, fake storage/crypto) ----
struct MemoryStore {
  bool present = false;
  brooklyn::admin::CredentialRecord record{};
};
bool loadRecord(void *c, brooklyn::admin::CredentialRecord &r, bool &present) {
  auto &s = *static_cast<MemoryStore *>(c);
  present = s.present;
  if (present) r = s.record;
  return true;
}
bool saveRecord(void *c, const brooklyn::admin::CredentialRecord &r) {
  auto &s = *static_cast<MemoryStore *>(c);
  s.record = r;
  s.present = true;
  return true;
}
bool clearRecord(void *c) {
  static_cast<MemoryStore *>(c)->present = false;
  return true;
}
bool fillRandom(void *c, uint8_t *out, size_t length) {
  uint8_t &counter = *static_cast<uint8_t *>(c);
  for (size_t i = 0; i < length; ++i) out[i] = static_cast<uint8_t>(counter + i * 3);
  ++counter;
  return true;
}
bool derive(void *, const char *password, size_t length, const uint8_t *salt, size_t saltLength,
            uint32_t, uint8_t *verifier, size_t verifierLength) {
  uint32_t state = 2166136261u;
  for (size_t i = 0; i < length; ++i) state = (state ^ static_cast<uint8_t>(password[i])) * 16777619u;
  for (size_t i = 0; i < saltLength; ++i) state = (state ^ salt[i]) * 16777619u;
  for (size_t i = 0; i < verifierLength; ++i) {
    state = (state ^ (i + 1)) * 16777619u;
    verifier[i] = static_cast<uint8_t>(state >> ((i % 4) * 8));
  }
  return true;
}

void testAuthorization() {
  const Bytes xiao = readFile("xiao_ok.bkota");
  MemoryStore store;
  uint8_t counter = 9;
  brooklyn::admin::Manager auth({&store, loadRecord, saveRecord, clearRecord},
                                {&counter, fillRandom, derive});
  CHECK(auth.load());
  // Administrator not configured: never authorized.
  CHECK(authorizeUpload(auth, "", "x", 1) == 403);
  CHECK(auth.setPassword("secure-pass-1", "secure-pass-1"));

  const auto login = auth.login("secure-pass-1", 1000);
  CHECK(login.code == brooklyn::admin::LoginResult::Code::Accepted);
  char cookie[96];
  snprintf(cookie, sizeof(cookie), "%s=%s", brooklyn::admin::kSessionCookieName, login.cookieToken);
  char csrf[40] = {};
  CHECK(auth.csrfForSession(cookie, 1001, csrf, sizeof(csrf)));

  CHECK(authorizeUpload(auth, nullptr, csrf, 1002) == 401);          // anonymous
  CHECK(authorizeUpload(auth, "", csrf, 1002) == 401);               // no session cookie
  CHECK(authorizeUpload(auth, "bkmap_session=deadbeef", csrf, 1002) == 401);
  CHECK(authorizeUpload(auth, cookie, nullptr, 1002) == 403);        // no CSRF
  CHECK(authorizeUpload(auth, cookie, "", 1002) == 403);
  CHECK(authorizeUpload(auth, cookie, "0000", 1002) == 403);         // bad CSRF
  CHECK(authorizeUpload(auth, cookie, csrf, 1003) == 0);             // valid

  // Valid authorization only permits starting; the package must still verify.
  Run denied;
  int status = authorizeUpload(auth, nullptr, csrf, 1004);
  if (status == 0) runPackage(denied, Target::Xiao, xiao, xiao.size(), 1436);
  CHECK(denied.writer.events.empty() && !denied.succeeded);

  Run unsignedButAuthorized;
  const Bytes raw(xiao.begin() + kPrefix, xiao.end());
  status = authorizeUpload(auth, cookie, csrf, 1005);
  CHECK(status == 0);
  if (status == 0) runPackage(unsignedButAuthorized, Target::Xiao, raw, raw.size(), 1436);
  CHECK(unsignedButAuthorized.result == Result::BadMagic && !unsignedButAuthorized.writer.bootable);

  // The new endpoint is under the Phase 4V protection policy.
  CHECK(brooklyn::admin::isProtectedEndpoint("GET", "/update"));
  CHECK(brooklyn::admin::isMutationEndpoint("POST", "/update"));
  CHECK(!brooklyn::admin::isMutationEndpoint("GET", "/update"));

  CHECK(authorizeUpload(auth, cookie, csrf,
                        1000 + brooklyn::admin::kSessionLifetimeMs + 1) == 401);  // expired
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 5) {
    printf("usage: ota_test <fixtures> <python> <oracle.py> <pubhex>\n");
    return 2;
  }
  gDir = argv[1];
  gPython = argv[2];
  gOracle = argv[3];
  gPubHex = argv[4];
  setupKeys();
  testValidPackages();
  testCrossTarget();
  testSignatureAndDigest();
  testMalformed();
  testLengthAndTruncation();
  testWriterAndKeys();
  testAuthorization();
  printf("ota tests: %d checks, %d failures (oracle verifications: %d)\n", checks,
         failures, gOracleCalls);
  return failures == 0 ? 0 : 1;
}

// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>
#include <stdint.h>

// Portable signed-firmware package validator and streaming installer.
// No Arduino/ESP headers: hardware access is injected through interfaces so
// the same code runs in host tests.
//
// Package layout (little-endian):
//   header    84 bytes   (signed)
//   signature 64 bytes   raw ECDSA P-256 r||s over the signed message
//   payload   payload_size bytes (ESP32-S3 application image)
// Signed message = "BROOKLYN-MAP-OTA-V1\0" || 84 header bytes.
// The header carries SHA-256(payload), so the signature covers the binary,
// target, flags, size and version.

namespace brooklyn::ota {

constexpr size_t kHeaderSize = 84;
constexpr size_t kSignatureSize = 64;
constexpr size_t kPublicKeySize = 65;  // uncompressed SEC1 point
constexpr size_t kDigestSize = 32;
constexpr size_t kVersionSize = 32;
constexpr uint16_t kFormatVersion = 1;
constexpr uint8_t kFlagNetworkEnabled = 0x01;
constexpr uint8_t kImageMagic = 0xE9;
constexpr char kMagic[8] = {'B', 'K', 'M', 'A', 'P', 'O', 'T', 'A'};
constexpr char kSignDomain[] = "BROOKLYN-MAP-OTA-V1";  // includes trailing NUL
constexpr size_t kSignDomainSize = sizeof(kSignDomain);  // 20
constexpr size_t kSignedMessageSize = kSignDomainSize + kHeaderSize;

enum class Target : uint8_t { BrooklynProduction = 1, Xiao = 2 };

enum class Result : uint8_t {
  Ok = 0,
  Busy,
  NoTrustedKeys,
  BadMagic,
  UnsupportedFormat,
  UnknownKey,
  WrongTarget,
  BadFlags,
  BadVersionString,
  EmptyPayload,
  PayloadTooLarge,
  LengthMismatch,
  BadSignature,
  BadImageMagic,
  WriterBegin,
  WriterWrite,
  Truncated,
  ExcessData,
  DigestMismatch,
  WriterFinalize,
  NotStarted,
};

const char *resultName(Result result);
// HTTP status appropriate for a failed result.
int resultHttpStatus(Result result);

struct TrustedKey {
  uint16_t id;
  uint8_t publicKey[kPublicKeySize];
};

struct Header {
  uint16_t formatVersion;
  uint16_t keyId;
  uint8_t target;
  uint8_t flags;
  uint32_t payloadSize;
  uint8_t payloadSha256[kDigestSize];
  char version[kVersionSize + 1];
};

bool keyProvisioned(const TrustedKey &key);
size_t provisionedKeyCount(const TrustedKey *keys, size_t count);

// Parses and structurally validates 84 header bytes (no signature check).
Result parseHeader(const uint8_t *bytes, Header &out);

// Compares dotted numeric versions; returns -1, 0, 1, or 2 when either side
// is not a comparable dotted-numeric version.
int compareVersions(const char *candidate, const char *current);

class Sha256Stream {
 public:
  virtual ~Sha256Stream() = default;
  virtual void begin() = 0;
  virtual void update(const uint8_t *data, size_t length) = 0;
  virtual void finish(uint8_t out[kDigestSize]) = 0;
};

class SignatureVerifier {
 public:
  virtual ~SignatureVerifier() = default;
  // Verifies ECDSA P-256/SHA-256 over message; signature is raw r||s.
  virtual bool verify(const uint8_t publicKey[kPublicKeySize],
                      const uint8_t *message, size_t messageLength,
                      const uint8_t signature[kSignatureSize]) = 0;
};

class ImageWriter {
 public:
  virtual ~ImageWriter() = default;
  virtual size_t capacity() = 0;  // largest acceptable payload in bytes
  virtual bool begin(uint32_t payloadSize) = 0;
  virtual bool write(const uint8_t *data, size_t length) = 0;
  // Validates the written image and only then makes it bootable.
  virtual bool finalizeAndActivate() = 0;
  // Discards the candidate; it must remain non-bootable.
  virtual void abort() = 0;
};

struct InstallerConfig {
  Target expectedTarget;
  const TrustedKey *keys;
  size_t keyCount;
  const char *currentVersion;
};

// Streaming installer. Order of operations:
//   start -> header -> target/flags/key -> signature -> writer.begin ->
//   payload stream -> digest -> writer.finalizeAndActivate.
// The signature is verified before any flash write. Nothing becomes bootable
// before the digest matches and the writer validates the image.
class Installer {
 public:
  Installer(const InstallerConfig &config, Sha256Stream &sha,
            SignatureVerifier &verifier, ImageWriter &writer);

  Result start(size_t contentLength);
  Result feed(const uint8_t *data, size_t length);
  Result finish();
  void abort();

  bool succeeded() const { return state_ == State::Done; }
  bool rebootPermitted() const { return state_ == State::Done; }
  Result failure() const { return failure_; }
  const Header &header() const { return header_; }
  bool headerValid() const { return headerValid_; }
  bool downgrade() const { return downgrade_; }
  bool versionComparable() const { return versionComparable_; }
  size_t payloadReceived() const { return payloadReceived_; }
  // Largest scratch buffer owned by the installer itself.
  static constexpr size_t scratchBytes() {
    return kHeaderSize + kSignatureSize;
  }

 private:
  enum class State : uint8_t { Idle, Header, Signature, Payload, Done, Failed };

  Result fail(Result result);
  Result consumeHeader();
  Result consumeSignature();

  InstallerConfig config_;
  Sha256Stream &sha_;
  SignatureVerifier &verifier_;
  ImageWriter &writer_;
  State state_ = State::Idle;
  Result failure_ = Result::Ok;
  size_t contentLength_ = 0;
  size_t fill_ = 0;
  uint8_t headerBytes_[kHeaderSize];
  uint8_t signature_[kSignatureSize];
  Header header_{};
  bool headerValid_ = false;
  bool writerStarted_ = false;
  bool downgrade_ = false;
  bool versionComparable_ = false;
  size_t payloadReceived_ = 0;
};

}  // namespace brooklyn::ota

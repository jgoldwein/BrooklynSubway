// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0

#include "ota_package.h"

#include <string.h>

namespace brooklyn::ota {
namespace {

uint16_t readU16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t readU32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

bool digestsEqual(const uint8_t *a, const uint8_t *b) {
  uint8_t difference = 0;
  for (size_t i = 0; i < kDigestSize; ++i) difference |= a[i] ^ b[i];
  return difference == 0;
}

// Parses ^[0-9]+(\.[0-9]+)*$ into up to 8 components.
bool parseDotted(const char *text, uint32_t (&parts)[8], size_t &count) {
  count = 0;
  if (text == nullptr || *text == '\0') return false;
  while (true) {
    if (*text < '0' || *text > '9' || count == 8) return false;
    uint32_t value = 0;
    while (*text >= '0' && *text <= '9') {
      value = value * 10 + static_cast<uint32_t>(*text - '0');
      if (value > 999999) return false;
      ++text;
    }
    parts[count++] = value;
    if (*text == '\0') return true;
    if (*text != '.') return false;
    ++text;
  }
}

}  // namespace

const char *resultName(Result result) {
  switch (result) {
    case Result::Ok: return "ok";
    case Result::Busy: return "update already in progress";
    case Result::NoTrustedKeys:
      return "no firmware signing key is provisioned on this device";
    case Result::BadMagic: return "not a Brooklyn Map firmware package";
    case Result::UnsupportedFormat: return "unsupported package format version";
    case Result::UnknownKey: return "package signed by an untrusted key";
    case Result::WrongTarget: return "package is for a different hardware target";
    case Result::BadFlags: return "package flags are not acceptable";
    case Result::BadVersionString: return "malformed package version";
    case Result::EmptyPayload: return "package contains no firmware";
    case Result::PayloadTooLarge: return "firmware does not fit the update slot";
    case Result::LengthMismatch: return "upload length does not match package";
    case Result::BadSignature: return "signature verification failed";
    case Result::BadImageMagic: return "payload is not an ESP32 application image";
    case Result::WriterBegin: return "unable to start flash update";
    case Result::WriterWrite: return "flash write failed";
    case Result::Truncated: return "package is truncated";
    case Result::ExcessData: return "package contains unexpected extra data";
    case Result::DigestMismatch: return "firmware digest does not match";
    case Result::WriterFinalize: return "firmware image validation failed";
    case Result::NotStarted: return "update was not started";
  }
  return "unknown error";
}

int resultHttpStatus(Result result) {
  switch (result) {
    case Result::Ok: return 200;
    case Result::Busy: return 409;
    case Result::PayloadTooLarge: return 413;
    case Result::WriterBegin:
    case Result::WriterWrite:
    case Result::WriterFinalize: return 500;
    case Result::NoTrustedKeys: return 503;
    default: return 422;
  }
}

bool keyProvisioned(const TrustedKey &key) {
  for (size_t i = 0; i < kPublicKeySize; ++i) {
    if (key.publicKey[i] != 0) return true;
  }
  return false;
}

size_t provisionedKeyCount(const TrustedKey *keys, size_t count) {
  size_t provisioned = 0;
  for (size_t i = 0; keys != nullptr && i < count; ++i) {
    if (keyProvisioned(keys[i]) && keys[i].publicKey[0] == 0x04) ++provisioned;
  }
  return provisioned;
}

Result parseHeader(const uint8_t *bytes, Header &out) {
  memset(&out, 0, sizeof(out));
  if (memcmp(bytes, kMagic, sizeof(kMagic)) != 0) return Result::BadMagic;
  out.formatVersion = readU16(bytes + 8);
  if (out.formatVersion != kFormatVersion) return Result::UnsupportedFormat;
  out.keyId = readU16(bytes + 10);
  out.target = bytes[12];
  out.flags = bytes[13];
  if (out.flags != kFlagNetworkEnabled || readU16(bytes + 14) != 0) {
    return Result::BadFlags;
  }
  out.payloadSize = readU32(bytes + 16);
  memcpy(out.payloadSha256, bytes + 20, kDigestSize);
  const uint8_t *version = bytes + 52;
  size_t length = 0;
  while (length < kVersionSize && version[length] != 0) {
    if (version[length] < 0x21 || version[length] > 0x7E) {
      return Result::BadVersionString;
    }
    ++length;
  }
  if (length == 0) return Result::BadVersionString;
  for (size_t i = length; i < kVersionSize; ++i) {
    if (version[i] != 0) return Result::BadVersionString;
  }
  memcpy(out.version, version, length);
  if (out.payloadSize == 0) return Result::EmptyPayload;
  return Result::Ok;
}

int compareVersions(const char *candidate, const char *current) {
  uint32_t a[8];
  uint32_t b[8];
  size_t an = 0;
  size_t bn = 0;
  if (!parseDotted(candidate, a, an) || !parseDotted(current, b, bn)) return 2;
  const size_t n = an > bn ? an : bn;
  for (size_t i = 0; i < n; ++i) {
    const uint32_t x = i < an ? a[i] : 0;
    const uint32_t y = i < bn ? b[i] : 0;
    if (x != y) return x < y ? -1 : 1;
  }
  return 0;
}

Installer::Installer(const InstallerConfig &config, Sha256Stream &sha,
                     SignatureVerifier &verifier, ImageWriter &writer)
    : config_(config), sha_(sha), verifier_(verifier), writer_(writer) {
  memset(headerBytes_, 0, sizeof(headerBytes_));
  memset(signature_, 0, sizeof(signature_));
}

Result Installer::fail(Result result) {
  if (writerStarted_) {
    writer_.abort();
    writerStarted_ = false;
  }
  if (state_ != State::Failed) failure_ = result;
  state_ = State::Failed;
  return failure_;
}

Result Installer::start(size_t contentLength) {
  if (state_ != State::Idle) return Result::Busy;
  if (provisionedKeyCount(config_.keys, config_.keyCount) == 0) {
    return fail(Result::NoTrustedKeys);
  }
  if (contentLength < kHeaderSize + kSignatureSize + 1) {
    return fail(contentLength == 0 ? Result::EmptyPayload : Result::Truncated);
  }
  contentLength_ = contentLength;
  fill_ = 0;
  state_ = State::Header;
  return Result::Ok;
}

Result Installer::consumeHeader() {
  Result parsed = parseHeader(headerBytes_, header_);
  if (parsed != Result::Ok) return parsed;
  if (header_.target != static_cast<uint8_t>(config_.expectedTarget)) {
    return Result::WrongTarget;
  }
  bool keyFound = false;
  for (size_t i = 0; i < config_.keyCount; ++i) {
    if (config_.keys[i].id == header_.keyId &&
        keyProvisioned(config_.keys[i]) &&
        config_.keys[i].publicKey[0] == 0x04) {
      keyFound = true;
    }
  }
  if (!keyFound) return Result::UnknownKey;
  if (header_.payloadSize > writer_.capacity()) return Result::PayloadTooLarge;
  if (contentLength_ != kHeaderSize + kSignatureSize + header_.payloadSize) {
    return Result::LengthMismatch;
  }
  headerValid_ = true;
  const int order = compareVersions(header_.version, config_.currentVersion);
  versionComparable_ = order != 2;
  downgrade_ = order == -1;
  return Result::Ok;
}

Result Installer::consumeSignature() {
  uint8_t message[kSignedMessageSize];
  memcpy(message, kSignDomain, kSignDomainSize);
  memcpy(message + kSignDomainSize, headerBytes_, kHeaderSize);
  bool verified = false;
  for (size_t i = 0; i < config_.keyCount && !verified; ++i) {
    const TrustedKey &key = config_.keys[i];
    if (key.id != header_.keyId || !keyProvisioned(key) ||
        key.publicKey[0] != 0x04) {
      continue;
    }
    verified = verifier_.verify(key.publicKey, message, sizeof(message),
                                signature_);
  }
  if (!verified) return Result::BadSignature;
  if (!writer_.begin(header_.payloadSize)) return Result::WriterBegin;
  writerStarted_ = true;
  sha_.begin();
  return Result::Ok;
}

Result Installer::feed(const uint8_t *data, size_t length) {
  if (state_ == State::Idle) return Result::NotStarted;
  if (state_ == State::Failed) return failure_;
  if (state_ == State::Done) return fail(Result::ExcessData);
  if (data == nullptr && length != 0) return fail(Result::Truncated);

  while (length != 0) {
    if (state_ == State::Header) {
      const size_t take = length < kHeaderSize - fill_ ? length : kHeaderSize - fill_;
      memcpy(headerBytes_ + fill_, data, take);
      fill_ += take;
      data += take;
      length -= take;
      if (fill_ == kHeaderSize) {
        const Result r = consumeHeader();
        if (r != Result::Ok) return fail(r);
        fill_ = 0;
        state_ = State::Signature;
      }
    } else if (state_ == State::Signature) {
      const size_t take =
          length < kSignatureSize - fill_ ? length : kSignatureSize - fill_;
      memcpy(signature_ + fill_, data, take);
      fill_ += take;
      data += take;
      length -= take;
      if (fill_ == kSignatureSize) {
        const Result r = consumeSignature();
        if (r != Result::Ok) return fail(r);
        fill_ = 0;
        state_ = State::Payload;
      }
    } else {
      if (payloadReceived_ + length > header_.payloadSize) {
        return fail(Result::ExcessData);
      }
      if (payloadReceived_ == 0 && data[0] != kImageMagic) {
        return fail(Result::BadImageMagic);
      }
      sha_.update(data, length);
      if (!writer_.write(data, length)) return fail(Result::WriterWrite);
      payloadReceived_ += length;
      data += length;
      length = 0;
    }
  }
  return Result::Ok;
}

Result Installer::finish() {
  if (state_ == State::Idle) return Result::NotStarted;
  if (state_ == State::Failed) return failure_;
  if (state_ == State::Done) return Result::Ok;
  if (state_ != State::Payload || payloadReceived_ != header_.payloadSize) {
    return fail(Result::Truncated);
  }
  uint8_t digest[kDigestSize];
  sha_.finish(digest);
  if (!digestsEqual(digest, header_.payloadSha256)) {
    return fail(Result::DigestMismatch);
  }
  if (!writer_.finalizeAndActivate()) return fail(Result::WriterFinalize);
  writerStarted_ = false;
  state_ = State::Done;
  return Result::Ok;
}

void Installer::abort() {
  if (state_ == State::Done) return;
  if (state_ == State::Idle || state_ == State::Failed) {
    if (state_ == State::Failed) return;
  }
  fail(Result::Truncated);
}

}  // namespace brooklyn::ota

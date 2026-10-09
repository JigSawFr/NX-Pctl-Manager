// sha256 — SHA-256 (FIPS 180-4), fed in pieces, to check a download against
// the digest GitHub publishes for a release asset. Plain C++ for the host
// tests (tests/dev_builds).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class Sha256
{
  public:
    Sha256();
    void update(const void* data, size_t size);
    // 64 lower-case hex digits. The object is spent afterwards.
    std::string hex();

  private:
    void block(const uint8_t* p);

    std::array<uint32_t, 8> h;
    uint8_t                 buf[64];
    size_t                  used = 0;
    uint64_t                total = 0;
};

// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/zip_read.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <zlib.h>

namespace zip_read
{

namespace
{
uint32_t u16(const std::vector<unsigned char>& b, size_t at) { return b[at] | b[at + 1] << 8; }
uint32_t u32(const std::vector<unsigned char>& b, size_t at)
{
    return b[at] | b[at + 1] << 8 | b[at + 2] << 16 | (uint32_t)b[at + 3] << 24;
}

bool fail(std::string* error, const std::string& why)
{
    if (error) *error = why;
    return false;
}

struct Entry
{
    uint32_t method = 0, crc = 0, packed = 0, size = 0, local = 0;
};

// The central directory's record of `name`.
bool find(const std::vector<unsigned char>& z, const std::string& name, Entry* e, std::string* error)
{
    // End of central directory: 22 bytes, then a comment of up to 64 KiB.
    if (z.size() < 22) return fail(error, "not a zip archive");
    size_t eocd = std::string::npos;
    for (size_t i = z.size() - 22 + 1; i-- > 0 && z.size() - i <= 22 + 0xFFFF;)
        if (u32(z, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd == std::string::npos) return fail(error, "not a zip archive");
    const uint32_t count = u16(z, eocd + 10);
    size_t at = u32(z, eocd + 16);
    for (uint32_t n = 0; n < count; n++) {
        if (at + 46 > z.size() || u32(z, at) != 0x02014b50) return fail(error, "damaged zip directory");
        const uint32_t name_len = u16(z, at + 28), extra = u16(z, at + 30), comment = u16(z, at + 32);
        if (at + 46 + name_len > z.size()) return fail(error, "damaged zip directory");
        if (name_len == name.size() && std::memcmp(&z[at + 46], name.data(), name_len) == 0) {
            e->method = u16(z, at + 10);
            e->crc = u32(z, at + 16);
            e->packed = u32(z, at + 20);
            e->size = u32(z, at + 24);
            e->local = u32(z, at + 42);
            if (e->packed == 0xFFFFFFFF || e->size == 0xFFFFFFFF || e->local == 0xFFFFFFFF)
                return fail(error, "Zip64 is not supported");
            return true;
        }
        at += 46 + name_len + extra + comment;
    }
    return fail(error, name + " is not in the archive");
}

bool write_all(FILE* f, const unsigned char* p, size_t n, uLong* crc)
{
    *crc = crc32(*crc, p, (uInt)n);
    return std::fwrite(p, 1, n, f) == n;
}
}   // namespace

bool extract(const std::string& zip_path, const std::string& name, const std::string& out_path, uint64_t max_size,
             std::string* error)
{
    std::vector<unsigned char> z;
    {
        FILE* f = std::fopen(zip_path.c_str(), "rb");
        if (!f) return fail(error, "cannot read the archive");
        unsigned char buf[64 * 1024];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) z.insert(z.end(), buf, buf + n);
        std::fclose(f);
    }
    Entry e;
    if (!find(z, name, &e, error)) return false;
    if (e.size > max_size) return fail(error, name + " is too large");
    if (e.method != 0 && e.method != 8) return fail(error, "unsupported zip compression");
    const size_t local = e.local;
    if (local + 30 > z.size() || u32(z, local) != 0x04034b50) return fail(error, "damaged zip entry");
    const size_t data = local + 30 + u16(z, local + 26) + u16(z, local + 28);
    if (data > z.size() || z.size() - data < e.packed) return fail(error, "damaged zip entry");

    // Owner-writable only (fopen would ask for 0666): an executable lands here.
    const int fd = ::open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE* out = fd >= 0 ? ::fdopen(fd, "wb") : nullptr;
    if (!out) {
        if (fd >= 0) ::close(fd);
        return fail(error, "cannot create " + out_path);
    }
    uLong crc = crc32(0, nullptr, 0);
    uint64_t written = 0;
    bool ok = true;
    if (e.method == 0) {
        ok = e.packed == e.size && write_all(out, &z[data], e.packed, &crc);
        written = e.packed;
    } else {
        z_stream s{};
        ok = inflateInit2(&s, -MAX_WBITS) == Z_OK;   // raw deflate, as zip stores it
        s.next_in = &z[data];
        s.avail_in = e.packed;
        unsigned char buf[64 * 1024];
        int rc = Z_OK;
        while (ok && rc != Z_STREAM_END) {
            s.next_out = buf;
            s.avail_out = sizeof(buf);
            rc = inflate(&s, Z_NO_FLUSH);
            const size_t n = sizeof(buf) - s.avail_out;
            written += n;
            ok = (rc == Z_OK || rc == Z_STREAM_END) && written <= e.size && write_all(out, buf, n, &crc);
            if (rc == Z_OK && n == 0 && s.avail_in == 0) ok = false;   // truncated
        }
        inflateEnd(&s);
    }
    if (std::fclose(out) != 0) ok = false;
    if (ok && (written != e.size || crc != e.crc)) {
        std::remove(out_path.c_str());
        return fail(error, name + " does not match its CRC-32");
    }
    if (!ok) {
        std::remove(out_path.c_str());
        return fail(error, "cannot extract " + name);
    }
    return true;
}

}   // namespace zip_read

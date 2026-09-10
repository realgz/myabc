// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/ipc-protocol/frame.cpp
// 依据：docs/architecture/system-overview.md §4

#include "frame.hpp"

#include <cstdint>

#include "protocol.hpp"

namespace myabc::ipc {

namespace {

// 循环读满 n 字节。返回 kOk / kClosed / kIoError。
FrameStatus ReadExact(const ReadFn& read, void* buf, std::size_t n) {
    auto* p = static_cast<unsigned char*>(buf);
    std::size_t got = 0;
    while (got < n) {
        const std::ptrdiff_t r = read(p + got, n - got);
        if (r == 0) return FrameStatus::kClosed;
        if (r < 0) return FrameStatus::kIoError;
        got += static_cast<std::size_t>(r);
    }
    return FrameStatus::kOk;
}

FrameStatus WriteExact(const WriteFn& write, const void* buf, std::size_t n) {
    const auto* p = static_cast<const unsigned char*>(buf);
    std::size_t put = 0;
    while (put < n) {
        const std::ptrdiff_t w = write(p + put, n - put);
        if (w == 0) return FrameStatus::kClosed;
        if (w < 0) return FrameStatus::kIoError;
        put += static_cast<std::size_t>(w);
    }
    return FrameStatus::kOk;
}

}  // namespace

FrameStatus ReadFrame(const ReadFn& read, std::string& out_body) {
    unsigned char len_le[4];
    if (const auto s = ReadExact(read, len_le, sizeof len_le); s != FrameStatus::kOk) {
        return s;
    }
    const std::uint32_t len = static_cast<std::uint32_t>(len_le[0]) |
                              (static_cast<std::uint32_t>(len_le[1]) << 8) |
                              (static_cast<std::uint32_t>(len_le[2]) << 16) |
                              (static_cast<std::uint32_t>(len_le[3]) << 24);
    if (len > kMaxFrameBytes) return FrameStatus::kTooLarge;

    out_body.resize(len);
    if (len == 0) return FrameStatus::kOk;
    return ReadExact(read, out_body.data(), len);
}

FrameStatus WriteFrame(const WriteFn& write, std::string_view body) {
    if (body.size() > kMaxFrameBytes) return FrameStatus::kTooLarge;

    const std::uint32_t len = static_cast<std::uint32_t>(body.size());
    const unsigned char len_le[4] = {
        static_cast<unsigned char>(len & 0xFF),
        static_cast<unsigned char>((len >> 8) & 0xFF),
        static_cast<unsigned char>((len >> 16) & 0xFF),
        static_cast<unsigned char>((len >> 24) & 0xFF),
    };
    if (const auto s = WriteExact(write, len_le, sizeof len_le); s != FrameStatus::kOk) {
        return s;
    }
    if (body.empty()) return FrameStatus::kOk;
    return WriteExact(write, body.data(), body.size());
}

}  // namespace myabc::ipc

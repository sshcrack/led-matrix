#pragma once

#include "matrix_control/LiveFrameSnapshot.h"

#include <cstdint>
#include <string>

namespace Server::LiveFrameProtocol {

inline void append_u16_le(std::string &out, const std::uint16_t value)
{
    out.push_back(static_cast<char>(value & 0xffU));
    out.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

inline void append_u32_le(std::string &out, const std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((value >> shift) & 0xffU));
}

inline std::string encode(const LiveFrame::Snapshot &snapshot)
{
    std::string body;
    body.reserve(12 + snapshot.rgb.size());
    body.append("LMF1", 4);
    append_u16_le(body, snapshot.width);
    append_u16_le(body, snapshot.height);
    append_u32_le(body, snapshot.sequence);
    body.append(reinterpret_cast<const char *>(snapshot.rgb.data()), snapshot.rgb.size());
    return body;
}

} // namespace Server::LiveFrameProtocol

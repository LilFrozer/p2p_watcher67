
#pragma once

#include <cstdint>
#include <vector>

enum class ModeCrc : uint8_t
{
    func = 0x0,
    table = 1
};

namespace Crc {
    static int CalcCrc(std::vector<uint8_t> &bytes, ModeCrc mode);
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    static void testCrc(std::string strHexBytes);
}

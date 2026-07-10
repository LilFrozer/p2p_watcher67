
#pragma once

#include <vector>
#include <cstdint>

using str = std::string;
using u32 = unsigned;
using u16 = uint16_t;
using u8 = uint8_t;
using vU8 = std::vector<uint8_t>;
static_assert(sizeof(u32)==4, "sizeof(u32)!=4");
static_assert(sizeof(u16)==2, "sizeof(u16)!=2");
static_assert(sizeof(u8)==1, "sizeof(u8)!=1");

// При помещении в буфер строки при сериализации сначала кладем длину, а затем уже объект
// htonl/ntohl -> для того, чтоб передавать корректно на устройство с другой архитектурой

namespace udp_data {
enum class DataTypes : u16 {
    TestMessage = 0x0,
    Image = 0x1
};
struct TestMessage {
    str message{""};
    vU8 serialize() const {
        vU8 buf{};
        u32 strlen = htonl(message.length());
        // !!!куда, что, сколько чего!!!
        buf.insert(buf.end(), reinterpret_cast<u8*>(&strlen), reinterpret_cast<u8*>(&strlen) + 4);
        buf.insert(buf.end(), message.begin(), message.end());
        return buf;
    }
    static TestMessage deserialize( const u8 *buf ) {
        TestMessage res{};

        u32 offset = 0;
        u32 _strlen = 0, strlen = 0;
        std::memcpy(&_strlen, buf + offset, 4);
        strlen = ntohl(_strlen);
        offset += 4;
        res.message.assign(reinterpret_cast<const char*>(buf + offset), strlen);

        return res;
    }
};
}

namespace tcp_data {
enum class DataTypes : u16 {
    TestStruct = 0x0,
    FirstData = 0x1
};
#pragma pack(push, 1)
// -> client opening udp socket with this info
struct FirstData {
    str client_addr{""};
    u16 client_port{0};
    // ...
    vU8 serialize() const {
        vU8 buffer{};
        u32 cl_addr_len = htonl(this->client_addr.length());
        buffer.insert(buffer.end(), reinterpret_cast<u8*>(&cl_addr_len),
            reinterpret_cast<u8*>(&cl_addr_len) + 4);
        buffer.insert(buffer.end(), client_addr.begin(), client_addr.end());

        u16 cl_port = this->client_port;
        buffer.insert(buffer.end(), reinterpret_cast<u8*>(&cl_port),
            reinterpret_cast<u8*>(&cl_port) + 2);

        return buffer;
    }
    static FirstData deserialize( const u8 *buffer ) {
        FirstData res;

        u32 offset = 0;
        u32 cl_addr_len{0};
        memcpy(&cl_addr_len, buffer + offset, 4);
        cl_addr_len = ntohl(cl_addr_len);
        offset += 4;
        res.client_addr.assign(reinterpret_cast<const char*>(buffer + offset), cl_addr_len);
        offset += cl_addr_len;

        u16 cl_port{0};
        memcpy(&cl_port, buffer + offset, 2);
        res.client_port = cl_port;

        return res;
    }
};
#pragma pack(pop)
#pragma pack(push, 1)
struct TestStruct
{
    std::string a{""};
    int b{0};
    double c{0.00};
    std::vector<int> d{};

    std::vector<uint8_t> serilize() const
    {
        std::vector<uint8_t> buffer;

        // !!!htonl -> into big_endian!!!

        // len(str) + str
        uint32_t str_len = htonl(a.length());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&str_len),
                    reinterpret_cast<uint8_t*>(&str_len) + 4);
        buffer.insert(buffer.end(), a.begin(), a.end());

        // int
        int32_t b_net = htonl(b);
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&b_net),
                    reinterpret_cast<uint8_t*>(&b_net) + 4);

        // double
        double this_c = c;
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&this_c),
                    reinterpret_cast<uint8_t*>(&this_c) + 8);

        // size(vector) + vector
        uint32_t vec_size = htonl(d.size());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&vec_size),
                    reinterpret_cast<uint8_t*>(&vec_size) + 4);

        for( int val : d )
        {
            int32_t val_net = htonl(val);
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&val_net),
                        reinterpret_cast<uint8_t*>(&val_net) + 4);
        }

        return buffer;
    }

    static TestStruct deserialize( const u8* data )
    {
        TestStruct result;
        u32 offset = 0;

        // !!!ntohl -> from big_endian!!!

        // lent(str) + str
        u32 str_len;
        memcpy(&str_len, data + offset, 4);
        str_len = ntohl(str_len);
        offset += 4;

        result.a.assign(reinterpret_cast<const char*>(data + offset), str_len);
        offset += str_len;

        // int
        memcpy(&result.b, data + offset, 4);
        result.b = ntohl(result.b);
        offset += 4;

        // double
        memcpy(&result.c, data + offset, 8);
        offset += 8;

        // size(vector) + vector
        uint32_t vec_size;
        memcpy(&vec_size, data + offset, 4);
        vec_size = ntohl(vec_size);
        offset += 4;

        result.d.resize(vec_size);
        for( size_t i=0;i<vec_size;++i )
        {
            memcpy(&result.d[i], data + offset, 4);
            result.d[i] = ntohl(result.d[i]);
            offset += 4;
        }

        return result;
    }
};
#pragma pack(pop)
}
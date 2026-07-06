#ifndef CONNECTION_H
#define CONNECTION_H

#include <iostream>
#include "GroupData.h"

namespace proto_project
{
const u16 kServerHash = 0x1337;
const u16 kServerPort = 1122;

struct PacketHeader
{
    u16 server_hash{kServerHash};

    u32 total_data_size{0x0};
    u16 total_cnt_packets{0x0};
    u16 cur_packet_number{0x0};
    u16 cur_packet_size{0x0};

    u8 isFirst : 1;
    u8 isLast : 1;
    u8 : 6;
};

using phr = PacketHeader;
using dpt = tcp_data::DataTypes;

struct Packet
{
    phr header{};
    dpt d_type{dpt::TestStruct};
    vU8 buffer{};
    static vU8 serialize(Packet &pkt) {
        vU8 buf{};

        buf.insert(buf.end(), reinterpret_cast<u8*>(&pkt.header.server_hash),
                   reinterpret_cast<u8*>(&pkt.header.server_hash) + 2);

        buf.insert(buf.end(), reinterpret_cast<u8*>(&pkt.header.total_data_size),
                   reinterpret_cast<u8*>(&pkt.header.total_data_size) + 4);

        buf.insert(buf.end(), reinterpret_cast<u8*>(&pkt.header.total_cnt_packets),
                   reinterpret_cast<u8*>(&pkt.header.total_cnt_packets) + 2);

        buf.insert(buf.end(), reinterpret_cast<u8*>(&pkt.header.cur_packet_number),
                   reinterpret_cast<u8*>(&pkt.header.cur_packet_number) + 2);

        buf.insert(buf.end(), reinterpret_cast<u8*>(&pkt.header.cur_packet_size),
                   reinterpret_cast<u8*>(&pkt.header.cur_packet_size) + 2);

        u8 flags = (pkt.header.isFirst ? 0x01 : 0x00) |
                   (pkt.header.isLast ? 0x02 : 0x00);
        buf.push_back(flags);
        buf.push_back(0);

        u16 dt = static_cast<u16>(pkt.d_type);
        buf.insert(buf.end(), reinterpret_cast<u8*>(&dt), reinterpret_cast<u8*>(&dt) + 2);

        buf.insert(buf.end(), pkt.buffer.begin(), pkt.buffer.end());

        return buf;
    }
};
}

class IConnection
{
public:
    virtual void connect(const str &host, const uint16_t port) = 0;
    virtual void disconnect() = 0;
    virtual void async_write(proto_project::dpt data_type, const vU8 &buffer) = 0;
    virtual void async_read() = 0;
    virtual void process_packet() = 0;
    virtual ~IConnection() = default;
};

// class SysSockConnection : public Interface
// {
// public:
//     SysSockConnection();
//     ~SysSockConnection() = default;
//     void connect() override final;
//     void disconnect() override final;
// };

#include <boost/asio.hpp>

using boost_tcp = boost::asio::ip::tcp;

class BoostConnection : public IConnection, public std::enable_shared_from_this<BoostConnection>
{
private:
    boost_tcp::socket socket_{nullptr};
    boost_tcp::resolver resolver_{nullptr};
    u32 size_{0};
    vU8 buffer_{};
private:
    void handle_error(const boost::system::error_code& ec, const str& context);
public:
    BoostConnection(boost::asio::io_context &io_context);
    ~BoostConnection();
    void connect(const str &host, const u16 port) override final;
    void disconnect() override final;
    void async_write(proto_project::dpt data_type, const vU8 &buffer) override final;
    void async_read() override final;
    void process_packet() override final;
};

#endif // CONNECTION_H

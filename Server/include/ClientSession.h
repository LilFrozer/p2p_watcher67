
#pragma once

#include "GroupData.h"

namespace proto_project {
    const str kServerAddr{"172.20.10.10"};
    const u16 kServerHash = 0x1337;
    const u16 kServerPort = 1122;

    struct PacketHeader {
        u16 server_hash{kServerHash};

        u32 total_data_size{0x0};
        u16 total_cnt_packets{0x0};
        u16 cur_packet_number{0x0};
        u16 cur_packet_size{0x0};

        u8 isFirst : 1;
        u8 isLast : 1;
        u8 : 6;
    };

    using dte = tcp_data::DataTypes;
    using phr = PacketHeader;

    struct Packet
    {
        phr header{};
        dte d_type{dte::TestStruct};
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

class IClientSession
{
public:
    virtual ~IClientSession() {}
    virtual void start_session() = 0;
    virtual void async_read() = 0;
    virtual void process_packet() = 0;
    virtual void async_send(proto_project::dte dtype, const vU8 &buffer) = 0;
};

#include <boost/asio.hpp>

using boost_udp = boost::asio::ip::udp;
using boost_tcp = boost::asio::ip::tcp;

class BoostClientSession : public IClientSession, public std::enable_shared_from_this<BoostClientSession>
{
protected:
    boost_tcp::socket socket_{nullptr};
    unsigned size_{0};
    vU8 buffer_{};
public:
    BoostClientSession(boost_tcp::socket socket);
    ~BoostClientSession() override {};
    void start_session() override final;
    void async_read() override final;
    void process_packet() override final;
    void async_send(proto_project::dte dtype, const vU8 &buffer) override final;
};


#pragma once

#include "GroupData.h"
#include <boost/asio.hpp>
#include "Logger.h"
#include <boost/signals2.hpp>

namespace Constants {
    const str SERVER_ADDR{"172.20.10.10"};
    const u16 SERVER_HASH{1337};
    const u16 SERVER_PORT{1122};
    const u16 BUFFER_SIZE = 1472;
    const u16 MAX_SIZE_UDP = 1472;
}

namespace proto_project {
#pragma pack(push, 1)
    struct PacketHeader {
        u16 server_hash{Constants::SERVER_HASH};

        u32 total_data_size{0x0};
        u16 total_cnt_packets{0x0};
        u16 cur_packet_number{0x0};
        u16 cur_packet_size{0x0};

        u8 isFirst : 1;
        u8 isLast : 1;
        u8 isCompressed: 1;
        u8 : 5;
    };
#pragma pack(pop)

    using dte = tcp_data::DataTypes;
    using phr = PacketHeader;

#pragma pack(push, 1)
    struct Packet
    {
        phr header{};
        u16 d_type{};
        vU8 buffer{};
        static vU8 serialize( Packet &pkt ) {
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
                    (pkt.header.isLast ? 0x02 : 0x00) |
                    (pkt.header.isCompressed ? 0x03 : 0x00);
            buf.push_back(flags);
            buf.push_back(0);

            u16 dt = static_cast<u16>(pkt.d_type);
            buf.insert(buf.end(), reinterpret_cast<u8*>(&dt), reinterpret_cast<u8*>(&dt) + 2);

            buf.insert(buf.end(), pkt.buffer.begin(), pkt.buffer.end());

            return buf;
        }
    };
#pragma pack(pop)
}

class ITcpClientSession
{
public:
    virtual void startSession() = 0;
    virtual void doRead() = 0;
    virtual void prcsPacket() = 0;
    virtual void doSend( const tcp_data::DataTypes &dtype, const vU8 &buffer ) = 0;
    virtual ~ITcpClientSession() = default;
};

namespace asio = boost::asio;

class AsioTcpClientSession : public ITcpClientSession, public std::enable_shared_from_this<AsioTcpClientSession> {
protected:
    u32 cur_client_id_{0};
    asio::ip::tcp::socket socket_{nullptr};
    u32 size_{0};
    vU8 buffer_{};
    boost::signals2::signal<void(u32)> signalDeleting_;
public:
    explicit AsioTcpClientSession( asio::ip::tcp::socket socket, const u32 &client_id );
    ~AsioTcpClientSession() override = default;
    void startSession() override;
    void doRead() override;
    void prcsPacket() override;
    void doSend( const tcp_data::DataTypes &dtype, const vU8 &buffer ) override;
    boost::signals2::signal<void(u32)> &get_signalDeleting() { return signalDeleting_; }
};

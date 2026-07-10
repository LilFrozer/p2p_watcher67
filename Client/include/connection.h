#ifndef CONNECTION_H
#define CONNECTION_H

#include <iostream>
#include "GroupData.h"
#include <boost/asio.hpp>

namespace Constants {
    const u16 SERVER_HASH = 1337;
    const u16 SERVER_PORT = 1122;
    const u16 BUFFER_SIZE = 1472;
    const u16 MAX_SIZE_UDP = 1472;
}

namespace proto_project
{

#pragma pack(push, 1)
struct PacketHeader
{
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

using phr = PacketHeader;
using dpt = tcp_data::DataTypes;

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
                   (pkt.header.isCompressed) ? 0x03 : 0x00;
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

class ITcpConnection {
public:
    virtual void doConnect( const str &addr, const u16 &port ) = 0;
    virtual void doDisconnect() = 0;
    virtual void asyncWrite( const tcp_data::DataTypes &data_type, const vU8 &buf) = 0;
    virtual void asyncRead() = 0;
    virtual void prcsPacket() = 0;
    virtual void sendUdpData( const udp_data::DataTypes &data_type, const vU8 &buf ) = 0;
    virtual ~ITcpConnection() = default;
};

class IUdpConnection {
public:
    virtual void listen() = 0;
    virtual void sendData( const str &addr, const udp_data::DataTypes &data_type, const vU8 &buf ) = 0;
    virtual void open( const str &addr, const u16 &port ) = 0;
    virtual void close() = 0;
    virtual void prcsPacket() = 0;
    virtual ~IUdpConnection() = default;
};

// class SysSockConnection : public Interface
// {
// public:
//     SysSockConnection();
//     ~SysSockConnection() = default;
//     void connect() override final;
//     void disconnect() override final;
// };

namespace asio = boost::asio;

struct UdpVar {
    asio::ip::udp::socket socket{nullptr};
    asio::ip::udp::endpoint remote_endpoint{};
    vU8 buffer{};
    UdpVar( asio::io_context &ctx ) : socket(ctx), buffer(Constants::BUFFER_SIZE, 0) {}
};

struct TcpVar {
    asio::ip::tcp::socket socket{nullptr};
    asio::ip::tcp::resolver resolver{nullptr};
    u32 size{0};
    vU8 buffer{};
    str server_addr{""};
    TcpVar( asio::io_context &ctx ) : socket(ctx), resolver(ctx) {}
};

class BoostUdpConnection : public IUdpConnection, public std::enable_shared_from_this<BoostUdpConnection> {
private:
    std::unique_ptr<UdpVar> udp_var_{nullptr};
public:
    BoostUdpConnection( asio::io_context &ctx );
    ~BoostUdpConnection() override = default;
    void listen() override;
    void sendData( const str &addr, const udp_data::DataTypes &data_type, const vU8 &buf ) override;
    void open( const str &addr, const u16 &port ) override;
    void close() override;
    void prcsPacket() override;
};

class BoostTcpConnection : public ITcpConnection, public std::enable_shared_from_this<BoostTcpConnection> {
private:
    std::unique_ptr<TcpVar> tcp_var_{nullptr};
    std::shared_ptr<BoostUdpConnection> udp_{nullptr};
public:
    BoostTcpConnection( asio::io_context &ctx );
    ~BoostTcpConnection() override = default;
    void doConnect( const str &addr, const u16 &port ) override;
    void doDisconnect() override;
    void asyncWrite( const tcp_data::DataTypes &data_type, const vU8 &buf) override;
    void asyncRead() override;
    void prcsPacket() override;
    void sendUdpData( const udp_data::DataTypes &data_type, const vU8 &buf ) override;
};

#endif // CONNECTION_H

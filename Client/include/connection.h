#ifndef CONNECTION_H
#define CONNECTION_H

#include <iostream>
#include "GroupData.h"
#include <boost/asio.hpp>

namespace Constants {
    const u16 SERVER_HASH = 1337;
    const u16 SERVER_PORT = 1122;
    const u16 BUFFER_SIZE = 1024;
}

namespace proto_project
{

struct PacketHeader
{
    u16 server_hash{Constants::SERVER_HASH};

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
    virtual void connect( const str &host, const u16 port ) = 0;
    virtual void disconnect() = 0;
    virtual void async_write( proto_project::dpt data_type, const vU8 &buffer ) = 0;
    virtual void async_read() = 0;
    virtual void process_packet() = 0;
    virtual void start_receive() = 0;
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

namespace asio = boost::asio;

struct UdpVar {
    asio::ip::udp::socket socket{nullptr};
    asio::ip::udp::endpoint remote_endpoint{};
    std::array<char, Constants::BUFFER_SIZE> buffer{};
    UdpVar( asio::io_context &ctx ) : socket(ctx) {}
};

struct TcpVar {
    asio::ip::tcp::socket socket{nullptr};
    asio::ip::tcp::resolver resolver{nullptr};
    u32 size{0};
    vU8 buffer{};
    TcpVar( asio::io_context &ctx ) : socket(ctx), resolver(ctx) {}
};

class BoostConnection : public IConnection, public std::enable_shared_from_this<BoostConnection>
{
protected:
    std::unique_ptr<UdpVar> udp_var_{nullptr};
    std::unique_ptr<TcpVar> tcp_var_{nullptr};
public:
    void handle_error( const boost::system::error_code& ec, const str& context );
    BoostConnection( boost::asio::io_context &ctx);
    ~BoostConnection() override;
    void connect( const str &host, const u16 port ) override;
    void disconnect() override;
    void async_write( proto_project::dpt data_type, const vU8 &buffer ) override;
    void async_read() override;
    void process_packet() override;
    void start_receive() override;
};

#endif // CONNECTION_H

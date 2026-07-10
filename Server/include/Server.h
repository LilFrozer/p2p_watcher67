
#pragma once

#include "ClientSession.h"
#include <unordered_map>
#include <atomic>
#include "Logger.h"
#include <chrono>

struct ClientInfo {
    str addr{""};
    u16 port{0};
    bool operator==( const ClientInfo &rhs ) const {
        return addr == rhs.addr && port == rhs.port;
    }
};

class IUdpInterface {
public:
    virtual void open( const str &addr, const u16 &port ) = 0;
    virtual void listen() = 0;
    virtual void close() = 0;
    virtual void sendData( const str &addr, const u16 &port, const udp_data::DataTypes &data_type, const vU8 &buf ) = 0;
    virtual void prcsPacket() = 0;
    virtual ~IUdpInterface() = default;
};

class ITcpInterface {
public:
    virtual void open( const str &addr, const u16 &port ) = 0;
    virtual void listen() = 0;
    virtual void close() = 0;
    virtual ~ITcpInterface() = default;
};

// --- boost::asio ---

namespace asio = boost::asio;
using std::unordered_map;

struct UdpVar {
    asio::ip::udp::socket socket{nullptr};
    asio::ip::udp::endpoint remote_endpoint{};
    vU8 buffer{};
    explicit UdpVar( asio::io_context &ctx ) : socket(ctx), buffer(Constants::BUFFER_SIZE, 0) {}
};

struct TcpVar {
    asio::ip::tcp::acceptor acceptor{nullptr};
    explicit TcpVar( asio::io_context &ctx ) : acceptor(ctx) {}
};

class AsioUdpServer : public IUdpInterface, public std::enable_shared_from_this<AsioUdpServer> {
protected:
    std::unique_ptr<UdpVar> udp_var_{nullptr};
public:
    AsioUdpServer( asio::io_context &ctx );
    ~AsioUdpServer() override = default;
    void open( const str &addr, const u16 &port ) override;
    void listen() override;
    void close() override;
    void sendData( const str &addr, const u16 &port, const udp_data::DataTypes &data_type, const vU8 &buf ) override;
    void prcsPacket() override;
};

class AsioTcpServer : public ITcpInterface, public std::enable_shared_from_this<AsioTcpServer> {
protected:
    std::unique_ptr<AsioUdpServer> udp_server_{nullptr};
    std::unique_ptr<TcpVar> tcp_var_{nullptr};
    unordered_map<u32, ClientInfo> clients_{};
    std::shared_ptr<asio::steady_timer> timer_{nullptr};
public:
    void open( const str &addr, const u16 &port ) override;
    AsioTcpServer(asio::io_context& ctx);
    ~AsioTcpServer() override;
    void listen() override;
    void close() override;
    void startTimer();
    void removeClient( const u32 &client_id );
};


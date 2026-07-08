
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

enum class errors : u8 {
    Success = 0,
};

class IInterface {
public:
    virtual void startListenTcp( const str &ip_str, const u16 &port ) = 0;
    virtual void startListenUdp( const str &ip_str, const u16 &port ) = 0;
    virtual void startTimer() = 0;
    virtual void sendUdpData() = 0;
    virtual ~IInterface() = default;
};

// class PosixServer : public IInterface {
//     std::atomic<bool> isRunning_{true};
//     int tcp_socket_{-1}, udp_socket_{-1};
// public:
//     errors Initialize() override;
//     errors ListenTcp(const str &addr, const u16 &port) override;
//     errors ListenUdp(const str &addr, const u16 &port) override;
//     ~PosixServer() override;
// };

// --- boost::asio ---

namespace asio = boost::asio;
using std::array;
using std::unordered_map;

struct UdpVar {
    asio::ip::udp::socket socket{nullptr};
    array<char, Constants::BUFFER_SIZE> buffer{};
    explicit UdpVar( asio::io_context &ctx ) : socket(ctx) {}
};

struct TcpVar {
    asio::ip::tcp::acceptor acceptor{nullptr};
    explicit TcpVar( asio::io_context &ctx ) : acceptor(ctx) {}
};

class BoostServer : public std::enable_shared_from_this<BoostServer>, public IInterface {
protected:
    std::unique_ptr<UdpVar> udp_var_{nullptr};
    std::unique_ptr<TcpVar> tcp_var_{nullptr};
    unordered_map<u32, ClientInfo> clients_{};
    std::shared_ptr<asio::steady_timer> timer_{nullptr};
public:
    void startListenTcp( const str &ip_str, const u16 &port ) override;
    void startListenUdp( const str &ip_str, const u16 &port ) override;
    BoostServer(asio::io_context& ctx);
    ~BoostServer() override;
    void continueListening();
    void startTimer() override;
    void sendUdpData() override;
    void removeClient( u32 id );
};


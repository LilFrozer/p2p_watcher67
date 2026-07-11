
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

class ISocketManager {
public:
    virtual void open( const str &addr, const u16 &port ) const = 0;
    virtual void listen() = 0;
    virtual void close() const = 0;
    virtual void broadCast( const u16 &data_type, const vU8 &buf ) const = 0;
    virtual void processPacket( const vU8 &buf ) const = 0;
    virtual void createClient( const u32 &client_id, const ClientInfo &i ) = 0;
    virtual void removeClient( const u32 &client_id ) = 0;
    virtual ~ISocketManager() = default;
};

// --- unix::sockets ---

class UnixUdpManager : public ISocketManager {};
class UnixTcpManager : public ISocketManager {};

// --- boost::asio ---

namespace asio = boost::asio;
using std::unordered_map;

struct AsioUdpTypes : std::enable_shared_from_this<AsioUdpTypes> {
    asio::ip::udp::socket socket{nullptr};
    asio::ip::udp::endpoint remote_endpoint{};
    vU8 buffer{};
    explicit AsioUdpTypes( asio::io_context &ctx ) : socket(ctx), buffer(Constants::BUFFER_SIZE, 0) {}
};

class AsioUdpManager : public ISocketManager, public std::enable_shared_from_this<AsioUdpManager> {
protected:
    std::shared_ptr<AsioUdpTypes> types_{nullptr};
    unordered_map<u32, ClientInfo> cl_{};
public:
    explicit AsioUdpManager( asio::io_context &ctx );
    void open( const str &addr, const u16 &port ) const override;
    void listen() override;
    void close() const override;
    void broadCast( const u16 &data_type, const vU8 &buf ) const override;
    void processPacket( const vU8 &buf ) const override;
    void createClient( const u32 &client_id, const ClientInfo &i ) override;
    void removeClient( const u32 &client_id ) override;
    ~AsioUdpManager() override = default;
};

struct AsioTcpTypes : std::enable_shared_from_this<AsioTcpTypes> {
    asio::ip::tcp::acceptor acceptor{nullptr};
    explicit AsioTcpTypes( asio::io_context &ctx ) : acceptor(ctx) {}
};

class AsioTcpManager : public ISocketManager, public std::enable_shared_from_this<AsioTcpManager> {
protected:
    std::shared_ptr<AsioTcpTypes> types_{nullptr};
    std::shared_ptr<AsioUdpManager> udp_server_{nullptr};
    std::shared_ptr<asio::steady_timer> timer_{nullptr};
    unordered_map<u32, std::shared_ptr<AsioTcpClientSession>> cl_{};
public:
    explicit AsioTcpManager( asio::io_context &ctx );
    void open( const str &addr, const u16 &port ) const override;
    void listen() override;
    void close() const override;
    void broadCast( const u16 &data_type, const vU8 &buf ) const override;
    void processPacket( const vU8 &buf ) const override;
    void createClient( const u32 &client_id, const ClientInfo &i ) override;
    void removeClient( const u32 &client_id ) override;
    ~AsioTcpManager() override;
    void startTest() const;
};

/*
 * ---ФАБРИЧНЫЙ МЕТОД---
 */
class Creator {
public:
    virtual ~Creator() = default;
    virtual std::shared_ptr<ISocketManager> factoryMethod() const = 0;
    void Execute( const str &addr, const u16 &port ) const {
        const std::shared_ptr<ISocketManager> ptr = this->factoryMethod();
        ptr->open(addr, port);
        ptr->listen();
        if (dynamic_cast<AsioTcpManager*>(ptr.get())) {
            static_cast<AsioTcpManager*>(ptr.get())->startTest();
        }
    }
};

class AsioCreator : public Creator {
protected:
    asio::io_context *ctx_{nullptr};
public:
    explicit AsioCreator( asio::io_context &ctx ) {
        this->ctx_ = &ctx;
    }
    std::shared_ptr<ISocketManager> factoryMethod() const override {
        if (!ctx_)
            throw std::runtime_error("!ctx");
        return std::make_shared<AsioTcpManager>(*ctx_);
    }
};

class UnixCreator : public Creator {
public:
    std::shared_ptr<ISocketManager> factoryMethod() const override {
        return nullptr;
        // return std::make_shared<UnixTcpManager>();
    }
};

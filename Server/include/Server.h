/*
 *  1. IServer = Имитатор ФАР, аналог VIOD, шлет ответы
 *  2. BoostServer = Сервер, написанный исключительно на boost::asio
 *  3. PosixServer = Сервер, написанный исключительно на системных сокетах
 */

#pragma once

#include "ImitatorFar.h"
#include "ClientSession.h"

class IServer {
protected:
    std::unique_ptr<ImitatorFar> imitator_far_;
public:
    IServer();
    virtual void ListenTcp() = 0;
    virtual ~IServer() {}
};

// --- boost::asio ---

class BoostServer : public std::enable_shared_from_this<BoostServer>, public IServer
{
private:
    // --- udp ---
    boost_udp::socket   udp_socker_{nullptr};
    boost_udp::endpoint udp_endPoint_{};
    std::array<char, 1024> udp_recvBuffer_{};
    // --- tcp ---
    boost_tcp::acceptor tcp_acceptor_;
public:
    void ListenTcp() override;
    BoostServer(boost::asio::io_context& io_context,
                const std::string &addr,
                const uint16_t &port);
    ~BoostServer() override;
};


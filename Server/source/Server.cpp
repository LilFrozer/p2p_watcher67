#include "Server.h"

BoostServer::BoostServer( asio::io_context& ctx ) :
    u(std::make_unique<UdpVar>(ctx))
    , t(std::make_unique<TcpVar>(ctx))
{
    Log::instance()("Initialization...", LoggerMode::info);
}

BoostServer::~BoostServer() {
    boost::system::error_code ec;
    t->acceptor.close(ec);
    u->socket.close(ec);
}

void BoostServer::startListenTcp( const str &ip_str, const u16 &port ) {
    const auto address = asio::ip::make_address(ip_str);
    const asio::ip::tcp::endpoint endpoint(address, port);
    t->acceptor.open(endpoint.protocol());
    t->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    t->acceptor.bind(endpoint);
    t->acceptor.listen(asio::socket_base::max_listen_connections);
    this->continueListening();
}

void BoostServer::startListenUdp( const str &ip_str, const u16 &port ) {
    const auto address = asio::ip::make_address(ip_str);
    const asio::ip::udp::endpoint endpoint(address, port);
    u->socket.open(endpoint.protocol());
    u->socket.set_option(asio::socket_base::reuse_address(true));
    u->socket.bind(endpoint);
}

void BoostServer::continueListening() {
    t->acceptor.async_accept (
    [this](boost::system::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            boost::system::error_code ec;
            const auto remote_ep = socket.remote_endpoint(ec);
            if (!ec) {
                tcp_data::FirstData clInfo{};
                const std::string ip = remote_ep.address().to_string();
                clInfo.client_addr = ip;
                const u16 port = remote_ep.port();
                clInfo.client_port = port;
                const auto cl = std::make_shared<BoostClientSession>(std::move(socket));
                cl->async_send(proto_project::dte::FirstData, clInfo.serialize());
                cl->start_session();
            }
        }
        this->continueListening();
    }
    );
}
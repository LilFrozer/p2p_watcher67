#include "Server.h"

BoostServer::BoostServer( asio::io_context& ctx ) :
    udp_var_(std::make_unique<UdpVar>(ctx))
    , tcp_var_(std::make_unique<TcpVar>(ctx))
{
    Log::instance()("Initialization...", LoggerMode::info);
}

BoostServer::~BoostServer() {
    boost::system::error_code ec;
    if (tcp_var_->acceptor.is_open())
        tcp_var_->acceptor.close(ec);
    if (udp_var_->socket.is_open())
        udp_var_->socket.close(ec);
}

void BoostServer::startListenTcp( const str &ip_str, const u16 &port ) {
    const auto address = asio::ip::make_address(ip_str);
    const asio::ip::tcp::endpoint endpoint(address, port);
    tcp_var_->acceptor.open(endpoint.protocol());
    tcp_var_->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    tcp_var_->acceptor.bind(endpoint);
    tcp_var_->acceptor.listen(asio::socket_base::max_listen_connections);
    this->continueListening();
}

void BoostServer::startListenUdp( const str &ip_str, const u16 &port ) {
    const auto address = asio::ip::make_address(ip_str);
    const asio::ip::udp::endpoint endpoint(address, port);
    udp_var_->socket.open(endpoint.protocol());
    udp_var_->socket.set_option(asio::socket_base::reuse_address(true));
    udp_var_->socket.bind(endpoint);
}

void BoostServer::continueListening() {
    tcp_var_->acceptor.async_accept (
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
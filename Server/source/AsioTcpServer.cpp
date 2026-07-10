#include "Server.h"

AsioTcpServer::AsioTcpServer( asio::io_context &ctx ) : tcp_var_(std::make_unique<TcpVar>(ctx))
                                          , udp_server_(std::make_unique<AsioUdpServer>(ctx))
                                          , timer_(std::make_shared<asio::steady_timer>(ctx)) {
    Log::instance()("initialization AsioTcpServer...", LoggerMode::info);
}

AsioTcpServer::~AsioTcpServer() {
    (this)->close();
}

void AsioTcpServer::open( const str &addr, const u16 &port ) {
    udp_server_->open(addr, port);
    udp_server_->listen();

    if (tcp_var_->acceptor.is_open())
        return;

    const auto address = asio::ip::make_address(addr);
    const asio::ip::tcp::endpoint endpoint(address, port);
    tcp_var_->acceptor.open(endpoint.protocol());
    tcp_var_->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    tcp_var_->acceptor.bind(endpoint);
    tcp_var_->acceptor.listen(asio::socket_base::max_listen_connections);
}

void AsioTcpServer::close() {
    boost::system::error_code ec;
    if (tcp_var_->acceptor.is_open())
        tcp_var_->acceptor.close(ec);
    udp_server_->close();
}

void AsioTcpServer::listen() {
    if (!tcp_var_->acceptor.is_open())
        return;

    tcp_var_->acceptor.async_accept([this](boost::system::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            boost::system::error_code ec;
            const auto remote_ep = socket.remote_endpoint(ec);
            if (!ec) {
                tcp_data::FirstData clInfo{};
                const std::string ip = remote_ep.address().to_string();
                clInfo.client_addr = ip;
                const u16 port = remote_ep.port();
                clInfo.client_port = port;
                static u32 id = 0;
                udp_server_->createClient(id, ClientInfo{clInfo.client_addr, clInfo.client_port});
                const auto cl = std::make_shared<AsioTcpClientSession>(std::move(socket), id);
                cl->signal_deleting.connect(boost::bind(&AsioUdpServer::removeClient, udp_server_.get(), boost::placeholders::_1));
                cl->doSend(proto_project::dte::FirstData, clInfo.serialize());
                cl->startSession();
                ++id;
            }
        }
        listen();
    }
    );
}

void AsioTcpServer::startTimer() {
    timer_->expires_after(std::chrono::seconds(5));
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            udp_data::TestMessage a{};
            a.message = "Hello from Boost.Asio! Timestamp: " + std::to_string(std::time(nullptr));
            udp_server_->broadCast(udp_data::DataTypes::TestMessage, a.serialize());
            startTimer();
        } else {
            Log::instance()("timer err:" + ec.message(), LoggerMode::error);
        }
    });
}
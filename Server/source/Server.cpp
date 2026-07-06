#include "Server.h"

BoostServer::BoostServer( asio::io_context& ctx ) :
    udp_var_(std::make_unique<UdpVar>(ctx))
    , tcp_var_(std::make_unique<TcpVar>(ctx))
    , timer_(std::make_shared<asio::steady_timer>(ctx))
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
                static u32 id = 0;
                clients_[id] = ClientInfo{clInfo.client_addr, clInfo.client_port};
                const auto cl = std::make_shared<BoostClientSession>(std::move(socket), id);
                cl->get_deleting_signal().connect(boost::bind(&BoostServer::removeClient, this, boost::placeholders::_1));
                cl->async_send(proto_project::dte::FirstData, clInfo.serialize());
                cl->start_session();
                ++id;
            }
        }
        this->continueListening();
    }
    );
}

void BoostServer::removeClient(u32 id) {
    Log::instance()("Removing client. id:" + std::to_string(id), LoggerMode::info);
    auto it = clients_.find(id);
    clients_.erase(it);
}

void BoostServer::startTimer() {
    timer_->expires_after(std::chrono::seconds(5));
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            sendUdpData();
            startTimer();
        } else {
            Log::instance()("timer err:" + ec.message(), LoggerMode::error);
        }
    });
}

void BoostServer::sendUdpData() {
    str message = "Hello from Boost.Asio! Timestamp: " + std::to_string(std::time(nullptr));
    for (const auto &i : clients_) {
        const auto address = asio::ip::make_address(i.second.addr);
        const asio::ip::udp::endpoint _endpoint(address, i.second.port);
        udp_var_->socket.async_send_to(asio::buffer(message), _endpoint,
        [](const boost::system::error_code& ec, std::size_t /*bytes_sent*/) {
            if (ec) {
                Log::instance()("Send error:" + ec.message(), LoggerMode::error);
            } else {
                Log::instance()("Message send successfully.", LoggerMode::info);
            }
        });
    }
}
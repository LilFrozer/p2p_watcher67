#include "Server.h"

AsioTcpManager::AsioTcpManager( asio::io_context &ctx )
    : types_(std::make_unique<AsioTcpTypes>(ctx))
    , udp_server_(std::make_unique<AsioUdpManager>(ctx))
    , timer_(std::make_shared<asio::steady_timer>(ctx))
{
    // -> constructor
    Log::instance()("initialization AsioTcpManager...", LoggerMode::info);
}

AsioTcpManager::~AsioTcpManager() {
    // -> closing all connections
    (this)->close();
}

void AsioTcpManager::open( const str &addr, const u16 &port ) const {
    // -> opening udp
    udp_server_->open(addr, port);
    udp_server_->listen();

    if (types_->acceptor.is_open())
        return;

    // -> opening tcp
    const auto address = asio::ip::make_address(addr);
    const asio::ip::tcp::endpoint endpoint(address, port);
    types_->acceptor.open(endpoint.protocol());
    types_->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    types_->acceptor.bind(endpoint);
    types_->acceptor.listen(asio::socket_base::max_listen_connections);
}

void AsioTcpManager::close() const {
    // -> closing all connections
    boost::system::error_code ec;
    if (types_->acceptor.is_open())
        types_->acceptor.close(ec);
    udp_server_->close();
}

void AsioTcpManager::listen() {
    if (!types_->acceptor.is_open())
        return;

    Log::instance()("listen!", LoggerMode::info);
    auto p = shared_from_this();
    types_->acceptor.async_accept([p](boost::system::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            boost::system::error_code ec;
            const auto remote_ep = socket.remote_endpoint(ec);
            if (!ec) {
                static u32 id = 0;
                p->cl_[id] = std::make_shared<AsioTcpClientSession>(std::move(socket), id);
                const auto i{ClientInfo{remote_ep.address().to_string(), remote_ep.port()}};
                p->udp_server_->createClient(id, i);
                p->createClient(id, i);
                ++id;
            }
        }
        p->listen();
    }
    );
}

void AsioTcpManager::createClient( const u32 &client_id, const ClientInfo &i ) {
    cl_[client_id]->signal_deleting.connect(boost::bind(&AsioUdpManager::removeClient, udp_server_.get(), boost::placeholders::_1));
    cl_[client_id]->signal_deleting.connect(boost::bind(&AsioTcpManager::removeClient, this, boost::placeholders::_1));
    cl_[client_id]->signal_process_packet.connect(boost::bind(&AsioTcpManager::processPacket, this, boost::placeholders::_1));
    cl_[client_id]->doSend(proto_project::dte::FirstData, (tcp_data::FirstData{i.addr, i.port}).serialize());
    cl_[client_id]->startSession();
    Log::instance()("AsioTcpManager->createClient, id:"+std::to_string(client_id), LoggerMode::info);
}

void AsioTcpManager::removeClient( const u32 &client_id ) {
    if (cl_.find(client_id) != cl_.end()) {
        cl_.erase(cl_.find(client_id));
        Log::instance()("AsioTcpManager->removeClient, id:"+std::to_string(client_id), LoggerMode::info);
    }
}

void AsioTcpManager::broadCast( const u16 &data_type, const vU8 &buf ) const {

}

void AsioTcpManager::processPacket( const vU8 &buf ) const {
    u32 offset = 0;

    proto_project::Packet pkt{};
    memcpy(&pkt.header.server_hash, &buf[offset], 2);
    offset += 2;
    memcpy(&pkt.header.total_data_size, &buf[offset], 4);
    offset += 4;
    memcpy(&pkt.header.total_cnt_packets, &buf[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_number, &buf[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_size, &buf[offset], 2);
    offset += 2;
    memcpy(&pkt.header.isFirst, &buf[offset], 1);
    offset += 1;
    memcpy(&pkt.header.isLast, &buf[offset], 1);
    offset += 1;
    memcpy(&pkt.header.isCompressed, &buf[offset], 1);
    offset += 1;
    memcpy(&pkt.d_type, &buf[offset], 2);
    offset += 2;

    pkt.buffer.assign(buf.begin() + offset, buf.end());

    if ( pkt.header.server_hash != Constants::SERVER_HASH ) {
        Log::instance()("!=SERVER_HASH", LoggerMode::error);
        return;
    }

    if ( pkt.header.isFirst != 1 || pkt.header.isLast != 1 ) {
        Log::instance()("!flags", LoggerMode::error);
        return;
    }

    switch (static_cast<tcp_data::DataTypes>(pkt.d_type)) {
        case tcp_data::DataTypes::TestStruct:
        {
            std::cout << "---TestStruct---" << std::endl;
            tcp_data::TestStruct a = tcp_data::TestStruct::deserialize(pkt.buffer.data());
            std::cout << a.a << std::endl;
            std::cout << a.b << std::endl;
            std::cout << a.c << std::endl;
            for (auto i{0};i<a.d.size();++i) {
                std::cout << a.d[i] << " ";
            }
            std::cout << std::endl;
            break;
        }
        default: break;
    }
}

void AsioTcpManager::startTest() const {
    timer_->expires_after(std::chrono::seconds(5));
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            udp_data::TestMessage a{};
            a.message = "Hello from Boost.Asio! Timestamp: " + std::to_string(std::time(nullptr));
            udp_server_->broadCast(static_cast<u16>(udp_data::DataTypes::TestMessage), a.serialize());
            startTest();
        } else {
            Log::instance()("timer err:" + ec.message(), LoggerMode::error);
        }
    });
}
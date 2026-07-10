#include "Server.h"

AsioUdpServer::AsioUdpServer( asio::io_context &ctx ) : udp_var_(std::make_unique<UdpVar>(ctx)) {
    Log::instance()("initialization AsioUdpServer...", LoggerMode::info);
}

void AsioUdpServer::open( const str &addr, const u16 &port ) {
    if (udp_var_->socket.is_open())
        return;

    const auto address = asio::ip::make_address(addr);
    const asio::ip::udp::endpoint endpoint(address, port);
    udp_var_->socket.open(endpoint.protocol());
    udp_var_->socket.set_option(asio::socket_base::reuse_address(true));
    udp_var_->socket.bind(endpoint);
}

void AsioUdpServer::listen() {
    if (!udp_var_->socket.is_open())
        return;

    udp_var_->socket.async_receive_from(asio::buffer(udp_var_->buffer), udp_var_->remote_endpoint,
        [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Receive error: " << ec.message() << std::endl;
                return;
            }
            if (bytes_transferred > 0) {
                prcsPacket();
                std::cout << "Received from "
                          << udp_var_->remote_endpoint.address().to_string()
                          << ":" << udp_var_->remote_endpoint.port() << std::endl;
            }
            if (udp_var_->socket.is_open()) {
                listen();
            }
        }
    );
}

void AsioUdpServer::close() {
    boost::system::error_code ec;
    if (udp_var_->socket.is_open())
        udp_var_->socket.close(ec);
}

void AsioUdpServer::sendData( const str &addr, const u16 &port, const udp_data::DataTypes &data_type, const vU8 &buf ) {
    if (!udp_var_->socket.is_open())
        return;

    const auto address = asio::ip::make_address(addr);
    const asio::ip::udp::endpoint target(address, port);
    auto makeHeader = [&](const u32 allSendDataSize,
                          const u32 curPacketNumber,
                          const u32 countAllPackets,
                          const u32 curPacketFullSize,
                          bool isF,
                          bool isE,
                          bool isCompressed) {
        proto_project::PacketHeader h;
        h.server_hash = Constants::SERVER_HASH;
        h.total_data_size = allSendDataSize;
        h.total_cnt_packets = countAllPackets;
        h.cur_packet_number = curPacketNumber + 1;
        h.cur_packet_size = curPacketFullSize;
        h.isFirst = isF ? 1 : 0;
        h.isLast = isE  ? 1 : 0;
        h.isCompressed = isCompressed ? 1 : 0;
        return h;
    };

    auto compressData = [&](const vU8 &input, int level = 3) -> vU8 {
        // const size_t maxCompressedSize = ZSTD_compressBound(input.size());
        // vU8 compressed(maxCompressedSize);

        // size_t const actualSize = ZSTD_compress(
        //     compressed.data(), compressed.size(),
        //     input.data(), input.size(),
        //     level);

        // if (ZSTD_isError(actualSize)) {
        //     throw std::runtime_error(std::string("ZSTD compress error: ") + ZSTD_getErrorName(actualSize));
        // }

        // compressed.resize(actualSize);
        // return compressed;
        return {};
    };

    auto compressedData = compressData(buf);
    bool useCompression = !compressedData.empty() && compressedData.size() < buf.size();

    const vU8 dataToSend = (useCompression) ? compressedData : buf;
    const auto sendTotalSize = static_cast<u32>(dataToSend.size());

    const auto cnt = (sendTotalSize + Constants::MAX_SIZE_UDP - 1) / Constants::MAX_SIZE_UDP;

    for (auto idx{0};idx<cnt;++idx) {
        const bool last = (idx + 1 == cnt);
        const auto chunk = last ? (sendTotalSize - Constants::MAX_SIZE_UDP * idx) : Constants::MAX_SIZE_UDP;
        const size_t offset = idx * Constants::MAX_SIZE_UDP;

        proto_project::Packet pkt{};
        proto_project::PacketHeader H = makeHeader(sendTotalSize, idx, cnt,
                                                   sizeof(proto_project::PacketHeader) + sizeof(u16) + chunk,
                                                   idx == 0, last, useCompression);

        pkt.header = H;
        pkt.d_type = static_cast<u16>(data_type);
        pkt.buffer.assign(dataToSend.begin() + offset,
                        dataToSend.begin() + offset + chunk);
        vU8 full_packet = proto_project::Packet::serialize(pkt);

        udp_var_->socket.async_send_to(asio::buffer(full_packet), target, [](const boost::system::error_code &ec, std::size_t) {
            if (ec) {
                std::cerr << "send error:" << ec.message() << std::endl;
            }
        });
    }
}

void AsioUdpServer::prcsPacket() {
    u32 offset = 0;

    proto_project::Packet pkt;
    memcpy(&pkt.header.server_hash, &(udp_var_->buffer)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.total_data_size, &(udp_var_->buffer)[offset], 4);
    offset += 4;
    memcpy(&pkt.header.total_cnt_packets, &(udp_var_->buffer)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_number, &(udp_var_->buffer)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_size, &(udp_var_->buffer)[offset], 2);
    offset += 2;
    u8 flags = (udp_var_->buffer)[offset];
    pkt.header.isFirst = (flags & 0x01) ? 1 : 0;
    pkt.header.isLast = (flags & 0x02) ? 1 : 0;
    pkt.header.isCompressed = (flags & 0x03) ? 1 : 0;
    offset += 2;
    memcpy(&pkt.d_type, &(udp_var_->buffer)[offset], 2);
    offset += 2;

    pkt.buffer.assign(udp_var_->buffer.begin() + offset, udp_var_->buffer.end());

    if ( pkt.header.server_hash != Constants::SERVER_HASH ) {
        std::cerr << "ERROR -> != kServerHash" << std::endl;
        return;
    }

    switch (static_cast<udp_data::DataTypes>(pkt.d_type))
    {
    case udp_data::DataTypes::TestMessage: {
        udp_data::TestMessage s = udp_data::TestMessage::deserialize(pkt.buffer.data());
        std::cout << s.message << std::endl;
        break;
    }
    default: break;
    }
}

AsioTcpServer::AsioTcpServer( asio::io_context &ctx ) : tcp_var_(std::make_unique<TcpVar>(ctx))
                                                        , udp_server_(std::make_unique<AsioUdpServer>(ctx))
                                                        , timer_(std::make_shared<asio::steady_timer>(ctx)) {
    Log::instance()("initialization AsioTcpServer...", LoggerMode::info);
}

AsioTcpServer::~AsioTcpServer() {
    close();
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
                clients_[id] = ClientInfo{clInfo.client_addr, clInfo.client_port};
                const auto cl = std::make_shared<AsioTcpClientSession>(std::move(socket), id);
                cl->get_signalDeleting().connect(boost::bind(&AsioTcpServer::removeClient, this, boost::placeholders::_1));
                cl->doSend(proto_project::dte::FirstData, clInfo.serialize());
                cl->startSession();
                ++id;
            }
        }
        listen();
    }
    );
}

void AsioTcpServer::removeClient( const u32 &client_id ) {
    if (clients_.find(client_id) != clients_.end()) {
        Log::instance()("Removed client.ID:" + std::to_string(client_id), LoggerMode::info);
        clients_.erase(clients_.find(client_id));
    }
}

void AsioTcpServer::startTimer() {
    timer_->expires_after(std::chrono::seconds(5));
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            udp_data::TestMessage a{};
            a.message = "Hello from Boost.Asio! Timestamp: " + std::to_string(std::time(nullptr));
            for (const auto &client : clients_) {
                udp_server_->sendData(client.second.addr, client.second.port, udp_data::DataTypes::TestMessage, a.serialize());
            }
            startTimer();
        } else {
            Log::instance()("timer err:" + ec.message(), LoggerMode::error);
        }
    });
}
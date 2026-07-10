#include "connection.h"

BoostUdpConnection::BoostUdpConnection( asio::io_context &ctx ) :
    udp_var_(std::make_unique<UdpVar>(ctx))
{
    std::cout << "initializing BoostUdpConnection..." << std::endl;
}

void BoostUdpConnection::open( const str &addr, const u16 &port ) {
    const auto address = asio::ip::make_address(addr);
    const asio::ip::udp::endpoint endpoint(address, port);
    udp_var_->socket.open(endpoint.protocol());
    udp_var_->socket.set_option(asio::socket_base::reuse_address(true));
    udp_var_->socket.bind(endpoint);
}

void BoostUdpConnection::listen() {
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
                // std::cout << "Received from "
                //           << udp_var_->remote_endpoint.address().to_string()
                //           << ":" << udp_var_->remote_endpoint.port() << std::endl;
            }
            if (udp_var_->socket.is_open()) {
                listen();
            }
        }
    );
}

void BoostUdpConnection::close() {
    if (udp_var_->socket.is_open()) {
        boost::system::error_code ec;
        udp_var_->socket.close(ec);
    }
}

void BoostUdpConnection::sendData( const str &addr, const udp_data::DataTypes &data_type, const vU8 &buf ) {
    if (!udp_var_->socket.is_open())
        return;

    const auto address = asio::ip::make_address(addr);
    const asio::ip::udp::endpoint target(address, Constants::SERVER_PORT);
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

BoostTcpConnection::BoostTcpConnection( asio::io_context &ctx ) :
    tcp_var_(std::make_unique<TcpVar>(ctx))
    , udp_(std::make_shared<BoostUdpConnection>(ctx)) {
    std::cout << "initializing BoostTcpConnection..." << std::endl;
}

void BoostTcpConnection::doConnect( const str &addr, const u16 &port )
{
    tcp_var_->server_addr = addr;
    try {
        tcp_var_->resolver.async_resolve(addr, std::to_string(port), [this](boost::system::error_code ec, asio::ip::tcp::resolver::results_type endpoints) {
            if (ec) {
                doDisconnect();
                return;
            }
            asio::async_connect(tcp_var_->socket, endpoints, [this](boost::system::error_code ec, asio::ip::tcp::endpoint)
            {
                if (ec) {
                    doDisconnect();
                } else {
                    std::cout << "Connected to server!" << std::endl;
                    asyncRead();
                }
             });
        });
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void BoostTcpConnection::asyncRead() {
    auto self = shared_from_this();
    asio::async_read(tcp_var_->socket, asio::buffer(&tcp_var_->size, sizeof(tcp_var_->size)),
        [this, self](boost::system::error_code ec, size_t) {
        if (ec == asio::error::eof) {
            std::cerr << "Server disconnect: " << ec.message() << std::endl;
            return;
        }
        tcp_var_->buffer.resize(tcp_var_->size);
        asio::async_read(tcp_var_->socket, asio::buffer(tcp_var_->buffer),
        [this, self](boost::system::error_code ec, size_t) {
            if (ec) {
                std::cerr << "Body read error: " << ec.message() << std::endl;
                return;
            }
            prcsPacket();
            asyncRead();
        });
    });
}

void BoostTcpConnection::prcsPacket() {
    u32 offset = 0;

    proto_project::Packet pkt;
    memcpy(&pkt.header.server_hash, &(tcp_var_->buffer)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.total_data_size, &(tcp_var_->buffer)[offset], 4);
    offset += 4;
    memcpy(&pkt.header.total_cnt_packets, &(tcp_var_->buffer)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_number, &(tcp_var_->buffer)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_size, &(tcp_var_->buffer)[offset], 2);
    offset += 2;

    memcpy(&pkt.header.isFirst, &(tcp_var_->buffer)[offset], 1);
    offset += 1;
    memcpy(&pkt.header.isLast, &(tcp_var_->buffer)[offset], 1);
    offset += 1;
    memcpy(&pkt.header.isCompressed, &(tcp_var_->buffer)[offset], 1);
    offset += 1;

    memcpy(&pkt.d_type, &(tcp_var_->buffer)[offset], 2);
    offset += 2;

    pkt.buffer.assign((tcp_var_->buffer).begin() + offset, (tcp_var_->buffer).end());

    if ( pkt.header.server_hash != Constants::SERVER_HASH ) {
        std::cerr << "ERROR -> != kServerHash" << std::endl;
        return;
    }

    if ( pkt.header.isFirst != 1 || pkt.header.isLast != 1 ) {
        std::cerr << "ERROR -> !flags" << std::endl;
        return;
    }

    switch (static_cast<tcp_data::DataTypes>(pkt.d_type)) {
    case tcp_data::DataTypes::TestStruct:
    {
        tcp_data::TestStruct a = tcp_data::TestStruct::deserialize(pkt.buffer.data());
        std::cout << "---TestStruct---" << std::endl;
        std::cout << a.a << std::endl;
        std::cout << a.b << std::endl;
        std::cout << a.c << std::endl;
        for (size_t i=0;i<a.d.size();++i) {
            std::cout << a.d[i] << " ";
        }
        std::cout << std::endl;
        break;
    }
    case tcp_data::DataTypes::FirstData: {
        tcp_data::FirstData a = tcp_data::FirstData::deserialize(pkt.buffer.data());
        std::cout << "addr=" << a.client_addr << std::endl;
        std::cout << "port=" << a.client_port << std::endl;
        udp_->open(a.client_addr, a.client_port);
        udp_->listen();
        break;
    }
    default: break;
    }
}

void BoostTcpConnection::doDisconnect()
{
    if (tcp_var_->socket.is_open()) {
        boost::system::error_code ec;
        tcp_var_->socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        tcp_var_->socket.close(ec);
    }
    udp_->close();
    std::cout << "Connection`s disconnect" << std::endl;
}

void BoostTcpConnection::asyncWrite( const tcp_data::DataTypes &data_type, const vU8 &buffer )
{
    if (!tcp_var_->socket.is_open()) {
        std::cerr << "Connect to server for send!" << std::endl;
        return;
    }

    proto_project::Packet pkt{};
    pkt.d_type = static_cast<u16>(data_type);
    pkt.header.server_hash = Constants::SERVER_HASH;
    pkt.header.total_data_size = buffer.size();
    pkt.header.total_cnt_packets = 1;
    pkt.header.cur_packet_number = 1;
    pkt.header.cur_packet_size = buffer.size() + sizeof(u16) + sizeof(proto_project::phr);
    pkt.header.isFirst = 1;
    pkt.header.isLast = 1;
    pkt.header.isCompressed = 0;
    pkt.buffer = buffer;

    vU8 full_packet = proto_project::Packet::serialize(pkt);

    u32 packet_size = static_cast<u32>(full_packet.size());
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(&packet_size, 4));
    buffers.push_back(boost::asio::buffer(full_packet));

    std::cout << "p_size=" << packet_size << std::endl;


    boost::asio::async_write(tcp_var_->socket, buffers, [this](boost::system::error_code ec, size_t length) {
        if (ec) {
            doDisconnect();
        } else {
            std::cout << "Отправлено " << length << " байт" << std::endl;
        }
    });

    std::cout << "Отправлен пакет #" << pkt.header.cur_packet_number
              << "/" << pkt.header.total_cnt_packets
              << ", размер: " << buffers.size() << " байт" << std::endl;
}

void BoostTcpConnection::sendUdpData( const udp_data::DataTypes &data_type, const vU8 &buf ) {
    udp_->sendData(tcp_var_->server_addr, data_type, buf);
}

void BoostUdpConnection::prcsPacket() {
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
    memcpy(&pkt.header.isFirst, &(udp_var_->buffer)[offset], 1);
    offset += 1;
    memcpy(&pkt.header.isLast, &(udp_var_->buffer)[offset], 1);
    offset += 1;
    memcpy(&pkt.header.isCompressed, &(udp_var_->buffer)[offset], 1);
    offset += 1;
    memcpy(&pkt.d_type, &(udp_var_->buffer)[offset], 2);
    offset += 2;

    pkt.buffer.assign(udp_var_->buffer.begin() + offset, udp_var_->buffer.end());

    if ( pkt.header.server_hash != Constants::SERVER_HASH ) {
        std::cerr << "!=SERVER_HASH" << std::endl;
        return;
    }

    switch (static_cast<udp_data::DataTypes>(pkt.d_type))
    {
    case udp_data::DataTypes::TestMessage: {
        const auto s = udp_data::TestMessage::deserialize(pkt.buffer.data());
        std::cout << "message=" << s.message << std::endl;
        break;
    }
    case udp_data::DataTypes::Image: {
        static vU8 assembly_buffer;                 // итоговый буфер
        static u32 assembly_total_size = 0;         // полный размер данных
        if (pkt.header.isFirst == 1) {
            assembly_total_size = pkt.header.total_data_size;
            assembly_buffer.assign(assembly_total_size, 0);
        }
        const u32 chunk_offset = (pkt.header.cur_packet_number - 1) * Constants::MAX_SIZE_UDP;
        const size_t copy_size = pkt.buffer.size();
        std::memcpy(assembly_buffer.data() + chunk_offset, pkt.buffer.data(), copy_size);
        if (pkt.header.isLast == 1) {
            if (pkt.header.isCompressed == 1) {
                // assembly_buffer = decompress_data_zstd(assembly_buffer);
            }
            std::cout << "size=" << assembly_buffer.size() << std::endl;
            this->signal_update_image_(assembly_buffer);
            assembly_buffer.clear();
            assembly_total_size = 0;
        }
        break;
    }
    default: break;
    }
}

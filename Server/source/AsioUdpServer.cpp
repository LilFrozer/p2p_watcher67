#include "Server.h"

AsioUdpManager::AsioUdpManager( asio::io_context &ctx ) : types_(std::make_unique<AsioUdpTypes>(ctx)) {
    Log::instance()("initialization AsioUdpManager...", LoggerMode::info);
}

void AsioUdpManager::open( const str &addr, const u16 &port ) const {
    if (types_->socket.is_open())
        return;

    const auto address = asio::ip::make_address(addr);
    const asio::ip::udp::endpoint endpoint(address, port);
    types_->socket.open(endpoint.protocol());
    types_->socket.set_option(asio::socket_base::reuse_address(true));
    types_->socket.bind(endpoint);
}

void AsioUdpManager::listen() {
    if (!types_->socket.is_open())
        return;

    types_->socket.async_receive_from(asio::buffer(types_->buffer), types_->remote_endpoint,
        [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                Log::instance()("Receive error:"+ec.message(), LoggerMode::error);
                return;
            }
            if (bytes_transferred > 0) {
                processPacket(types_->buffer);
                // std::cout << "Received from "
                //           << udp_var_->remote_endpoint.address().to_string()
                //           << ":" << udp_var_->remote_endpoint.port() << std::endl;
            }
            if (types_->socket.is_open()) {
                listen();
            }
        }
    );
}

void AsioUdpManager::close() const {
    boost::system::error_code ec;
    if (types_->socket.is_open())
        types_->socket.close(ec);
}

void AsioUdpManager::broadCast( const u16 &data_type, const vU8 &buf ) const {
    if (!types_->socket.is_open())
        return;

    for (const auto &client : cl_) {
        const auto address = asio::ip::make_address(client.second.addr);
        const asio::ip::udp::endpoint target(address, client.second.port);
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

            types_->socket.async_send_to(asio::buffer(full_packet), target, [](const boost::system::error_code &ec, std::size_t) {
                if (ec) {
                    Log::instance()("Send error:"+ec.message(), LoggerMode::error);
                }
            });
        }
    }
}

void AsioUdpManager::processPacket( const vU8 &buf ) const {
    u32 offset = 0;

    proto_project::Packet pkt;
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

    switch (static_cast<udp_data::DataTypes>(pkt.d_type))
    {
    case udp_data::DataTypes::TestMessage: {
        const auto s = udp_data::TestMessage::deserialize(pkt.buffer.data());
        Log::instance()("message="+s.message, LoggerMode::info);
        break;
    }
    case udp_data::DataTypes::Image: {
        static vU8 assembly_buffer;   // итоговый буфер
        static u32 assembly_total_size = 0;        // полный размер данных
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
            Log::instance()("size="+std::to_string(assembly_buffer.size()), LoggerMode::info);
            broadCast(static_cast<u16>(udp_data::DataTypes::Image), assembly_buffer);
            assembly_buffer.clear();
            assembly_total_size = 0;
        }
        break;
    }
    default: break;
    }
}

void AsioUdpManager::createClient( const u32 &client_id, const ClientInfo &i ) {
    cl_[client_id] = i;
    Log::instance()("AsioUdpManager->createClient, id:"+std::to_string(client_id), LoggerMode::info);
}

void AsioUdpManager::removeClient( const u32 &client_id ) {
    if (cl_.find(client_id) != cl_.end()) {
        cl_.erase(cl_.find(client_id));
        Log::instance()("AsioUdpManager->removeClient, id:" + std::to_string(client_id), LoggerMode::info);
    }
}
#include "ClientSession.h"
#include <iostream>

AsioTcpClientSession::AsioTcpClientSession( skt socket, const u32 &client_id )
    : socket_(std::move(socket))
    , cur_client_id_{client_id}
{
    Log::instance()("!...!", LoggerMode::info);
}

void AsioTcpClientSession::startSession() {
    doRead();
}

void AsioTcpClientSession::doRead() {
    auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(&size_, sizeof(size_)),
        [this, self](boost::system::error_code ec, size_t) {
        if (ec == asio::error::eof) {
            this->signal_deleting(cur_client_id_);
            Log::instance()("!disconnected!", LoggerMode::error);
            return;
        }
        buffer_.resize(size_);
        asio::async_read(socket_, asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, size_t) {
            if (ec) {
                Log::instance()(ec.message(), LoggerMode::error);
                return;
            }
            this->signal_process_packet(buffer_);
            doRead();
        });
    });
}

void AsioTcpClientSession::doSend( const tcp_data::DataTypes &dtype, const vU8 &buffer ) {
    if( !socket_.is_open() ) {
        Log::instance()("socket`s not connected!", LoggerMode::error);
        return;
    }

    proto_project::Packet pkt{};
    pkt.header.server_hash = Constants::SERVER_HASH;
    pkt.header.total_cnt_packets = 1;
    pkt.header.total_data_size = buffer.size();
    pkt.header.cur_packet_size = buffer.size() + sizeof(proto_project::PacketHeader) + sizeof(u16);
    pkt.header.isFirst = 1;
    pkt.header.isLast = 1;
    pkt.header.isCompressed = 0;
    pkt.d_type = static_cast<u16>(dtype);
    pkt.buffer = buffer;

    vU8 full_packet = proto_project::Packet::serialize(pkt);

    u32 size = full_packet.size();
    std::vector<asio::const_buffer> buffers{};
    buffers.emplace_back(asio::buffer(&size, 4));
    buffers.emplace_back(asio::buffer(full_packet));

    boost::asio::async_write(this->socket_, buffers, [this](boost::system::error_code ec, size_t length) {
        if ( ec ) {
            Log::instance()("socket send err!", LoggerMode::error);
        } else {
            Log::instance()("sended..." + std::to_string(length) + "bytes", LoggerMode::info);
        }
    });
}

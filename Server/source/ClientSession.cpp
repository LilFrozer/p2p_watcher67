#include "ClientSession.h"
#include <iostream>

BoostClientSession::BoostClientSession(asio::ip::tcp::socket socket, const u32 &id) :
    socket_(std::move(socket))
    , id_{id}
{
    Log::instance()("NewClient!", LoggerMode::info);
}

void BoostClientSession::start_session()
{
    this->async_read();
}

void BoostClientSession::async_read() {
    auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(&size_, sizeof(size_)),
        [this, self](boost::system::error_code ec, size_t) {
        if (ec == boost::asio::error::eof) {
            signalDeleting_(id_);
            Log::instance()("ClientDisconnected!", LoggerMode::error);
            return;
        }
        buffer_.resize(size_);
        asio::async_read(socket_, asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, size_t) {
            if (ec) {
                Log::instance()(ec.message(), LoggerMode::error);
                return;
            }
            this->process_packet();
            this->async_read();
        });
    });
}

void BoostClientSession::process_packet()
{
    unsigned offset = 0;

    proto_project::Packet pkt{};

    memcpy(&pkt.header.server_hash, &(this->buffer_)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.total_data_size, &(this->buffer_)[offset], 4);
    offset += 4;
    memcpy(&pkt.header.total_cnt_packets, &(this->buffer_)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_number, &(this->buffer_)[offset], 2);
    offset += 2;
    memcpy(&pkt.header.cur_packet_size, &(this->buffer_)[offset], 2);
    offset += 2;
    uint8_t flags = (this->buffer_)[offset];
    pkt.header.isFirst = (flags & 0x01) ? 1 : 0;
    pkt.header.isLast = (flags & 0x02) ? 1 : 0;
    offset += 2;
    memcpy(&pkt.d_type, &(this->buffer_)[offset], 2);
    offset += 2;

    pkt.buffer.assign((this->buffer_).begin() + offset, (this->buffer_).end());

    if ( pkt.header.server_hash != Constants::SERVER_HASH ) {
        std::cerr << "ERROR -> != kServerHash" << std::endl;
        return;
    }

    if ( pkt.header.isFirst != 1 || pkt.header.isLast != 1 ) {
        std::cerr << "ERROR -> !flags" << std::endl;
        return;
    }

    switch (pkt.d_type) {
        case tcp_data::DataTypes::TestStruct:
        {
            std::cout << "---TestStruct---" << std::endl;
            tcp_data::TestStruct a = tcp_data::TestStruct::deserialize(pkt.buffer.data());
            std::cout << a.a << std::endl;
            std::cout << a.b << std::endl;
            std::cout << a.c << std::endl;
            for (size_t i=0;i<a.d.size();++i) {
                std::cout << a.d[i] << " ";
            }
            std::cout << std::endl;
            break;
        }
        default: {
            std::cerr << "wtf is this data" << std::endl;
            break;
        }
    }
}

void BoostClientSession::async_send(proto_project::dte dtype, const vU8 &buffer) {
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
    pkt.d_type = dtype;
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

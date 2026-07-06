#include "connection.h"

BoostConnection::BoostConnection(boost::asio::io_context &ctx) :
    udp_var_(std::make_unique<UdpVar>(ctx))
    , tcp_var_(std::make_unique<TcpVar>(ctx))
{
    std::cout << "initializing Iserver" << std::endl;
}

BoostConnection::~BoostConnection()
{
    this->disconnect();
}

void BoostConnection::start_receive() {
    udp_var_->socket.async_receive_from(asio::buffer(udp_var_->buffer), udp_var_->remote_endpoint,
        [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Receive error: " << ec.message() << std::endl;
                return;
            }
            if (bytes_transferred > 0) {
                std::string received_data(udp_var_->buffer.data(), bytes_transferred);
                std::cout << "Received from "
                          << udp_var_->remote_endpoint.address().to_string()
                          << ":" << udp_var_->remote_endpoint.port()
                          << " -> " << received_data << std::endl;
            }
            if (udp_var_->socket.is_open()) {
                start_receive();
            }
        }
    );
}

void BoostConnection::connect( const str &host, const u16 port )
{
    try {
        tcp_var_->resolver.async_resolve(host, std::to_string(port), [this](boost::system::error_code ec, asio::ip::tcp::resolver::results_type endpoints) {
            if (ec) {
                this->handle_error(ec, "resolve");
                return;
            }
            boost::asio::async_connect(tcp_var_->socket, endpoints, [this](boost::system::error_code ec, asio::ip::tcp::endpoint)
            {
                if (ec) {
                    this->handle_error(ec, "connect");
                } else {
                    std::cout << "Connected to server!" << std::endl;
                    this->async_read();
                }
             });
        });
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void BoostConnection::async_read() {
    auto self = shared_from_this();
    boost::asio::async_read(tcp_var_->socket, boost::asio::buffer(&tcp_var_->size, sizeof(tcp_var_->size)),
        [this, self](boost::system::error_code ec, size_t) {
        if (ec == boost::asio::error::eof) {
            std::cerr << "Client disconnected" << std::endl;
            return;
        }
        tcp_var_->buffer.resize(tcp_var_->size);
        boost::asio::async_read(tcp_var_->socket, boost::asio::buffer(tcp_var_->buffer),
        [this, self](boost::system::error_code ec, size_t) {
            if (ec) {
                std::cerr << "Body read error: " << ec.message() << std::endl;
                return;
            }
            this->process_packet();
            this->async_read();
        });
    });
}

void BoostConnection::process_packet() {
    unsigned offset = 0;

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
    uint8_t flags = (tcp_var_->buffer)[offset];
    pkt.header.isFirst = (flags & 0x01) ? 1 : 0;
    pkt.header.isLast = (flags & 0x02) ? 1 : 0;
    offset += 2;
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

    switch (pkt.d_type) {
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
        const auto address = asio::ip::make_address(a.client_addr);
        const asio::ip::udp::endpoint endpoint(address, a.client_port);
        udp_var_->socket.open(endpoint.protocol());
        udp_var_->socket.set_option(asio::socket_base::reuse_address(true));
        udp_var_->socket.bind(endpoint);
        this->start_receive();
        break;
    }
    default: {
        std::cerr << "wtf is this data" << std::endl;
        break;
    }
    }
}

void BoostConnection::disconnect()
{
    if (tcp_var_->socket.is_open()) {
        boost::system::error_code ec;
        tcp_var_->socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        tcp_var_->socket.close(ec);
    }
    if (udp_var_->socket.is_open()) {
        boost::system::error_code ec;
        udp_var_->socket.close(ec);
    }
    std::cout << "Connection`s disconnect" << std::endl;
}

void BoostConnection::async_write(proto_project::dpt data_type, const vU8 &buffer)
{
    if (!tcp_var_->socket.is_open()) {
        std::cerr << "Connect to server for send!" << std::endl;
        return;
    }

    proto_project::Packet pkt{};
    pkt.d_type = data_type;
    pkt.header.server_hash = Constants::SERVER_HASH;
    pkt.header.total_data_size = buffer.size();
    pkt.header.total_cnt_packets = 1;
    pkt.header.cur_packet_number = 1;
    pkt.header.cur_packet_size = buffer.size() + sizeof(u16) + sizeof(proto_project::phr);
    pkt.header.isFirst = 1;
    pkt.header.isLast = 1;
    pkt.buffer = buffer;

    vU8 full_packet = proto_project::Packet::serialize(pkt);

    u32 packet_size = static_cast<u32>(full_packet.size());
    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(&packet_size, 4));
    buffers.push_back(boost::asio::buffer(full_packet));

    std::cout << "p_size=" << packet_size << std::endl;


    boost::asio::async_write(tcp_var_->socket, buffers, [this](boost::system::error_code ec, size_t length) {
        if (ec) {
            handle_error(ec, "send");
        } else {
            std::cout << "Отправлено " << length << " байт" << std::endl;
        }
    });

    std::cout << "Отправлен пакет #" << pkt.header.cur_packet_number
              << "/" << pkt.header.total_cnt_packets
              << ", размер: " << buffers.size() << " байт" << std::endl;
}

void BoostConnection::handle_error(const boost::system::error_code& ec, const std::string& context)
{
    if( ec == boost::asio::error::eof )
    {
        std::cout << "Connection closed by server" << std::endl;
        this->disconnect();
    }
    else if( ec == boost::asio::error::connection_reset)
    {
        std::cout << "Connection reset by server" << std::endl;
        this->disconnect();
    }
    else if( ec == boost::asio::error::connection_aborted)
    {
        std::cout << "Connection aborted" << std::endl;
        this->disconnect();
    }
    else if( ec == boost::asio::error::broken_pipe)
    {
        std::cout << "Connection (broken pipe)" << std::endl;
        this->disconnect();
    }
    else
    {
        std::cout << "Error in: " << context << " ;ec = " << ec.message() << std::endl;
    }
}

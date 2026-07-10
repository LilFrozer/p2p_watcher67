#include "crc.h"
#include "Server.h"

int main(int argc, char *argv[])
{
    // - res = 38f6
    // Crc::testCrc("A590006C00000000000004000000");
    // Crc::testCrc("A50000400002a000000000000000");
    try {
        asio::io_context io_context;
        const auto Server{std::make_shared<AsioTcpServer>(io_context)};
        Server->open(Constants::SERVER_ADDR, Constants::SERVER_PORT);
        Server->listen();
        Server->startTimer();
        io_context.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    return EXIT_SUCCESS;
}

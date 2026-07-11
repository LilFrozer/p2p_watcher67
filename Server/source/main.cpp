#include "crc.h"
#include "Server.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        Log::instance()("err, argc < 2", LoggerMode::error);
        return EXIT_FAILURE;
    }
    const bool useBoostAsio = argv[0];
    // - res = 38f6
    // Crc::testCrc("A590006C00000000000004000000");
    // Crc::testCrc("A50000400002a000000000000000");
    try {
        std::shared_ptr<Creator> p{nullptr};

        if (useBoostAsio > 0) {
            asio::io_context io_context;
            p = std::make_shared<AsioCreator>(io_context);
            p->Execute(Constants::SERVER_ADDR, Constants::SERVER_PORT);
            io_context.run();
        } else {
            p = std::make_shared<UnixCreator>();
            p->Execute(Constants::SERVER_ADDR, Constants::SERVER_PORT);
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
    return EXIT_SUCCESS;
}
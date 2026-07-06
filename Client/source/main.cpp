#include "mainclient.h"
#include <QApplication>
#include <thread>

int main(int argc, char *argv[])
{
    try {
        QApplication a(argc, argv);

        if (!CGPreflightScreenCaptureAccess()) {
            CGRequestScreenCaptureAccess();
            throw std::runtime_error("");
        }

        boost::asio::io_context io_context;
        auto work_guard = boost::asio::make_work_guard(io_context);
        std::thread asio_thread([&io_context]() { io_context.run(); });

        std::unique_ptr<MainClient> client = \
            std::make_unique<MainClient>(nullptr, io_context);
        client->show();

        int ret = a.exec();

        io_context.stop();
        asio_thread.join();
        return ret;
    } catch ( const std::exception &e ) {
        std::cerr << e.what() << std::endl;
    }
}

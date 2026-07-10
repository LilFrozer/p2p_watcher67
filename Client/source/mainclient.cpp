#include "mainclient.h"
#include "ui_mainclient.h"

MainClient::MainClient( QWidget *parent, asio::io_context &ctx ) : QMainWindow(parent)
    , ui(new Ui::MainClient)
    , timer_{std::make_shared<asio::steady_timer>(ctx)}
{
    ui->setupUi(this);

    QObject::connect(ui->qpsh_connect, &QPushButton::clicked, this, &MainClient::slotConnect);
    QObject::connect(ui->qpsh_disconnect, &QPushButton::clicked, this, &MainClient::slotDisconnect);
    QObject::connect(ui->qpsh_send, &QPushButton::clicked, this, &MainClient::slotSend);

    if( !conn_ ) {
        conn_ = std::make_shared<BoostTcpConnection>(ctx);
        static_cast<BoostTcpConnection*>(conn_.get())->get_signal_update_image().connect(boost::bind(&MainClient::showImage, this, boost::placeholders::_1));
    }
}

MainClient::~MainClient() {
    conn_->doDisconnect();
    delete ui;
}

void MainClient::slotConnect() {
    const auto host = ui->qlineedit_host->text();
    conn_->doConnect(host.toStdString(), Constants::SERVER_PORT);
}

void MainClient::slotDisconnect() {
    conn_->doDisconnect();
}

void MainClient::slotSend() {
    // ---tcp---
    tcp_data::TestStruct _struct;
    _struct.a = "test";
    _struct.b = 1;
    _struct.c = 0.75;
    _struct.d.reserve(5);
    for( size_t i=0;i<5;++i )
        _struct.d.push_back(i+i*i);
    conn_->asyncWrite(proto_project::dpt::TestStruct, _struct.serilize());

    // --udp--
    udp_data::TestMessage testMessage{};
    testMessage.message = "hi, this is image!";
    conn_->sendUdpData(udp_data::DataTypes::TestMessage, testMessage.serialize());

    CGImageRef screenshot = CGDisplayCreateImage(CGMainDisplayID());
    if (!screenshot)
        throw std::runtime_error("");
    conn_->sendUdpData(udp_data::DataTypes::Image, imageToBytes(screenshot));
}

void MainClient::showImage(vU8 buf) {
    std::cout << "image=" << buf.size() << std::endl;

    CGImageRef screenshot = CGDisplayCreateImage(CGMainDisplayID());
    if (!screenshot)
        throw std::runtime_error("");
    CGImageRelease(screenshot);

    QImage image(buf.data(), 2880, 1800, 2880 * 4, QImage::Format_RGBA8888);
    QPixmap pixmap = QPixmap::fromImage(image);
    if (pixmap.isNull())
        throw std::runtime_error("Не удалось создать QPixmap");

    ui->image->setPixmap(pixmap.scaled(ui->image->size(), Qt::KeepAspectRatio));
}

vU8 MainClient::imageToBytes(CGImageRef cgImage)
{
    if (!cgImage)
        return {};

    size_t width = CGImageGetWidth(cgImage);
    size_t height = CGImageGetHeight(cgImage);
    size_t bitsPerPixel = CGImageGetBitsPerPixel(cgImage);
    size_t bytesPerRow = CGImageGetBytesPerRow(cgImage);

    // !!!Создаём контекст для извлечения данных в формате RGBA!!!
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    vU8 rawData(height * bytesPerRow);

    CGContextRef context = CGBitmapContextCreate(
        rawData.data(),
        width,
        height,
        8,              // bits per component
        bytesPerRow,
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );

    if (!context) {
        CGColorSpaceRelease(colorSpace);
        return {};
    }

    // !!!Рисуем изображение в контекст, чтобы получить пиксельные данные!!!
    CGRect rect = CGRectMake(0, 0, width, height);
    CGContextDrawImage(context, rect, cgImage);

    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);

    return rawData;
}

void MainClient::startTimer() {
    timer_->expires_after(std::chrono::seconds(5));
    timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            startTimer();
        } else {
            std::cerr << "timer err:" + ec.message() << std::endl;
        }
    });
}

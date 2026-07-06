#include "mainclient.h"
#include "ui_mainclient.h"

MainClient::MainClient(QWidget *parent, boost::asio::io_context &io)
    : QMainWindow(parent)
    , ui(new Ui::MainClient)
{
    ui->setupUi(this);

    QObject::connect(ui->qpsh_connect, &QPushButton::clicked, this, &MainClient::slot_connect);
    QObject::connect(ui->qpsh_disconnect, &QPushButton::clicked, this, &MainClient::slot_disconnect);
    QObject::connect(ui->qpsh_send, &QPushButton::clicked, this, &MainClient::slot_send);

    if( !Iserver_ ) {
        Iserver_ = std::make_shared<BoostConnection>(io);
    }
}

MainClient::~MainClient()
{
    delete ui;
}

void MainClient::slot_connect()
{
    QString host = ui->qlineedit_host->text();
    Iserver_->connect(host.toStdString(), Constants::SERVER_PORT);
}

void MainClient::slot_disconnect()
{
    Iserver_->disconnect();
}

void MainClient::slot_send()
{
    tcp_data::TestStruct _struct;
    _struct.a = "test";
    _struct.b = 1;
    _struct.c = 0.75;
    _struct.d.reserve(5);
    for( size_t i=0;i<5;++i )
        _struct.d.push_back(i+i*i);

    // CGImageRef screenshot = CGDisplayCreateImage(CGMainDisplayID());
    // if (!screenshot)
    //     throw std::runtime_error("");

    // image_ = imageToBytes(screenshot);

    // CGImageRelease(screenshot);

    // QImage image(image_.data(), 2880, 1800, 2880 * 4, QImage::Format_RGBA8888);
    // QPixmap pixmap = QPixmap::fromImage(image);
    // if (pixmap.isNull())
    //     throw std::runtime_error("Не удалось создать QPixmap");

    // ui->image->setPixmap(pixmap.scaled(ui->image->size(), Qt::KeepAspectRatio));

    Iserver_->async_write(proto_project::dpt::TestStruct, _struct.serilize());
}

// vU8 MainClient::imageToBytes(CGImageRef cgImage)
// {
//     if (!cgImage)
//         return {};

//     size_t width = CGImageGetWidth(cgImage);
//     size_t height = CGImageGetHeight(cgImage);
//     size_t bitsPerPixel = CGImageGetBitsPerPixel(cgImage);
//     size_t bytesPerRow = CGImageGetBytesPerRow(cgImage);

//     // !!!Создаём контекст для извлечения данных в формате RGBA!!!
//     CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
//     vU8 rawData(height * bytesPerRow);

//     CGContextRef context = CGBitmapContextCreate(
//         rawData.data(),
//         width,
//         height,
//         8,              // bits per component
//         bytesPerRow,
//         colorSpace,
//         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
//     );

//     if (!context) {
//         CGColorSpaceRelease(colorSpace);
//         return {};
//     }

//     // !!!Рисуем изображение в контекст, чтобы получить пиксельные данные!!!
//     CGRect rect = CGRectMake(0, 0, width, height);
//     CGContextDrawImage(context, rect, cgImage);

//     CGContextRelease(context);
//     CGColorSpaceRelease(colorSpace);

//     return rawData;
// }

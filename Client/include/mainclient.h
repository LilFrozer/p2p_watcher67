#ifndef MAINCLIENT_H
#define MAINCLIENT_H

#include <QMainWindow>
#include <memory>
#include <boost/asio.hpp>
#include <iostream>
#include "connection.h"
#include "qcustomplot.h"

#include <CoreGraphics/CoreGraphics.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainClient;
}
QT_END_NAMESPACE

class MainClient : public QMainWindow
{
    Q_OBJECT

public:
    MainClient(QWidget *parent, boost::asio::io_context &io);
    ~MainClient();
private:
    Ui::MainClient *ui;
    std::shared_ptr<IConnection> Iserver_{nullptr};
    std::unique_ptr<QCustomPlot> plot_{nullptr};

    // (формат RGBA)
    vU8 image_{};
    vU8 imageToBytes(CGImageRef cgImage);
private slots:
    void slot_connect();
    void slot_disconnect();
    void slot_send();
};
#endif // MAINCLIENT_H

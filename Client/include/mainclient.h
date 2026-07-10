#ifndef MAINCLIENT_H
#define MAINCLIENT_H

#include <QMainWindow>
#include <memory>
#include <boost/asio.hpp>
#include <iostream>
#include "connection.h"
#include "qcustomplot.h"
#include <chrono>
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
    MainClient( QWidget *parent, boost::asio::io_context &ctx );
    ~MainClient();
    void startTimer();
    void showImage(vU8 buf);
private:
    Ui::MainClient *ui;
    std::shared_ptr<ITcpConnection> conn_{nullptr};
    std::unique_ptr<QCustomPlot> plot_{nullptr};

    std::shared_ptr<asio::steady_timer> timer_{nullptr};
    // ---format RGBA---
    vU8 imageToBytes(CGImageRef cgImage);
private slots:
    void slotConnect();
    void slotDisconnect();
    void slotSend();
};
#endif // MAINCLIENT_H

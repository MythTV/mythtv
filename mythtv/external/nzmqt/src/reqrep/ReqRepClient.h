// Copyright 2011-2012 Johann Duscher. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice, this list of
//       conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright notice, this list
//       of conditions and the following disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY JOHANN DUSCHER ''AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are those of the
// authors and should not be interpreted as representing official policies, either expressed
// or implied, of Johann Duscher.

#ifndef REQREPCLIENT_H
#define REQREPCLIENT_H

#include <QObject>
#include <QRunnable>
#include <QDebug>
#include <QList>
#include <QByteArray>
#include <QTimer>
#include <QDateTime>


#include "nzmqt/nzmqt.hpp"


namespace nzmqt
{

namespace samples
{

class ReqRepClient : public QObject, public QRunnable
{
    Q_OBJECT

    typedef QObject super;

public:
    explicit ReqRepClient(const QString& address, const QString& requestMsg, QObject *parent)
        : super(parent), address_(address), requestMsg_(requestMsg)
    {
        ZMQContext* context = createDefaultContext(this);
        context->start();

        socket_ = context->createSocket(ZMQSocket::TYP_REQ);
        connect(socket_, SIGNAL(messageReceived(const QList<QByteArray>&)), SLOT(replyReceived(const QList<QByteArray>&)));

        timer_ = new QTimer(socket_);
        timer_->setSingleShot(true);
        timer_->setInterval(1000);
        connect(timer_, SIGNAL(timeout()), SLOT(sendRequest()));
    }

    void run()
    {
        socket_->connectTo(address_);

        timer_->start();
    }

protected slots:
    void sendRequest()
    {
        static quint64 counter = 0;

        QList<QByteArray> request;
        request += QString("REQUEST[%1: %2]").arg(++counter).arg(QDateTime::currentDateTime().toString(Qt::ISODate)).toLocal8Bit();
        request += requestMsg_.toLocal8Bit();
        qDebug() << "ReqRepClient::sendRequest> " << request;
        socket_->sendMessage(request);
    }

    void replyReceived(const QList<QByteArray>& reply)
    {
        qDebug() << "ReqRepClient::replyReceived> " << reply;

        // Start timer again in order to trigger the next sendRequest() call.
        timer_->start();
    }

private:
    QString address_;
    QString requestMsg_;

    ZMQSocket* socket_;
    QTimer* timer_;
};

}

}

#endif // REQREPCLIENT_H

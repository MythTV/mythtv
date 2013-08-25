// Copyright 2011-2013 Johann Duscher (a.k.a. Jonny Dee). All rights reserved.
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

#ifndef PUBSUBCLIENT_H
#define PUBSUBCLIENT_H

#include <QObject>
#include <QRunnable>
#include <QDebug>
#include <QList>
#include <QByteArray>

#include "nzmqt/nzmqt.hpp"


namespace nzmqt
{

namespace samples
{

class PubSubClient : public QObject, public QRunnable
{
    Q_OBJECT

    typedef QObject super;

public:
    explicit PubSubClient(const QString& address, const QString& topic, QObject *parent)
        : super(parent), address_(address), topic_(topic)
    {
        ZMQContext* context = createDefaultContext(this);
        context->start();

        socket_ = context->createSocket(ZMQSocket::TYP_SUB);
        connect(socket_, SIGNAL(messageReceived(const QList<QByteArray>&)), SLOT(messageReceived(const QList<QByteArray>&)));
    }

    void run()
    {
        socket_->subscribeTo(topic_);
        socket_->connectTo(address_);
    }

protected slots:
    void messageReceived(const QList<QByteArray>& message)
    {
        qDebug() << "PubSubClient> " << message;
    }

private:
    QString address_;
    QString topic_;

    ZMQSocket* socket_;
};

}

}

#endif // PUBSUBCLIENT_H

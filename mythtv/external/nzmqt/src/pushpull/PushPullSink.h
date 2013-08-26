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

#ifndef PUSHPULLSINK_H
#define PUSHPULLSINK_H

#include <QObject>
#include <QCoreApplication>
#include <QTime>
#include <QRunnable>
#include <QDebug>
#include <QList>
#include <QByteArray>

#include "nzmqt/nzmqt.hpp"


namespace nzmqt
{

namespace samples
{

class PushPullSink : public QObject, public QRunnable
{
    Q_OBJECT

    typedef QObject super;

public:
    explicit PushPullSink(const QString& sinkAddress, QObject *parent)
        : super(parent), sinkAddress_(sinkAddress), numberOfWorkItems_(-1)
    {
        ZMQContext* context = createDefaultContext(this);
        context->start();

        sink_ = context->createSocket(ZMQSocket::TYP_PULL);
        connect(sink_, SIGNAL(messageReceived(const QList<QByteArray>&)), SLOT(batchEvent(const QList<QByteArray>&)));
    }

    void run()
    {
        sink_->bindTo(sinkAddress_);
    }

protected slots:
    void batchEvent(const QList<QByteArray>& message)
    {
        if (numberOfWorkItems_ < 0)
        {
            // 'message' is a batch start message.
            numberOfWorkItems_ = message[0].toUInt();
            qDebug() << "Batch started for >" << numberOfWorkItems_ << "< work items.";
            stopWatch_.start();

            if (numberOfWorkItems_)
                return;
        }

        if (numberOfWorkItems_ > 0)
        {
            if (numberOfWorkItems_ % 10 == 0)
                qDebug() << numberOfWorkItems_;
            else
                qDebug() << ".";

            --numberOfWorkItems_;
        }

        if (!numberOfWorkItems_)
        {
            int msec = stopWatch_.elapsed();
            qDebug() << "FINISHED all task in " << msec << "msec";
            numberOfWorkItems_ = -1;
        }
    }

private:
    QString sinkAddress_;

    ZMQSocket* sink_;
    int numberOfWorkItems_;
    QTime stopWatch_;
};

}

}

#endif // PUSHPULLSINK_H

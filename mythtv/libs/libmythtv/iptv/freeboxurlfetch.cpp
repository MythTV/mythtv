// -*- Mode: c++ -*-

// Qt headers
#include <qapplication.h>
#include <qurloperator.h>
#include <qdeepcopy.h>

// MythTV headers
#include "freeboxurlfetch.h"

FreeboxUrlFetch::FreeboxUrlFetch(const QString &url) :
    op(new QUrlOperator(url)),
    state(QNetworkProtocol::StInProgress),
    str(QString::null)
{
    connect(op,   SIGNAL( finished(QNetworkOperation*)),
            this, SLOT(   Finished(QNetworkOperation*)));
    connect(op,   SIGNAL( data(const QByteArray&, QNetworkOperation*)),
            this, SLOT(   Data(const QByteArray&, QNetworkOperation*)));
    op->get();
}

void FreeboxUrlFetch::deleteLater(void)
{
    disconnect();
    if (op)
    {
        op->disconnect();
        op->deleteLater();
        op = NULL;
    }
}

void FreeboxUrlFetch::Finished(QNetworkOperation *op)
{
    state = op->state();
}

void FreeboxUrlFetch::Data(const QByteArray &data, QNetworkOperation *op)
{
    // TODO FIXME need to run fromUtf8 on whole buffer, otherwise
    //            multi-byte characters will be missed.
    if (!data.isNull())
        str += QString::fromUtf8(data.data(), data.size());
    state = op->state();
}

QString FreeboxUrlFetch::FetchData(const QString &url, bool inQtThread)
{
    FreeboxUrlFetch *instance = new FreeboxUrlFetch(url);

    while (instance->state == QNetworkProtocol::StWaiting ||
           instance->state == QNetworkProtocol::StInProgress)
    {
        if (inQtThread)
            qApp->processEvents();

        usleep(10000);
    }

    QString ret = QString::null;
    if (instance->state == QNetworkProtocol::StDone)
        ret = QDeepCopy<QString>(instance->str);

    instance->deleteLater();

    return ret;
}

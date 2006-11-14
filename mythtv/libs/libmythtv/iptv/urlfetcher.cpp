// -*- Mode: c++ -*-

// POSIX headers
#include <stdint.h>
#include <unistd.h>

// Qt headers
#include <qapplication.h>
#include <qurloperator.h>
#include <qdeepcopy.h>

// MythTV headers
#include "urlfetcher.h"

URLFetcher::URLFetcher(const QString &url) :
    op(new QUrlOperator(url)),
    state(QNetworkProtocol::StInProgress)
{
    connect(op,   SIGNAL( finished(QNetworkOperation*)),
            this, SLOT(   Finished(QNetworkOperation*)));
    connect(op,   SIGNAL( data(const QByteArray&, QNetworkOperation*)),
            this, SLOT(   Data(const QByteArray&, QNetworkOperation*)));
    op->get();
}

void URLFetcher::deleteLater(void)
{
    disconnect();
    if (op)
    {
        op->disconnect();
        op->deleteLater();
        op = NULL;
    }
}

void URLFetcher::Finished(QNetworkOperation *op)
{
    state = op->state();
}

void URLFetcher::Data(const QByteArray &data, QNetworkOperation *op)
{
    if (!data.isNull())
    {
        uint64_t beg = buf.size();
        buf.resize(beg + data.size());
        memcpy(&buf[beg], data.data(), data.size());
    }
    state = op->state();
}

QString URLFetcher::FetchData(const QString &url, bool inQtThread)
{
    URLFetcher *instance = new URLFetcher(url);

    while (instance->state == QNetworkProtocol::StWaiting ||
           instance->state == QNetworkProtocol::StInProgress)
    {
        if (inQtThread)
            qApp->processEvents();

        usleep(10000);
    }

    QString ret = QString::null;
    if (instance->state == QNetworkProtocol::StDone)
    {
        ret = QDeepCopy<QString>(
            QString::fromUtf8((const char*) &instance->buf[0],
                              instance->buf.size()));
    }

    instance->deleteLater();

    return ret;
}

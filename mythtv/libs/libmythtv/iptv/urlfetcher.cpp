// -*- Mode: c++ -*-

// POSIX headers
#include <stdint.h>
#include <unistd.h>

// Qt headers
#include <qapplication.h>
#include <q3urloperator.h>
#include <q3deepcopy.h>

// MythTV headers
#include "urlfetcher.h"

URLFetcher::URLFetcher(const QString &url) :
    op(new Q3UrlOperator(url)),
    state(Q3NetworkProtocol::StInProgress)
{
    connect(op,   SIGNAL( finished(Q3NetworkOperation*)),
            this, SLOT(   Finished(Q3NetworkOperation*)));
    connect(op,   SIGNAL( data(const QByteArray&, Q3NetworkOperation*)),
            this, SLOT(   Data(const QByteArray&, Q3NetworkOperation*)));
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

void URLFetcher::Finished(Q3NetworkOperation *op)
{
    state = op->state();
}

void URLFetcher::Data(const QByteArray &data, Q3NetworkOperation *op)
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

    while (instance->state == Q3NetworkProtocol::StWaiting ||
           instance->state == Q3NetworkProtocol::StInProgress)
    {
        if (inQtThread)
            qApp->processEvents();

        usleep(10000);
    }

    QString ret = QString::null;
    if (instance->state == Q3NetworkProtocol::StDone)
    {
        ret = Q3DeepCopy<QString>(
            QString::fromUtf8((const char*) &instance->buf[0],
                              instance->buf.size()));
    }

    instance->deleteLater();

    return ret;
}

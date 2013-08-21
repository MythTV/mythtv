/* MHEG Interaction Channel
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#include "mhegic.h"

// C/C++ lib
#include <cstdlib>
using std::getenv;

// Qt
#include <QByteArray>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QStringList>
#include <QScopedPointer>
#include <QApplication>

// Myth
#include "netstream.h"
#include "mythlogging.h"
#include "mythcorecontext.h"

#define LOC QString("[mhegic] ")


MHInteractionChannel::MHInteractionChannel(QObject* parent) : QObject(parent)
{
    setObjectName("MHInteractionChannel");
    moveToThread(&NAMThread::manager());
}

// virtual
MHInteractionChannel::~MHInteractionChannel()
{
    QMutexLocker locker(&m_mutex);
    for ( map_t::iterator it = m_pending.begin(); it != m_pending.end(); ++it)
        (*it)->deleteLater();
    for ( map_t::iterator it = m_finished.begin(); it != m_finished.end(); ++it)
        (*it)->deleteLater();
}

// Get network status
// static
MHInteractionChannel::EStatus MHInteractionChannel::status()
{
    if (!NetStream::isAvailable())
    {
        LOG(VB_MHEG, LOG_INFO, LOC + "WARN network is unavailable");
        return kInactive;
    }

    if (!gCoreContext->GetNumSetting("EnableMHEG", 0))
        return kDisabled;

    QStringList opts = QString(getenv("MYTHMHEG")).split(':');
    if (opts.contains("noice", Qt::CaseInsensitive))
        return kDisabled;
    else if (opts.contains("ice", Qt::CaseInsensitive))
        return kActive;

    return gCoreContext->GetNumSetting("EnableMHEGic", 1) ? kActive : kDisabled;
}

static inline bool isCached(const QString& csPath)
{
    return NetStream::GetLastModified(csPath).isValid();
}

// Is a file ready to read?
bool MHInteractionChannel::CheckFile(const QString& csPath, const QByteArray &cert)
{
    QMutexLocker locker(&m_mutex);

    // Is it complete?
    if (m_finished.contains(csPath))
        return true;

    // Is it pending?
    if (m_pending.contains(csPath))
        return false; // It's pending so unavailable

    // Is it in the cache?
    if (isCached(csPath))
        return true; // It's cached

    // Queue a request
    LOG(VB_MHEG, LOG_DEBUG, LOC + QString("CheckFile queue %1").arg(csPath));
    QScopedPointer< NetStream > p(new NetStream(csPath, NetStream::kPreferCache, cert));
    if (!p || !p->IsOpen())
    {
        LOG(VB_MHEG, LOG_WARNING, LOC + QString("CheckFile failed %1").arg(csPath) );
        return false;
    }

    connect(p.data(), SIGNAL(Finished(QObject*)), this, SLOT(slotFinished(QObject*)) );
    m_pending.insert(csPath, p.take());

    return false; // It's now pending so unavailable
}

// Read a file. -1= error, 0= OK, 1= not ready
MHInteractionChannel::EResult
MHInteractionChannel::GetFile(const QString &csPath, QByteArray &data,
        const QByteArray &cert /*=QByteArray()*/)
{
    QMutexLocker locker(&m_mutex);

    // Is it pending?
    if (m_pending.contains(csPath))
        return kPending;

    // Is it complete?
    QScopedPointer< NetStream > p(m_finished.take(csPath));
    if (p)
    {
        if (p->GetError() == QNetworkReply::NoError)
        {
            data = p->ReadAll();
            LOG(VB_MHEG, LOG_DEBUG, LOC + QString("GetFile finished %1").arg(csPath) );
            return kSuccess;
        }

        LOG(VB_MHEG, LOG_WARNING, LOC + QString("GetFile failed %1").arg(csPath) );
        return kError;
    }

    // Is it in the cache?
    if (isCached(csPath))
    {
        LOG(VB_MHEG, LOG_DEBUG, LOC + QString("GetFile cache read %1").arg(csPath) );

        locker.unlock();

        NetStream req(csPath, NetStream::kAlwaysCache);
        if (req.WaitTillFinished(3000) && req.GetError() == QNetworkReply::NoError)
        {
            data = req.ReadAll();
            LOG(VB_MHEG, LOG_DEBUG, LOC + QString("GetFile cache read %1 bytes %2")
                .arg(data.size()).arg(csPath) );
            return kSuccess;
        }

        LOG(VB_MHEG, LOG_WARNING, LOC + QString("GetFile cache read failed %1").arg(csPath) );
        //return kError;
        // Retry
        locker.relock();
    }

    // Queue a download
    LOG(VB_MHEG, LOG_DEBUG, LOC + QString("GetFile queue %1").arg(csPath) );
    p.reset(new NetStream(csPath, NetStream::kPreferCache, cert));
    if (!p || !p->IsOpen())
    {
        LOG(VB_MHEG, LOG_WARNING, LOC + QString("GetFile failed %1").arg(csPath) );
        return kError;
    }

    connect(p.data(), SIGNAL(Finished(QObject*)), this, SLOT(slotFinished(QObject*)) );
    m_pending.insert(csPath, p.take());

    return kPending;
}

// signal from NetStream
void MHInteractionChannel::slotFinished(QObject *obj)
{
    NetStream* p = dynamic_cast< NetStream* >(obj);
    if (!p)
        return;

    QByteArray url = p->Url().toEncoded();

    if (p->GetError() == QNetworkReply::NoError)
    {
        LOG(VB_MHEG, LOG_DEBUG, LOC + QString("Finished %1").arg(url.constData()) );
    }
    else
    {
        LOG(VB_MHEG, LOG_WARNING, LOC + QString("Finished %1").arg(p->GetErrorString()) );
    }

    p->disconnect();

    QMutexLocker locker(&m_mutex);

    if (m_pending.remove(url.constData()) < 1)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Finished %1 wasn't pending").arg(url.constData()) );
    m_finished.insert(url.constData(), p);
}

/* End of file */

/* MHEG Interaction Channel
 * Copyright 2011 Lawrence Rust <lvr at softsystem dot co dot uk>
 */
#include "mhegic.h"

// Qt
#include <QByteArray>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QStringList>
#include <QApplication>

// Myth
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "netstream.h"

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
    for (const auto & req : std::as_const(m_pending))
        req->deleteLater();
    for (const auto & req : std::as_const(m_finished))
        req->deleteLater();
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

    if (!gCoreContext->GetBoolSetting("EnableMHEG", false))
        return kDisabled;

    QStringList opts = qEnvironmentVariable("MYTHMHEG").split(':');
    if (opts.contains("noice", Qt::CaseInsensitive))
        return kDisabled;
    if (opts.contains("ice", Qt::CaseInsensitive))
        return kActive;
    return gCoreContext->GetBoolSetting("EnableMHEGic", true) ? kActive : kDisabled;
}

static inline bool isCached(const QUrl& url)
{
    return NetStream::GetLastModified(url).isValid();
}

// Is a file ready to read?
bool MHInteractionChannel::CheckFile(const QString& csPath, const QByteArray &cert)
{
    QUrl url(csPath);
    QMutexLocker locker(&m_mutex);

    // Is it complete?
    if (m_finished.contains(url))
        return true;

    // Is it pending?
    if (m_pending.contains(url))
        return false; // It's pending so unavailable

    // Is it in the cache?
    if (isCached(url))
        return true; // It's cached

    // Queue a request
    LOG(VB_MHEG, LOG_DEBUG, LOC + QString("CheckFile queue %1").arg(csPath));
    std::unique_ptr< NetStream > p(new NetStream(url, NetStream::kPreferCache, cert));
    if (!p || !p->IsOpen())
    {
        LOG(VB_MHEG, LOG_WARNING, LOC + QString("CheckFile failed %1").arg(csPath) );
        return false;
    }

    connect(p.get(), &NetStream::Finished, this, &MHInteractionChannel::slotFinished );
    m_pending.insert(url, p.release());

    return false; // It's now pending so unavailable
}

// Read a file. -1= error, 0= OK, 1= not ready
MHInteractionChannel::EResult
MHInteractionChannel::GetFile(const QString &csPath, QByteArray &data,
        const QByteArray &cert /*=QByteArray()*/)
{
    QUrl url(csPath);
    QMutexLocker locker(&m_mutex);

    // Is it pending?
    if (m_pending.contains(url))
        return kPending;

    // Is it complete?
    std::unique_ptr< NetStream > p(m_finished.take(url));
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
    if (isCached(url))
    {
        LOG(VB_MHEG, LOG_DEBUG, LOC + QString("GetFile cache read %1").arg(csPath) );

        locker.unlock();

        NetStream req(url, NetStream::kAlwaysCache);
        if (req.WaitTillFinished(3s) && req.GetError() == QNetworkReply::NoError)
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
    p = std::make_unique<NetStream>(url, NetStream::kPreferCache, cert);
    if (!p || !p->IsOpen())
    {
        LOG(VB_MHEG, LOG_WARNING, LOC + QString("GetFile failed %1").arg(csPath) );
        return kError;
    }

    connect(p.get(), &NetStream::Finished, this, &MHInteractionChannel::slotFinished );
    m_pending.insert(url, p.release());

    return kPending;
}

// signal from NetStream
void MHInteractionChannel::slotFinished(QObject *obj)
{
    auto* p = qobject_cast< NetStream* >(obj);
    if (!p)
        return;

    QUrl url = p->Url();

    if (p->GetError() == QNetworkReply::NoError)
    {
        LOG(VB_MHEG, LOG_DEBUG, LOC + QString("Finished %1").arg(url.toString()) );
    }
    else
    {
        LOG(VB_MHEG, LOG_WARNING, LOC + QString("Finished %1").arg(p->GetErrorString()) );
    }

    p->disconnect();

    QMutexLocker locker(&m_mutex);

    if (m_pending.remove(url) < 1)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Finished %1 wasn't pending").arg(url.toString()) );
    m_finished.insert(url, p);
}

/* End of file */

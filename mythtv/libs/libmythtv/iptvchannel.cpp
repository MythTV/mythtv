/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV headers
#include "iptvchannel.h"
#include "mythlogging.h"
#include "mythdb.h"

#define LOC QString("IPTVChan(%1): ").arg(GetCardID())

IPTVChannel::IPTVChannel(TVRec *rec) :
    DTVChannel(rec), m_open(false)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
}

IPTVChannel::~IPTVChannel()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "dtor");
}

bool IPTVChannel::Open(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Open()");

    if (IsOpen())
        return true;

    QMutexLocker locker(&m_lock);

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    m_open = true;

    return m_open;
}

void IPTVChannel::Close(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Close()");
    QMutexLocker locker(&m_lock);
    m_open = false;
}

bool IPTVChannel::IsOpen(void) const
{
    QMutexLocker locker(&m_lock);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("IsOpen() -> %1")
        .arg(m_open ? "true" : "false"));
    return m_open;
}

bool IPTVChannel::Tune(const QString &freqid, int finetune)
{
    (void) finetune;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Tune(%1) TO BE IMPLEMENTED")
        .arg(freqid));

    // TODO IMPLEMENT

    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

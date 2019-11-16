/*  -*- Mode: c++ -*-
 *   Class ExternalFetcher
 *
 *   Copyright (C) John Poet 2018
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// C/C++ includes
#include <utility>

// Qt includes
#include <QString>

// MythTV includes
#include "ExternalRecChannelFetcher.h"
#include "ExternalStreamHandler.h"

#define LOC QString("ExternalRec[%1](%2): ").arg(m_cardid).arg(m_command)

ExternalRecChannelFetcher::ExternalRecChannelFetcher(int cardid,
                                                     QString cmd)
    : m_cardid(cardid)
    , m_command(std::move(cmd))
{
    m_stream_handler = ExternalStreamHandler::Get(m_command, m_cardid, m_cardid);
}

ExternalRecChannelFetcher::~ExternalRecChannelFetcher(void)
{
    Close();
}

void ExternalRecChannelFetcher::Close(void)
{
    if (m_stream_handler)
        ExternalStreamHandler::Return(m_stream_handler, m_cardid);
}

bool ExternalRecChannelFetcher::Valid(void) const
{
    if (!m_stream_handler)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "Failed to open external app.");
        return false;
    }

    if (!m_stream_handler->HasTuner())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "External app does not have a tuner.");
        return false;
    }

    return true;
}

bool ExternalRecChannelFetcher::FetchChannel(const QString & cmd,
                                             QString & channum,
                                             QString & name,
                                             QString & callsign,
                                             QString & xmltvid)
{
    if (!Valid())
        return false;

    QString result;

    if (!m_stream_handler->ProcessCommand(cmd, result))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString("%1 command failed.").arg(cmd));
        return false;
    }

    if (result.startsWith("ERR"))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString("%1: %2")
            .arg(cmd).arg(result));
        return false;
    }
    if (result.startsWith("OK:DONE"))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + result);
        return false;
    }

    // Expect csv:  channum, name, callsign, xmltvid
    QStringList fields = result.mid(3).split(",");

    if (fields.size() != 4)
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC +
            QString("Expecting channum, name, callsign, xmltvid; "
                    "Received '%1").arg(result));
        return false;
    }

    channum  = fields[0];
    name     = fields[1];
    callsign = fields[2];
    xmltvid  = fields[3];

    return true;
}

int ExternalRecChannelFetcher::LoadChannels(void)
{
    if (!Valid())
        return 0;

    QString result;
    int     cnt = -1;

    if (!m_stream_handler->ProcessCommand("LoadChannels", result, 50000))
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + "LoadChannels command failed.");
        return -1;
    }

    if (result.startsWith("FOUND"))
        cnt = result.mid(6).toInt();
    else if (result.startsWith("OK"))
        cnt = result.mid(3).toInt();
    else
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + QString("LoadChannels: %1").arg(result));
        return -1;
    }

    return cnt;
}

//
//  mythnotification.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

#include "mythnotification.h"

#include <QCoreApplication>

QEvent::Type MythNotification::New =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Update =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Info =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Error =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Warning =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Check =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythNotification::Busy =
    (QEvent::Type) QEvent::registerEventType();

void MythNotification::SetId(int id)
{
    m_id = id;
    // default registered notification is to not expire
    if (m_id > 0 && m_duration == 0)
    {
        m_duration = -1;
    }
}

/**
 * stringFromSeconds:
 *
 * Usage: stringFromSeconds(seconds).
 * Description: create a string in the format HH:mm:ss from a duration in seconds.
 * HH: will not be displayed if there's less than one hour.
 */
QString MythPlaybackNotification::stringFromSeconds(int time)
{
    int   hour    = time / 3600;
    int   minute  = (time - hour * 3600) / 60;
    int seconds   = time - hour * 3600 - minute * 60;
    QString str;
    
    if (hour)
    {
        str += QString("%1:").arg(hour);
    }
    if (minute < 10)
    {
        str += "0";
    }
    str += QString("%1:").arg(minute);
    if (seconds < 10)
    {
        str += "0";
    }
    str += QString::number(seconds);
    return str;
}

MythNotification::Type MythNotification::TypeFromString(const QString &type)
{
    if (type == "error")
    {
        return MythNotification::Error;
    }
    else if (type == "warning")
    {
        return MythNotification::Warning;
    }
    else if (type == "check")
    {
        return MythNotification::Check;
    }
    else if (type == "busy")
    {
        return MythNotification::Busy;
    }
    else
    {
        return MythNotification::New;
    }
}

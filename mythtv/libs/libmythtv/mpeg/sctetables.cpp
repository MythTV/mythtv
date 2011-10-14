// -*- Mode: c++ -*-
/**
 *   SCTE System information tables.
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Author: Daniel Kristjansson
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

#include "sctetables.h"

void SCTENetworkInformationTable::Parse(void) const
{
    // TODO
}

QString SCTENetworkInformationTable::toString(void) const
{
    // TODO
    return "";
}

QString SCTENetworkInformationTable::toStringXML(void) const
{
    // TODO
    return "";
}

//////////////////////////////////////////////////////////////////////

void NetworkTextTable::Parse(void) const
{
    // TODO
}

QString NetworkTextTable::toString(void) const
{
    // TODO
    return "";
}

QString NetworkTextTable::toStringXML(void) const
{
    // TODO
    return "";
}

//////////////////////////////////////////////////////////////////////

void ShortVirtualChannelMap::Parse(void) const
{
    // TODO
}

QString ShortVirtualChannelMap::toString(void) const
{
    // TODO
    return "";
}

QString ShortVirtualChannelMap::toStringXML(void) const
{
    // TODO
    return "";
}

//////////////////////////////////////////////////////////////////////

void SCTESystemTimeTable::Parse(void) const
{
    // TODO
}

QString SCTESystemTimeTable::toString(void) const
{
    QString str =
        QString("SCTESystemTimeTable  GPSTime(%1) GPS2UTC_Offset(%2) ")
        .arg(SystemTimeGPS().toString(Qt::LocalDate)).arg(GPSOffset());
    return str;
}

QString SCTESystemTimeTable::toStringXML(void) const
{
    // TODO
    return "";
}

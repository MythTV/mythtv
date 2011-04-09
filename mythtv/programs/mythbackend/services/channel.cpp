//////////////////////////////////////////////////////////////////////////////
// Program Name: channel.cpp
// Created     : Apr. 8, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QList>

#include "channel.h"

#include "channelutil.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

//QList<uint> Channel::GetChannels( int SourceID )
//{
//    QList<uint> lChannels;
//
//    vector<uint> channelList = ChannelUtil::GetChanIDs(SourceID);
//    QVector<uint> channelIDList = QVector<uint>::fromStdVector(channelList);
//    lChannels = QList<uint>::fromVector(channelIDList);
//
//    return lChannels;
//}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

//QList<uint> Channel::GetCardIDs( uint ChanID )
//{
//    QList<uint> lCardIDs;
//
//    vector<uint> cardList = ChannelUtil::GetCardIDs(ChanID);
//    QVector<uint> cardIDList = QVector<uint>::fromStdVector(cardList);
//    lCardIDs = QList<uint>::fromVector(cardIDList);
//
//    return lCardIDs;
//}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Channel::GetIcon( uint ChanID )
{
    QString sIcon;

    sIcon = ChannelUtil::GetIcon(ChanID);

    return sIcon;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

uint Channel::GetMplexID( uint ChanID )
{
    uint nMplex = 0;

    nMplex = ChannelUtil::GetMplexID(ChanID);

    return nMplex;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Channel::GetDefaultAuthority( uint ChanID )
{
    QString sDefaultAuthority;

    sDefaultAuthority = ChannelUtil::GetDefaultAuthority(ChanID);

    return sDefaultAuthority;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

uint Channel::GetSourceIDForChannel( uint ChanID )
{
    uint nSourceID = 0;

    nSourceID = ChannelUtil::GetSourceIDForChannel(ChanID);

    return nSourceID;
}

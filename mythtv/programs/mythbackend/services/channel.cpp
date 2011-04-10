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

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "channelutil.h"

#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ChannelInfoList* Channel::GetChannelInfoList( int nSourceID,
                                                   int nStartIndex,
                                                   int nCount )
{
    vector<uint> chanList;

    chanList = ChannelUtil::GetChanIDs(nSourceID);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ChannelInfoList *pChannelInfos = new DTC::ChannelInfoList();

    nStartIndex = min( nStartIndex, (int)chanList.size() );
    nCount      = (nCount > 0) ? min( nCount, (int)chanList.size() ) : chanList.size();

    for( int n = nStartIndex; n < nCount; n++)
    {
        DTC::ChannelInfo *pChannelInfo = pChannelInfos->AddNewChannelInfo();

        int chanid = chanList.at(n);
        uint mplexid = ChannelUtil::GetMplexID(chanid);
        QString channum = ChannelUtil::GetChanNum(chanid);

        pChannelInfo->setChanId(chanid);
        pChannelInfo->setChanNum(channum);
        pChannelInfo->setCallSign(ChannelUtil::GetCallsign(chanid));
        pChannelInfo->setIconURL(ChannelUtil::GetIcon(chanid)); //Wrong!
        pChannelInfo->setChannelName(ChannelUtil::GetServiceName(chanid));
        pChannelInfo->setMplexId(mplexid);

        pChannelInfo->setChanFilters(ChannelUtil::GetVideoFilters(nSourceID, channum));
        pChannelInfo->setSourceId(nSourceID);
        // This needs to return QVector<int> instead.
        pChannelInfo->setInputId(0);
        // Not sure how to get this with channelutil, TODO SQL or expand channelutil
        pChannelInfo->setCommFree(false);
    }

    pChannelInfos->setStartIndex    ( nStartIndex     );
    pChannelInfos->setCount         ( nCount          );
    pChannelInfos->setTotalAvailable( chanList.size() );
    pChannelInfos->setAsOf          ( QDateTime::currentDateTime() );
    pChannelInfos->setVersion       ( MYTH_BINARY_VERSION );
    pChannelInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pChannelInfos;
}

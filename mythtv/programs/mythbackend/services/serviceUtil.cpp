//////////////////////////////////////////////////////////////////////////////
// Program Name: serviceUtil.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

#include "serviceUtil.h"

#include "programinfo.h"
#include "recordinginfo.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillProgramInfo( DTC::Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel /* = true */,
                      bool          bDetails    /* = true */)
{
    if ((pProgram == NULL) || (pInfo == NULL))
        return;

    pProgram->setStartTime(  pInfo->GetScheduledStartTime());
    pProgram->setEndTime  (  pInfo->GetScheduledEndTime  ());
    pProgram->setTitle    (  pInfo->GetTitle()             );
    pProgram->setSubTitle (  pInfo->GetSubtitle()          );
    pProgram->setCategory (  pInfo->GetCategory()          );
    pProgram->setCatType  (  pInfo->GetCategoryType()      );
    pProgram->setRepeat   (  pInfo->IsRepeat()             );

    pProgram->setSerializeDetails( bDetails );

    if (bDetails)
    {
        pProgram->setSeriesId    ( pInfo->GetSeriesID()         );
        pProgram->setProgramId   ( pInfo->GetProgramID()        );
        pProgram->setStars       ( pInfo->GetStars()            );
        pProgram->setFileSize    ( pInfo->GetFilesize()         );
        pProgram->setLastModified( pInfo->GetLastModifiedTime() );
        pProgram->setProgramFlags( pInfo->GetProgramFlags()     );
        pProgram->setHostname    ( pInfo->GetHostname()         );

        if (pInfo->GetOriginalAirDate().isValid())
            pProgram->setAirdate( pInfo->GetOriginalAirDate() );

        pProgram->setDescription( pInfo->GetDescription() );
    }

    pProgram->setSerializeChannel( bIncChannel );

    if ( bIncChannel )
    {
        // Build Channel Child Element

        FillChannelInfo( pProgram->Channel(), pInfo, bDetails );
    }

    // Build Recording Child Element

    if ( pInfo->GetRecordingStatus() != rsUnknown )
    {
        pProgram->setSerializeRecording( true );

        DTC::RecordingInfo *pRecording = pProgram->Recording();

        pRecording->setStatus  ( pInfo->GetRecordingStatus()    );
        pRecording->setPriority( pInfo->GetRecordingPriority()  );
        pRecording->setStartTs ( pInfo->GetRecordingStartTime() );
        pRecording->setEndTs   ( pInfo->GetRecordingEndTime()   );

        pRecording->setSerializeDetails( bDetails );

        if (bDetails)
        {
            pRecording->setRecordId ( pInfo->GetRecordingRuleID()       );
            pRecording->setRecGroup ( pInfo->GetRecordingGroup()        );
            pRecording->setPlayGroup( pInfo->GetPlaybackGroup()         );
            pRecording->setRecType  ( pInfo->GetRecordingRuleType()     );
            pRecording->setDupInType( pInfo->GetDuplicateCheckSource()  );
            pRecording->setDupMethod( pInfo->GetDuplicateCheckMethod()  );
            pRecording->setEncoderId( pInfo->GetCardID()                );

            const RecordingInfo ri(*pInfo);
            pRecording->setProfile( ri.GetProgramRecordingProfile() );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillChannelInfo( DTC::ChannelInfo *pChannel,
                      ProgramInfo      *pInfo,
                      bool              bDetails  /* = true */ )
{
    if (pInfo)
    {
/*
        QString sHostName = gCoreContext->GetHostName();
        QString sPort     = gCoreContext->GetSettingOnHost( "BackendStatusPort",
                                                        sHostName);
        QString sIconURL  = QString( "http://%1:%2/getChannelIcon?ChanId=%3" )
                                   .arg( sHostName )
                                   .arg( sPort )
                                   .arg( pInfo->chanid );
*/

        pChannel->setChanId     ( pInfo->GetChanID()              );
        pChannel->setChanNum    ( pInfo->GetChanNum()             );
        pChannel->setCallSign   ( pInfo->GetChannelSchedulingID() );
      //pChannel->setIconURL    ( sIconURL                        );
        pChannel->setChannelName( pInfo->GetChannelName()         );

        pChannel->setSerializeDetails( bDetails );

        if (bDetails)
        {
            pChannel->setChanFilters( pInfo->GetChannelPlaybackFilters() );
            pChannel->setSourceId   ( pInfo->GetSourceID()               );
            pChannel->setInputId    ( pInfo->GetInputID()                );
            pChannel->setCommFree   ( (pInfo->IsCommercialFree()) ? 1 : 0);
        }
    }

}

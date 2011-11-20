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
#include "channelutil.h"

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

    pProgram->setStartTime (  pInfo->GetScheduledStartTime());
    pProgram->setEndTime   (  pInfo->GetScheduledEndTime  ());
    pProgram->setTitle     (  pInfo->GetTitle()             );
    pProgram->setSubTitle  (  pInfo->GetSubtitle()          );
    pProgram->setCategory  (  pInfo->GetCategory()          );
    pProgram->setCatType   (  pInfo->GetCategoryType()      );
    pProgram->setRepeat    (  pInfo->IsRepeat()             );
    pProgram->setVideoProps(  pInfo->GetVideoProperties()   );
    pProgram->setAudioProps(  pInfo->GetAudioProperties()   );
    pProgram->setSubProps  (  pInfo->GetSubtitleType()      );

    pProgram->setSerializeDetails( bDetails );

    if (bDetails)
    {
        pProgram->setSeriesId    ( pInfo->GetSeriesID()         );
        pProgram->setProgramId   ( pInfo->GetProgramID()        );
        pProgram->setStars       ( pInfo->GetStars()            );
        pProgram->setFileSize    ( pInfo->GetFilesize()         );
        pProgram->setLastModified( pInfo->GetLastModifiedTime() );
        pProgram->setProgramFlags( pInfo->GetProgramFlags()     );
        pProgram->setFileName    ( pInfo->GetPathname()         );
        pProgram->setHostName    ( pInfo->GetHostname()         );

        if (pInfo->GetOriginalAirDate().isValid())
            pProgram->setAirdate( pInfo->GetOriginalAirDate() );

        pProgram->setDescription( pInfo->GetDescription() );
        pProgram->setInetref    ( pInfo->GetInetRef()     );
        pProgram->setSeason     ( pInfo->GetSeason()      );
        pProgram->setEpisode    ( pInfo->GetEpisode()     );
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
            pRecording->setRecordId    ( pInfo->GetRecordingRuleID()      );
            pRecording->setRecGroup    ( pInfo->GetRecordingGroup()       );
            pRecording->setPlayGroup   ( pInfo->GetPlaybackGroup()        );
            pRecording->setStorageGroup( pInfo->GetStorageGroup()         );
            pRecording->setRecType     ( pInfo->GetRecordingRuleType()    );
            pRecording->setDupInType   ( pInfo->GetDuplicateCheckSource() );
            pRecording->setDupMethod   ( pInfo->GetDuplicateCheckMethod() );
            pRecording->setEncoderId   ( pInfo->GetCardID()               );

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
        if (!ChannelUtil::GetIcon(pInfo->GetChanID()).isEmpty())
        {
            QString sIconURL  = QString( "/Guide/GetChannelIcon?ChanId=%3")
                                       .arg( pInfo->GetChanID() );
            pChannel->setIconURL    ( sIconURL                    );
        }

        pChannel->setChanId     ( pInfo->GetChanID()              );
        pChannel->setChanNum    ( pInfo->GetChanNum()             );
        pChannel->setCallSign   ( pInfo->GetChannelSchedulingID() );
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

void FillRecRuleInfo( DTC::RecRule  *pRecRule,
                      RecordingRule *pRule    )
{
    if ((pRecRule == NULL) || (pRule == NULL))
        return;

    pRecRule->setId             (  pRule->m_recordID          );
    pRecRule->setParentId       (  pRule->m_parentRecID       );
    pRecRule->setInactive       (  pRule->m_isInactive        );
    pRecRule->setTitle          (  pRule->m_title             );
    pRecRule->setSubTitle       (  pRule->m_subtitle          );
    pRecRule->setDescription    (  pRule->m_description       );
    pRecRule->setSeason         (  pRule->m_season            );
    pRecRule->setEpisode        (  pRule->m_episode           );
    pRecRule->setCategory       (  pRule->m_category          );
    pRecRule->setStartTime      (  QDateTime(pRule->m_startdate,
                                             pRule->m_starttime) );
    pRecRule->setEndTime        (  QDateTime(pRule->m_enddate,
                                             pRule->m_endtime) );
    pRecRule->setSeriesId       (  pRule->m_seriesid          );
    pRecRule->setProgramId      (  pRule->m_programid         );
    pRecRule->setInetref        (  pRule->m_inetref           );
    pRecRule->setChanId         (  pRule->m_channelid         );
    pRecRule->setCallSign       (  pRule->m_station           );
    pRecRule->setDay            (  pRule->m_findday           );
    pRecRule->setTime           (  pRule->m_findtime          );
    pRecRule->setFindId         (  pRule->m_findid            );
    pRecRule->setType           (  pRule->m_type              );
    pRecRule->setSearchType     (  pRule->m_searchType        );
    pRecRule->setRecPriority    (  pRule->m_recPriority       );
    pRecRule->setPreferredInput (  pRule->m_prefInput         );
    pRecRule->setStartOffset    (  pRule->m_startOffset       );
    pRecRule->setEndOffset      (  pRule->m_endOffset         );
    pRecRule->setDupMethod      (  pRule->m_dupMethod         );
    pRecRule->setDupIn          (  pRule->m_dupIn             );
    pRecRule->setFilter         (  pRule->m_filter            );
    pRecRule->setRecProfile     (  pRule->m_recProfile        );
    pRecRule->setRecGroup       (  pRule->m_recGroup          );
    pRecRule->setStorageGroup   (  pRule->m_storageGroup      );
    pRecRule->setPlayGroup      (  pRule->m_playGroup         );
    pRecRule->setAutoExpire     (  pRule->m_autoExpire        );
    pRecRule->setMaxEpisodes    (  pRule->m_maxEpisodes       );
    pRecRule->setMaxNewest      (  pRule->m_maxNewest         );
    pRecRule->setAutoCommflag   (  pRule->m_autoCommFlag      );
    pRecRule->setAutoTranscode  (  pRule->m_autoTranscode     );
    pRecRule->setAutoMetaLookup (  pRule->m_autoMetadataLookup);
    pRecRule->setAutoUserJob1   (  pRule->m_autoUserJob1      );
    pRecRule->setAutoUserJob2   (  pRule->m_autoUserJob2      );
    pRecRule->setAutoUserJob3   (  pRule->m_autoUserJob3      );
    pRecRule->setAutoUserJob4   (  pRule->m_autoUserJob4      );
    pRecRule->setTranscoder     (  pRule->m_transcoder        );
    pRecRule->setNextRecording  (  pRule->m_nextRecording     );
    pRecRule->setLastRecorded   (  pRule->m_lastRecorded      );
    pRecRule->setLastDeleted    (  pRule->m_lastDeleted       );
    pRecRule->setAverageDelay   (  pRule->m_averageDelay      );
}

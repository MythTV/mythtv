//////////////////////////////////////////////////////////////////////////////
// Program Name: serviceUtil.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QUrl>

#include "serviceUtil.h"

#include "programinfo.h"
#include "recordinginfo.h"
#include "recordingtypes.h"
#include "channelutil.h"
#include "channelinfo.h"
#include "videoutils.h"
#include "metadataimagehelper.h"
#include "cardutil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillProgramInfo( DTC::Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel /* = true */,
                      bool          bDetails    /* = true */,
                      bool          bIncCast    /* = true */)
{
    if ((pProgram == NULL) || (pInfo == NULL))
        return;

    pProgram->setStartTime (  pInfo->GetScheduledStartTime());
    pProgram->setEndTime   (  pInfo->GetScheduledEndTime  ());
    pProgram->setTitle     (  pInfo->GetTitle()             );
    pProgram->setSubTitle  (  pInfo->GetSubtitle()          );
    pProgram->setCategory  (  pInfo->GetCategory()          );
    pProgram->setCatType   (  pInfo->GetCategoryTypeString());
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
        else if (pInfo->GetYearOfInitialRelease() > 0)
        {
            QDate year;
            year.setDate(pInfo->GetYearOfInitialRelease(), 1, 1);
            pProgram->setAirdate( year );
        }

        pProgram->setDescription( pInfo->GetDescription() );
        pProgram->setInetref    ( pInfo->GetInetRef()     );
        pProgram->setSeason     ( pInfo->GetSeason()      );
        pProgram->setEpisode    ( pInfo->GetEpisode()     );
        pProgram->setTotalEpisodes( pInfo->GetEpisodeTotal() );
    }

    pProgram->setSerializeCast(bIncCast);
    if (bIncCast)
    {
        FillCastMemberList( pProgram->Cast(), pInfo );
    }

    pProgram->setSerializeChannel( bIncChannel );

    if ( bIncChannel )
    {
        // Build Channel Child Element
        if (!FillChannelInfo( pProgram->Channel(), pInfo->GetChanID(), bDetails ))
        {
            // The channel associated with a given recording may no longer exist
            // however the ChanID is one half of the unique identifier for the
            // recording and therefore MUST be included in the return data
            pProgram->Channel()->setChanId(pInfo->GetChanID());
        }
    }

    // Build Recording Child Element

    if ( pInfo->GetRecordingStatus() != rsUnknown )
    {
        pProgram->setSerializeRecording( true );

        DTC::RecordingInfo *pRecording = pProgram->Recording();

        pRecording->setRecordedId ( pInfo->GetRecordingID()     );
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
            if (pProgram->Channel())
            {
                QString encoderName = CardUtil::GetDisplayName(pInfo->GetCardID(),
                                                               pProgram->Channel()->SourceId());
                pRecording->setEncoderName( encoderName );
            }

            const RecordingInfo ri(*pInfo);
            pRecording->setProfile( ri.GetProgramRecordingProfile() );
        }
    }

    if (!pInfo->GetInetRef().isEmpty() )
    {
        pProgram->setSerializeArtwork( true );

        FillArtworkInfoList( pProgram->Artwork(), pInfo->GetInetRef(),
                             pInfo->GetSeason());
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool FillChannelInfo( DTC::ChannelInfo *pChannel,
                      uint              nChanID,
                      bool              bDetails  /* = true */ )
{
    ChannelInfo channel;
    if (channel.Load(nChanID))
    {
        return FillChannelInfo(pChannel, channel, bDetails);
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool FillChannelInfo( DTC::ChannelInfo *pChannel,
                      const ChannelInfo &channelInfo,
                      bool              bDetails  /* = true */ )
{

    // TODO update DTC::ChannelInfo to match functionality of ChannelInfo,
    //      ultimately replacing it's progenitor?
    pChannel->setChanId(channelInfo.chanid);
    pChannel->setChanNum(channelInfo.channum);
    pChannel->setCallSign(channelInfo.callsign);
    if (!channelInfo.icon.isEmpty())
    {
        QString sIconURL  = QString( "/Guide/GetChannelIcon?ChanId=%3")
                                    .arg( channelInfo.chanid );
        pChannel->setIconURL( sIconURL );
    }
    pChannel->setChannelName(channelInfo.name);
    pChannel->setVisible(channelInfo.visible);

    pChannel->setSerializeDetails( bDetails );

    if (bDetails)
    {
        pChannel->setMplexId(channelInfo.mplexid);
        pChannel->setServiceId(channelInfo.serviceid);
        pChannel->setATSCMajorChan(channelInfo.atsc_major_chan);
        pChannel->setATSCMinorChan(channelInfo.atsc_minor_chan);
        pChannel->setFormat(channelInfo.tvformat);
        pChannel->setFineTune(channelInfo.finetune);
        pChannel->setFrequencyId(channelInfo.freqid);
        pChannel->setChanFilters(channelInfo.videofilters);
        pChannel->setSourceId(channelInfo.sourceid);
        pChannel->setCommFree(channelInfo.commmethod == -2);
        pChannel->setUseEIT(channelInfo.useonairguide);
        pChannel->setXMLTVID(channelInfo.xmltvid);
        pChannel->setDefaultAuth(channelInfo.default_authority);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillChannelGroup(DTC::ChannelGroup* pGroup, ChannelGroupItem pGroupItem)
{
    if (!pGroup)
        return;

    pGroup->setGroupId(pGroupItem.grpid);
    pGroup->setName(pGroupItem.name);
    pGroup->setPassword(""); // Not currently supported
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillRecRuleInfo( DTC::RecRule  *pRecRule,
                      RecordingRule *pRule    )
{
    if ((pRecRule == NULL) || (pRule == NULL))
        return;

    pRecRule->setId             (  pRule->m_recordID               );
    pRecRule->setParentId       (  pRule->m_parentRecID            );
    pRecRule->setInactive       (  pRule->m_isInactive             );
    pRecRule->setTitle          (  pRule->m_title                  );
    pRecRule->setSubTitle       (  pRule->m_subtitle               );
    pRecRule->setDescription    (  pRule->m_description            );
    pRecRule->setSeason         (  pRule->m_season                 );
    pRecRule->setEpisode        (  pRule->m_episode                );
    pRecRule->setCategory       (  pRule->m_category               );
    pRecRule->setStartTime      (  QDateTime(pRule->m_startdate,
                                             pRule->m_starttime, Qt::UTC));
    pRecRule->setEndTime        (  QDateTime(pRule->m_enddate,
                                             pRule->m_endtime, Qt::UTC));
    pRecRule->setSeriesId       (  pRule->m_seriesid               );
    pRecRule->setProgramId      (  pRule->m_programid              );
    pRecRule->setInetref        (  pRule->m_inetref                );
    pRecRule->setChanId         (  pRule->m_channelid              );
    pRecRule->setCallSign       (  pRule->m_station                );
    pRecRule->setFindDay        (  pRule->m_findday                );
    pRecRule->setFindTime       (  pRule->m_findtime               );
    pRecRule->setType           (  toRawString(pRule->m_type)      );
    pRecRule->setSearchType     (  toRawString(pRule->m_searchType));
    pRecRule->setRecPriority    (  pRule->m_recPriority            );
    pRecRule->setPreferredInput (  pRule->m_prefInput              );
    pRecRule->setStartOffset    (  pRule->m_startOffset            );
    pRecRule->setEndOffset      (  pRule->m_endOffset              );
    pRecRule->setDupMethod      (  toRawString(pRule->m_dupMethod) );
    pRecRule->setDupIn          (  toRawString(pRule->m_dupIn)     );
    pRecRule->setFilter         (  pRule->m_filter                 );
    pRecRule->setRecProfile     (  pRule->m_recProfile             );
    pRecRule->setRecGroup       (  RecordingInfo::GetRecgroupString(pRule->m_recGroupID) );
    pRecRule->setStorageGroup   (  pRule->m_storageGroup           );
    pRecRule->setPlayGroup      (  pRule->m_playGroup              );
    pRecRule->setAutoExpire     (  pRule->m_autoExpire             );
    pRecRule->setMaxEpisodes    (  pRule->m_maxEpisodes            );
    pRecRule->setMaxNewest      (  pRule->m_maxNewest              );
    pRecRule->setAutoCommflag   (  pRule->m_autoCommFlag           );
    pRecRule->setAutoTranscode  (  pRule->m_autoTranscode          );
    pRecRule->setAutoMetaLookup (  pRule->m_autoMetadataLookup     );
    pRecRule->setAutoUserJob1   (  pRule->m_autoUserJob1           );
    pRecRule->setAutoUserJob2   (  pRule->m_autoUserJob2           );
    pRecRule->setAutoUserJob3   (  pRule->m_autoUserJob3           );
    pRecRule->setAutoUserJob4   (  pRule->m_autoUserJob4           );
    pRecRule->setTranscoder     (  pRule->m_transcoder             );
    pRecRule->setNextRecording  (  pRule->m_nextRecording          );
    pRecRule->setLastRecorded   (  pRule->m_lastRecorded           );
    pRecRule->setLastDeleted    (  pRule->m_lastDeleted            );
    pRecRule->setAverageDelay   (  pRule->m_averageDelay           );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillArtworkInfoList( DTC::ArtworkInfoList *pArtworkInfoList,
                          const QString        &sInetref,
                          uint                  nSeason )
{
    ArtworkMap map = GetArtwork(sInetref, nSeason);

    for (ArtworkMap::const_iterator i = map.begin();
         i != map.end(); ++i)
    {
        DTC::ArtworkInfo *pArtInfo = pArtworkInfoList->AddNewArtworkInfo();
        pArtInfo->setFileName(i.value().url);
        switch (i.key())
        {
            case kArtworkFanart:
                pArtInfo->setStorageGroup("Fanart");
                pArtInfo->setType("fanart");
                pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Fanart")
                              .arg(QUrl(i.value().url).path()));
                break;
            case kArtworkBanner:
                pArtInfo->setStorageGroup("Banners");
                pArtInfo->setType("banner");
                pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Banners")
                              .arg(QUrl(i.value().url).path()));
                break;
            case kArtworkCoverart:
            default:
                pArtInfo->setStorageGroup("Coverart");
                pArtInfo->setType("coverart");
                pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Coverart")
                              .arg(QUrl(i.value().url).path()));
                break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillVideoMetadataInfo (
                      DTC::VideoMetadataInfo *pVideoMetadataInfo,
                      VideoMetadataListManager::VideoMetadataPtr pMetadata,
                      bool          bDetails)
{
    pVideoMetadataInfo->setId(pMetadata->GetID());
    pVideoMetadataInfo->setTitle(pMetadata->GetTitle());
    pVideoMetadataInfo->setSubTitle(pMetadata->GetSubtitle());
    pVideoMetadataInfo->setTagline(pMetadata->GetTagline());
    pVideoMetadataInfo->setDirector(pMetadata->GetDirector());
    pVideoMetadataInfo->setStudio(pMetadata->GetStudio());
    pVideoMetadataInfo->setDescription(pMetadata->GetPlot());
    pVideoMetadataInfo->setCertification(pMetadata->GetRating());
    pVideoMetadataInfo->setInetref(pMetadata->GetInetRef());
    pVideoMetadataInfo->setCollectionref(pMetadata->GetCollectionRef());
    pVideoMetadataInfo->setHomePage(pMetadata->GetHomepage());
    pVideoMetadataInfo->setReleaseDate(
        QDateTime(pMetadata->GetReleaseDate(),
                  QTime(0,0),Qt::LocalTime).toUTC());
    pVideoMetadataInfo->setAddDate(
        QDateTime(pMetadata->GetInsertdate(),
                  QTime(0,0),Qt::LocalTime).toUTC());
    pVideoMetadataInfo->setUserRating(pMetadata->GetUserRating());
    pVideoMetadataInfo->setLength(pMetadata->GetLength());
    pVideoMetadataInfo->setPlayCount(pMetadata->GetPlayCount());
    pVideoMetadataInfo->setSeason(pMetadata->GetSeason());
    pVideoMetadataInfo->setEpisode(pMetadata->GetEpisode());
    pVideoMetadataInfo->setParentalLevel(pMetadata->GetShowLevel());
    pVideoMetadataInfo->setVisible(pMetadata->GetBrowse());
    pVideoMetadataInfo->setWatched(pMetadata->GetWatched());
    pVideoMetadataInfo->setProcessed(pMetadata->GetProcessed());
    pVideoMetadataInfo->setContentType(ContentTypeToString(
                                       pMetadata->GetContentType()));
    pVideoMetadataInfo->setFileName(pMetadata->GetFilename());
    pVideoMetadataInfo->setHash(pMetadata->GetHash());
    pVideoMetadataInfo->setHostName(pMetadata->GetHost());
    pVideoMetadataInfo->setCoverart(pMetadata->GetCoverFile());
    pVideoMetadataInfo->setFanart(pMetadata->GetFanart());
    pVideoMetadataInfo->setBanner(pMetadata->GetBanner());
    pVideoMetadataInfo->setScreenshot(pMetadata->GetScreenshot());
    pVideoMetadataInfo->setTrailer(pMetadata->GetTrailer());
    pVideoMetadataInfo->setSerializeArtwork( true );

    if (bDetails)
    {
        if (!pMetadata->GetFanart().isEmpty())
        {
            DTC::ArtworkInfo *pArtInfo =
                pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Fanart");
            pArtInfo->setType("fanart");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Fanart")
                              .arg(pMetadata->GetFanart()));
        }
        if (!pMetadata->GetCoverFile().isEmpty())
        {
            DTC::ArtworkInfo *pArtInfo =
                pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Coverart");
            pArtInfo->setType("coverart");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Coverart")
                              .arg(pMetadata->GetCoverFile()));
        }
        if (!pMetadata->GetBanner().isEmpty())
        {
            DTC::ArtworkInfo *pArtInfo =
                    pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Banners");
            pArtInfo->setType("banner");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Banners")
                              .arg(pMetadata->GetBanner()));
        }
        if (!pMetadata->GetScreenshot().isEmpty())
        {
            DTC::ArtworkInfo *pArtInfo =
                    pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Screenshots");
            pArtInfo->setType("screenshot");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                              "&FileName=%2").arg("Screenshots")
                              .arg(pMetadata->GetScreenshot()));
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillInputInfo(DTC::Input* input, InputInfo inputInfo)
{
    input->setId(inputInfo.inputid);
    input->setInputName(inputInfo.name);
    input->setCardId(inputInfo.cardid);
    input->setSourceId(inputInfo.sourceid);
    input->setDisplayName(inputInfo.displayName);
    input->setLiveTVOrder(inputInfo.livetvorder);
    input->setScheduleOrder(inputInfo.scheduleOrder);
    input->setRecPriority(inputInfo.recPriority);
    input->setQuickTune(inputInfo.quickTune);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillCastMemberList(DTC::CastMemberList* pCastMemberList,
                        ProgramInfo* pInfo)
{
    if (!pCastMemberList || !pInfo)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    if (pInfo->GetFilesize() > 0) // FIXME: This shouldn't be the way to determine what is or isn't a recording!
        query.prepare("SELECT role, people.name FROM recordedcredits"
                        " AS credits"
                        " LEFT JOIN people ON credits.person = people.person"
                        " WHERE credits.chanid = :CHANID"
                        " AND credits.starttime = :STARTTIME"
                        " ORDER BY role;");
    else
        query.prepare("SELECT role, people.name FROM credits"
                        " LEFT JOIN people ON credits.person = people.person"
                        " WHERE credits.chanid = :CHANID"
                        " AND credits.starttime = :STARTTIME"
                        " ORDER BY role;");
    query.bindValue(":CHANID",    pInfo->GetChanID());
    query.bindValue(":STARTTIME", pInfo->GetScheduledStartTime());

    if (query.exec() && query.size() > 0)
    {
        QMap<QString, QString> translations;
        translations["ACTOR"] = QObject::tr("Actors");
        translations["DIRECTOR"] = QObject::tr("Director");
        translations["PRODUCER"] = QObject::tr("Producer");
        translations["EXECUTIVE_PRODUCER"] = QObject::tr("Executive Producer");
        translations["WRITER"] = QObject::tr("Writer");
        translations["GUEST_STAR"] = QObject::tr("Guest Star");
        translations["HOST"] = QObject::tr("Host");
        translations["ADAPTER"] = QObject::tr("Adapter");
        translations["PRESENTER"] = QObject::tr("Presenter");
        translations["COMMENTATOR"] = QObject::tr("Commentator");
        translations["GUEST"] = QObject::tr("Guest");

        while (query.next())
        {
            DTC::CastMember *pCastMember = pCastMemberList->AddNewCastMember();

            QString role = query.value(0).toString();
            pCastMember->setRole(role);
            pCastMember->setTranslatedRole(translations.value(role.toUpper()));
            /* The people.name column uses utf8_bin collation.
                * Qt-MySQL drivers use QVariant::ByteArray for string-type
                * MySQL fields marked with the BINARY attribute (those using a
                * *_bin collation) and QVariant::String for all others.
                * Since QVariant::toString() uses QString::fromAscii()
                * (through QVariant::convert()) when the QVariant's type is
                * QVariant::ByteArray, we have to use QString::fromUtf8()
                * explicitly to prevent corrupting characters.
                * The following code should be changed to use the simpler
                * toString() approach, as above, if we do a DB update to
                * coalesce the people.name values that differ only in case and
                * change the collation to utf8_general_ci, to match the
                * majority of other columns, or we'll have the same problem in
                * reverse.
                */
            pCastMember->setName(QString::fromUtf8(query.value(1)
                                        .toByteArray().constData()));

        }
    }

    //pCastMemberList->setCount(query.size());
    //pCastMemberList->setTotalAvailable(query.size());
}

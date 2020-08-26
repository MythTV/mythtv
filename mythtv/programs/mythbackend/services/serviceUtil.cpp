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
#include "datacontracts/cutList.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillProgramInfo( DTC::Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel /* = true */,
                      bool          bDetails    /* = true */,
                      bool          bIncCast    /* = true */)
{
    if ((pProgram == nullptr) || (pInfo == nullptr))
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
        pProgram->setLastModified( pInfo->GetLastModifiedTime() );
        pProgram->setProgramFlags( pInfo->GetProgramFlags()     );

        // ----
        // DEPRECATED - See RecordingInfo instead
        pProgram->setFileName    ( pInfo->GetPathname()         );
        pProgram->setFileSize    ( pInfo->GetFilesize()         );
        pProgram->setHostName    ( pInfo->GetHostname()         );
        // ----

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

    if ( pInfo->GetRecordingStatus() != RecStatus::Unknown )
    {
        pProgram->setSerializeRecording( true );

        DTC::RecordingInfo *pRecording = pProgram->Recording();

        const RecordingInfo pRecInfo(*pInfo);

        pRecording->setRecordedId ( pRecInfo.GetRecordingID()     );
        pRecording->setStatus  ( pRecInfo.GetRecordingStatus()    );
        pRecording->setPriority( pRecInfo.GetRecordingPriority()  );
        pRecording->setStartTs ( pRecInfo.GetRecordingStartTime() );
        pRecording->setEndTs   ( pRecInfo.GetRecordingEndTime()   );

        pRecording->setSerializeDetails( bDetails );

        if (bDetails)
        {
            pRecording->setFileName    ( pRecInfo.GetPathname() );
            pRecording->setFileSize    ( pRecInfo.GetFilesize() );
            pRecording->setHostName    ( pRecInfo.GetHostname() );
            pRecording->setLastModified( pRecInfo.GetLastModifiedTime() );

            pRecording->setRecordId    ( pRecInfo.GetRecordingRuleID()      );
            pRecording->setRecGroup    ( pRecInfo.GetRecordingGroup()       );
            pRecording->setPlayGroup   ( pRecInfo.GetPlaybackGroup()        );
            pRecording->setStorageGroup( pRecInfo.GetStorageGroup()         );
            pRecording->setRecType     ( pRecInfo.GetRecordingRuleType()    );
            pRecording->setDupInType   ( pRecInfo.GetDuplicateCheckSource() );
            pRecording->setDupMethod   ( pRecInfo.GetDuplicateCheckMethod() );
            pRecording->setEncoderId   ( pRecInfo.GetInputID()              );
            pRecording->setEncoderName ( pRecInfo.GetInputName()            );
            pRecording->setProfile     ( pRecInfo.GetProgramRecordingProfile() );
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
    pChannel->setChanId(channelInfo.m_chanId);
    pChannel->setChanNum(channelInfo.m_chanNum);
    pChannel->setCallSign(channelInfo.m_callSign);
    if (!channelInfo.m_icon.isEmpty())
    {
        QString sIconURL  = QString( "/Guide/GetChannelIcon?ChanId=%3")
                                    .arg( channelInfo.m_chanId );
        pChannel->setIconURL( sIconURL );
    }
    pChannel->setChannelName(channelInfo.m_name);
    pChannel->setVisible(channelInfo.m_visible > kChannelNotVisible);
    pChannel->setExtendedVisible(toRawString(channelInfo.m_visible));

    pChannel->setSerializeDetails( bDetails );

    if (bDetails)
    {
        pChannel->setMplexId(channelInfo.m_mplexId);
        pChannel->setServiceId(channelInfo.m_serviceId);
        pChannel->setATSCMajorChan(channelInfo.m_atscMajorChan);
        pChannel->setATSCMinorChan(channelInfo.m_atscMinorChan);
        pChannel->setFormat(channelInfo.m_tvFormat);
        pChannel->setFineTune(channelInfo.m_fineTune);
        pChannel->setFrequencyId(channelInfo.m_freqId);
        pChannel->setChanFilters(channelInfo.m_videoFilters);
        pChannel->setSourceId(channelInfo.m_sourceId);
        pChannel->setCommFree(channelInfo.m_commMethod == -2);
        pChannel->setUseEIT(channelInfo.m_useOnAirGuide);
        pChannel->setXMLTVID(channelInfo.m_xmltvId);
        pChannel->setDefaultAuth(channelInfo.m_defaultAuthority);
        pChannel->setServiceType(channelInfo.m_serviceType);

        QList<uint> groupIds = channelInfo.GetGroupIds();
        QString sGroupIds;
        for (int x = 0; x < groupIds.size(); x++)
        {
            if (x > 0)
                sGroupIds += ",";

            sGroupIds += QString::number(groupIds.at(x));
        }
        pChannel->setChannelGroups(sGroupIds);

        QList<uint> inputIds = channelInfo.GetInputIds();
        QString sInputIds;
        for (int x = 0; x < inputIds.size(); x++)
        {
            if (x > 0)
                sInputIds += ",";

            sInputIds += QString::number(inputIds.at(x));
        }
        pChannel->setInputs(sInputIds);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillChannelGroup(DTC::ChannelGroup* pGroup, const ChannelGroupItem& pGroupItem)
{
    if (!pGroup)
        return;

    pGroup->setGroupId(pGroupItem.m_grpId);
    pGroup->setName(pGroupItem.m_name);
    pGroup->setPassword(""); // Not currently supported
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillRecRuleInfo( DTC::RecRule  *pRecRule,
                      RecordingRule *pRule    )
{
    if ((pRecRule == nullptr) || (pRule == nullptr))
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
    pRecRule->setNewEpisOnly    (  newEpifromDupIn(pRule->m_dupIn) );
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

void FillGenreList(DTC::GenreList* pGenreList, int videoID)
{
    if (!pGenreList)
        return;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT genre from videogenre "
                  "LEFT JOIN videometadatagenre ON videometadatagenre.idgenre = videogenre.intid "
                  "WHERE idvideo = :ID "
                  "ORDER BY genre;");
    query.bindValue(":ID",    videoID);

    if (query.exec() && query.size() > 0)
    {
        while (query.next())
        {
            DTC::Genre *pGenre = pGenreList->AddNewGenre();
            QString genre = query.value(0).toString();
            pGenre->setName(genre);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillVideoMetadataInfo (
                      DTC::VideoMetadataInfo *pVideoMetadataInfo,
                      const VideoMetadataListManager::VideoMetadataPtr& pMetadata,
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
    pVideoMetadataInfo->setChildID(pMetadata->GetChildID());
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

    FillGenreList(pVideoMetadataInfo->Genres(), pVideoMetadataInfo->Id());
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillMusicMetadataInfo (DTC::MusicMetadataInfo *pVideoMetadataInfo,
                            MusicMetadata *pMetadata, bool bDetails)
{
    pVideoMetadataInfo->setId(pMetadata->ID());
    pVideoMetadataInfo->setArtist(pMetadata->Artist());
    pVideoMetadataInfo->setCompilationArtist(pMetadata->CompilationArtist());
    pVideoMetadataInfo->setAlbum(pMetadata->Album());
    pVideoMetadataInfo->setTitle(pMetadata->Title());
    pVideoMetadataInfo->setTrackNo(pMetadata->Track());
    pVideoMetadataInfo->setGenre(pMetadata->Genre());
    pVideoMetadataInfo->setYear(pMetadata->Year());
    pVideoMetadataInfo->setPlayCount(pMetadata->PlayCount());
    pVideoMetadataInfo->setLength(pMetadata->Length());
    pVideoMetadataInfo->setRating(pMetadata->Rating());
    pVideoMetadataInfo->setFileName(pMetadata->Filename());
    pVideoMetadataInfo->setHostName(pMetadata->Hostname());
    pVideoMetadataInfo->setLastPlayed(pMetadata->LastPlay());
    pVideoMetadataInfo->setCompilation(pMetadata->Compilation());

    if (bDetails)
    {
        //TODO add coverart here
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillInputInfo(DTC::Input* input, const InputInfo& inputInfo)
{
    input->setId(inputInfo.m_inputId);
    input->setInputName(inputInfo.m_name);
    input->setCardId(inputInfo.m_inputId);
    input->setSourceId(inputInfo.m_sourceId);
    input->setDisplayName(inputInfo.m_displayName);
    input->setLiveTVOrder(inputInfo.m_liveTvOrder);
    input->setScheduleOrder(inputInfo.m_scheduleOrder);
    input->setRecPriority(inputInfo.m_recPriority);
    input->setQuickTune(inputInfo.m_quickTune);
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
    {
        query.prepare("SELECT role, people.name FROM recordedcredits"
                        " AS credits"
                        " LEFT JOIN people ON credits.person = people.person"
                        " WHERE credits.chanid = :CHANID"
                        " AND credits.starttime = :STARTTIME"
                        " ORDER BY role;");
    }
    else
    {
        query.prepare("SELECT role, people.name FROM credits"
                        " LEFT JOIN people ON credits.person = people.person"
                        " WHERE credits.chanid = :CHANID"
                        " AND credits.starttime = :STARTTIME"
                        " ORDER BY role;");
    }
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillCutList(DTC::CutList* pCutList, RecordingInfo* rInfo, int marktype)
{
    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        rInfo->QueryCutList(markMap);

        for (it = markMap.begin(); it != markMap.end(); ++it)
        {
            bool isend = (*it) == MARK_CUT_END || (*it) == MARK_COMM_END;
            if (marktype == 0)
            {
                DTC::Cutting *pCutting = pCutList->AddNewCutting();
                pCutting->setMark(*it);
                pCutting->setOffset(it.key());
            }
            else if (marktype == 1)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFramePosition(&offset, it.key(), isend))
                {
                  DTC::Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
            else if (marktype == 2)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFrameDuration(&offset, it.key(), isend))
                {
                  DTC::Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillCommBreak(DTC::CutList* pCutList, RecordingInfo* rInfo, int marktype)
{
    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        rInfo->QueryCommBreakList(markMap);

        for (it = markMap.begin(); it != markMap.end(); ++it)
        {
            bool isend = (*it) == MARK_CUT_END || (*it) == MARK_COMM_END;
            if (marktype == 0)
            {
                DTC::Cutting *pCutting = pCutList->AddNewCutting();
                pCutting->setMark(*it);
                pCutting->setOffset(it.key());
            }
            else if (marktype == 1)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFramePosition(&offset, it.key(), isend))
                {
                  DTC::Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
            else if (marktype == 2)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFrameDuration(&offset, it.key(), isend))
                {
                  DTC::Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void FillSeek(DTC::CutList* pCutList, RecordingInfo* rInfo, MarkTypes marktype)
{
    frm_pos_map_t markMap;
    frm_pos_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        rInfo->QueryPositionMap(markMap, marktype);

        for (it = markMap.begin(); it != markMap.end(); ++it)
        {
            DTC::Cutting *pCutting = pCutList->AddNewCutting();
            pCutting->setMark(it.key());
            pCutting->setOffset(it.value());
        }
    }
}

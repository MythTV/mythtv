#include "v2serviceUtil.h"
#include "videoutils.h"
#include "programinfo.h"
#include "recordinginfo.h"
#include "recordingtypes.h"
#include "channelutil.h"
#include "channelinfo.h"
#include "channelgroup.h"

void V2FillProgramInfo( V2Program *pProgram,
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
    pProgram->setVideoPropNames( pInfo->GetVideoPropertyNames() );
    pProgram->setAudioProps(  pInfo->GetAudioProperties()   );
    pProgram->setAudioPropNames( pInfo->GetAudioPropertyNames() );
    pProgram->setSubProps  (  pInfo->GetSubtitleType()      );
    pProgram->setSubPropNames( pInfo->GetSubtitleTypeNames() );

    if (bDetails)
    {
        pProgram->setSeriesId    ( pInfo->GetSeriesID()         );
        pProgram->setProgramId   ( pInfo->GetProgramID()        );
        pProgram->setStars       ( pInfo->GetStars()            );
        pProgram->setLastModified( pInfo->GetLastModifiedTime() );
        pProgram->setProgramFlags( pInfo->GetProgramFlags()     );
        pProgram->setProgramFlagNames( pInfo->GetProgramFlagNames() );

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

    if (bIncCast)
    {
        V2FillCastMemberList( pProgram->Cast(), pInfo );
    }

    if ( bIncChannel )
    {
        // Build Channel Child Element
        if (!V2FillChannelInfo( pProgram->Channel(), pInfo->GetChanID(), bDetails ))
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
        V2RecordingInfo *pRecording = pProgram->Recording();

        const RecordingInfo pRecInfo(*pInfo);

        pRecording->setRecordedId ( pRecInfo.GetRecordingID()     );
        pRecording->setStatus  ( pRecInfo.GetRecordingStatus()    );
        pRecording->setPriority( pRecInfo.GetRecordingPriority()  );
        pRecording->setStartTs ( pRecInfo.GetRecordingStartTime() );
        pRecording->setEndTs   ( pRecInfo.GetRecordingEndTime()   );

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
        V2FillArtworkInfoList( pProgram->Artwork(), pInfo->GetInetRef(),
                             pInfo->GetSeason());
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2FillChannelInfo( V2ChannelInfo *pChannel,
                      uint              nChanID,
                      bool              bDetails  /* = true */ )
{
    ChannelInfo channel;
    if (channel.Load(nChanID))
    {
        return V2FillChannelInfo(pChannel, channel, bDetails);
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2FillChannelInfo( V2ChannelInfo *pChannel,
                      const ChannelInfo &channelInfo,
                      bool              bDetails  /* = true */ )
{

    // TODO update V2ChannelInfo to match functionality of ChannelInfo,
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

void V2FillChannelGroup(V2ChannelGroup* pGroup, const ChannelGroupItem& pGroupItem)
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

void V2FillRecRuleInfo( V2RecRule  *pRecRule,
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

void V2FillArtworkInfoList( V2ArtworkInfoList *pArtworkInfoList,
                          const QString        &sInetref,
                          uint                  nSeason )
{
    ArtworkMap map = GetArtwork(sInetref, nSeason);
    for (auto i = map.cbegin(); i != map.cend(); ++i)
    {
        V2ArtworkInfo *pArtInfo = pArtworkInfoList->AddNewArtworkInfo();
        pArtInfo->setFileName(i.value().url);
        switch (i.key())
        {
            case kArtworkFanart:
                pArtInfo->setStorageGroup("Fanart");
                pArtInfo->setType("fanart");
                pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                    "&FileName=%2")
                    .arg("Fanart",
                         QString(QUrl::toPercentEncoding(
                            QUrl(i.value().url).path()))));
                break;
            case kArtworkBanner:
                pArtInfo->setStorageGroup("Banners");
                pArtInfo->setType("banner");
                pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                    "&FileName=%2")
                    .arg("Banners",
                         QString(QUrl::toPercentEncoding(
                            QUrl(i.value().url).path()))));
                break;
            case kArtworkCoverart:
            default:
                pArtInfo->setStorageGroup("Coverart");
                pArtInfo->setType("coverart");
                pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                    "&FileName=%2")
                    .arg("Coverart",
                         QString(QUrl::toPercentEncoding(
                            QUrl(i.value().url).path()))));
                break;
        }
    }
}

void V2FillGenreList(V2GenreList* pGenreList, int videoID)
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
            V2Genre *pGenre = pGenreList->AddNewGenre();
            QString genre = query.value(0).toString();
            pGenre->setName(genre);
        }
    }
}


void V2FillVideoMetadataInfo (
                      V2VideoMetadataInfo *pVideoMetadataInfo,
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
    pVideoMetadataInfo->setLength(pMetadata->GetLength().count());
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

    if (bDetails)
    {
        if (!pMetadata->GetFanart().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Fanart");
            pArtInfo->setType("fanart");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Fanart",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetFanart()))));
        }
        if (!pMetadata->GetCoverFile().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Coverart");
            pArtInfo->setType("coverart");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Coverart",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetCoverFile()))));
        }
        if (!pMetadata->GetBanner().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                    pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Banners");
            pArtInfo->setType("banner");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Banners",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetBanner()))));
        }
        if (!pMetadata->GetScreenshot().isEmpty())
        {
            V2ArtworkInfo *pArtInfo =
                    pVideoMetadataInfo->Artwork()->AddNewArtworkInfo();
            pArtInfo->setStorageGroup("Screenshots");
            pArtInfo->setType("screenshot");
            pArtInfo->setURL(QString("/Content/GetImageFile?StorageGroup=%1"
                      "&FileName=%2")
                      .arg("Screenshots",
                           QString(
                           QUrl::toPercentEncoding(pMetadata->GetScreenshot()))));
        }
    }

    V2FillGenreList(pVideoMetadataInfo->Genres(), pVideoMetadataInfo->GetId());
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void V2FillMusicMetadataInfo (V2MusicMetadataInfo *pVideoMetadataInfo,
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
    pVideoMetadataInfo->setLength(pMetadata->Length().count());
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

void V2FillInputInfo(V2Input* input, const InputInfo& inputInfo)
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



void V2FillCastMemberList(V2CastMemberList* pCastMemberList,
                        ProgramInfo* pInfo)
{
    if (!pCastMemberList || !pInfo)
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    QString table;
    if (pInfo->GetFilesize() > 0) // FIXME: This shouldn't be the way to determine what is or isn't a recording!
        table = "recordedcredits";
    else
        table = "credits";

    query.prepare(QString("SELECT role, people.name, roles.name FROM %1"
                          " AS credits"
                          " LEFT JOIN people ON"
                          "  credits.person = people.person"
                          " LEFT JOIN roles ON"
                          "  credits.roleid = roles.roleid"
                          " WHERE credits.chanid = :CHANID"
                          " AND credits.starttime = :STARTTIME"
                          " ORDER BY role, priority;").arg(table));

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
            V2CastMember *pCastMember = pCastMemberList->AddNewCastMember();

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
            pCastMember->setCharacterName(QString::fromUtf8(query.value(2)
                                                   .toByteArray().constData()));
        }
    }

}


void V2FillCutList(V2CutList* pCutList, RecordingInfo* rInfo, int marktype)
{
    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        rInfo->QueryCutList(markMap);

        for (it = markMap.cbegin(); it != markMap.cend(); ++it)
        {
            bool isend = (*it) == MARK_CUT_END || (*it) == MARK_COMM_END;
            if (marktype == 0)
            {
                V2Cutting *pCutting = pCutList->AddNewCutting();
                pCutting->setMark(*it);
                pCutting->setOffset(it.key());
            }
            else if (marktype == 1)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFramePosition(&offset, it.key(), isend))
                {
                  V2Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
            else if (marktype == 2)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFrameDuration(&offset, it.key(), isend))
                {
                  V2Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
        }
    }
}

void V2FillCommBreak(V2CutList* pCutList, RecordingInfo* rInfo, int marktype)
{
    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        rInfo->QueryCommBreakList(markMap);

        for (it = markMap.cbegin(); it != markMap.cend(); ++it)
        {
            bool isend = (*it) == MARK_CUT_END || (*it) == MARK_COMM_END;
            if (marktype == 0)
            {
                V2Cutting *pCutting = pCutList->AddNewCutting();
                pCutting->setMark(*it);
                pCutting->setOffset(it.key());
            }
            else if (marktype == 1)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFramePosition(&offset, it.key(), isend))
                {
                  V2Cutting *pCutting = pCutList->AddNewCutting();
                  pCutting->setMark(*it);
                  pCutting->setOffset(offset);
                }
            }
            else if (marktype == 2)
            {
                uint64_t offset = 0;
                if (rInfo->QueryKeyFrameDuration(&offset, it.key(), isend))
                {
                  V2Cutting *pCutting = pCutList->AddNewCutting();
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

void V2FillSeek(V2CutList* pCutList, RecordingInfo* rInfo, MarkTypes marktype)
{
    frm_pos_map_t markMap;
    frm_pos_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        rInfo->QueryPositionMap(markMap, marktype);

        for (it = markMap.cbegin(); it != markMap.cend(); ++it)
        {
            V2Cutting *pCutting = pCutList->AddNewCutting();
            pCutting->setMark(it.key());
            pCutting->setOffset(it.value());
        }
    }
}

int V2CreateRecordingGroup(const QString& groupName)
{
    int groupID = -1;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("INSERT INTO recgroups SET recgroup = :NAME, "
                  "displayname = :DISPLAYNAME");
    query.bindValue(":NAME", groupName);
    query.bindValue(":DISPLAYNAME", groupName);

    if (query.exec())
        groupID = query.lastInsertId().toInt();

    if (groupID <= 0)
        LOG(VB_GENERAL, LOG_ERR, QString("Could not create recording group (%1). "
                                         "Does it already exist?").arg(groupName));

    return groupID;
}

DBCredits * V2jsonCastToCredits(const QJsonObject &cast)
{
    int priority = 1;
    auto* credits = new DBCredits;

    QJsonArray members = cast["CastMembers"].toArray();
    for (const auto & m : members)
    {
        QJsonObject actor     = m.toObject();
        QString     name      = actor.value("Name").toString("");
        QString     character = actor.value("CharacterName").toString("");
        QString     role      = actor.value("Role").toString("");

        credits->push_back(DBPerson(role, name, priority, character));
        ++priority;
    }

    return credits;
}

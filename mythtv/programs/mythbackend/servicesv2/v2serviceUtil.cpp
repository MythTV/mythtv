// Standard UNIX C headers
#include <unistd.h>
#include <fcntl.h>
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__OpenBSD__) || defined(_WIN32)
#include <sys/types.h>
#else
#include <sys/sysmacros.h>
#endif
#include <sys/stat.h>

// Qt
#include <QDir>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonObject>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythscheduler.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingtypes.h"
#include "libmythmetadata/videoutils.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelgroup.h"
#include "libmythtv/channelinfo.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/recorders/firewiredevice.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/tv_rec.h"

// MythBackend
#include "backendcontext.h"
#include "encoderlink.h"
#include "scheduler.h"
#include "v2encoder.h"
#include "v2frontend.h"
#include "v2serviceUtil.h"

void V2FillProgramInfo( V2Program *pProgram,
                      ProgramInfo  *pInfo,
                      bool          bIncChannel   /* = true */,
                      bool          bDetails      /* = true */,
                      bool          bIncCast      /* = true */,
                      bool          bIncArtwork   /* = true */,
                      bool          bIncRecording /* = true */)
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
        V2FillCastMemberList( pProgram->Cast(), pInfo );
    else
        pProgram->enableCast(false);

    if (bIncChannel)
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
    else
    {
        pProgram->enableChannel(false);
    }

    // Build Recording Child Element

    if ( bIncRecording && pInfo->GetRecordingStatus() != RecStatus::Unknown )
    {
        V2RecordingInfo *pRecording = pProgram->Recording();

        const RecordingInfo pRecInfo(*pInfo);

        pRecording->setRecordedId ( pRecInfo.GetRecordingID()     );
        pRecording->setStatus  ( pRecInfo.GetRecordingStatus()    );
        pRecording->setStatusName  (  RecStatus::toString( pRecInfo.GetRecordingStatus() )   );
        pRecording->setRecTypeStatus  ( pRecInfo.GetRecTypeStatus(true)    );
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
    else
    {
        pProgram->enableRecording(false);
    }

    if ( bIncArtwork && !pInfo->GetInetRef().isEmpty() )
        V2FillArtworkInfoList( pProgram->Artwork(), pInfo->GetInetRef(), pInfo->GetSeason());
    else
        pProgram->enableArtwork(false);
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
        QString sIconURL  = QString( "/Guide/GetChannelIcon?FileName=%1")
                                    .arg( channelInfo.m_icon );
        pChannel->setIconURL( sIconURL );
        pChannel->setIcon( channelInfo.m_icon );
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
        pChannel->setRecPriority(channelInfo.m_recPriority);
        pChannel->setTimeOffset(channelInfo.m_tmOffset);
        pChannel->setCommMethod(channelInfo.m_commMethod);

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
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    pRecRule->setStartTime      (  QDateTime(pRule->m_startdate,
                                             pRule->m_starttime, Qt::UTC));
    pRecRule->setEndTime        (  QDateTime(pRule->m_enddate,
                                             pRule->m_endtime, Qt::UTC));
#else
    static const QTimeZone utc(QTimeZone::UTC);
    pRecRule->setStartTime      (  QDateTime(pRule->m_startdate,
                                             pRule->m_starttime, utc));
    pRecRule->setEndTime        (  QDateTime(pRule->m_enddate,
                                             pRule->m_endtime, utc));
#endif
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
    pRecRule->setAutoExtend     (  toString(pRule->m_autoExtend)   );
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
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    pVideoMetadataInfo->setReleaseDate(
        QDateTime(pMetadata->GetReleaseDate(),
                  QTime(0,0),Qt::LocalTime).toUTC());
    pVideoMetadataInfo->setAddDate(
        QDateTime(pMetadata->GetInsertdate(),
                  QTime(0,0),Qt::LocalTime).toUTC());
#else
    static const QTimeZone localtime(QTimeZone::LocalTime);
    pVideoMetadataInfo->setReleaseDate(
        QDateTime(pMetadata->GetReleaseDate(),
                  QTime(0,0),localtime).toUTC());
    pVideoMetadataInfo->setAddDate(
        QDateTime(pMetadata->GetInsertdate(),
                  QTime(0,0),localtime).toUTC());
#endif
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
    pVideoMetadataInfo->setCategory(pMetadata->GetCategoryID());

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

    auto castList = pMetadata->GetCast();
    V2CastMemberList* pCastMemberList = pVideoMetadataInfo->Cast();

    QString actors = QObject::tr("Actors");
    for (const VideoMetadata::cast_entry& ent : castList )
    {
        V2CastMember *pCastMember = pCastMemberList->AddNewCastMember();
        pCastMember->setTranslatedRole(actors);
        pCastMember->setRole("ACTOR");
        pCastMember->setName(ent.second);
    }

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
            pCastMember->setTranslatedRole(translations.value(role.toUpper()));
            pCastMember->setRole(role); // role is invalid after this call.
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


void V2FillCutList(V2CutList* pCutList, ProgramInfo* rInfo, int marktype, bool includeFps)
{
    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;

    if (rInfo && rInfo->GetChanID())
    {
        if (includeFps)
        {
            rInfo->QueryMarkupMap(markMap, MARK_VIDEO_RATE);
            it = markMap.cbegin();
            if (it != markMap.cend())
            {
                V2Cutting *pCutting = pCutList->AddNewCutting();
                pCutting->setMark(*it);
                pCutting->setOffset(it.key());
            }
            markMap.clear();
        }
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

void V2FillCommBreak(V2CutList* pCutList, ProgramInfo* rInfo, int marktype, bool includeFps)
{
    frm_dir_map_t markMap;
    frm_dir_map_t::const_iterator it;

    if (rInfo)
    {
        if (includeFps)
        {
            rInfo->QueryMarkupMap(markMap, MARK_VIDEO_RATE);
            it = markMap.cbegin();
            if (it != markMap.cend())
            {
                V2Cutting *pCutting = pCutList->AddNewCutting();
                pCutting->setMark(*it);
                pCutting->setOffset(it.key());
            }
            markMap.clear();
        }
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

void FillEncoderList(QVariantList &list, QObject* parent)
{
    QReadLocker tvlocker(&TVRec::s_inputsLock);
    QList<InputInfo> inputInfoList = CardUtil::GetAllInputInfo(true);
    for (auto * elink : std::as_const(gTVList))
    {
        if (elink != nullptr)
        {
            // V2Encoder *pEncoder = list->AddNewEncoder();
            auto *pEncoder = new V2Encoder( parent );
            list.append( QVariant::fromValue<QObject *>( pEncoder ));

            pEncoder->setId            ( elink->GetInputID()      );
            pEncoder->setState         ( elink->GetState()        );
            pEncoder->setLocal         ( elink->IsLocal()         );
            pEncoder->setConnected     ( elink->IsConnected()     );
            pEncoder->setSleepStatus   ( elink->GetSleepStatus()  );

            if (pEncoder->GetLocal())
                pEncoder->setHostName( gCoreContext->GetHostName() );
            else
                pEncoder->setHostName( elink->GetHostName() );

            for (const auto & inputInfo : std::as_const(inputInfoList))
            {
                if (inputInfo.m_inputId == static_cast<uint>(elink->GetInputID()))
                {
                    V2Input *input = pEncoder->AddNewInput();
                    V2FillInputInfo(input, inputInfo);
                }
            }

            bool progFound = false;
            V2Program *pProgram = pEncoder->Recording();
            switch ( pEncoder->GetState() )
            {
                case kState_WatchingLiveTV:
                case kState_RecordingOnly:
                case kState_WatchingRecording:
                {
                    ProgramInfo  *pInfo = elink->GetRecording();

                    if (pInfo)
                    {
                        progFound= true;
                        V2FillProgramInfo( pProgram, pInfo, true, true );
                        delete pInfo;
                    }

                    break;
                }

                default:
                    break;
            }
            if (!progFound)
                pProgram->setProperty("isNull",QVariant(true));
        }
    }
}

// Note - special value -999 for nRecStatus means all values less than 0.
// This is needed by BackendStatus API
int FillUpcomingList(QVariantList &list, QObject* parent,
                                        int& nStartIndex,
                                        int& nCount,
                                        bool bShowAll,
                                        int  nRecordId,
                                        int  nRecStatus,
                                        const QString &Sort,
                                        const QString &RecGroup )
{
    RecordingList  recordingList; // Auto-delete deque
    RecList  tmpList; // Standard deque, objects must be deleted

    if (nRecordId <= 0)
        nRecordId = -1;

    // For nRecStatus to be effective, showAll must be true.
    if (nRecStatus != 0)
        bShowAll = true;

    // NOTE: Fetching this information directly from the schedule is
    //       significantly faster than using ProgramInfo::LoadFromScheduler()
    auto *scheduler = dynamic_cast<Scheduler*>(gCoreContext->GetScheduler());
    if (scheduler)
        scheduler->GetAllPending(tmpList, nRecordId);

    // Sort the upcoming into only those which will record
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = tmpList.begin(); it < tmpList.end(); ++it)
    {
        if ((nRecStatus == -999
               && (*it)->GetRecordingStatus() >= 0)
         || (nRecStatus != 0 && nRecStatus != -999
               && (*it)->GetRecordingStatus() != nRecStatus))
        {
            delete *it;
            *it = nullptr;
            continue;
        }

        if (!RecGroup.isEmpty())
        {
            if ( (*it)-> GetRecordingGroup() != RecGroup )
            {
                delete *it;
                *it = nullptr;
                continue;
            }
        }

        if (!bShowAll && ((((*it)->GetRecordingStatus() >= RecStatus::Pending) &&
                           ((*it)->GetRecordingStatus() <= RecStatus::WillRecord)) ||
                          ((*it)->GetRecordingStatus() == RecStatus::Offline) ||
                          ((*it)->GetRecordingStatus() == RecStatus::Conflict)) &&
            ((*it)->GetRecordingEndTime() > MythDate::current()))
        {   // NOLINT(bugprone-branch-clone)
            recordingList.push_back(new RecordingInfo(**it));
        }
        else if (bShowAll &&
                 ((*it)->GetRecordingEndTime() > MythDate::current()))
        {
            recordingList.push_back(new RecordingInfo(**it));
        }

        delete *it;
        *it = nullptr;
    }

    // Sort the list

    int sortType = 0;
    if (Sort.startsWith("channum", Qt::CaseInsensitive))
        sortType = 10;
    else if (Sort.startsWith("title", Qt::CaseInsensitive))
        sortType = 20;
    else if (Sort.startsWith("length", Qt::CaseInsensitive))
        sortType = 30;
    else if (Sort.startsWith("status", Qt::CaseInsensitive))
        sortType = 40;
    if (Sort.endsWith("desc", Qt::CaseInsensitive))
        sortType += 1;

    static QRegularExpression regex("[_-]");

    auto comp = [sortType](const RecordingInfo *First, const RecordingInfo *Second)
    {
        switch (sortType)
        {
            case 0:
                return First->GetScheduledStartTime() < Second->GetScheduledStartTime();
            case 1:
                return First->GetScheduledStartTime() > Second->GetScheduledStartTime();
            case 10:
                return First->GetChanNum().replace(regex,".").toDouble()
                     < Second->GetChanNum().replace(regex,".").toDouble();
            case 11:
                return First->GetChanNum().replace(regex,".").toDouble()
                     > Second->GetChanNum().replace(regex,".").toDouble();
            case 20:
                return QString::compare(First->GetTitle(), Second->GetTitle(), Qt::CaseInsensitive) < 0 ;
            case 21:
                return QString::compare(First->GetTitle(), Second->GetTitle(), Qt::CaseInsensitive) > 0 ;
            case 30:
            {
                qint64 time1 = First->GetScheduledStartTime().msecsTo( First->GetScheduledEndTime());
                qint64 time2 = Second->GetScheduledStartTime().msecsTo( Second->GetScheduledEndTime());
                return time1 < time2 ;
            }
            case 31:
            {
                qint64 time1 = First->GetScheduledStartTime().msecsTo( First->GetScheduledEndTime());
                qint64 time2 = Second->GetScheduledStartTime().msecsTo( Second->GetScheduledEndTime());
                return time1 > time2 ;
            }
            case 40:
                return QString::compare(RecStatus::toString(First->GetRecordingStatus()),
                                        RecStatus::toString(Second->GetRecordingStatus()),
                                        Qt::CaseInsensitive) < 0 ;
            case 41:
                return QString::compare(RecStatus::toString(First->GetRecordingStatus()),
                                        RecStatus::toString(Second->GetRecordingStatus()),
                                        Qt::CaseInsensitive) > 0 ;
        }
        return false;
    };

    // no need to sort when zero because that is the default order from the scheduler
    if (sortType > 0)
        std::stable_sort(recordingList.begin(), recordingList.end(), comp);

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)recordingList.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)recordingList.size() ) : recordingList.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)recordingList.size() );

    for( int n = nStartIndex; n < nEndIndex; n++)
    {
        ProgramInfo *pInfo = recordingList[ n ];
        auto *pProgram = new V2Program( parent );
        list.append( QVariant::fromValue<QObject *>( pProgram ));
        V2FillProgramInfo( pProgram, pInfo, true );
    }

    return recordingList.size();
}

void FillFrontendList(QVariantList &list, QObject* parent, bool OnLine)
{
    QMap<QString, Frontend*> frontends;
    if (OnLine)
        frontends = gBackendContext->GetConnectedFrontends();
    else
        frontends = gBackendContext->GetFrontends();

    for (auto * fe : std::as_const(frontends))
    {
        auto *pFrontend = new V2Frontend( parent );
        list.append( QVariant::fromValue<QObject *>( pFrontend ));
        pFrontend->setName(fe->m_name);
        pFrontend->setIP(fe->m_ip.toString());
        int port = gCoreContext->GetNumSettingOnHost("FrontendStatusPort",
                                                        fe->m_name, 6547);
        pFrontend->setPort(port);
        pFrontend->setOnLine(fe->m_connectionCount > 0);
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
    for (const auto & m : std::as_const(members))
    {
        QJsonObject actor     = m.toObject();
        QString     name      = actor.value("Name").toString("");
        QString     character = actor.value("CharacterName").toString("");
        QString     role      = actor.value("Role").toString("");

        credits->emplace_back(role, name, priority++, character);
    }

    return credits;
}

// Code copied from class VideoDevice
V2CaptureDeviceList* getV4l2List  ( const QRegularExpression &driver, const QString & cardType )
{
    auto* pList = new V2CaptureDeviceList();
    uint minor_min = 0;
    uint minor_max = 15;
    QString card  = QString();

    // /dev/v4l/video*
    QDir dev("/dev/v4l", "video*", QDir::Name, QDir::System);
    fillSelectionsFromDir(dev, minor_min, minor_max,
                            card, driver, false, pList, cardType);

    // /dev/video*
    dev.setPath("/dev");
    fillSelectionsFromDir(dev, minor_min, minor_max,
                            card, driver, false, pList, cardType);

    // /dev/dtv/video*
    dev.setPath("/dev/dtv");
    fillSelectionsFromDir(dev, minor_min, minor_max,
                            card, driver, false, pList, cardType);

    // /dev/dtv*
    dev.setPath("/dev");
    dev.setNameFilters(QStringList("dtv*"));
    fillSelectionsFromDir(dev, minor_min, minor_max,
                            card, driver, false, pList, cardType);

    return pList;
}

uint fillSelectionsFromDir(const QDir& dir,
                            uint minor_min, uint minor_max,
                            const QString& card, const QRegularExpression& driver,
                            bool allow_duplicates, V2CaptureDeviceList *pList,
                            const QString & cardType)
{
    uint cnt = 0;
    QMap<uint, uint> minorlist;
    QFileInfoList entries = dir.entryInfoList();
    for (const auto & fi : std::as_const(entries))
    {
        struct stat st {};
        QString filepath = fi.absoluteFilePath();
        int err = lstat(filepath.toLocal8Bit().constData(), &st);

        if (err)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Could not stat file: %1").arg(filepath));
            continue;
        }

        // is this is a character device?
        if (!S_ISCHR(st.st_mode))
            continue;

        // is this device is in our minor range?
        uint minor_num = minor(st.st_rdev);
        if (minor_min > minor_num || minor_max < minor_num)
            continue;

        // ignore duplicates if allow_duplicates not set
        if (!allow_duplicates && minorlist[minor_num])
            continue;

        // if the driver returns any info add this device to our list
        QByteArray tmp = filepath.toLatin1();
        int videofd = open(tmp.constData(), O_RDWR);
        if (videofd >= 0)
        {
            QString card_name;
            QString driver_name;
            if (CardUtil::GetV4LInfo(videofd, card_name, driver_name))
            {
                auto match = driver.match(driver_name);
                if ((!driver.pattern().isEmpty() || match.hasMatch()) &&
                    (card.isEmpty() || (card_name == card)))
                {
                    auto* pDev = pList->AddCaptureDevice();
                    pDev->setCardType (cardType);
                    pDev->setVideoDevice (filepath);
                    pDev->setFrontendName(card_name);
                    QStringList inputs;
                    CardUtil::GetDeviceInputNames(filepath, cardType, inputs);
                    pDev->setInputNames(inputs);
                    inputs = CardUtil::ProbeAudioInputs(filepath, cardType);
                    pDev->setAudioDevices(inputs);
                    if (cardType == "HDPVR")
                        pDev->setChannelTimeout ( 15000 );
                    cnt++;
                }
            }
            close(videofd);
        }
        // add to list of minors discovered to avoid duplicates
        minorlist[minor_num] = 1;
    }

    return cnt;
}

V2CaptureDeviceList* getFirewireList ([[maybe_unused]] const QString & cardType)
{
    auto* pList = new V2CaptureDeviceList();

#if CONFIG_FIREWIRE
    std::vector<AVCInfo> list = FirewireDevice::GetSTBList();
    for (auto & info : list)
    {
        auto* pDev = pList->AddCaptureDevice();
        pDev->setCardType (cardType);
        QString guid = info.GetGUIDString();
        pDev->setVideoDevice (guid);
        QString model = FirewireDevice::GetModelName(info.m_vendorid, info.m_modelid);
        pDev->setFirewireModel(model);
        pDev->setDescription(info.m_product_name);
        pDev->setSignalTimeout ( 2000 );
        pDev->setChannelTimeout ( 9000 );
    }
#endif // CONFIG_FIREWIRE
    return pList;
}

#include <QApplication>
#include <QList>
#include <QUrl>

#include "mythcontext.h"
#include "videoutils.h"
#include "mythlogging.h"
#include "compat.h"
#include "remoteutil.h"

// Needed to perform a lookup
#include "metadatadownload.h"
#include "metadataimagedownload.h"

// Needed for video scanning
#include "videometadatalistmanager.h"
#include "globals.h"

// Input for a lookup
#include "videometadata.h"
#include "programinfo.h"
#include "recordingrule.h"

#include "metadatafactory.h"

QEvent::Type MetadataFactoryNoResult::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type MetadataFactorySingleResult::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type MetadataFactoryMultiResult::kEventType =
    (QEvent::Type) QEvent::registerEventType();

QEvent::Type MetadataFactoryVideoChanges::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataFactory::MetadataFactory(QObject *parent) :
    QObject(parent), m_scanning(false),
    m_returnList(), m_sync(false)
{
    m_lookupthread = new MetadataDownload(this);
    m_imagedownload = new MetadataImageDownload(this);
    m_videoscanner = new VideoScannerThread(this);

    m_mlm = new VideoMetadataListManager();
}

MetadataFactory::~MetadataFactory()
{
    if (m_lookupthread)
    {
        m_lookupthread->cancel();
        delete m_lookupthread;
        m_lookupthread = NULL;
    }

    if (m_imagedownload)
    {
        m_imagedownload->cancel();
        delete m_imagedownload;
        m_imagedownload = NULL;
    }

    if (m_videoscanner && m_videoscanner->wait())
        delete m_videoscanner;

    delete m_mlm;
    m_mlm = NULL;
}

void MetadataFactory::Lookup(RecordingRule *recrule, bool automatic,
                             bool getimages, bool allowgeneric)
{
    if (!recrule)
        return;

    MetadataLookup *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataRecording);
    lookup->SetSubtype(GuessLookupType(recrule));
    lookup->SetData(qVariantFromValue(recrule));
    lookup->SetAutomatic(automatic);
    lookup->SetHandleImages(getimages);
    lookup->SetAllowGeneric(allowgeneric);
    lookup->SetHost(gCoreContext->GetMasterHostName());
    lookup->SetTitle(recrule->m_title);
    lookup->SetSubtitle(recrule->m_subtitle);
    lookup->SetInetref(recrule->m_inetref);
    lookup->SetSeason(recrule->m_season);
    lookup->SetEpisode(recrule->m_episode);

    if (m_lookupthread->isRunning())
        m_lookupthread->prependLookup(lookup);
    else
        m_lookupthread->addLookup(lookup);
}

void MetadataFactory::Lookup(ProgramInfo *pginfo, bool automatic,
                             bool getimages, bool allowgeneric)
{
    if (!pginfo)
        return;

    MetadataLookup *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataRecording);
    lookup->SetSubtype(GuessLookupType(pginfo));
    lookup->SetData(qVariantFromValue(pginfo));
    lookup->SetAutomatic(automatic);
    lookup->SetHandleImages(getimages);
    lookup->SetAllowGeneric(allowgeneric);
    lookup->SetHost(gCoreContext->GetMasterHostName());
    lookup->SetTitle(pginfo->GetTitle());
    lookup->SetSubtitle(pginfo->GetSubtitle());
    lookup->SetSeason(pginfo->GetSeason());
    lookup->SetEpisode(pginfo->GetEpisode());
    lookup->SetInetref(pginfo->GetInetRef());

    if (m_lookupthread->isRunning())
        m_lookupthread->prependLookup(lookup);
    else
        m_lookupthread->addLookup(lookup);
}

void MetadataFactory::Lookup(VideoMetadata *metadata, bool automatic,
                             bool getimages, bool allowgeneric)
{
    if (!metadata)
        return;

    MetadataLookup *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataVideo);
    if (metadata->GetSeason() > 0 || metadata->GetEpisode() > 0)
        lookup->SetSubtype(kProbableTelevision);
    else if (metadata->GetSubtitle().isEmpty())
        lookup->SetSubtype(kProbableMovie);
    else
        lookup->SetSubtype(kUnknownVideo);
    lookup->SetData(qVariantFromValue(metadata));
    lookup->SetAutomatic(automatic);
    lookup->SetHandleImages(getimages);
    lookup->SetAllowGeneric(allowgeneric);
    lookup->SetHost(metadata->GetHost());
    lookup->SetTitle(metadata->GetTitle());
    lookup->SetSubtitle(metadata->GetSubtitle());
    lookup->SetSeason(metadata->GetSeason());
    lookup->SetEpisode(metadata->GetEpisode());
    lookup->SetInetref(metadata->GetInetRef());
    QString fntmp;
    if (metadata->GetHost().isEmpty())
        fntmp = metadata->GetFilename();
    else
        fntmp = generate_file_url("Videos", metadata->GetHost(),
                                      metadata->GetFilename());
    lookup->SetFilename(fntmp);

    if (m_lookupthread->isRunning())
        m_lookupthread->prependLookup(lookup);
    else
        m_lookupthread->addLookup(lookup);
}

void MetadataFactory::Lookup(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (m_lookupthread->isRunning())
        m_lookupthread->prependLookup(lookup);
    else
        m_lookupthread->addLookup(lookup);
}

META_PUBLIC MetadataLookupList MetadataFactory::SynchronousLookup(QString title,
                                                      QString subtitle,
                                                      QString inetref,
                                                      int season,
                                                      int episode,
                                                      QString grabber,
                                                      bool allowgeneric)
{
    MetadataLookup *lookup = new MetadataLookup();
    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataRecording);
    lookup->SetAutomatic(false);
    lookup->SetHandleImages(false);
    lookup->SetAllowGeneric(allowgeneric);
    lookup->SetTitle(title);
    lookup->SetSubtitle(subtitle);
    lookup->SetSeason(season);
    lookup->SetEpisode(episode);
    lookup->SetInetref(inetref);
    if (grabber.toLower() == "movie")
        lookup->SetSubtype(kProbableMovie);
    else if (grabber.toLower() == "tv" ||
             grabber.toLower() == "television")
        lookup->SetSubtype(kProbableTelevision);
    else
        lookup->SetSubtype(GuessLookupType(lookup));

    return SynchronousLookup(lookup);
}

MetadataLookupList MetadataFactory::SynchronousLookup(MetadataLookup *lookup)
{
    if (!lookup)
        return MetadataLookupList();

    m_sync = true;

    if (m_lookupthread->isRunning())
        m_lookupthread->prependLookup(lookup);
    else
        m_lookupthread->addLookup(lookup);

    while (m_returnList.isEmpty() && m_sync)
    {
        sleep(1);
        qApp->processEvents();
    }

    m_sync = false;

    delete lookup;

    return m_returnList;
}

bool MetadataFactory::VideoGrabbersFunctional()
{
    return (m_lookupthread->MovieGrabberWorks() &&
            m_lookupthread->TelevisionGrabberWorks());
}

void MetadataFactory::VideoScan()
{
    if (IsRunning())
        return;

    QStringList hosts;
    if (!RemoteGetActiveBackends(&hosts))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Could not retrieve list of "
                            "available backends.");
        return;
    }

    VideoScan(hosts);
}

void MetadataFactory::VideoScan(QStringList hosts)
{
    if (IsRunning())
        return;

    m_scanning = true;

    m_videoscanner->SetHosts(hosts);
    m_videoscanner->SetDirs(GetVideoDirs());
    m_videoscanner->start();
}

void MetadataFactory::OnMultiResult(MetadataLookupList list)
{
    if (list.isEmpty())
        return;

    if (parent())
        QCoreApplication::postEvent(parent(),
            new MetadataFactoryMultiResult(list));
}

void MetadataFactory::OnSingleResult(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (lookup->GetHandleImages())
    {
        DownloadMap map;

        ArtworkList coverartlist = lookup->GetArtwork(kArtworkCoverart);
        if (coverartlist.size())
        {
            ArtworkInfo info;
            info.url = coverartlist.takeLast().url;
            map.insert(kArtworkCoverart, info);
        }

        ArtworkList fanartlist = lookup->GetArtwork(kArtworkFanart);
        if (fanartlist.size())
        {
            ArtworkInfo info;
            int index = fanartlist.size();
            int season = (int)lookup->GetSeason();
            if (season > 0)
                index = max(0, index-season);
            else
                index--;
            info.url = fanartlist.takeAt(index).url;
            map.insert(kArtworkFanart, info);
        }

        ArtworkList bannerlist = lookup->GetArtwork(kArtworkBanner);
        if (bannerlist.size())
        {
            ArtworkInfo info;
            info.url = bannerlist.takeLast().url;
            map.insert(kArtworkBanner, info);
        }

        if (!lookup->GetType() == kMetadataRecording)
        {
            ArtworkList screenshotlist = lookup->GetArtwork(kArtworkScreenshot);
            if (screenshotlist.size())
            {
                ArtworkInfo info;
                info.url = screenshotlist.takeLast().url;
                map.insert(kArtworkScreenshot, info);
            }
        }
        lookup->SetDownloads(map);
        m_imagedownload->addDownloads(lookup);
    }
    else
    {
        if (m_scanning)
            OnVideoResult(lookup);
        else if (parent())
            QCoreApplication::postEvent(parent(),
                new MetadataFactorySingleResult(lookup));
    }
}

void MetadataFactory::OnNoResult(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (parent())
        QCoreApplication::postEvent(parent(),
            new MetadataFactoryNoResult(lookup));
}

void MetadataFactory::OnImageResult(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    if (parent())
        QCoreApplication::postEvent(parent(),
            new MetadataFactorySingleResult(lookup));
}

void MetadataFactory::OnVideoResult(MetadataLookup *lookup)
{
    if (!lookup)
       return;

    VideoMetadata *metadata = qVariantValue<VideoMetadata *>(lookup->GetData());

    if (!metadata)
        return;

    metadata->SetTitle(lookup->GetTitle());
    metadata->SetSubtitle(lookup->GetSubtitle());

    if (metadata->GetTagline().isEmpty())
        metadata->SetTagline(lookup->GetTagline());
    if (metadata->GetYear() == 1895 || metadata->GetYear() == 0)
        metadata->SetYear(lookup->GetYear());
    if (metadata->GetReleaseDate() == QDate())
        metadata->SetReleaseDate(lookup->GetReleaseDate());
    if (metadata->GetDirector() == VIDEO_DIRECTOR_UNKNOWN ||
        metadata->GetDirector().isEmpty())
    {
        QList<PersonInfo> director = lookup->GetPeople(kPersonDirector);
        if (director.count() > 0)
            metadata->SetDirector(director.takeFirst().name);
    }
    if (metadata->GetStudio().isEmpty())
    {
        QStringList studios = lookup->GetStudios();
        if (studios.count() > 0)
            metadata->SetStudio(studios.takeFirst());
    }
    if (metadata->GetPlot() == VIDEO_PLOT_DEFAULT ||
        metadata->GetPlot().isEmpty())
        metadata->SetPlot(lookup->GetDescription());
    if (metadata->GetUserRating() == 0)
        metadata->SetUserRating(lookup->GetUserRating());
    if (metadata->GetRating() == VIDEO_RATING_DEFAULT)
        metadata->SetRating(lookup->GetCertification());
    if (metadata->GetLength() == 0)
        metadata->SetLength(lookup->GetRuntime());
    if (metadata->GetSeason() == 0)
        metadata->SetSeason(lookup->GetSeason());
    if (metadata->GetEpisode() == 0)
        metadata->SetEpisode(lookup->GetEpisode());
    if (metadata->GetHomepage().isEmpty())
        metadata->SetHomepage(lookup->GetHomepage());

    metadata->SetInetRef(lookup->GetInetref());

//    m_d->AutomaticParentalAdjustment(metadata);

    // Cast
    QList<PersonInfo> actors = lookup->GetPeople(kPersonActor);
    QList<PersonInfo> gueststars = lookup->GetPeople(kPersonGuestStar);

    for (QList<PersonInfo>::const_iterator p = gueststars.begin();
        p != gueststars.end(); ++p)
    {
        actors.append(*p);
    }

    VideoMetadata::cast_list cast;
    QStringList cl;

    for (QList<PersonInfo>::const_iterator p = actors.begin();
        p != actors.end(); ++p)
    {
        cl.append((*p).name);
    }

    for (QStringList::const_iterator p = cl.begin();
        p != cl.end(); ++p)
    {
        QString cn = (*p).trimmed();
        if (cn.length())
        {
            cast.push_back(VideoMetadata::cast_list::
                        value_type(-1, cn));
        }
    }

    metadata->SetCast(cast);

    // Genres
    VideoMetadata::genre_list video_genres;
    QStringList genres = lookup->GetCategories();

    for (QStringList::const_iterator p = genres.begin();
        p != genres.end(); ++p)
    {
        QString genre_name = (*p).trimmed();
        if (genre_name.length())
        {
            video_genres.push_back(
                    VideoMetadata::genre_list::value_type(-1, genre_name));
        }
    }

    metadata->SetGenres(video_genres);

    // Countries
    VideoMetadata::country_list video_countries;
    QStringList countries = lookup->GetCountries();

    for (QStringList::const_iterator p = countries.begin();
        p != countries.end(); ++p)
    {
        QString country_name = (*p).trimmed();
        if (country_name.length())
        {
            video_countries.push_back(
                    VideoMetadata::country_list::value_type(-1,
                            country_name));
        }
    }

    metadata->SetCountries(video_countries);

    ArtworkMap map = lookup->GetDownloads();

    QUrl coverurl(map.value(kArtworkCoverart).url);
    if (!coverurl.path().isEmpty())
        metadata->SetCoverFile(coverurl.path().remove(0,1));

    QUrl fanarturl(map.value(kArtworkFanart).url);
    if (!fanarturl.path().isEmpty())
        metadata->SetFanart(fanarturl.path().remove(0,1));

    QUrl bannerurl(map.value(kArtworkBanner).url);
    if (!bannerurl.path().isEmpty())
        metadata->SetBanner(bannerurl.path().remove(0,1));

    QUrl sshoturl(map.value(kArtworkScreenshot).url);
    if (!sshoturl.path().isEmpty())
        metadata->SetScreenshot(sshoturl.path().remove(0,1));

    metadata->SetProcessed(true);
    metadata->UpdateDatabase();

    if (gCoreContext->HasGUI() && parent())
    {
        QCoreApplication::postEvent(parent(),
            new MetadataFactorySingleResult(lookup));
    }
    else
    {
        lookup->deleteLater();
    }
}

void MetadataFactory::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataLookupEvent::kEventType)
    {
        MetadataLookupEvent *lue = (MetadataLookupEvent *)levent;

        MetadataLookupList lul = lue->lookupList;

        if (lul.isEmpty())
            return;

        if (m_sync)
        {
            m_returnList = lul;
        }
        else if (lul.count() == 1)
        {
            OnSingleResult(lul.takeFirst());
        }
        else
        {
            OnMultiResult(lul);
        }
    }
    else if (levent->type() == MetadataLookupFailure::kEventType)
    {
        MetadataLookupFailure *luf = (MetadataLookupFailure *)levent;

        MetadataLookupList lul = luf->lookupList;

        if (lul.isEmpty())
            return;

        if (m_sync)
        {
            m_returnList = MetadataLookupList();
            m_sync = false;
        }
        if (lul.size())
        {
            OnNoResult(lul.takeFirst());
        }
    }
    else if (levent->type() == ImageDLEvent::kEventType)
    {
        ImageDLEvent *ide = (ImageDLEvent *)levent;

        MetadataLookup *lookup = ide->item;

        if (!lookup)
            return;

        if (m_scanning)
            OnVideoResult(lookup);
        else
            OnImageResult(lookup);
    }
    else if (levent->type() == VideoScanChanges::kEventType)
    {
        VideoScanChanges *vsc = (VideoScanChanges *)levent;

        if (!vsc)
            return;

        QList<int> additions = vsc->additions;
        QList<int> moves = vsc->moved;
        QList<int> deletions = vsc->deleted;

        if (!m_scanning)
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Video Scan Complete: a(%1) m(%2) d(%3)")
                .arg(additions.count()).arg(moves.count())
                .arg(deletions.count()));

            if (parent())
                QCoreApplication::postEvent(parent(),
                    new MetadataFactoryVideoChanges(additions, moves,
                                                    deletions));
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Video Scan Complete: a(%1) m(%2) d(%3)")
                .arg(additions.count()).arg(moves.count())
                .arg(deletions.count()));

            VideoMetadataListManager::metadata_list ml;
            VideoMetadataListManager::loadAllFromDatabase(ml);
            m_mlm->setList(ml);

            for (QList<int>::const_iterator it = additions.begin();
                it != additions.end(); ++it)
            {
                VideoMetadata *metadata = m_mlm->byID(*it).get();

                if (metadata)
                    Lookup(metadata, true, true);
            }
        }
        m_videoscanner->ResetCounts();
    }
}

// These functions exist to determine if we have enough
// information to conclusively call something a Show vs. Movie

LookupType GuessLookupType(ProgramInfo *pginfo)
{
    LookupType ret = kUnknownVideo;

    ProgramInfo::CategoryType catType = pginfo->GetCategoryType();
    if (catType == ProgramInfo::kCategoryNone)
        catType = pginfo->QueryCategoryType();

    if ((!pginfo->GetSubtitle().isEmpty() || pginfo->GetEpisode() > 0) &&
       (catType == ProgramInfo::kCategorySeries ||
        catType == ProgramInfo::kCategoryTVShow))
        ret = kProbableTelevision;
    else if (catType == ProgramInfo::kCategoryMovie)
        ret = kProbableMovie;
    else if (pginfo->GetSeason() > 0 || pginfo->GetEpisode() > 0 ||
        !pginfo->GetSubtitle().isEmpty())
        ret = kProbableTelevision;
    else
    {
        // Before committing to something being a movie, we
        // want to check its rule.  If the rule has a season
        // or episode number, but the recording doesn't,
        // and the rec doesn't have a subtitle, this is a
        // generic recording. If neither the rule nor the
        // recording have an inetref, season, episode, or
        // subtitle, it's *probably* a movie.  If it's some
        // weird combination of both, we've got to try everything.
        RecordingRule *rule = new RecordingRule();
        rule->m_recordID = pginfo->GetRecordingRuleID();
        rule->Load();
        int ruleepisode = rule->m_episode;
        delete rule;

        if (ruleepisode == 0 && pginfo->GetEpisode() == 0 &&
            pginfo->GetSubtitle().isEmpty())
            ret = kProbableMovie;
        else if (ruleepisode > 0 && pginfo->GetSubtitle().isEmpty())
            ret = kProbableGenericTelevision;
        else
            ret = kUnknownVideo;
    }

    return ret;
}

LookupType GuessLookupType(MetadataLookup *lookup)
{
    LookupType ret = kUnknownVideo;

    if (lookup->GetSeason() > 0 || lookup->GetEpisode() > 0 ||
        !lookup->GetSubtitle().isEmpty())
        ret = kProbableTelevision;
    else
        ret = kProbableMovie;

    return ret;
}


LookupType GuessLookupType(RecordingRule *recrule)
{
    LookupType ret = kUnknownVideo;

    if (recrule->m_season > 0 || recrule->m_episode > 0 ||
        !recrule->m_subtitle.isEmpty())
        ret = kProbableTelevision;
    else
        ret = kProbableMovie;

    return ret;
}


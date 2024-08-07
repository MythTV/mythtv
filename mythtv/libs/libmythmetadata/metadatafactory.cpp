
#include "metadatafactory.h"

// C++
#include <algorithm>
#include <unistd.h> // for sleep()

// QT
#include <QApplication>
#include <QList>
#include <QUrl>

// mythtv
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remoteutil.h"
#include "libmythtv/recordingrule.h"

// libmythmetadata
#include "videoutils.h"

// Needed to perform a lookup
#include "metadatadownload.h"
#include "metadataimagedownload.h"

// Needed for video scanning
#include "videometadatalistmanager.h"
#include "globals.h"

// Input for a lookup
#include "videometadata.h"


const QEvent::Type MetadataFactoryNoResult::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataFactoryNoResult::~MetadataFactoryNoResult()
{
    if (m_result)
    {
        m_result->DecrRef();
        m_result = nullptr;
    }
}

const QEvent::Type MetadataFactorySingleResult::kEventType =
    (QEvent::Type) QEvent::registerEventType();

MetadataFactorySingleResult::~MetadataFactorySingleResult()
{
    if (m_result)
    {
        m_result->DecrRef();
        m_result = nullptr;
    }
}

const QEvent::Type MetadataFactoryMultiResult::kEventType =
    (QEvent::Type) QEvent::registerEventType();

// Force this class to have a vtable so that dynamic_cast works.
// NOLINTNEXTLINE(modernize-use-equals-default)
MetadataFactoryMultiResult::~MetadataFactoryMultiResult()
{
}

const QEvent::Type MetadataFactoryVideoChanges::kEventType =
    (QEvent::Type) QEvent::registerEventType();

// Force this class to have a vtable so that dynamic_cast works.
// NOLINTNEXTLINE(modernize-use-equals-default)
MetadataFactoryVideoChanges::~MetadataFactoryVideoChanges()
{
}

MetadataFactory::MetadataFactory(QObject *parent) :
    QObject(parent),
    m_lookupthread(new MetadataDownload(this)),
    m_imagedownload(new MetadataImageDownload(this)),
    m_videoscanner(new VideoScannerThread(this)),
    m_mlm(new VideoMetadataListManager())
{
}

MetadataFactory::~MetadataFactory()
{
    if (m_lookupthread)
    {
        m_lookupthread->cancel();
        delete m_lookupthread;
        m_lookupthread = nullptr;
    }

    if (m_imagedownload)
    {
        m_imagedownload->cancel();
        delete m_imagedownload;
        m_imagedownload = nullptr;
    }

    if (m_videoscanner && m_videoscanner->wait())
        delete m_videoscanner;

    delete m_mlm;
    m_mlm = nullptr;
}

void MetadataFactory::Lookup(RecordingRule *recrule, bool automatic,
                             bool getimages, bool allowgeneric)
{
    if (!recrule)
        return;

    auto *lookup = new MetadataLookup();

    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataRecording);
    lookup->SetSubtype(GuessLookupType(recrule));
    lookup->SetData(QVariant::fromValue(recrule));
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

    auto *lookup = new MetadataLookup();

    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataRecording);
    lookup->SetSubtype(GuessLookupType(pginfo));
    lookup->SetData(QVariant::fromValue(pginfo));
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

    auto *lookup = new MetadataLookup();

    lookup->SetStep(kLookupSearch);
    lookup->SetType(kMetadataVideo);
    lookup->SetSubtype(GuessLookupType(metadata));
    lookup->SetData(QVariant::fromValue(metadata));
    lookup->SetAutomatic(automatic);
    lookup->SetHandleImages(getimages);
    lookup->SetAllowGeneric(allowgeneric);
    lookup->SetHost(metadata->GetHost());
    lookup->SetTitle(metadata->GetTitle());
    lookup->SetSubtitle(metadata->GetSubtitle());
    lookup->SetSeason(metadata->GetSeason());
    lookup->SetEpisode(metadata->GetEpisode());
    lookup->SetInetref(metadata->GetInetRef());
    lookup->SetFilename(generate_file_url("Videos", metadata->GetHost(),
                                      metadata->GetFilename()));

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

META_PUBLIC MetadataLookupList MetadataFactory::SynchronousLookup(const QString& title,
                                                      const QString& subtitle,
                                                      const QString& inetref,
                                                      int season,
                                                      int episode,
                                                      const QString& grabber,
                                                      bool allowgeneric)
{
    auto *lookup = new MetadataLookup();

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
        return {};

    m_sync = true;

    if (m_lookupthread->isRunning())
        m_lookupthread->prependLookup(lookup);
    else
        m_lookupthread->addLookup(lookup);

    while (m_returnList.isEmpty() && m_sync)
    {
        sleep(1);
        QCoreApplication::processEvents();
    }

    m_sync = false;

    return m_returnList;
}

bool MetadataFactory::VideoGrabbersFunctional()
{
    return (MetadataDownload::MovieGrabberWorks() &&
            MetadataDownload::TelevisionGrabberWorks());
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

void MetadataFactory::VideoScan(const QStringList& hosts)
{
    if (IsRunning())
        return;

    m_scanning = true;

    m_videoscanner->SetHosts(hosts);
    m_videoscanner->SetDirs(GetVideoDirs());
    m_videoscanner->start();
}

void MetadataFactory::OnMultiResult(const MetadataLookupList& list)
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
        if (!coverartlist.empty())
        {
            ArtworkInfo info;
            info.url = coverartlist.takeFirst().url;
            map.insert(kArtworkCoverart, info);
        }

        ArtworkList fanartlist = lookup->GetArtwork(kArtworkFanart);
        if (!fanartlist.empty())
        {
            ArtworkInfo info;
            int index = 0;
            int season = lookup->GetIsCollection() ? 0 : (int)lookup->GetSeason();
            if (season > 0 && season <= fanartlist.count())
                index = season - 1;
            info.url = fanartlist.takeAt(index).url;
            map.insert(kArtworkFanart, info);
        }

        ArtworkList bannerlist = lookup->GetArtwork(kArtworkBanner);
        if (!bannerlist.empty())
        {
            ArtworkInfo info;
            info.url = bannerlist.takeFirst().url;
            map.insert(kArtworkBanner, info);
        }

        if (lookup->GetType() != kMetadataRecording)
        {
            ArtworkList screenshotlist = lookup->GetArtwork(kArtworkScreenshot);
            if (!screenshotlist.empty())
            {
                ArtworkInfo info;
                info.url = screenshotlist.takeFirst().url;
                map.insert(kArtworkScreenshot, info);
            }
        }
        lookup->SetDownloads(map);
        lookup->IncrRef();
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

    auto *metadata = lookup->GetData().value<VideoMetadata *>();

    if (!metadata)
        return;

    metadata->SetTitle(lookup->GetTitle());
    metadata->SetSubtitle(lookup->GetSubtitle());

    if (metadata->GetTagline().isEmpty())
        metadata->SetTagline(lookup->GetTagline());
    if (metadata->GetYear() == VIDEO_YEAR_DEFAULT || metadata->GetYear() == 0)
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
    if (metadata->GetLength() == 0min)
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

    for (const auto& actor : std::as_const(gueststars))
        actors.append(actor);

    VideoMetadata::cast_list cast;
    QStringList cl;

    for (const auto& actor : std::as_const(actors))
        cl.append(actor.name);

    for (const auto& name : std::as_const(cl))
    {
        QString cn = name.trimmed();
        if (!cn.isEmpty())
        {
            cast.emplace_back(-1, cn);
        }
    }

    metadata->SetCast(cast);

    // Genres
    VideoMetadata::genre_list video_genres;
    QStringList genres = lookup->GetCategories();

    for (const auto& str : std::as_const(genres))
    {
        QString genre_name = str.trimmed();
        if (!genre_name.isEmpty())
        {
            video_genres.emplace_back(-1, genre_name);
        }
    }

    metadata->SetGenres(video_genres);

    // Countries
    VideoMetadata::country_list video_countries;
    QStringList countries = lookup->GetCountries();

    for (const auto& str : std::as_const(countries))
    {
        QString country_name = str.trimmed();
        if (!country_name.isEmpty())
        {
            video_countries.emplace_back(-1, country_name);
        }
    }

    metadata->SetCountries(video_countries);

    DownloadMap map = lookup->GetDownloads();

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
}

void MetadataFactory::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataLookupEvent::kEventType)
    {
        auto *lue = dynamic_cast<MetadataLookupEvent *>(levent);
        if (lue == nullptr)
            return;

        MetadataLookupList lul = lue->m_lookupList;
        if (lul.isEmpty())
            return;

        if (m_sync)
        {
            m_returnList = lul;
        }
        else if (lul.count() == 1)
        {
            OnSingleResult(lul[0]);
        }
        else
        {
            OnMultiResult(lul);
        }
    }
    else if (levent->type() == MetadataLookupFailure::kEventType)
    {
        auto *luf = dynamic_cast<MetadataLookupFailure *>(levent);
        if (luf == nullptr)
            return;

        MetadataLookupList lul = luf->m_lookupList;
        if (lul.isEmpty())
            return;

        if (m_sync)
        {
            m_returnList = MetadataLookupList();
            m_sync = false;
        }
        if (!lul.empty())
        {
            OnNoResult(lul[0]);
        }
    }
    else if (levent->type() == ImageDLEvent::kEventType)
    {
        auto *ide = dynamic_cast<ImageDLEvent *>(levent);
        if (ide == nullptr)
            return;

        MetadataLookup *lookup = ide->m_item;
        if (!lookup)
            return;

        if (m_scanning)
            OnVideoResult(lookup);
        else
            OnImageResult(lookup);
    }
    else if (levent->type() == ImageDLFailureEvent::kEventType)
    {
        auto *ide = dynamic_cast<ImageDLFailureEvent *>(levent);
        if (ide == nullptr)
            return;

        MetadataLookup *lookup = ide->m_item;
        if (!lookup)
            return;

        // propagate event on image download failure
        if (parent())
            QCoreApplication::postEvent(parent(),
                            new ImageDLFailureEvent(lookup));
    }
    else if (levent->type() == VideoScanChanges::kEventType)
    {
        auto *vsc = dynamic_cast<VideoScanChanges *>(levent);
        if (!vsc)
            return;

        QList<int> additions = vsc->m_additions;
        QList<int> moves = vsc->m_moved;
        QList<int> deletions = vsc->m_deleted;

        if (!m_scanning)
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("Video Scan Complete: a(%1) m(%2) d(%3)")
                .arg(additions.count()).arg(moves.count())
                .arg(deletions.count()));

            if (parent())
            {
                QCoreApplication::postEvent(parent(),
                    new MetadataFactoryVideoChanges(additions, moves,
                                                    deletions));
            }
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

            for (int id : std::as_const(additions))
            {
                VideoMetadata *metadata = m_mlm->byID(id).get();

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
    LookupType ret = GuessLookupType(pginfo->GetInetRef());

    if (ret != kUnknownVideo)
        return ret;

    ProgramInfo::CategoryType catType = pginfo->GetCategoryType();
    if (catType == ProgramInfo::kCategoryNone)
        catType = pginfo->QueryCategoryType();

    if ((!pginfo->GetSubtitle().isEmpty() || pginfo->GetEpisode() > 0) &&
       (catType == ProgramInfo::kCategorySeries ||
        catType == ProgramInfo::kCategoryTVShow))
        ret = kProbableTelevision; // NOLINT(bugprone-branch-clone)
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
        auto *rule = new RecordingRule();
        rule->m_recordID = pginfo->GetRecordingRuleID();
        // Load rule information from the database
        rule->Load();
        int ruleepisode = rule->m_episode;
        RecordingType rulerectype = rule->m_type;
        delete rule;

        // If recording rule is periodic, it's probably a TV show.
        if ((rulerectype == kDailyRecord) ||
            (rulerectype == kWeeklyRecord))
        {
            ret = kProbableTelevision;
        }
        else if (ruleepisode == 0 && pginfo->GetEpisode() == 0 &&
            pginfo->GetSubtitle().isEmpty())
        {
            ret = kProbableMovie;
        }
        else if (ruleepisode > 0 && pginfo->GetSubtitle().isEmpty())
        {
            ret = kProbableGenericTelevision;
        }
        else
        {
            ret = kUnknownVideo;
        }
    }

    return ret;
}

LookupType GuessLookupType(MetadataLookup *lookup)
{
    LookupType ret = GuessLookupType(lookup->GetInetref());

    if (ret != kUnknownVideo)
        return ret;

    if (lookup->GetSeason() > 0 || lookup->GetEpisode() > 0 ||
        !lookup->GetSubtitle().isEmpty())
        ret = kProbableTelevision;
    else
        ret = kProbableMovie;

    return ret;
}

LookupType GuessLookupType(VideoMetadata *metadata)
{
    LookupType ret = GuessLookupType(metadata->GetInetRef());

    if (ret != kUnknownVideo)
        return ret;

    if (metadata->GetSeason() > 0 || metadata->GetEpisode() > 0 ||
        !metadata->GetSubtitle().isEmpty())
        ret = kProbableTelevision;
    else
        ret = kProbableMovie;

    return ret;
}

LookupType GuessLookupType(RecordingRule *recrule)
{
    LookupType ret = GuessLookupType(recrule->m_inetref);

    if (ret != kUnknownVideo)
        return ret;

    if (recrule->m_season > 0 || recrule->m_episode > 0 ||
        (recrule->m_type == kDailyRecord) ||
        (recrule->m_type == kWeeklyRecord) ||
        !recrule->m_subtitle.isEmpty())
        ret = kProbableTelevision;
    else
        ret = kProbableMovie;

    return ret;
}

LookupType GuessLookupType(const QString &inetref)
{
    if (inetref.isEmpty() || inetref == "00000000" ||
        inetref == MetaGrabberScript::CleanedInetref(inetref))
    {
        // can't determine subtype from inetref
        return kUnknownVideo;
    }

    // inetref is defined, see if we have a pre-defined grabber
    MetaGrabberScript grabber =
        MetaGrabberScript::FromInetref(inetref);

    if (!grabber.IsValid())
    {
        return kUnknownVideo;
    }

    switch (grabber.GetType())
    {
        case kGrabberMovie:
            return kProbableMovie;
        case kGrabberTelevision:
            return kProbableTelevision;
        default:
            return kUnknownVideo;
    }
}

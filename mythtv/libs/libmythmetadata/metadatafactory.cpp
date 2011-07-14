#include <QApplication>
#include <QList>
#include <QUrl>

#include "mythcontext.h"
#include "videoutils.h"

// Needed to perform a lookup
#include "metadatadownload.h"
#include "metadataimagedownload.h"

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

MetadataFactory::MetadataFactory(QObject *parent) :
    m_parent(parent)
{
    m_lookupthread = new MetadataDownload(this);
    m_imagedownload = new MetadataImageDownload(this);
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
}

void MetadataFactory::Lookup(RecordingRule *recrule, bool automatic,
                             bool getimages)
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
                             bool getimages)
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
                             bool getimages)
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

void MetadataFactory::OnMultiResult(MetadataLookupList list)
{
    if (!list.size())
        return;

    QCoreApplication::postEvent(m_parent,
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
            int index = 0;
            int season = (int)lookup->GetSeason();
            if (season > 0 && season <= fanartlist.count())
                index = season - 1;
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
        QCoreApplication::postEvent(m_parent,
            new MetadataFactorySingleResult(lookup));
    }
}

void MetadataFactory::OnNoResult(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    QCoreApplication::postEvent(m_parent,
        new MetadataFactoryNoResult(lookup));
}

void MetadataFactory::OnImageResult(MetadataLookup *lookup)
{
    if (!lookup)
        return;

    QCoreApplication::postEvent(m_parent,
        new MetadataFactorySingleResult(lookup));
}

void MetadataFactory::customEvent(QEvent *levent)
{
    if (levent->type() == MetadataLookupEvent::kEventType)
    {
        MetadataLookupEvent *lue = (MetadataLookupEvent *)levent;

        MetadataLookupList lul = lue->lookupList;

        if (lul.isEmpty())
            return;

        if (lul.count() == 1)
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

        OnImageResult(lookup);
    }
}

// These functions exist to determine if we have enough
// information to conclusively call something a Show vs. Movie

LookupType GuessLookupType(ProgramInfo *pginfo)
{
    LookupType ret = kUnknownVideo;

    QString catType = pginfo->GetCategoryType();
    if (catType.isEmpty())
        catType = pginfo->QueryCategoryType();

    if (catType == "series" || catType == "tvshow" ||
        catType == "show")
        ret = kProbableTelevision;
    else if (catType == "movie" || catType == "film")
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
        rule->LoadByProgram(pginfo);
        int ruleepisode = 0;
        if (rule && rule->Load())
            ruleepisode = rule->m_episode;
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


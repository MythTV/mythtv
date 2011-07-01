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

void MetadataFactory::Lookup(ProgramInfo *pginfo, bool automatic,
                             bool getimages)
{
    if (!pginfo)
        return;

    MetadataLookup *lookup = new MetadataLookup();
    lookup->SetStep(SEARCH);
    lookup->SetType(VID);
    lookup->SetData(qVariantFromValue(pginfo));
    lookup->SetAutomatic(automatic);
    lookup->SetHandleImages(getimages);
    lookup->SetHost(pginfo->GetHostname());
    lookup->SetTitle(pginfo->GetTitle());
    lookup->SetSubtitle(pginfo->GetSubtitle());

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
    lookup->SetStep(SEARCH);
    lookup->SetType(VID);
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

        ArtworkList coverartlist = lookup->GetArtwork(COVERART);
        if (coverartlist.size())
        {
            ArtworkInfo info;
            info.url = coverartlist.takeFirst().url;
            map.insert(COVERART, info);
        }

        ArtworkList fanartlist = lookup->GetArtwork(FANART);
        if (fanartlist.size())
        {
            ArtworkInfo info;
            info.url = fanartlist.takeFirst().url;
            map.insert(FANART, info);
        }

        ArtworkList bannerlist = lookup->GetArtwork(BANNER);
        if (bannerlist.size())
        {
            ArtworkInfo info;
            info.url = bannerlist.takeFirst().url;
            map.insert(BANNER, info);
        }

        ArtworkList screenshotlist = lookup->GetArtwork(SCREENSHOT);
        if (screenshotlist.size())
        {
            ArtworkInfo info;
            info.url = screenshotlist.takeFirst().url;
            map.insert(SCREENSHOT, info);
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


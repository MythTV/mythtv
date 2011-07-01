#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuistatetype.h"
#include "mythmainwindow.h"
#include "mythdialogbox.h"
#include "mythdirs.h"

#include "mythuimetadataresults.h"

MetadataResultsDialog::MetadataResultsDialog(
    MythScreenStack *lparent,
    const MetadataLookupList results) :

    MythScreenType(lparent, "metadataresultspopup"),
    m_results(results),
    m_resultsList(0)
{
    m_imageDownload = new MetadataImageDownload(this);
}

MetadataResultsDialog::~MetadataResultsDialog()
{
    cleanCacheDir();

    if (m_imageDownload)
    {
        delete m_imageDownload;
        m_imageDownload = NULL;
    }
}

bool MetadataResultsDialog::Create()
{
    if (!LoadWindowFromXML("base.xml", "MythMetadataResults", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_resultsList, "results", &err);
    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythMetadataResults'");
        return false;
    }

    for (MetadataLookupList::const_iterator i = m_results.begin();
            i != m_results.end(); ++i)
    {
        MythUIButtonListItem *button =
            new MythUIButtonListItem(m_resultsList,
                (*i)->GetTitle());
        MetadataMap metadataMap;
        (*i)->toMap(metadataMap);

        QString coverartfile;
        ArtworkList art = (*i)->GetArtwork(COVERART);
        if (art.count() > 0)
            coverartfile = art.takeFirst().thumbnail;
        else
        {
            art = (*i)->GetArtwork(SCREENSHOT);
            if (art.count() > 0)
                coverartfile = art.takeFirst().thumbnail;
        }

        QString dlfile = getDownloadFilename((*i)->GetTitle(),
            coverartfile);

        if (!coverartfile.isEmpty())
        {
            int pos = m_resultsList->GetItemPos(button);

            if (QFile::exists(dlfile))
                button->SetImage(dlfile);
            else
                m_imageDownload->addThumb((*i)->GetTitle(),
                                 coverartfile,
                                 qVariantFromValue<uint>(pos));
        }

        button->SetTextFromMap(metadataMap);
        button->SetData(qVariantFromValue<MetadataLookup*>(*i));
    }

    connect(m_resultsList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(sendResult(MythUIButtonListItem *)));

    BuildFocusList();

    return true;
}

void MetadataResultsDialog::cleanCacheDir()
{
    QString cache = QString("%1/thumbcache")
                       .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (QStringList::const_iterator i = thumbs.end() - 1;
            i != thumbs.begin() - 1; --i)
    {
        QString filename = QString("%1/%2").arg(cache).arg(*i);
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(2) < QDateTime::currentDateTime())
        {
            VERBOSE(VB_GENERAL|VB_EXTRA, QString("Deleting old cache file %1")
                  .arg(filename));
            QFile::remove(filename);
        }
    }
}

void MetadataResultsDialog::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        ThumbnailDLEvent *tde = (ThumbnailDLEvent *)event;

        ThumbnailData *data = tde->thumb;

        QString file = data->url;
        uint pos = qVariantValue<uint>(data->data);

        if (file.isEmpty())
            return;

        if (!((uint)m_resultsList->GetCount() >= pos))
            return;

        MythUIButtonListItem *item =
                  m_resultsList->GetItemAt(pos);

        if (item)
        {
            item->SetImage(file);
        }
        delete data;
        data = NULL;
    }
}

void MetadataResultsDialog::sendResult(MythUIButtonListItem* item)
{
    emit haveResult(qVariantValue<MetadataLookup *>(item->GetData()));
    Close();
}


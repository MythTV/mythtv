
#include "mythuimetadataresults.h"

#include "libmythbase/mythdate.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuistatetype.h"
#include "libmythui/mythuitext.h"

#include "metadatadownload.h"
#include "metadataimagedownload.h"

MetadataResultsDialog::MetadataResultsDialog(
    MythScreenStack *lparent,
    const MetadataLookupList &results) :

    MythScreenType(lparent, "metadataresultspopup"),
    m_results(results),
    m_imageDownload(new MetadataImageDownload(this))
{
}

MetadataResultsDialog::~MetadataResultsDialog()
{
    cleanCacheDir();

    if (m_imageDownload)
    {
        delete m_imageDownload;
        m_imageDownload = nullptr;
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
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythMetadataResults'");
        return false;
    }

    for (int i = 0;
            i != m_results.count(); ++i)
    {
        auto *button = new MythUIButtonListItem(m_resultsList,
                                                m_results[i]->GetTitle());
        InfoMap metadataMap;
        m_results[i]->toMap(metadataMap);

        QString coverartfile;
        ArtworkList art = m_results[i]->GetArtwork(kArtworkCoverart);
        if (art.count() > 0)
            coverartfile = art.takeFirst().thumbnail;

        if (coverartfile.isEmpty())
        {
            art = m_results[i]->GetArtwork(kArtworkBanner);
            if (art.count() > 0)
               coverartfile = art.takeFirst().thumbnail;
        }

        if (coverartfile.isEmpty())
        {
            art = m_results[i]->GetArtwork(kArtworkScreenshot);
            if (art.count() > 0)
                coverartfile = art.takeFirst().thumbnail;
        }

        QString dlfile = getDownloadFilename(m_results[i]->GetTitle(),
            coverartfile);

        if (!coverartfile.isEmpty())
        {
            int pos = m_resultsList->GetItemPos(button);

            if (QFile::exists(dlfile))
                button->SetImage(dlfile);
            else
            {
                m_imageDownload->addThumb(m_results[i]->GetTitle(),
                                 coverartfile,
                                 QVariant::fromValue<uint>(pos));
            }
        }

        button->SetTextFromMap(metadataMap);
        button->SetData(QVariant::fromValue<uint>(i));
    }

    connect(m_resultsList, &MythUIButtonList::itemClicked,
            this, &MetadataResultsDialog::sendResult);

    BuildFocusList();

    return true;
}

void MetadataResultsDialog::cleanCacheDir()
{
    QString cache = QString("%1/cache/metadata-thumbcache")
                       .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (auto i = thumbs.crbegin(); i != thumbs.crend(); ++i)
    {
        QString filename = QString("%1/%2").arg(cache, *i);
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(2) < MythDate::current())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Deleting old cache file %1")
                  .arg(filename));
            QFile::remove(filename);
        }
    }
}

void MetadataResultsDialog::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        auto *tde = dynamic_cast<ThumbnailDLEvent *>(event);
        if (tde == nullptr)
            return;

        ThumbnailData *data = tde->m_thumb;

        QString file = data->url;
        uint pos = data->data.value<uint>();

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
    }
}

void MetadataResultsDialog::sendResult(MythUIButtonListItem* item)
{
    RefCountHandler<MetadataLookup> lookup = m_results.takeAtAndDecr(item->GetData().value<uint>());
    m_results.clear();
    emit haveResult(lookup);
    Close();
}

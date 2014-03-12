#include <QFile>
#include <QDir>

#include "mythdirs.h"
#include "mythdate.h"

#include "mythuiimageresults.h"

ImageSearchResultsDialog::ImageSearchResultsDialog(
    MythScreenStack *lparent,
    const ArtworkList list,
    const VideoArtworkType type) :

    MythScreenType(lparent, "videosearchresultspopup"),
    m_list(list),
    m_type(type),
    m_resultsList(0)
{
    m_imageDownload = new MetadataImageDownload(this);
}

ImageSearchResultsDialog::~ImageSearchResultsDialog()
{
    cleanCacheDir();

    if (m_imageDownload)
    {
        delete m_imageDownload;
        m_imageDownload = NULL;
    }
}

bool ImageSearchResultsDialog::Create()
{
    if (!LoadWindowFromXML("base.xml", "MythArtworkResults", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_resultsList, "results", &err);
    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythArtworkResults'");
        return false;
    }

    for (ArtworkList::const_iterator i = m_list.begin();
            i != m_list.end(); ++i)
    {
            ArtworkInfo info = (*i);
            MythUIButtonListItem *button =
                new MythUIButtonListItem(m_resultsList,
                QString());
            button->SetText(info.label, "label");
            button->SetText(info.thumbnail, "thumbnail");
            button->SetText(info.url, "url");
            QString width = QString::number(info.width);
            QString height = QString::number(info.height);
            button->SetText(width, "width");
            button->SetText(height, "height");
            if (info.width > 0 && info.height > 0)
                button->SetText(QString("%1x%2").arg(width).arg(height),
                    "resolution");

            QString artfile = info.thumbnail;

            if (artfile.isEmpty())
                artfile = info.url;

            QString dlfile = getDownloadFilename(info.label,
                artfile);

            if (!artfile.isEmpty())
            {
                int pos = m_resultsList->GetItemPos(button);

                if (QFile::exists(dlfile))
                    button->SetImage(dlfile);
                else
                    m_imageDownload->addThumb(info.label,
                                     artfile,
                                     qVariantFromValue<uint>(pos));
            }

            button->SetData(qVariantFromValue<ArtworkInfo>(*i));
        }

    connect(m_resultsList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(sendResult(MythUIButtonListItem *)));

    BuildFocusList();

    return true;
}

void ImageSearchResultsDialog::cleanCacheDir()
{
    QString cache = QString("%1/cache/metadata-thumbcache")
               .arg(GetConfDir());
    QDir cacheDir(cache);
    QStringList thumbs = cacheDir.entryList(QDir::Files);

    for (QStringList::const_iterator i = thumbs.end() - 1;
            i != thumbs.begin() - 1; --i)
    {
        QString filename = QString("%1/%2").arg(cache).arg(*i);
        QFileInfo fi(filename);
        QDateTime lastmod = fi.lastModified();
        if (lastmod.addDays(2) < MythDate::current())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Deleting file %1")
                  .arg(filename));
            QFile::remove(filename);
        }
    }
}

void ImageSearchResultsDialog::customEvent(QEvent *event)
{
    if (event->type() == ThumbnailDLEvent::kEventType)
    {
        ThumbnailDLEvent *tde = (ThumbnailDLEvent *)event;

        ThumbnailData *data = tde->thumb;

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

void ImageSearchResultsDialog::sendResult(MythUIButtonListItem* item)
{
    emit haveResult(item->GetData().value<ArtworkInfo>(),
                    m_type);
    Close();
}


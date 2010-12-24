#include <QHash>

#include "bluraymetadata.h"
#include "mythdirs.h"

BlurayMetadata::BlurayMetadata(const QString path) :
    m_bdnav(NULL),         m_metadata(NULL),
    m_title(QString()),    m_alttitle(QString()),
    m_language(QString()), m_discnumber(0),
    m_disctotal(0),        m_path(path),
    m_images(QStringList())
{
}

BlurayMetadata::~BlurayMetadata()
{
    if (m_bdnav)
       bd_close(m_bdnav);
}

void BlurayMetadata::Parse(void)
{
    QString keyfile = QString("%1/KEYDB.cfg").arg(GetConfDir());
    QByteArray keyarray = keyfile.toAscii();
    const char *keyfilepath = keyarray.data();

    m_bdnav = bd_open(m_path.toLatin1().data(), keyfilepath);

    if (!m_bdnav)
        return;

    m_metadata = bd_get_meta(m_bdnav);

    if (!m_metadata)
        return;

    m_title = QString(m_metadata->di_name);
    m_alttitle = QString(m_metadata->di_alternative);
    m_language = QString(m_metadata->language_code);
    m_discnumber = m_metadata->di_set_number;
    m_disctotal = m_metadata->di_num_sets;

    for (unsigned i = 0; i < m_metadata->toc_count; i++)
    {
        uint num = m_metadata->toc_entries[i].title_number;
        QString title = QString(m_metadata->toc_entries[i].title_name);
        QPair<uint,QString> ret(num,title); 
        m_titles.append(ret);
    }

    for (unsigned i = 0; i < m_metadata->thumb_count; i++)
    {
        QString filepath = QString("%1/BDMV/META/DL/%2")
                               .arg(m_path)
                               .arg(m_metadata->thumbnails[i].path);
        m_images.append(filepath);
    }
}

void BlurayMetadata::toMap(MetadataMap &metadataMap)
{
    metadataMap["title"] = m_title;
    metadataMap["alttitle"] = m_alttitle;
    metadataMap["language"] = m_language;

    metadataMap["discnumber"] = QString::number(m_discnumber);
    metadataMap["disctotal"] = QString::number(m_disctotal);
    metadataMap["discseries"] = QObject::tr("%1 of %2")
                                    .arg(m_discnumber)
                                    .arg(m_disctotal);

    metadataMap["numtitles"] = m_titles.count();
    metadataMap["numthumbs"] = m_images.count();
}
